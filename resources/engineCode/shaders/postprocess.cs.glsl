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

vec3 gammaCorrect ( vec3 col ) {
	return pow( col, vec3( 1.0 / gamma ) );
}

void main() {
	// this isn't done in tiles - it may need to be, for when rendering larger resolution screenshots - tbd
	ivec2 location = ivec2( gl_GlobalInvocationID.xy );

	// Color comes in at 32-bit per channel precision
	vec4 toStore = imageLoad( accumulatorColor, location );
	toStore.rgb = gammaCorrect( toStore.rgb );

	// RGB is normal, A is depth value, 32-bit per channel - these are currently unpopulated
	// vec4 normalAndDepth = imageLoad( accumulatorNormal, location );

	// do any postprocessing work, store back in display texture
		// this is things like:
		//	- depth fog
		//	- tonemapping
		//	- dithering

	imageStore( display, location, uvec4( toStore.xyz * 255.0, 255 ) );
}
