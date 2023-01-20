#ifndef ENGINE
#define ENGINE
#include "includes.h"

class engine {
public:
	engine()  { Init(); }
	~engine() { Quit(); }

	bool MainLoop (); // called from main

private:
	// application handles + basic data
	// windowHandler w; // this was partially implemented in Voraldo13, consider bringing that over
	SDL_Window * window;
	SDL_GLContext GLcontext;

	// loaded from config.json
	configData config;

	// text renderer framework
	layerManager textRenderer;

	// pathtracer config
	hostParameters host;
	coreParameters core;
	lensParameters lens;
	sceneParameters scene;
	postParameters post;

	// OpenGL data handles
		// render
	GLuint colorAccumulatorTexture;
	GLuint normalAccumulatorTexture;
	GLuint blueNoiseTexture;
	GLuint pathtraceShader;
	GLuint postprocessShader;
		// present
	GLuint displayTexture;
	GLuint displayShader;
	GLuint displayVAO;
	GLuint displayVBO;

	// // OpenGL data
	// GLuint blueNoiseTexture;
	// GLuint accumulatorTexture;
	// GLuint displayTexture;
	// GLuint displayShader;
	// GLuint displayVAO;
	// GLuint displayVBO;
	// GLuint dummyDrawShader;
	// GLuint tonemapShader;

	// initialization
	void Init ();
	void StartMessage ();
	void LoadConfig ();
	void CreateWindowAndContext ();
	void DisplaySetup ();
	void SetupTextureData ();
	void ShaderCompile ();
	void ImguiSetup ();

	// main loop functions
	void BlitToScreen ();
	void HandleEvents ();
	void UpdateNoiseOffsets ();
	void PathtraceUniformUpdate ();
	void PostprocessUniformUpdate ();
	void ImguiPass ();
	void ImguiFrameStart ();
	void ImguiFrameEnd ();
	void DrawTextEditor ();
	void ResetAccumulators ();
	void QuitConf ( bool* open );
	void HelpMarker( const char* message );

	// rendering functions
	void Render(); 				// swichable functionality
	void Postprocess();			// tonemap, dither
	glm::ivec2 GetTile();		// tile renderer offset

	// screenshot functions
	void BasicScreenShot();		// pull render target from texture memory
	void EXRScreenshot();		// pull accumulator data directly and save 32-bit float RGBA EXR

	// large screenshot
	// void offlineScreenShot();	// render out with prescribed sample count + resolution
		// GLint maxTextureSizeCheck;
		// glGetIntegerv( GL_MAX_TEXTURE_SIZE, &maxTextureSizeCheck );

	// shutdown procedures
	void ImguiQuit ();
	void SDLQuit ();
	void Quit ();

	// program flags
	bool quitConfirm = false;
	bool pQuit = false;

	// performance monitoring histories
	std::deque<float> fpsHistory;
	std::deque<float> tileHistory;
};
#endif
