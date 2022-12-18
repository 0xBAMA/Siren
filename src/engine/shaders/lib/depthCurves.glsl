// assumes depthScale, depthMode, fogColor, maxDistance setup as uniforms, or at least available in included scope
void addDepthFog ( inout vec3 color, float inputDepth ) {
	inputDepth *= depthScale;
	float depthTerm;
	switch ( depthMode ) { // compute the depth scale term
		case 0:
			break;

		case 1:
			depthTerm = 2.0f - 2.0f * ( 1.0f / ( 1.0f - inputDepth ) );
			color.rgb = mix( color.rgb, fogColor.rgb, depthTerm );
			break;

		case 2:
			depthTerm = 1.0f - ( 1.0f / ( 1.0f + 0.1f * inputDepth * inputDepth ) );
			color.rgb = mix( color.rgb, fogColor.rgb, depthTerm );
			break;

		case 3:
			depthTerm = ( 1.0f - pow( inputDepth / 30.0f, 1.618f ) );
			color.rgb = mix( color.rgb, fogColor.rgb, depthTerm );
			break;

		case 4:
			depthTerm = clamp( exp( 0.25f * inputDepth - 3.0f ), 0.0f, 10.0f );
			color.rgb = mix( color.rgb, fogColor.rgb, depthTerm );
			break;

		case 5:
			depthTerm = exp( 0.25f * inputDepth - 3.0f );
			color.rgb = mix( color.rgb, fogColor.rgb, depthTerm );
			break;

		case 6:
			depthTerm = exp( -0.002f * inputDepth * inputDepth * inputDepth );
			color.rgb = mix( color.rgb, fogColor.rgb, depthTerm );
			break;

		case 7:
			depthTerm = exp( -0.6f * max( inputDepth - 3.0f, 0.0f ) );
			color.rgb = mix( color.rgb, fogColor.rgb, depthTerm );
			break;

		case 8:
			depthTerm = ( sqrt( inputDepth ) / 8.0f ) * inputDepth;
			color.rgb = mix( color.rgb, fogColor.rgb, depthTerm );
			break;

		case 9:
			depthTerm = sqrt( inputDepth / 9.0f );
			color.rgb = mix( color.rgb, fogColor.rgb, depthTerm );
			break;

		case 10:
			depthTerm = pow( inputDepth / 10.0f, 2.0f );
			color.rgb = mix( color.rgb, fogColor.rgb, depthTerm );
			break;

		case 11:
			color.rgb += 1.0f / ( 1.0f + exp( -2.0f * ( inputDepth * 0.1f - 2.0f ) ) ) * fogColor.rgb;
			// color.rgb = mix( color.rgb, fogColor.rgb, depthTerm );
			break;

		case 12:
			depthTerm = inputDepth / maxDistance;
			color.rgb = mix( color.rgb, fogColor.rgb, depthTerm );
			break;

		default:
			break;
	}
}
