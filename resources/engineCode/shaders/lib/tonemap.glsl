
// tonemapping stuff
// APPROX
// --------------------------
vec3 CheapACES ( vec3 v ) {
	v *= 0.6f;
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0f, 1.0f);
}

// OFFICIAL
// --------------------------
mat3 ACESInputMat = mat3(
	0.59719f, 0.35458f, 0.04823f,
	0.07600f, 0.90834f, 0.01566f,
	0.02840f, 0.13383f, 0.83777f
);

mat3 ACESOutputMat = mat3(
	1.60475f, -0.53108f, -0.07367f,
	-0.10208f,  1.10813f, -0.00605f,
	-0.00327f, -0.07276f,  1.07602f
);

vec3 MatrixMultiply ( mat3 m, vec3 v ) {
	float x = m[ 0 ][ 0 ] * v[ 0 ] + m[ 0 ][ 1 ] * v[ 1 ] + m[ 0 ][ 2 ] * v[ 2 ];
	float y = m[ 1 ][ 0 ] * v[ 1 ] + m[ 1 ][ 1 ] * v[ 1 ] + m[ 1 ][ 2 ] * v[ 2 ];
	float z = m[ 2 ][ 0 ] * v[ 1 ] + m[ 2 ][ 1 ] * v[ 1 ] + m[ 2 ][ 2 ] * v[ 2 ];
	return vec3( x, y, z );
}

vec3 RTT_ODT_Fit ( vec3 v ) {
	vec3 a = v * ( v + 0.0245786f) - 0.000090537f;
	vec3 b = v * ( 0.983729f * v + 0.4329510f ) + 0.238081f;
	return a / b;
}

vec3 aces_fitted ( vec3 v ) {
	v = MatrixMultiply( ACESInputMat, v );
	v = RTT_ODT_Fit( v );
	return MatrixMultiply( ACESOutputMat, v );
}


vec3 uncharted2(vec3 v) {
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;

	float ExposureBias = 2.0f;
	v *= ExposureBias;

	return ( ( ( v * ( A * v + C * B ) + D * E ) / ( v * ( A * v + B ) + D * F ) ) - E / F )
				* ( ( ( W * ( A * W + C * B ) + D * E ) / ( W * ( A * W + B ) + D * F ) ) - E / F );
}

vec3 Rienhard ( vec3 v ) {
	return v / ( vec3( 1.0 ) + v );
}

vec3 Rienhard2 ( vec3 v ) {
	const float L_white = 4.0;
	return (v * ( vec3( 1.0 ) + v / ( L_white * L_white ) ) ) / ( 1.0 + v );
}

vec3 TonemapUchimura ( vec3 v ) {
	const float P = 1.0;  // max display brightness
	const float a = 1.0;  // contrast
	const float m = 0.22; // linear section start
	const float l = 0.4;  // linear section length
	const float c = 1.33; // black
	const float b = 0.0;  // pedestal

	// Uchimura 2017, "HDR theory and practice"
	// Math: https://www.desmos.com/calculator/gslcdxvipg
	// Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
	float l0 = ( ( P - m ) * l ) / a;
	float L0 = m - m / a;
	float L1 = m + ( 1.0 - m ) / a;
	float S0 = m + l0;
	float S1 = m + a * l0;
	float C2 = ( a * P ) / ( P - S1 );
	float CP = -C2 / P;

	vec3 w0 = 1.0 - smoothstep( 0.0, m, v );
	vec3 w2 = step( m + l0, v );
	vec3 w1 = 1.0 - w0 - w2;

	vec3 T = m * pow( v / m, vec3( c ) ) + vec3( b );
	vec3 S = P - ( P - S1 ) * exp( CP * ( v - S0 ) );
	vec3 L = m + a * ( v - vec3( m ) );

	return T * w0 + L * w1 + S * w2;
}

vec3 TonemapUchimura2 ( vec3 v ) {
	const float P = 1.0;  // max display brightness
	const float a = 1.7;  // contrast
	const float m = 0.1; // linear section start
	const float l = 0.0;  // linear section length
	const float c = 1.33; // black
	const float b = 0.0;  // pedestal

	float l0 = ( ( P - m ) * l ) / a;
	float L0 = m - m / a;
	float L1 = m + ( 1.0 - m ) / a;
	float S0 = m + l0;
	float S1 = m + a * l0;
	float C2 = ( a * P ) / ( P - S1 );
	float CP = -C2 / P;

	vec3 w0 = 1.0 - smoothstep( 0.0, m, v );
	vec3 w2 = step( m + l0, v );
	vec3 w1 = 1.0 - w0 - w2;

	vec3 T = m * pow( v / m, vec3( c ) ) + vec3( b );
	vec3 S = P - ( P - S1 ) * exp( CP * ( v - S0 ) );
	vec3 L = m + a * ( v - vec3( m ) );

	return T * w0 + L * w1 + S * w2;
}

vec3 tonemapUnreal3 ( vec3 v ) {
	return v / ( v + 0.155 ) * 1.019;
}


#define toLum(color) dot(color, vec3(0.2125, 0.7154, 0.0721) )
#define lightAjust(a,b) ((1.0-b)*(pow(1.0-a,vec3(b+1.0))-1.0)+a)/b
#define reinhard(c,l) c*(l/(1.0+l)/l)
vec3 JTtonemap ( vec3 x ) {
	float l = toLum( x );
	x = reinhard( x, l );
	float m = max( x.r, max( x.g, x.b ) );
	return min( lightAjust( x / m, m ), x );
}
#undef toLum
#undef lightAjust
#undef reinhard


vec3 robobo1221sTonemap ( vec3 x ) {
	return sqrt( x / ( x + 1.0f / x ) ) - abs( x ) + x;
}

vec3 roboTonemap ( vec3 c ) {
	return c / sqrt( 1.0 + c * c );
}

vec3 jodieRoboTonemap ( vec3 c ) {
	float l = dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
	vec3 tc = c / sqrt( c * c + 1.0 );
	return mix( c / sqrt( l * l + 1.0 ), tc, tc );
}

vec3 jodieRobo2ElectricBoogaloo ( const vec3 color ) {
	float luma = dot( color, vec3( 0.2126, 0.7152, 0.0722 ) );
	// tonemap curve goes on this line
	// (I used robo here)
	vec4 rgbl = vec4( color, luma ) * inversesqrt( luma * luma + 1.0 );
	vec3 mappedColor = rgbl.rgb;
	float mappedLuma = rgbl.a;
	float channelMax = max( max( max( mappedColor.r, mappedColor.g ), mappedColor.b ), 1.0 );

	// this is just the simplified/optimised math
	// of the more human readable version below
	return ( ( mappedLuma * mappedColor - mappedColor ) - ( channelMax * mappedLuma - mappedLuma ) ) / ( mappedLuma - channelMax );

	const vec3 white = vec3( 1.0 );

	// prevent clipping
	vec3 clampedColor = mappedColor / channelMax;

	// x is how much white needs to be mixed with
	// clampedColor so that its luma equals the
	// mapped luma
	//
	// mix(mappedLuma/channelMax,1.,x) = mappedLuma;
	//
	// mix is defined as
	// x*(1-a)+y*a
	// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/mix.xhtml
	//
	// (mappedLuma/channelMax)*(1.-x)+1.*x = mappedLuma

	float x = ( mappedLuma - mappedLuma * channelMax ) / ( mappedLuma - channelMax );
	return mix( clampedColor, white, x );
}

vec3 jodieReinhardTonemap ( vec3 c ) {
	float l = dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
	vec3 tc = c / ( c + 1.0 );
	return mix( c /( l + 1.0 ), tc, tc );
}

vec3 jodieReinhard2ElectricBoogaloo(const vec3 color){
	float luma = dot(color, vec3(.2126, .7152, .0722));

	// tonemap curve goes on this line
	// (I used reinhard here)
	vec4 rgbl = vec4(color, luma) / (luma + 1.);

	vec3 mappedColor = rgbl.rgb;
	float mappedLuma = rgbl.a;
	float channelMax = max( max( max( mappedColor.r, mappedColor.g ), mappedColor.b ), 1.0 );

	// this is just the simplified/optimised math
	// of the more human readable version below
	return ( ( mappedLuma * mappedColor - mappedColor ) - ( channelMax * mappedLuma - mappedLuma ) ) / ( mappedLuma - channelMax );

	const vec3 white = vec3( 1.0 );

	// prevent clipping
	vec3 clampedColor = mappedColor / channelMax;

	// x is how much white needs to be mixed with
	// clampedColor so that its luma equals the
	// mapped luma
	//
	// mix(mappedLuma/channelMax,1.,x) = mappedLuma;
	//
	// mix is defined as
	// x*(1-a)+y*a
	// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/mix.xhtml
	//
	// (mappedLuma/channelMax)*(1.-x)+1.*x = mappedLuma

	float x = ( mappedLuma - mappedLuma * channelMax ) / ( mappedLuma - channelMax );
	return mix( clampedColor, white, x );
}

vec3 tonemap( int tonemapMode, vec3 col ) {
	switch ( tonemapMode ) {
			case 0: // None (Linear)
					break;
			case 1: // ACES (Narkowicz 2015)
					col.xyz = CheapACES( col.xyz );
					break;
			case 2: // Unreal Engine 3
					col.xyz = pow( tonemapUnreal3( col.xyz ), vec3( 2.8 ) );
					break;
			case 3: // Unreal Engine 4
					col.xyz = aces_fitted( col.xyz );
					break;
			case 4: // Uncharted 2
					col.xyz = uncharted2( col.xyz );
					break;
			case 5: // Gran Turismo
					col.xyz = TonemapUchimura( col.xyz );
					break;
			case 6: // Modified Gran Turismo
					col.xyz = TonemapUchimura2( col.xyz );
					break;
			case 7: // Rienhard
					col.xyz = Rienhard( col.xyz );
					break;
			case 8: // Modified Rienhard
					col.xyz = Rienhard2( col.xyz );
					break;
			case 9: // jt_tonemap
					col.xyz = JTtonemap( col.xyz );
					break;
			case 10: // robobo1221s
					col.xyz = robobo1221sTonemap( col.xyz );
					break;
			case 11: // robo
					col.xyz = roboTonemap( col.xyz );
					break;
			case 12: // jodieRobo
					col.xyz = jodieRoboTonemap( col.xyz );
					break;
			case 13: // jodieRobo2
					col.xyz = jodieRobo2ElectricBoogaloo( col.xyz );
					break;
			case 14: // jodieReinhard
					col.xyz = jodieReinhardTonemap( col.xyz );
					break;
			case 15: // jodieReinhard2
					col.xyz = jodieReinhard2ElectricBoogaloo( col.xyz );
					break;
	}
	return col;
}

vec3 gammaCorrect ( float gammaValue, vec3 col ) {
	return pow( col, vec3( 1.0 / gammaValue ) );
}
