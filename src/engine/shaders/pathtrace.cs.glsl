#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 1, rgba32f ) uniform image2D accumulatorColor;
layout( binding = 2, rgba32f ) uniform image2D accumulatorNormalsAndDepth;
layout( binding = 3, rgba8ui ) uniform uimage2D blueNoise;

#include "hg_sdf.glsl" // SDF modeling functions

#define AA 1 // AA value of 2 means each sample is actually 2*2 = 4 offset samples, slows things way down

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
uniform float lensIoR;				// index of refraction for the lens

// scene parameters
uniform vec3 redWallColor;
uniform vec3 greenWallColor;
uniform vec3 whiteWallColor;
uniform vec3 floorCielingColor;
uniform vec3 metallicDiffuse;

// global state
	// requires manual management of geo, to ensure that the lens material does not intersect with itself
bool enteringRefractive = false; // multiply by the lens distance estimate, to invert when inside a refractive object
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

float normalizedRandomFloat () {
	return float( wangHash() ) / 4294967296.0f;
}

vec3 randomUnitVector () {
	float z = normalizedRandomFloat() * 2.0f - 1.0f;
	float a = normalizedRandomFloat() * 2.0f * PI;
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

#define NOHIT 0
#define DIFFUSE 1
#define PERFECTREFLECT 2
#define METALLIC 3
#define EMISSIVE 4
#define REFRACTIVE 5

// eventually, probably define a list of materials, and index into that - that will allow for
	// e.g. refractive materials of multiple different indices of refraction


// tdhooper variant 1 - spherical inversion
vec2 wrap ( vec2 x, vec2 a, vec2 s ) {
  x -= s;
  return (x - a * floor(x / a)) + s;
}

void TransA ( inout vec3 z, inout float DF, float a, float b ) {
  float iR = 1. / dot( z, z );
  z *= -iR;
  z.x = -b - z.x;
  z.y = a + z.y;
  DF *= iR; // max( 1.0, iR );
}

float deFractal( vec3 z ) {
  vec3 InvCenter = vec3( 0.0, 1.0, 1.0 );
  float rad = 0.8;
  float KleinR = 1.5 + 0.39;
  float KleinI = ( 0.55 * 2.0 - 1.0 );
  vec2 box_size = vec2( -0.40445, 0.34 ) * 2.0;
  vec3 lz = z + vec3( 1.0 ), llz = z + vec3( -1.0 );
  float d = 0.0; float d2 = 0.0;
  z = z - InvCenter;
  d = length( z );
  d2 = d * d;
  z = ( rad * rad / d2 ) * z + InvCenter;
  float DE = 1e12;
  float DF = 1.0;
  float a = KleinR;
  float b = KleinI;
  float f = sign( b ) * 0.45;
  for ( int i = 0; i < 80; i++ ) {
    z.x += b / a * z.y;
    z.xz = wrap( z.xz, box_size * 2.0, -box_size );
    z.x -= b / a * z.y;
    if ( z.y >= a * 0.5 + f * ( 2.0 * a - 1.95 ) / 4.0 * sign( z.x + b * 0.5 ) *
     ( 1.0 - exp( -( 7.2 - ( 1.95 - a ) * 15.0 )* abs(z.x + b * 0.5 ) ) ) ) {
      z = vec3( -b, a, 0.0 ) - z;
    } //If above the separation line, rotate by 180° about (-b/2, a/2)
    TransA( z, DF, a, b ); //Apply transformation a
    if ( dot( z - llz, z - llz ) < 1e-5 ) {
      break;
    } //If the iterated points enters a 2-cycle, bail out
    llz = lz; lz = z; //Store previous iterates
  }
  float y =  min(z.y, a - z.y);
  DE = min( DE, min( y, 0.3 ) / max( DF, 2.0 ) );
  DE = DE * d2 / ( rad + d * DE );
  return DE;
}

vec3 hitpointColor = vec3( 0.0f );
int hitpointSurfaceType = NOHIT; // identifier for the hit surface
float deLens ( vec3 p ) {
	// lens SDF
	p *= lensScaleFactor;
	float dFinal;
	float center1 = lensRadius1 - lensThickness / 2.0f;
	float center2 = -lensRadius2 + lensThickness / 2.0f;
	vec3 pRot = rotate3D( 0.1f * lensRotate, vec3( 1.0f ) ) * p;
	float sphere1 = distance( pRot, vec3( 0.0f, center1, 0.0f ) ) - lensRadius1;
	float sphere2 = distance( pRot, vec3( 0.0f, center2, 0.0f ) ) - lensRadius2;
	dFinal = fOpIntersectionRound( sphere1, sphere2, 0.03f );
	return dFinal / lensScaleFactor;
}

float deRoundedBox ( vec3 p, vec3 boxDims, float radius ){
	return length( max( abs( p ) - boxDims, 0.0f ) ) - radius;
}

vec3 GetColorForTemperature ( float temperature ) {
	mat3 m = ( temperature <= 6500.0f )
		? mat3( vec3( 0.0f, -2902.1955373783176f, -8257.7997278925690f ),
				vec3( 0.0f, 1669.5803561666639f, 2575.2827530017594f ),
				vec3( 1.0f, 1.3302673723350029f, 1.8993753891711275f ) )
		: mat3( vec3( 1745.0425298314172f, 1216.6168361476490f, -8257.7997278925690f ),
				vec3( -2666.3474220535695f, -2173.1012343082230f, 2575.2827530017594f ),
				vec3( 0.55995389139931482f, 0.70381203140554553f, 1.8993753891711275f ) );
	return mix( clamp( vec3( m[ 0 ] / ( vec3( clamp( temperature, 1000.0f, 40000.0f ) ) + m[ 1 ] )
		+ m[ 2 ] ), vec3( 0.0f ), vec3( 1.0f ) ), vec3( 1.0f ), smoothstep( 1000.0f, 0.0f, temperature ) );
}

// surface distance estimate for the whole scene
float de ( vec3 p ) {
	// init nohit, far from surface, no diffuse color
	hitpointSurfaceType = NOHIT;
	float sceneDist = 1000.0f;
	hitpointColor = vec3( 0.0f );

	// if ( room 1 bounds )

		// North, South, East, West walls
		float dNorthWall = fPlane( p, vec3(  0.0f, 0.0f, -1.0f ), 24.0f );
		float dSouthWall = fPlane( p, vec3(  0.0f, 0.0f, 1.0f ), 24.0f );
		float dEastWall = fPlane( p, vec3( -1.0f,  0.0f, 0.0f ), 10.0f );
		float dWestWall = fPlane( p, vec3( 1.0f,  0.0f, 0.0f ), 10.0f );
		float dWalls = fOpUnionRound( fOpUnionRound( fOpUnionRound( dNorthWall, dSouthWall, 0.5f ), dEastWall, 0.5f ), dWestWall, 0.5f );
		sceneDist = min( dWalls, sceneDist );
		if ( sceneDist == dWalls && dWalls < epsilon ) {
			hitpointColor = whiteWallColor;
			hitpointSurfaceType = DIFFUSE;
		}

		float dFloor = fPlane( p, vec3( 0.0f, 1.0f, 0.0f ), 4.0f );
		sceneDist = min( dFloor, sceneDist );
		if ( sceneDist == dFloor && dFloor < epsilon ) {
			hitpointColor = floorCielingColor;
			hitpointSurfaceType = DIFFUSE;
		}

		// balcony floor
		float dEastBalcony = fBox( p - vec3( 10.0f, 0.0f, 0.0f ), vec3( 4.0f, 0.1f, 48.0f ) );
		float dWestBalcony = fBox( p - vec3( -10.0f, 0.0f, 0.0f ), vec3( 4.0f, 0.1f, 48.0f ) );
		float dBalconies = min( dEastBalcony, dWestBalcony );
		sceneDist = min( dBalconies, sceneDist );
		if ( sceneDist == dBalconies && dBalconies < epsilon ) {
			hitpointColor = floorCielingColor;
			hitpointSurfaceType = DIFFUSE;
		}

		// store point value before applying repeat
		vec3 pCache = p;
		pMirror( p.x, 0.0f );

		// compute bounding box for the rails on both sides, using the mirrored point
		float dRailBounds = fBox( p - vec3( 7.0f, 1.625f, 0.0f ), vec3( 1.0f, 1.2f, 24.0f ) );

		// if railing bounding box is true
		float dRails;
		if ( dRailBounds < 0.0f )  {
			// railings - probably use some instancing on them, also want to use a bounding volume
			dRails = fCapsule( p, vec3( 7.0f, 2.4f, 24.0f ), vec3( 7.0f, 2.4f, -24.0f ), 0.3f );
			dRails = min( dRails, fCapsule( p, vec3( 7.0f, 0.6f, 24.0f ), vec3( 7.0f, 0.6f, -24.0f ), 0.1f ) );
			dRails = min( dRails, fCapsule( p, vec3( 7.0f, 1.1f, 24.0f ), vec3( 7.0f, 1.1f, -24.0f ), 0.1f ) );
			dRails = min( dRails, fCapsule( p, vec3( 7.0f, 1.6f, 24.0f ), vec3( 7.0f, 1.6f, -24.0f ), 0.1f ) );
			sceneDist = min( dRails, sceneDist );
			if ( sceneDist == dRails && dRails <= epsilon ) {
				hitpointColor = vec3( 0.618f );
				hitpointSurfaceType = METALLIC;
			}
		} // end railing bounding box

		// revert to original point value
		p = pCache;

		pMod1( p.x, 14.0f );
		p.z += 2.0f;
		pModMirror1( p.z, 4.0f );
		float dArches = fBox( p - vec3( 0.0f, 4.9f, 0.0f ), vec3( 10.0f, 5.0f, 5.0f ) );
		dArches = fOpDifferenceRound( dArches, deRoundedBox( p - vec3( 0.0f, 0.0f, 3.0f ), vec3( 10.0f, 4.5f, 1.0f ), 3.0f ), 0.2f );
		dArches = fOpDifferenceRound( dArches, deRoundedBox( p, vec3( 3.0f, 4.5f, 10.0f ), 3.0f ), 0.2f );

		// if railing bounding box is true
		if ( dRailBounds < 0.0f )  {
			dArches = fOpDifferenceRound( dArches, dRails - 0.05f, 0.1f );
		} // end railing bounding box

		sceneDist = min( dArches, sceneDist );
		if ( sceneDist == dArches && dArches < epsilon ) {
			hitpointColor = floorCielingColor;
			hitpointSurfaceType = DIFFUSE;
		}

		p = pCache;

		// the bar lights are the primary source of light in the scene
		float dCenterLightBar = fBox( p - vec3( 0.0f, 7.4f, 0.0f ), vec3( 1.0f, 0.1f, 24.0f ) );
		sceneDist = min( dCenterLightBar, sceneDist );
		if ( sceneDist == dCenterLightBar && dCenterLightBar <= epsilon ) {
			hitpointColor = 0.6f * GetColorForTemperature( 6500.0f );
			hitpointSurfaceType = EMISSIVE;
		}

		const vec3 coolColor = 0.8f * pow( GetColorForTemperature( 1000000.0f ), vec3( 3.0f ) );
		const vec3 midColor = 0.8f * GetColorForTemperature( 4000.0f );
		const vec3 warmColor = 0.8f * pow( GetColorForTemperature( 1000.0f ), vec3( 1.2f ) );

		float dSideLightBar1 = fBox( p - vec3( 7.5f, -0.4f, 0.0f ), vec3( 0.618f, 0.05f, 24.0f ) );
		sceneDist = min( dSideLightBar1, sceneDist );
		if ( sceneDist == dSideLightBar1 && dSideLightBar1 <= epsilon ) {
			hitpointColor = coolColor;
			hitpointSurfaceType = EMISSIVE;
		}

		float dSideLightBar2 = fBox( p - vec3( -7.5f, -0.4f, 0.0f ), vec3( 0.618f, 0.05f, 24.0f ) );
		sceneDist = min( dSideLightBar2, sceneDist );
		if ( sceneDist == dSideLightBar2 && dSideLightBar2 <= epsilon ) {
			hitpointColor = warmColor;
			hitpointSurfaceType = EMISSIVE;
		}

		float dLens = ( enteringRefractive ? -1.0f : 1.0f ) * deLens( p );
		sceneDist = min( dLens, sceneDist );
		if ( sceneDist == dLens && dLens <= epsilon ) {
			hitpointColor = vec3( 0.11f );
			hitpointSurfaceType = REFRACTIVE;
			enteringRefractive = !enteringRefractive;
		}

		// float scalar = 0.6f;
		// float dFractal = deFractal( p / scalar ) * scalar;
		// sceneDist = min( dFractal, sceneDist );
		// if ( sceneDist == dFractal && dFractal <= epsilon ) {
		// 	hitpointColor = metallicDiffuse;
		// 	hitpointSurfaceType = METALLIC;
		// }

	// end if for first room bounds

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
			e = vec2( 1.0f, -1.0f ) * epsilon;
			return normalize( e.xyy * de( p + e.xyy ) + e.yyx * de( p + e.yyx ) + e.yxy * de( p + e.yxy ) + e.xxx * de( p + e.xxx ) );
			break;

		case 1: // from iq = more efficient, 4 DE evaluations
			e = vec2( epsilon, 0.0f );
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

// there's definitely a better way to do this, instead of two separate functions - some preprocessor fuckery? tbd
vec3 lensNormal ( vec3 p ) {
	vec2 e;
	switch( normalMethod ) {
		case 0: // tetrahedron version, unknown original source - 4 DE evaluations
			e = vec2( 1.0f, -1.0f ) * epsilon;
			return normalize( e.xyy * deLens( p + e.xyy ) + e.yyx * deLens( p + e.yyx ) + e.yxy * deLens( p + e.yxy ) + e.xxx * deLens( p + e.xxx ) );
			break;

		case 1: // from iq = more efficient, 4 DE evaluations
			e = vec2( epsilon, 0.0f );
			return normalize( vec3( deLens( p ) ) - vec3( deLens( p - e.xyy ), deLens( p - e.yxy ), deLens( p - e.yyx ) ) );
			break;

		case 2: // from iq - less efficient, 6 DE evaluations
			e = vec2( epsilon, 0.0f );
			return normalize( vec3( deLens( p + e.xyy ) - deLens( p - e.xyy ), deLens( p + e.yxy ) - deLens( p - e.yxy ), deLens( p + e.yyx ) - deLens( p - e.yyx ) ) );
			break;

		default:
			break;
	}
}

float reflectance ( float cosTheta, float IoR ) {
	// Use Schlick's approximation for reflectance
	float r0 = ( 1.0f - IoR ) / ( 1.0f + IoR );
	r0 = r0 * r0;
	return r0 + ( 1.0f - r0 ) * pow( ( 1.0f - cosTheta ), 5.0f );
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
		// // certain chance to scatter in a random direction, per step - one of Nameless' methods for fog
		// if ( normalizedRandomFloat() < 0.005f ) { // massive slowdown doing this
		// 	direction = normalize( direction + 0.4f * randomUnitVector() );
		// }
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
	// rayOrigin += rayDirection * ( 0.9f + 0.1f * blueNoiseReference( location ).x );

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
				case PREVIEW_SHADED: return hitpointColor * ( 1.0f / calcAO( pHit, hitpointNormal ) ); break;
			}
		} else {
			return vec3( 0.0f );
		}
	}

	// loop to max bounces
	for( int bounce = 0; bounce < maxBounces; bounce++ ) {
		float dResult = raymarch( rayOrigin, rayDirection );
		int hitpointSurfaceType_cache = hitpointSurfaceType;
		vec3 hitpointColor_cache = hitpointColor;

		// cache previous values of rayOrigin, rayDirection, and get new hit position
		previousRayOrigin = rayOrigin;
		previousRayDirection = rayDirection;
		rayOrigin = rayOrigin + dResult * rayDirection;

		// surface normal at the new hit position
		vec3 hitNormal = normal( rayOrigin );

		// bump rayOrigin along the normal to prevent false positive hit on next bounce
			// now you are at least epsilon distance from the surface, so you won't immediately hit
		if ( hitpointSurfaceType_cache != REFRACTIVE ) {
			rayOrigin += 2.0f * epsilon * hitNormal;
		}

	// these are mixed per-material
		// construct new rayDirection vector, diffuse reflection off the surface
		vec3 reflectedVector = reflect( previousRayDirection, hitNormal );

		vec3 randomVectorDiffuse = normalize( ( 1.0f + epsilon ) * hitNormal + randomUnitVector() );
		vec3 randomVectorSpecular = normalize( ( 1.0f + epsilon ) * hitNormal + mix( reflectedVector, randomUnitVector(), 0.1f ) );

		// currently just implementing diffuse and emissive behavior
			// eventually add different ray behaviors for each material here
		switch ( hitpointSurfaceType_cache ) {

			case EMISSIVE:
				finalColor += throughput * hitpointColor;
				break;

			case DIFFUSE:
				rayDirection = randomVectorDiffuse;
				throughput *= hitpointColor_cache; // attenuate throughput by surface albedo
				break;

			case METALLIC:
				rayDirection = mix( randomVectorDiffuse, randomVectorSpecular, 0.7f );
				throughput *= hitpointColor_cache;
				break;

			case REFRACTIVE: // ray refracts, instead of bouncing
				// bump by the appropriate amount
				vec3 lensNorm = ( enteringRefractive ? 1.0f : -1.0f ) * lensNormal( rayOrigin );
				rayOrigin -= 2.0f * epsilon * lensNorm;

				// entering or leaving
				// float IoR = enteringRefractive ? lensIoR : 1.0f / lensIoR;
				float IoR = enteringRefractive ? ( 1.0f / lensIoR ) : lensIoR;
				float cosTheta = min( dot( -normalize( rayDirection ), lensNorm ), 1.0f );
				float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );

				// accounting for TIR effects
				bool cannotRefract = ( IoR * sinTheta ) > 1.0f;
				if ( cannotRefract || reflectance( cosTheta, IoR ) > normalizedRandomFloat() ) {
					rayDirection = reflect( normalize( rayDirection ), lensNorm );
				} else {
					rayDirection = refract( normalize( rayDirection ), lensNorm, IoR );
				}
				break;

			default:
				break;
		}

		if ( hitpointSurfaceType_cache != REFRACTIVE ) {
			// russian roulette termination - chance for ray to quit early
			float maxChannel = max( throughput.r, max( throughput.g, throughput.b ) );
			if ( normalizedRandomFloat() > maxChannel ) { break; }
			// russian roulette compensation term
			throughput *= 1.0f / maxChannel;
		}
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
	#ifdef RANDOM // wang hash random offsets
		return vec2( normalizedRandomFloat(), normalizedRandomFloat() );
	#endif
	#ifdef BLUE
		return blueNoiseReference( ivec2( gl_GlobalInvocationID.xy ) ).xy;
	#endif
}

void storeNormalAndDepth ( vec3 normal, float depth ) {
	// blend with history and imageStore
	vec4 prevResult = imageLoad( accumulatorNormalsAndDepth, location );
	vec4 blendResult = mix( prevResult, vec4( normal, depth ), 1.0f / sampleCount );
	imageStore( accumulatorNormalsAndDepth, location, blendResult );
}

vec3 pathtraceSample ( ivec2 location, int n ) {
	vec3  cResult = vec3( 0.0f );
	vec3  nResult = vec3( 0.0f );
	float dResult = 0.0f;
	const float aspectRatio = float( imageSize( accumulatorColor ).x ) / float( imageSize( accumulatorColor ).y );

#if AA != 1
	// at AA = 2, this is 4 samples per invocation
	const float normalizeTerm = float( AA * AA );
	for ( int x = 0; x < AA; x++ ) {
		for ( int y = 0; y < AA; y++ ) {
#endif

			// pixel offset + mapped position
			// vec2 offset = vec2( x + normalizedRandomFloat(), y + normalizedRandomFloat() ) / float( AA ) - 0.5; // previous method
			vec2 subpixelOffset  = getRandomOffset( n );
			vec2 halfScreenCoord = vec2( imageSize( accumulatorColor ) / 2.0f );
			vec2 mappedPosition  = ( vec2( location + subpixelOffset ) - halfScreenCoord ) / halfScreenCoord;

			vec3 rayDirection = normalize( aspectRatio * mappedPosition.x * basisX + mappedPosition.y * basisY + ( 1.0f / FoV ) * basisZ );
			vec3 rayOrigin    = viewerPosition;

			// thin lens DoF - adjust view vectors to converge at focusDistance
				// this is a small adjustment to the ray origin and direction - not working correctly - need to revist this
			vec3 focuspoint = rayOrigin + ( ( rayDirection * focusDistance ) / dot( rayDirection, basisZ ) );
			vec2 diskOffset = thinLensIntensity * randomInUnitDisk();
			rayOrigin       += diskOffset.x * basisX + diskOffset.y * basisY;
			rayDirection    = normalize( focuspoint - rayOrigin );

			// get depth and normals - think about special handling for refractive hits, maybe consider total distance traveled after all bounces?
			float distanceToFirstHit = raymarch( rayOrigin, rayDirection ); // may need to use more raymarch steps / decrease understep, creates some artifacts
			storeNormalAndDepth( normal( rayOrigin + distanceToFirstHit * rayDirection ), distanceToFirstHit );

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
	if ( !boundsCheck( location ) ) return; // abort on out of bounds

	seed = location.x * 1973 + location.y * 9277 + wangSeed;

	switch ( modeSelect ) {
		case PATHTRACE:
			vec4 prevResult = imageLoad( accumulatorColor, location );
			sampleCount = prevResult.a + 1.0f;
			vec3 blendResult = mix( prevResult.rgb, pathtraceSample( location, int( sampleCount ) ), 1.0f / sampleCount );
			imageStore( accumulatorColor, location, vec4( blendResult, sampleCount ) );
			break;

		case PREVIEW_DIFFUSE:
		case PREVIEW_NORMAL:
		case PREVIEW_DEPTH:
		case PREVIEW_SHADED:
			location = ivec2( gl_GlobalInvocationID.xy );
			imageStore( accumulatorColor, location, vec4( pathtraceSample( location, 0 ), 1.0f ) );
			break;

		default:
			return;
			break;
	}
}
