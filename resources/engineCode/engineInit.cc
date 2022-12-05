#include "engine.h"

void engine::StartMessage () {
	cout << endl << T_YELLOW << BOLD << "NQADE - Not Quite A Demo Engine" << newline;
	cout << " By Jon Baker ( 2020 - 2022 ) " << RESET << newline;
	cout << "  https://jbaker.graphics/ " << newline << newline;
}

void engine::LoadConfig () {
	cout << T_BLUE << "    Configuring Application" << RESET << " ................... ";
	json j;
	// load the config json, populate config struct - this will probably have more data, eventually
	ifstream i( "resources/engineCode/config.json" );
	i >> j; i.close();
	config.windowTitle = j[ "windowTitle" ];
	config.width = j[ "screenWidth" ];
	config.height = j[ "screenHeight" ];
	config.linearFilter = j[ "linearFilterDisplayTex" ];
	config.windowOffset.x = j[ "windowOffset" ][ "x" ];
	config.windowOffset.y = j[ "windowOffset" ][ "y" ];
	config.startOnScreen = j[ "startOnScreen" ];

	config.windowFlags |= ( j[ "SDL_WINDOW_FULLSCREEN" ] ? SDL_WINDOW_FULLSCREEN : 0 );
	config.windowFlags |= ( j[ "SDL_WINDOW_FULLSCREEN_DESKTOP" ] ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0 );
	config.windowFlags |= ( j[ "SDL_WINDOW_BORDERLESS" ] ? SDL_WINDOW_BORDERLESS : 0 );
	config.windowFlags |= ( j[ "SDL_WINDOW_RESIZABLE" ] ? SDL_WINDOW_RESIZABLE : 0 );
	config.windowFlags |= ( j[ "SDL_WINDOW_INPUT_GRABBED" ] ? SDL_WINDOW_INPUT_GRABBED : 0 );
	config.vSyncEnable = j[ "vSyncEnable" ];
	config.MSAACount = j[ "MSAACount" ];
	config.OpenGLVersionMajor = j[ "OpenGLVersionMajor" ];
	config.OpenGLVersionMinor = j[ "OpenGLVersionMinor" ];
	config.reportPlatformInfo = j[ "reportPlatformInfo" ];
	config.enableDepthTesting = j[ "enableDepthTesting" ];

	config.clearColor.r = j[ "clearColor" ][ "r" ];
	config.clearColor.g = j[ "clearColor" ][ "g" ];
	config.clearColor.b = j[ "clearColor" ][ "b" ];
	config.clearColor.a = j[ "clearColor" ][ "a" ];

	// color grading stuff
	tonemap.tonemapMode = j[ "colorGrade" ][ "tonemapMode" ];
	tonemap.gamma = j[ "colorGrade" ][ "gamma" ];
	tonemap.colorTemp = j[ "colorGrade" ][ "colorTemp" ];

	cout << T_GREEN << "done." << RESET << newline;
}

void engine::CreateWindowAndContext () {
	cout << T_BLUE << "    Initializing SDL2" << RESET << " ......................... ";
	if ( SDL_Init( SDL_INIT_EVERYTHING ) != 0 ) {
		cout << "Error: " << SDL_GetError() << newline;
	}
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER,       1 );
	SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE,           8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE,         8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE,          8 );
	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE,         8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE,        24 );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE,       8 );
	// multisampling AA, for edges when evaluating API geometry
	if ( config.MSAACount > 1 ) {
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, config.MSAACount );
	}
	cout << T_GREEN << "done." << RESET << newline;
	cout << T_BLUE << "    Creating Window" << RESET << " ........................... ";

	// prep for window creation
	SDL_DisplayMode displayMode;
	SDL_GetDesktopDisplayMode( 0, &displayMode );

	// 0 or negative numbers will size the window relative to the display
	config.width = ( config.width <= 0 ) ? displayMode.w + config.width : config.width;
	config.height = ( config.height <= 0 ) ? displayMode.h + config.height : config.height;

	// always need OpenGL, always start hidden till init finishes
	config.windowFlags |= SDL_WINDOW_OPENGL;
	config.windowFlags |= SDL_WINDOW_HIDDEN;
	// todo: offset so that it starts on the selected screen, config.startOnScreen ( bump by n * screenWidth )
	window = SDL_CreateWindow( config.windowTitle.c_str(), config.windowOffset.x + config.startOnScreen * displayMode.w,
		config.windowOffset.y, config.width, config.height, config.windowFlags );

	cout << T_GREEN << "done." << RESET << newline;
	cout << T_BLUE << "    Setting Up OpenGL Context" << RESET << " ................. ";
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
	// defaults to OpenGL 4.3
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, config.OpenGLVersionMajor );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, config.OpenGLVersionMinor );
	GLcontext = SDL_GL_CreateContext( window );
	SDL_GL_MakeCurrent( window, GLcontext );

	// config vsync enable/disable
	SDL_GL_SetSwapInterval( config.vSyncEnable ? 1 : 0 );

	// load OpenGL functions
	if ( gl3wInit() != 0 ) { cout << "Failed to Initialize OpenGL Loader!" << newline; abort(); }

	// basic OpenGL Config
	glEnable( GL_DEPTH_TEST );
	// glEnable( GL_LINE_SMOOTH );
	// glPointSize( 3.0 );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	cout << T_GREEN << "done." << RESET << newline;
}

// split up into vertex, texture funcs + report platform info ( maybe do this later? )
void engine::DisplaySetup () {
	// some info on your current platform
	if ( config.reportPlatformInfo ) {
		cout << T_BLUE << "    Platform Info :" << RESET << newline;
		const GLubyte *vendor = glGetString( GL_VENDOR );
		cout << T_RED << "      Vendor : " << T_CYAN << vendor << RESET << newline;
		const GLubyte *renderer = glGetString( GL_RENDERER );
		cout << T_RED << "      Renderer : " << T_CYAN << renderer << RESET << newline;
		const GLubyte *version = glGetString( GL_VERSION );
		cout << T_RED << "      OpenGL Version Supported : " << T_CYAN << version << RESET << newline;
		const GLubyte *glslVersion = glGetString( GL_SHADING_LANGUAGE_VERSION );
		cout << T_RED << "      GLSL Version Supported : " << T_CYAN << glslVersion << RESET << newline << newline;
	}

	SetupVertexData();
	SetupTextureData();
}

void engine::SetupVertexData () {
	cout << T_BLUE << "    Setting Up Vertex Data" << RESET << " .................... ";


	// OpenGL core spec requires a VAO bound when calling glDrawArrays
	glGenVertexArrays( 1, &displayVAO );
	glBindVertexArray( displayVAO );

	// corresponding VBO, unused
	glGenBuffers( 1, &displayVBO );
	glBindBuffer( GL_ARRAY_BUFFER, displayVBO );

	// no op, by default...
		// load models, setup vertex attributes, etc, here

	cout << T_GREEN << "done." << RESET << newline;
}

void engine::SetupTextureData () {
	cout << T_BLUE << "    Setting Up Textures" << RESET << " ....................... ";

	// create the image textures
	Image initial( config.width, config.height, false );
	glGenTextures( 1, &accumulatorTexture );
	glActiveTexture( GL_TEXTURE3 );
	glBindTexture( GL_TEXTURE_2D, accumulatorTexture );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, config.width, config.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &initial.data[ 0 ] );

	glGenTextures( 1, &displayTexture );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, displayTexture );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, config.linearFilter ? GL_LINEAR : GL_NEAREST );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, config.linearFilter ? GL_LINEAR : GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, config.width, config.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &initial.data[ 0 ] );

	// blue noise image on the GPU
	Image blueNoiseImage{ "resources/noise/blueNoise.png", LODEPNG };
	glGenTextures( 1, &blueNoiseTexture );
	glActiveTexture( GL_TEXTURE4 );
	glBindTexture( GL_TEXTURE_2D, blueNoiseTexture );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, blueNoiseImage.width, blueNoiseImage.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &blueNoiseImage.data[ 0 ] );

	cout << T_GREEN << "done." << RESET << newline;
}

void engine::ShaderCompile () {
	cout << T_BLUE << "    Compiling Shaders" << RESET << " ......................... ";

	// create the shader for the triangles to cover the screen
	displayShader = regularShader( "resources/engineCode/shaders/blit.vs.glsl", "resources/engineCode/shaders/blit.fs.glsl" ).shaderHandle;

	// initialize the text renderer
	textRenderer.Init( config.width, config.height, computeShader( "resources/fonts/fontRenderer/font.cs.glsl" ).shaderHandle );

	// something to put data in the accumulator texture
	dummyDrawShader = computeShader( "resources/engineCode/shaders/dummyDraw.cs.glsl" ).shaderHandle;

	// tonemapping shader
	tonemapShader = computeShader( "resources/engineCode/shaders/tonemap.cs.glsl" ).shaderHandle;

	cout << T_GREEN << "done." << RESET << newline;
}

void engine::ImguiSetup () {
	cout << T_BLUE << "    Configuring dearImGUI" << RESET << " ..................... ";

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();

	// enable docking
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	// Setup Platform/Renderer bindings
	ImGui_ImplSDL2_InitForOpenGL( window, GLcontext );
	const char *glsl_version = "#version 430";
	ImGui_ImplOpenGL3_Init( glsl_version );

	// initial value for clear color
	glClearColor( config.clearColor.x, config.clearColor.y, config.clearColor.z, config.clearColor.w );
	glClear( GL_COLOR_BUFFER_BIT );
	SDL_GL_SwapWindow( window ); // show clear color

	// setting custom font, if desired
	// io.Fonts->AddFontFromFileTTF( "resources/fonts/star_trek/titles/TNG_Title.ttf", 16 );

	ImGui::StyleColorsDark();
	ImVec4 *colors = ImGui::GetStyle().Colors;

	colors[ ImGuiCol_Text ]
		= ImVec4( 0.64f, 0.37f, 0.37f, 1.00f );
	colors[ ImGuiCol_TextDisabled ]
		= ImVec4( 0.49f, 0.26f, 0.26f, 1.00f );
	colors[ ImGuiCol_WindowBg ]
		= ImVec4( 0.17f, 0.00f, 0.00f, 0.98f );
	colors[ ImGuiCol_ChildBg ]
		= ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
	colors[ ImGuiCol_PopupBg ]
		= ImVec4( 0.18f, 0.00f, 0.00f, 0.94f );
	colors[ ImGuiCol_Border ]
		= ImVec4( 0.35f, 0.00f, 0.03f, 0.50f );
	colors[ ImGuiCol_BorderShadow ]
		= ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
	colors[ ImGuiCol_FrameBg ]
		= ImVec4( 0.14f, 0.04f, 0.00f, 1.00f );
	colors[ ImGuiCol_FrameBgHovered ]
		= ImVec4( 0.14f, 0.04f, 0.00f, 1.00f );
	colors[ ImGuiCol_FrameBgActive ]
		= ImVec4( 0.14f, 0.04f, 0.00f, 1.00f );
	colors[ ImGuiCol_TitleBg ]
		= ImVec4( 0.14f, 0.04f, 0.00f, 1.00f );
	colors[ ImGuiCol_TitleBgActive ]
		= ImVec4( 0.14f, 0.04f, 0.00f, 1.00f );
	colors[ ImGuiCol_TitleBgCollapsed ]
		= ImVec4( 0.00f, 0.00f, 0.00f, 0.51f );
	colors[ ImGuiCol_MenuBarBg ]
		= ImVec4( 0.14f, 0.14f, 0.14f, 1.00f );
	colors[ ImGuiCol_ScrollbarBg ]
		= ImVec4( 0.02f, 0.02f, 0.02f, 0.53f );
	colors[ ImGuiCol_ScrollbarGrab ]
		= ImVec4( 0.31f, 0.31f, 0.31f, 1.00f );
	colors[ ImGuiCol_ScrollbarGrabHovered ]
		= ImVec4( 0.41f, 0.41f, 0.41f, 1.00f );
	colors[ ImGuiCol_ScrollbarGrabActive ]
		= ImVec4( 0.51f, 0.51f, 0.51f, 1.00f );
	colors[ ImGuiCol_CheckMark ]
		= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
	colors[ ImGuiCol_SliderGrab ]
		= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
	colors[ ImGuiCol_SliderGrabActive ]
		= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
	colors[ ImGuiCol_Button ]
		= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
	colors[ ImGuiCol_ButtonHovered ]
		= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
	colors[ ImGuiCol_ButtonActive ]
		= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
	colors[ ImGuiCol_Header ]
		= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
	colors[ ImGuiCol_HeaderHovered ]
		= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
	colors[ ImGuiCol_HeaderActive ]
		= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
	colors[ ImGuiCol_Separator ]
		= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
	colors[ ImGuiCol_SeparatorHovered ]
		= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
	colors[ ImGuiCol_SeparatorActive ]
		= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
	colors[ ImGuiCol_ResizeGrip ]
		= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
	colors[ ImGuiCol_ResizeGripHovered ]
		= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
	colors[ ImGuiCol_ResizeGripActive ]
		= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
	colors[ ImGuiCol_Tab ]
		= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
	colors[ ImGuiCol_TabHovered ]
		= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
	colors[ ImGuiCol_TabActive ]
		= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
	colors[ ImGuiCol_TabUnfocused ]
		= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
	colors[ ImGuiCol_TabUnfocusedActive ]
		= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
	colors[ ImGuiCol_PlotLines ]
		= ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
	colors[ ImGuiCol_PlotLinesHovered ]
		= ImVec4( 1.00f, 0.43f, 0.35f, 1.00f );
	colors[ ImGuiCol_PlotHistogram ]
		= ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
	colors[ ImGuiCol_PlotHistogramHovered ]
		= ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
	colors[ ImGuiCol_TextSelectedBg ]
		= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
	colors[ ImGuiCol_DragDropTarget ]
		= ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
	colors[ ImGuiCol_NavHighlight ]
		= ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
	colors[ ImGuiCol_NavWindowingHighlight ]
		= ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
	colors[ ImGuiCol_NavWindowingDimBg ]
		= ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
	colors[ ImGuiCol_ModalWindowDimBg ]
		= ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );

	cout << T_GREEN << "done." << RESET << newline << newline;
}
