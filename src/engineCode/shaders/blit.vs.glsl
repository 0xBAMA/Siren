#version 430
void main() {
	switch( gl_VertexID ) {
		case 0: gl_Position = vec4( -1.0, -1.0, 0.99, 1.0 ); break;
		case 1: gl_Position = vec4(  3.0, -1.0, 0.99, 1.0 ); break;
		case 2: gl_Position = vec4( -1.0,  3.0, 0.99, 1.0 ); break;
	}
}
