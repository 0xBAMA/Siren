#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba8ui ) uniform uimage2D accumulatorTexture;

void main () {
	// basic XOR pattern
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	uint x = uint( writeLoc.x ) % 256;
	uint y = uint( writeLoc.y ) % 256;
	uvec3 result = uvec3( x ^ y ) / 2;

	// add some blue noise, for shits
	result += imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ).xyz;

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, uvec4( result, 255 ) );
}
