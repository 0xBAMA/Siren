#version 430 core
layout( local_size_x = 32, local_size_y = 32, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D display;
layout( binding = 1, rgba32f ) uniform image2D accumulatorColor;
layout( binding = 2, rgba32f ) uniform image2D accumulatorNormal;

uniform int ditherMode; 	// colorspace to do the dithering in
uniform int ditherMethod; 	// bitcrush bitcount or exponential scalar
uniform int ditherPattern; 	// pattern used to dither the output - probably make this an image/texture
uniform int tonemapMode; 	// tonemap curve to use
uniform int depthMode; 		// depth fog method
uniform float depthScale; 	// scalar for depth term, when computing depth effects ( fog )
uniform float gamma; 		// gamma correction term for the color result
uniform int displayType; 	// mode selector - show normals, show depth, show color, show postprocessed version

#define COLOR	0
#define NORMAL	1
#define DEPTH	2

#include "tonemap.glsl"

vec3 gammaCorrect ( vec3 col ) {
	return pow( col, vec3( 1.0 / gamma ) );
}

void main() {
	// this isn't done in tiles - it may need to be, for when rendering larger resolution screenshots - tbd
	ivec2 location = ivec2( gl_GlobalInvocationID.xy );
	vec4 toStore;

	// color, normal, depth values come in at 32-bit per channel precision
	switch ( displayType ) {
		case COLOR:
			toStore = imageLoad( accumulatorColor, location );
			toStore.rgb = gammaCorrect( toStore.rgb );
			toStore.rgb = tonemap( tonemapMode, toStore.rgb );
			break;
		case NORMAL:
			toStore = imageLoad( accumulatorNormal, location );
			toStore.a = 1.0f;
			break;
		case DEPTH:
			toStore = vec4( 1.0f / imageLoad( accumulatorNormal, location ).a );
			toStore.a = 1.0f;
			break;
	}

// do any other postprocessing work, store back in display texture
	// this is things like:
		// - depth fog
		// - dithering

	// storing back as LDR 8-bits per channel RGB for output
	imageStore( display, location, uvec4( toStore.rgb * 255.0, 255 ) );
}
