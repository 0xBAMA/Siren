#version 430 core
layout( local_size_x = 32, local_size_y = 32, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D display;
layout( binding = 1, rgba32f ) uniform image2D accumulatorColor;
layout( binding = 2, rgba32f ) uniform image2D accumulatorNormal;

uniform int ditherMode; 	// colorspace to do the dithering in
uniform int ditherMethod; 	// bitcrush bitcount or exponential scalar
uniform int ditherPattern; 	// pattern used to dither the output - probably make this an image/texture
uniform int tonemapMode; 	// tonemap curve to use
uniform vec3 fogColor;		// color of the depth fog
uniform int depthMode; 		// depth fog method
uniform float depthScale; 	// scalar for depth term, when computing depth effects ( fog )
uniform float maxDistance;	// maximum depth on the raymarch, used for some of the depth curves
uniform float gamma; 		// gamma correction term for the color result
uniform int displayType; 	// mode selector - show normals, show depth, show color, show postprocessed version

#define COLOR	0
#define NORMAL	1
#define DEPTH	2

#include "tonemap.glsl"
#include "depthCurves.glsl"

void main() {
	// this isn't done in tiles - it may need to be, for when rendering larger resolution screenshots - tbd
	ivec2 location = ivec2( gl_GlobalInvocationID.xy );
	vec4 toStore;

	vec4 color = imageLoad( accumulatorColor, location );
	vec4 normalAndDepth = imageLoad( accumulatorNormal, location );

	// color, normal, depth values come in at 32-bit per channel precision
	switch ( displayType ) {
		case COLOR:
			toStore.rgb = color.rgb;
			addDepthFog( toStore.rgb, normalAndDepth.a );
			toStore.rgb = gammaCorrect( toStore.rgb );
			toStore.rgb = tonemap( tonemapMode, toStore.rgb );
			// do any other postprocessing work
			//	this is things like:
			//		- denoising? tbd
			//		- dithering
			break;
		case NORMAL:
			toStore.rgb = normalAndDepth.xyz;
			break;
		case DEPTH:
			toStore.rgb = vec3( 1.0f / normalAndDepth.a );
			break;
	}

	// all cases take 1.0f alpha
	toStore.a = 1.0f;

	// storing back as LDR 8-bits per channel RGB for output
	imageStore( display, location, uvec4( toStore.rgb * 255.0, 255 ) );
}
