#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 1, rgba32f ) uniform image2D accumulatorColor;
layout( binding = 2, rgba32f ) uniform image2D accumulatorNormalsAndDepth;
layout( binding = 3, rgba8ui ) uniform uimage2D blueNoise;

#define PI 3.1415926535897932384626433832795
#define AA 1 // AA value of 2 means each sample is actually 2*2 = 4 offset samples

// core rendering stuff
uniform ivec2	tileOffset;			// tile renderer offset for the current tile
uniform ivec2	noiseOffset;		// jitters the noise sample read locations
uniform int		maxSteps;			// max steps to hit
uniform int		maxBounces;			// number of pathtrace bounces
uniform float	maxDistance;		// maximum ray travel
uniform float 	understep;			// scale factor on distance, when added as raymarch step
uniform float	epsilon;			// how close is considered a surface hit
uniform int		normalMethod;		// selector for normal computation method
uniform float	focusDistance;		// for thin lens approx
uniform float	thinLensIntensity;	// scalar on the thin lens DoF effect
uniform float	FoV;				// field of view
uniform float	exposure;			// exposure adjustment
uniform vec3	viewerPosition;		// position of the viewer
uniform vec3	basisX;				// x basis vector
uniform vec3	basisY;				// y basis vector
uniform vec3	basisZ;				// z basis vector
uniform int		wangSeed;			// integer value used for seeding the wang hash rng
uniform int		modeSelect;			// do we do a pathtrace sample, or just the preview

// render modes
#define PATHTRACE		0
#define PREVIEW_DIFFUSE	1
#define PREVIEW_NORMAL	2
#define PREVIEW_DEPTH	3
#define PREVIEW_SHADED	4 // TODO: some basic phong + AO... tbd

// lens parameters
uniform float lensScaleFactor;		// scales the lens DE
uniform float lensRadius1;			// radius of the sphere for the first side
uniform float lensRadius2;			// radius of the sphere for the second side
uniform float lensThickness;		// offset between the two spheres
uniform float lensRotate;			// rotating the displacement offset betwee spheres
uniform float lensIOR;				// index of refraction for the lens

// scene parameters
uniform vec3 redWallColor;
uniform vec3 greenWallColor;
uniform vec3 whiteWallColor;
uniform vec3 floorCielingColor;
uniform vec3 metallicDiffuse;

// global state
	// requires manual management of geo, to ensure that the lens material does not intersect with itself
float refractState = 1.0f; // multiply by the lens distance estimate, to invert when inside a refractive object
float sampleCount = 0.0f;

bool boundsCheck ( ivec2 loc ) { // used to abort off-image samples
	ivec2 bounds = ivec2( imageSize( accumulatorColor ) ).xy;
	return ( loc.x < bounds.x && loc.y < bounds.y );
}

vec4 blueNoiseReference ( ivec2 location ) { // jitter source
	location += noiseOffset;
	location.x = location.x % imageSize( blueNoise ).x;
	location.y = location.y % imageSize( blueNoise ).y;
	return vec4( imageLoad( blueNoise, location ) / 255.0f );
}

// random utilites
uint seed = 0;
uint wangHash () {
	seed = uint( seed ^ uint( 61 ) ) ^ uint( seed >> uint( 16 ) );
	seed *= uint( 9 );
	seed = seed ^ ( seed >> 4 );
	seed *= uint( 0x27d4eb2d );
	seed = seed ^ ( seed >> 15 );
	return seed;
}

float randomFloat () {
	return float( wangHash() ) / 4294967296.0f;
}

vec3 randomUnitVector () {
	float z = randomFloat() * 2.0f - 1.0f;
	float a = randomFloat() * 2.0f * PI;
	float r = sqrt( 1.0f - z * z );
	float x = r * cos( a );
	float y = r * sin( a );
	return vec3( x, y, z );
}

vec2 randomInUnitDisk () {
	return randomUnitVector().xy;
}

mat3 rotate3D ( float angle, vec3 axis ) {
	vec3 a = normalize( axis );
	float s = sin( angle );
	float c = cos( angle );
	float r = 1.0f - c;
	return mat3(
		a.x * a.x * r + c,
		a.y * a.x * r + a.z * s,
		a.z * a.x * r - a.y * s,
		a.x * a.y * r - a.z * s,
		a.y * a.y * r + c,
		a.z * a.y * r + a.x * s,
		a.x * a.z * r + a.y * s,
		a.y * a.z * r - a.x * s,
		a.z * a.z * r + c
	);
}

float fOpIntersectionRound ( float a, float b, float r ) {
	vec2 u = max( vec2( r + a, r + b ), vec2( 0.0f ) );
	return min( -r, max ( a, b ) ) + length( u );
}

// Repeat in two dimensions
vec2 pMod2 ( inout vec2 p, vec2 size ) {
	vec2 c = floor( ( p + size * 0.5f ) / size );
	p = mod( p + size * 0.5f, size ) - size * 0.5f;
	return c;
}

// 0 nohit
#define NOHIT 0
// 1 diffuse
#define DIFFUSE 1
// 2 specular
#define SPECULAR 2
// 3 emissive
#define EMISSIVE 3
// 4 refractive
#define REFRACTIVE 4

vec3 hitpointColor = vec3( 0.0f );
int hitpointSurfaceType = NOHIT; // identifier for the hit surface
float deLens ( vec3 p ){
	// lens SDF
	p /= lensScaleFactor;
	float dFinal;
	float center1 = lensRadius1 - lensThickness / 2.0f;
	float center2 = -lensRadius2 + lensThickness / 2.0f;
	vec3 pRot = rotate3D( 0.1f * lensRotate, vec3( 1.0f ) ) * p;
	float sphere1 = distance( pRot, vec3( 0.0f, center1, 0.0f ) ) - lensRadius1;
	float sphere2 = distance( pRot, vec3( 0.0f, center2, 0.0f ) ) - lensRadius2;

	// dFinal = fOpIntersectionRound( sphere1, sphere2, 0.03 );
	dFinal = max( sphere1, sphere2 );
	return dFinal * lensScaleFactor * refractState;
}

float dePlane ( vec3 p, vec3 normal, float distanceFromOrigin ) {
	return dot( p, normal ) + distanceFromOrigin;
}

//sphere inversion
bool SI = false;
vec3 InvCenter = vec3( 0.0f, 1.0f, 1.0f );
float rad = 0.8f;
vec2 box_size = vec2( -0.40445f, 0.34f ) * 2.0f;
vec2 wrap ( vec2 x, vec2 a, vec2 s ) {
	x -= s;
	return ( x - a * floor( x / a ) ) + s;
}
void TransA ( inout vec3 z, inout float DF, float a, float b ) {
	float iR = 1.0f / dot( z, z );
	z *= -iR;
	z.x = -b - z.x;
	z.y = a + z.y;
	DF *= iR;//max(1.,iR);
}
float deLoopy ( vec3 p ) {
	p = p.yzx;
	float adjust = 6.28f; // use this for time varying behavior
	float box_size_x = 1.0f;
	float box_size_z = 1.0f;
	float KleinR = 1.95859103011179f;
	float KleinI = 0.0112785606117658f;
	vec3 lz = p + vec3( 1.0f ), llz = p + vec3( -1.0f );
	float d = 0.0f; float d2 = 0.0f;
	vec3 InvCenter = vec3( 1.0f, 1.0f, 0.0f );
	float rad = 0.8f;
	p = p - InvCenter;
	d = length( p );
	d2 = d * d;
	p = ( rad * rad / d2 ) * p + InvCenter;
	float DE = 1e10f;
	float DF = 1.0f;
	float a = KleinR;
	float b = KleinI;
	float f = sign( b ) * 1.0f;
	for ( int i = 0; i < 69; i++ ) {
		p.x = p.x + b / a * p.y;
		p.xz = wrap( p.xz, vec2( 2.0f * box_size_x, 2.0f * box_size_z ), vec2( -box_size_x, - box_size_z ) );
		p.x = p.x - b / a * p.y;
		if ( p.y >= a * 0.5f + f *( 2.0f * a - 1.95f ) / 4.0f * sign( p.x + b * 0.5f ) * ( 1.0f - exp( -( 7.2f - ( 1.95f - a ) * 15.0f )* abs( p.x + b * 0.5f ) ) ) ) {
			p = vec3( -b, a, 0.0f ) - p; //If above the separation line, rotate by 180° about (-b/2, a/2)
		}
		TransA( p, DF, a, b ); //Apply transformation a
		if ( dot( p - llz, p - llz ) < 1e-5f ) {
			break; //If the iterated points enters a 2-cycle , bail out.
		}
		llz = lz; lz = p; //Store previous iterates
	}
	float y = min( p.y, a-p.y );
	DE = min( DE, min( y, 0.3f ) / max( DF, 2.0f ) );
	DE = DE * d2 / ( rad + d * DE );
	return DE;
}

mat3 rotZ ( float t ) {
	float s = sin( t );
	float c = cos( t );
	return mat3( c, s, 0.0f, -s, c, 0.0f, 0.0f, 0.0f, 1.0f );
}
mat3 rotX ( float t ) {
	float s = sin( t );
	float c = cos( t );
	return mat3( 1.0f, 0.0f, 0.0f, 0.0f, c, s, 0.0f, -s, c );
}
mat3 rotY ( float t ) {
	float s = sin( t );
	float c = cos( t );
	return mat3( c, 0.0f, -s, 0.0f, 1.0f, 0.0f, s, 0.0f, c );
}
float deBowl ( vec3 p ) {
	p = rotY( PI / 2.0f ) * p;
	p = rotZ( PI / 2.0f ) * p;
	vec2 rm = radians( 360.0f ) * vec2( 0.468359f, 0.95317f ); // vary x,y 0.0 - 1.0
	mat3 scene_mtx = rotX( rm.x ) * rotY( rm.x ) * rotZ( rm.x ) * rotX( rm.y );
	float scaleAccum = 1.;
	for ( int i = 0; i < 24; ++i ) {
		p.yz = sqrt( p.yz * p.yz + 0.16406f );
		p *= 1.21f;
		scaleAccum *= 1.21f;
		p -= vec3( 2.43307f, 5.28488f, 0.9685f );
		p = scene_mtx * p;
	}
	return length( p ) / scaleAccum - 0.15f;
}

// surface distance estimate for the whole scene
float de ( vec3 p ) {
	// init nohit, far from surface, no diffuse color
	hitpointSurfaceType = NOHIT;
	float sceneDist = 1000.0f;
	hitpointColor = vec3( 0.0f );

	// cornell box
	// red wall ( left )
	float dRedWall = dePlane( p, vec3( 1.0f, 0.0f, 0.0f ), 10.0f );
	sceneDist = min( dRedWall, sceneDist );
	if ( sceneDist == dRedWall && dRedWall <= epsilon ) {
		hitpointColor = redWallColor;
		hitpointSurfaceType = DIFFUSE;
	}

	// green wall ( right )
	float dGreenWall = dePlane( p, vec3( -1.0f, 0.0f, 0.0f ), 10.0f );
	sceneDist = min( dGreenWall, sceneDist );
	if ( sceneDist == dGreenWall && dGreenWall <= epsilon ) {
		hitpointColor = greenWallColor;
		hitpointSurfaceType = DIFFUSE;
	}

	// white walls ( front and back )
	float dWhiteWalls = min( dePlane( p, vec3( 0.0f, 0.0f, 1.0f ), 20.0f ),  dePlane( p, vec3( 0.0f, 0.0f, -1.0f ), 20.0f ) );
	sceneDist = min( dWhiteWalls, sceneDist );
	if ( sceneDist == dWhiteWalls && dWhiteWalls <= epsilon ) {
		hitpointColor = whiteWallColor;
		hitpointSurfaceType = DIFFUSE;
	}

	// fractal object
	float dFractal = 0.05f * deBowl( ( p / 0.05f ) - vec3( 0.0f, 1.0f, 2.0f ) );
	sceneDist = min( dFractal, sceneDist );
	if ( sceneDist == dFractal && dFractal <= epsilon ) {
		hitpointColor = metallicDiffuse;
		hitpointSurfaceType = SPECULAR;
	}

	// lens object
	float dLens = deLens( p );
	sceneDist = min( dLens, sceneDist );
	if ( sceneDist == dLens && dLens <= epsilon ) {
		hitpointColor = vec3( 0.11f );
		hitpointSurfaceType = REFRACTIVE;
	}

	// cieling and floor
	float dFloorCieling = min( dePlane( p, vec3( 0.0f, -1.0f, 0.0f ), 10.0f ), dePlane( p, vec3( 0.0f, 1.0f, 0.0f ), 7.5f ) );
	sceneDist = min( dFloorCieling, sceneDist );
	if ( sceneDist == dFloorCieling && dFloorCieling <= epsilon ) {
		hitpointColor = floorCielingColor;
		hitpointSurfaceType = DIFFUSE;
	}

	pMod2( p.xz, vec2( 2.0f ) );
	float dLightBall = distance( p, vec3( 0.0f, 7.5f, 0.0f ) ) - 0.1618f;
	sceneDist = min( dLightBall, sceneDist );
	if ( sceneDist == dLightBall && dLightBall <= epsilon ) {
		hitpointColor = vec3( 0.8f );
		hitpointSurfaceType = EMISSIVE;
	}

	// wang hash seeded scattered emissive spheres of random colors in the negative space? maybe refractive, not sure
		// need to make sure that the seed is constant, and the existing seed is cached and restored, if I'm going to do this

	return sceneDist;
}

// fake AO, computed from SDF
float calcAO ( in vec3 position, in vec3 normal ) {
	float occ = 0.0f;
	float sca = 1.0f;
	for( int i = 0; i < 5; i++ ) {
		float h = 0.001f + 0.15f * float( i ) / 4.0f;
		float d = de( position + h * normal );
		occ += ( h - d ) * sca;
		sca *= 0.95f;
	}
	return clamp( 1.0f - 1.5f * occ, 0.0f, 1.0f );
}

// normalized gradient of the SDF - 3 different methods
vec3 normal ( vec3 p ) {
	vec2 e;
	switch( normalMethod ) {
		case 0: // tetrahedron version, unknown original source - 4 DE evaluations
			e = vec2( 1.0f, -1.0f ) * epsilon / 10.0f;
			return normalize( e.xyy * de( p + e.xyy ) + e.yyx * de( p + e.yyx ) + e.yxy * de( p + e.yxy ) + e.xxx * de( p + e.xxx ) );
			break;

		case 1: // from iq = more efficient, 4 DE evaluations
			e = vec2( epsilon, 0.0f ) / 10.0f;
			return normalize( vec3( de( p ) ) - vec3( de( p - e.xyy ), de( p - e.yxy ), de( p - e.yyx ) ) );
			break;

		case 2: // from iq - less efficient, 6 DE evaluations
			e = vec2( epsilon, 0.0f );
			return normalize( vec3( de( p + e.xyy ) - de( p - e.xyy ), de( p + e.yxy ) - de( p - e.yxy ), de( p + e.yyx ) - de( p - e.yyx ) ) );
			break;

		default:
			break;
	}
}

// raymarches to the next hit
float raymarch ( vec3 origin, vec3 direction ) {
	float dQuery = 0.0f;
	float dTotal = 0.0f;
	for ( int steps = 0; steps < maxSteps; steps++ ) {
		vec3 pQuery = origin + dTotal * direction;
		dQuery = de( pQuery );
		dTotal += dQuery * understep;
		if ( dTotal > maxDistance || abs( dQuery ) < epsilon ) {
			break;
		}
	}
	return dTotal;
}

ivec2 location = ivec2( 0, 0 );	// 2d location, pixel coords
vec3 colorSample ( vec3 rayOrigin_in, vec3 rayDirection_in ) {

	vec3 rayOrigin = rayOrigin_in, previousRayOrigin;
	vec3 rayDirection = rayDirection_in, previousRayDirection;
	vec3 finalColor = vec3( 0.0f );
	vec3 throughput = vec3( 1.0f );

	// bump origin up by unit vector - creates fuzzy / soft section plane
	rayOrigin += rayDirection * ( 0.9f + 0.1f * blueNoiseReference( location ).x );

	// debug output
	if ( modeSelect != PATHTRACE ) {
		const float rayDistance = raymarch( rayOrigin, rayDirection );
		const vec3 pHit = rayOrigin + rayDistance * rayDirection;
		const vec3 hitpointNormal = normal( pHit );
		const vec3 hitpointDepth = vec3( 1.0f / rayDistance );
		if ( de( pHit ) < epsilon ) {
			switch ( modeSelect ) {
				case PREVIEW_DIFFUSE: return hitpointColor; break;
				case PREVIEW_NORMAL: return hitpointNormal; break;
				case PREVIEW_DEPTH: return hitpointDepth; break;
			}
		} else {
			return vec3( 0.0f );
		}
	}

	// loop to max bounces
	for( int bounce = 0; bounce < maxBounces; bounce++ ) {
		float dResult = raymarch( rayOrigin, rayDirection );

		// cache previous values of rayOrigin, rayDirection, and get new hit position
		previousRayOrigin = rayOrigin;
		previousRayDirection = rayDirection;
		rayOrigin = rayOrigin + dResult * rayDirection;

		// surface normal at the new hit position
		vec3 hitNormal = normal( rayOrigin );

		// bump rayOrigin along the normal to prevent false positive hit on next bounce
			// now you are at least epsilon distance from the surface, so you won't immediately hit
		rayOrigin += 2.0f * epsilon * hitNormal;

	// these are mixed per-material
		// construct new rayDirection vector, diffuse reflection off the surface
		vec3 reflectedVector = reflect( previousRayDirection, hitNormal );

		vec3 randomVectorDiffuse = normalize( ( 1.0f + epsilon ) * hitNormal + randomUnitVector() );
		vec3 randomVectorSpecular = normalize( ( 1.0f + epsilon ) * hitNormal + mix( reflectedVector, randomUnitVector(), 0.1f ) );

		// currently just implementing diffuse and emissive behavior
			// eventually add different ray behaviors for each material here
		switch ( hitpointSurfaceType ) {

			case EMISSIVE:
				finalColor += throughput * hitpointColor;
				break;

			case DIFFUSE:
				rayDirection = randomVectorDiffuse;
				throughput *= hitpointColor; // attenuate throughput by surface albedo
				break;

			case SPECULAR:
				rayDirection = mix( randomVectorDiffuse, randomVectorSpecular, 0.7f );
				throughput *= hitpointColor;
				break;

			case REFRACTIVE:
				// ray refracts, instead of bouncing
				// for now, perfect reflector with small attenuation
				rayDirection = reflectedVector;
				throughput *= 0.7f;

				// flip the sign to invert the lens material distance estimate, because the ray is either entering or leaving a refractive medium
				// refractState = -1.0 * refractState;
				break;

			default:
				break;

		}

		// russian roulette termination - chance for ray to quit early
		float maxChannel = max( throughput.r, max( throughput.g, throughput.b ) );
		if ( randomFloat() > maxChannel ) break;

		// russian roulette compensation term
		throughput *= 1.0f / maxChannel;
	}

	return finalColor;
}

#define BLUE
vec2 getRandomOffset ( int n ) {
	// weyl sequence from http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/ and https://www.shadertoy.com/view/4dtBWH
	#ifdef UNIFORM
		return fract( vec2( 0.0f ) + vec2( n * 12664745, n * 9560333 ) / exp2( 24.0f ) );	// integer mul to avoid round-off
	#endif
	#ifdef UNIFORM2
		return fract( vec2( 0.0f ) + float( n ) * vec2( 0.754877669f, 0.569840296f ) );
	#endif
	// wang hash random offsets
	#ifdef RANDOM
		return vec2( randomFloat(), randomFloat() );
	#endif
	#ifdef BLUE
		return blueNoiseReference( ivec2( gl_GlobalInvocationID.xy ) ).xy;
	#endif
}

// void storeNormalAndDepth ( vec3 normal, float depth ) {
// 	// blend with history and imageStore
// 	vec4 prevResult = imageLoad( accumulatorNormalsAndDepth, location );
// 	vec4 blendResult = mix( prevResult, vec4( normal, depth ), 1.0f / sampleCount );
// 	imageStore( accumulatorNormalsAndDepth, location, blendResult );
// }

vec3 pathtraceSample ( ivec2 location, int n ) {
	vec3  cResult = vec3( 0.0f );
	vec3  nResult = vec3( 0.0f );
	float dResult = 0.0f;

#if AA != 1
	// at AA = 2, this is 4 samples per invocation
	const float normalizeTerm = float( AA * AA );
	for ( int x = 0; x < AA; x++ ) {
		for ( int y = 0; y < AA; y++ ) {
#endif

			// pixel offset + mapped position
			// vec2 offset = vec2( x + randomFloat(), y + randomFloat() ) / float( AA ) - 0.5; // previous method
			vec2 offset = getRandomOffset( n );
			vec2 halfScreenCoord = vec2( imageSize( accumulatorColor ) / 2.0f );
			vec2 mappedPosition = ( vec2( location + offset ) - halfScreenCoord ) / halfScreenCoord;

			// aspect ratio
			float aspectRatio = float( imageSize( accumulatorColor ).x ) / float( imageSize( accumulatorColor ).y );

			// ray origin + direction
			vec3 rayDirection = normalize( aspectRatio * mappedPosition.x * basisX + mappedPosition.y * basisY + ( 1.0f / FoV ) * basisZ );
			vec3 rayOrigin    = viewerPosition;

			// thin lens DoF - adjust view vectors to converge at focusDistance
				// this is a small adjustment to the ray origin and direction - not working correctly - need to revist this
			// vec3 focuspoint = rayOrigin + ( ( rayDirection * focusDistance ) / dot( rayDirection, basisZ ) );
			// vec2 diskOffset = thinLensIntensity * randomInUnitDisk();
			// rayOrigin += diskOffset.x * basisX + diskOffset.y * basisY + thinLensIntensity * randomFloat() * basisZ;
			// rayDirection = normalize( focuspoint - rayOrigin );

			// get depth and normals - think about special handling for refractive hits
			// float distanceToFirstHit = raymarch( rayOrigin, rayDirection );
			// storeNormalAndDepth( normal( rayOrigin + distanceToFirstHit * rayDirection ), distanceToFirstHit );

			// get the result for a ray
			cResult += colorSample( rayOrigin, rayDirection );

#if AA != 1
		}
	}
	// multisample compensation req'd
	cResult /= normalizeTerm;
#endif

	return cResult * exposure;
}

void main () {
	location = ivec2( gl_GlobalInvocationID.xy ) + tileOffset;
	seed = location.x * 1973 + location.y * 9277 + wangSeed;

	switch ( modeSelect ) {
		case PATHTRACE:
			if ( !boundsCheck( location ) ) return; // abort on out of bounds
			vec4 prevResult = imageLoad( accumulatorColor, location );
			sampleCount = prevResult.a + 1.0f;
			vec3 blendResult = mix( prevResult.rgb, pathtraceSample( location, int( sampleCount ) ), 1.0f / sampleCount );
			imageStore( accumulatorColor, location, vec4( blendResult, sampleCount ) );
			break;

		case PREVIEW_DIFFUSE:
		case PREVIEW_NORMAL:
		case PREVIEW_DEPTH:
			location = ivec2( gl_GlobalInvocationID.xy );
			imageStore( accumulatorColor, location, vec4( pathtraceSample( location, 0 ), 1.0f ) );
			break;

		default:
			return;
			break;
	}
}
