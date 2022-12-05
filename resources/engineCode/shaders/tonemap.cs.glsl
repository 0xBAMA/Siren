#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D accumulatorTexture;
layout( binding = 1, rgba8ui ) uniform uimage2D displayTexture;
#include "tonemap.glsl" // tonemapping curves

uniform int tonemapMode;
uniform float gamma;
uniform vec3 colorTempAdjust;

// and then maybe the color temperature adjustment could be applied here

void main () {
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// temporary hack for inverted image
	uvec4 originalValue = imageLoad( accumulatorTexture, ivec2( loc.x, imageSize( accumulatorTexture ).y - loc.y ) );

	vec3 color = tonemap( tonemapMode, colorTempAdjust * ( vec3( originalValue.xyz ) / 255.0 ) );
	color = gammaCorrect( gamma, color );
	uvec4 tonemappedValue = uvec4( uvec3( color * 255.0 ), originalValue.a );

	imageStore( displayTexture, loc, tonemappedValue );
}
