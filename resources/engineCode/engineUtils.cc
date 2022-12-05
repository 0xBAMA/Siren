#include "engine.h"

bool engine::MainLoop () {
	ZoneScoped;
	HandleEvents();					// handle keyboard / mouse events
	ClearColorAndDepth();			// if I just disable depth testing, this can disappear
	DrawAPIGeometry();				// draw any API geometry desired
	ComputePasses();				// multistage update of displayTexture
	BlitToScreen();					// fullscreen triangle copying the displayTexture to the screen
	ImguiPass();					// do all the gui stuff
	SDL_GL_SwapWindow( window );	// show what has just been drawn to the back buffer ( displayTexture + ImGui )
	FrameMark;						// tells tracy that this is the end of a frame
	return pQuit;					// break main loop when pQuit turns true
}

void engine::DrawAPIGeometry () {

	GLuint64 startTime, stopTime;
	GLuint queryID[2];
	glGenQueries( 2, queryID );
	glQueryCounter( queryID[ 0 ], GL_TIMESTAMP );

	// draw ur shit

	glQueryCounter( queryID[ 1 ], GL_TIMESTAMP );
	GLint timeAvailable = 0;
	while( !timeAvailable ) {
		glGetQueryObjectiv( queryID[ 1 ], GL_QUERY_RESULT_AVAILABLE, &timeAvailable );
	}

	glGetQueryObjectui64v( queryID[ 0 ], GL_QUERY_RESULT, &startTime );
	glGetQueryObjectui64v( queryID[ 1 ], GL_QUERY_RESULT, &stopTime );
	float passTimeMs = ( stopTime - startTime ) / 1000000.0f; // operation time in ms

	( void ) passTimeMs; // avoid unused variable warning
}

void engine::ComputePasses () {
	ZoneScoped;

// dummy draw
	// set up environment ( 0:blue noise, 1: accumulator )
	glBindImageTexture( 0, blueNoiseTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );
	glBindImageTexture( 1, accumulatorTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );

	// blablah draw something into accumulatorTexture
	glUseProgram( dummyDrawShader );
	glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

// postprocessing
	// set up environment ( 0:accumulator, 1:display )
	glBindImageTexture( 0, accumulatorTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );
	glBindImageTexture( 1, displayTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );

	// shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
	glUseProgram( tonemapShader );
	SendTonemappingParameters();
	glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

	// shader to apply dithering
		// ...

	// other postprocessing
		// ...

	// text rendering timestamp, as final step - required texture binds are handled internally
	// textRenderer.Update( ImGui::GetIO().DeltaTime );
	textRenderer.Draw( displayTexture ); // displayTexture is the writeTarget
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void engine::ClearColorAndDepth () {
	ZoneScoped;
	// clear the screen
	glClearColor( config.clearColor.x, config.clearColor.y, config.clearColor.z, config.clearColor.w );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	ImGuiIO &io = ImGui::GetIO();
	const int width = ( int ) io.DisplaySize.x;
	const int height = ( int ) io.DisplaySize.y;
	// prevent -1, -1 being passed on first frame, since ImGui hasn't rendered yet
	glViewport( 0, 0, width > 0 ? width : config.width, height > 0 ? height : config.height ); // should this be elsewhere?
}

void engine::SendTonemappingParameters () {
	ZoneScoped;
	glUniform3fv( glGetUniformLocation( tonemapShader, "colorTempAdjust" ), 1, glm::value_ptr( GetColorForTemperature( tonemap.colorTemp ) ) );
	glUniform1i( glGetUniformLocation( tonemapShader, "tonemapMode" ), tonemap.tonemapMode );
	glUniform1f( glGetUniformLocation( tonemapShader, "gamma" ), tonemap.gamma );
}

void engine::BlitToScreen () {
	ZoneScoped;
	// bind the displayTexture and display its current state - there are more efficient ways to do this, look into it
	glBindImageTexture( 0, displayTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );
	glUseProgram( displayShader );
	glBindVertexArray( displayVAO );
	ImGuiIO &io = ImGui::GetIO();
	glUniform2f( glGetUniformLocation( displayShader, "resolution" ), io.DisplaySize.x, io.DisplaySize.y );
	glDrawArrays( GL_TRIANGLES, 0, 3 );
}

void engine::ImguiPass () {
	ZoneScoped;
	ImguiFrameStart();						// start the imgui frame
	TonemapControlsWindow();
	if ( true ) ImGui::ShowDemoWindow();	// show the demo window
	QuitConf( &quitConfirm );				// show quit confirm window, if triggered
	ImguiFrameEnd();						// finish up the imgui stuff and put it in the framebuffer
}

void engine::HandleEvents () {
	ZoneScoped;

	// can handle multiple simultaneous inputs like this
	const uint8_t *state = SDL_GetKeyboardState( NULL );
	// const float scalar = SDL_GetModState() & KMOD_SHIFT ? 1000.0f : 10.0f;
	if ( state[ SDL_SCANCODE_RIGHT ] ) {	cout << "Right Key Pressed" << newline; }
	if ( state[ SDL_SCANCODE_LEFT ] ) {		cout << "Left Key Pressed" << newline; }
	if ( state[ SDL_SCANCODE_UP ] ) {		cout << "Up Key Pressed" << newline; }
	if ( state[ SDL_SCANCODE_DOWN ] ) {		cout << "Down Key Pressed" << newline; }
	if ( state[ SDL_SCANCODE_PAGEDOWN ] ) {	cout << "PageDown Pressed" << newline; }
	if ( state[ SDL_SCANCODE_PAGEUP ] ) {	cout << "PageUp Pressed" << newline; }
	if ( state[ SDL_SCANCODE_W ] ) {		cout << "W Pressed" << newline; }
	if ( state[ SDL_SCANCODE_S ] ) {		cout << "S Pressed" << newline; }
	if ( state[ SDL_SCANCODE_A ] ) {		cout << "A Pressed" << newline; }
	if ( state[ SDL_SCANCODE_D ] ) {		cout << "D Pressed" << newline; }

//==============================================================================
// Need to keep this for pQuit handling ( force quit )
// In particular - checking for window close and the SDL_QUIT event can't really be determined
//  via the keyboard state, and then imgui needs it too, so can't completely kill the event
//  polling loop - maybe eventually I'll find a solution for this
	SDL_Event event;
	SDL_PumpEvents();
	while ( SDL_PollEvent( &event ) ) {
		// imgui event handling
		ImGui_ImplSDL2_ProcessEvent( &event );
		// swap out the multiple if statements for a big chained boolean setting the value of pQuit
		pQuit = ( event.type == SDL_QUIT ) ||
				( event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID( window ) ) ||
				( event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE && SDL_GetModState() & KMOD_SHIFT );
		// this has to stay because it doesn't seem like ImGui::IsKeyReleased is stable enough to use
		if ( ( event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE ) || ( event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_X1 )  )
			quitConfirm = !quitConfirm;
	}
}
