#ifndef INCLUDES
#define INCLUDES

#include <stdio.h>

// stl includes
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// iostream stuff
using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::flush;
using std::string;
using std::stringstream;
constexpr char newline = '\n';

// pi definition - definitely sufficient precision
constexpr double pi = 3.14159265358979323846;

// vector math library GLM
#define GLM_FORCE_SWIZZLE
#define GLM_SWIZZLE_XYZW
#include "../GLM/glm.hpp"					//general vector types
#include "../GLM/gtc/matrix_transform.hpp"	// for glm::ortho
#include "../GLM/gtc/type_ptr.hpp"			//to send matricies gpu-side
#include "../GLM/gtx/rotate_vector.hpp"
#include "../GLM/gtx/transform.hpp"
#include "../GLM/gtx/string_cast.hpp"		// to_string for glm types

// not sure as to the utility of this
// #define GLX_GLEXT_PROTOTYPES

// convenience defines for GLM
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::ivec2;
using glm::ivec3;
using glm::ivec4;
using glm::mat3;
using glm::mat4;

// OpenGL Loader
#include "../ImGUI/gl3w.h"

// GUI library (dear ImGUI)
#include "../ImGUI/TextEditor.h"
#include "../ImGUI/imgui.h"
#include "../ImGUI/imgui_impl_sdl.h"
#include "../ImGUI/imgui_impl_opengl3.h"

// SDL includes - windowing, gl context, system info
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
// #include <SDL2/SDL_opengl_glext.h>

// image load/save/resize/access/manipulation wrapper
#include "../ImageHandling/Image.h"

// simple std::chrono wrapper
#include "Timer.h"

// tracy profiler annotation
#include "../tracy/public/tracy/Tracy.hpp"

// font rendering header
#include "../fonts/fontRenderer/renderer.h"

// wrapper for TinyOBJLoader
#include "../ModelLoading/TinyOBJLoader/tiny_obj_loader.h"

// software rasterizer reimplementation
#include "../SoftRast/SoftRast.h"

// shader compilation wrapper
#include "shaders/lib/shaderWrapper.h"

// coloring of CLI output
#include "../fonts/colors.h"

// diamond square heightmap generation
#include "../noise/diamondSquare/diamond_square.h"

// Brent Werness' Voxel Automata Terrain
#include "../noise/VAT/VAT.h"

// more general noise solution
#include "../noise/FastNoise2/include/FastNoise/FastNoise.h"

// bringing the old perlin impl back
#include "../noise/perlin.h"

// Niels Lohmann - JSON for Modern C++
#include "../Serialization/JSON/json.hpp"
using json = nlohmann::json;

// tinyXML2 XML parser
#include "../Serialization/tinyXML2/tinyxml2.h"
using XMLDocument = tinyxml2::XMLDocument;

struct configData {
	uint32_t windowFlags = 0;
	string windowTitle = string( "NQADE" );
	int32_t width = 0;
	int32_t height = 0;
	bool linearFilter = false;
	ivec2 windowOffset = ivec2( 0, 0 );
	uint8_t startOnScreen = 0;

	uint8_t MSAACount = 0;
	vec4 clearColor = vec4( 0.0f );
	bool vSyncEnable = true;
	uint8_t OpenGLVersionMajor = 4;
	uint8_t OpenGLVersionMinor = 3;
	bool reportPlatformInfo = true;
	bool enableDepthTesting = false;

	// anything else ... ?
};

struct colorGradeParameters {
	int tonemapMode = 6; // todo: write an enum for this
	float gamma = 1.1f;
	float colorTemp = 6500.0f;
};


// Siren specific structs
enum class renderMode { previewColor, previewNormal, previewDepth, previewShaded, pathtrace };
struct hostParameters {
	bool rendererRequiresUpdate = true;				// has the viewer moved since the screen has updated?
	renderMode currentMode = renderMode::previewColor; // how should we render the scene?
	int performanceHistory = 250;					// how many datapoints to keep for tile count / fps
	int fullscreenPasses = 0;						// how many times have we exhausted the tile vector

	int tilePerFrameCap = 128;						// max number of tiles allowed to be executed in a frame update
	bool tileSizeUpdated = true;					// (re)builds the tile list
	int tileSize = 512;								// size of one rendering tile ( square )

	// this stuff is still WIP
	// int screenshotDim = WIDTH; 					// width of the screenshot - the code maintains the aspect ratio of HEIGHT/WIDTH
	// int numSamplesScreenshot = 128; 				// how many samples to take when rendering the screenshot
};

struct coreParameters {
	glm::ivec2 tileOffset = glm::ivec2( 0, 0 ); 	// x, y of current tile
	glm::ivec2 noiseOffset = glm::ivec2( 0, 0 );	// updated once a frame, offset blue noise texture so there isn't Voraldo's stroke pattern in v1.2
	int maxSteps = 100;								// max raymarch steps
	int maxBounces = 32;							// max pathtrace bounces
	float maxDistance = 100.0f;						// max raymarch distance
	float epsilon = 0.0001f;						// raymarch surface epsilon
	float exposure = 0.98f;							// scale factor for the final color result
	float focusDistance = 10.0f;					// used for the thin lens approximation ( include an intensity scalar to this as well ( resize jitter disk ) )
	float thinLensIntensity = 1.0f;					// scales the disk offset for the thin lens approximation ( scales the intensity of the effect )
	int normalMethod = 1;							// method for calculating the surface normal for the SDF geometry
	float FoV = 0.618f;								// FoV for the rendering - higher is wider
	glm::vec3 viewerPosition = glm::vec3( 0.0f );	// location of the viewer
	glm::vec3 basisX = glm::vec3( 1.0f, 0.0f, 0.0f );	// basis vectors are used to control viewer movement and rotation, as well as create the camera
	glm::vec3 basisY = glm::vec3( 0.0f, 1.0f, 0.0f );
	glm::vec3 basisZ = glm::vec3( 0.0f, 0.0f, 1.0f );
	float understep = 0.618;						// scale factor on distance estimate when applied to the step during marching - lower is slower, as more steps are taken before reaching the surface
};

struct lensParameters {
	float lensScaleFactor = 1.0f;					// size of the lens object - vessica pices, in 3d ( intersection of two offset spheres )
	float lensRadius1 = 10.0f;						// radius of the first side of the lens
	float lensRadius2 = 3.0f;						// radius of the second side of the lens
	float lensThickness = 0.3f;						// amount by which the spheres are offset from one another, before intersection
	float lensRotate = 0.0f;						// rotates the lens - should this be done this way, or two points? maybe a quaternion
	float lensIOR = 1.2f;							// index of refraction of the lens material
};

struct sceneParameters {
	glm::vec3 redWallColor		= glm::vec3( 1.0f, 0.0f, 0.0f );
	glm::vec3 greenWallColor	= glm::vec3( 0.0f, 1.0f, 0.0f );
	glm::vec3 whiteWallColor	= glm::vec3( 0.618f );
	glm::vec3 floorCielingColor	= glm::vec3( 0.9f );
	glm::vec3 metallicDiffuse	= glm::vec3( 0.618f, 0.362f, 0.04f );
};

struct postParameters {
	int ditherMode = 0;								// colorspace
	int ditherMethod = 0;							// bitcrush bitcount or exponential scalar
	int ditherPattern = 0;							// pattern used to dither the output
	int tonemapMode = 0;							// tonemap curve to use
	int depthMode = 0;								// depth fog method
	float depthScale = 0.0;							// scalar for depth term, when computing depth effects ( fog )
	float gamma = 1.6;								// gamma correction term for the color result
	float colorTemp = 6500.0f;						// warmer or cooler colored image, 6500k neutral by default
	int displayType = 0;							// mode selector - show normals, show depth, show color, show postprocessed version
};


// Function to get color temperature from shadertoy user BeRo
// from the author:
//   Color temperature (sRGB) stuff
//   Copyright (C) 2014 by Benjamin 'BeRo' Rosseaux
//   Because the german law knows no public domain in the usual sense,
//   this code is licensed under the CC0 license
//   http://creativecommons.org/publicdomain/zero/1.0/
// Valid from 1000 to 40000 K (and additionally 0 for pure full white)
inline vec3 GetColorForTemperature ( float temperature ) {
	// Values from:
	// http://blenderartists.org/forum/showthread.php?270332-OSL-Goodness&p=2268693&viewfull=1#post2268693
	mat3 m =
		( temperature <= 6500.0f )
			? mat3( vec3( 0.0f, -2902.1955373783176f, -8257.7997278925690f ),
					vec3( 0.0f, 1669.5803561666639f, 2575.2827530017594f ),
					vec3( 1.0f, 1.3302673723350029f, 1.8993753891711275f ) )
			: mat3( vec3( 1745.0425298314172f, 1216.6168361476490f, -8257.7997278925690f ),
					vec3( -2666.3474220535695f, -2173.1012343082230f, 2575.2827530017594f ),
					vec3( 0.55995389139931482f, 0.70381203140554553f, 1.8993753891711275f ) );

	return glm::mix( glm::clamp( vec3( m[ 0 ] / ( vec3( glm::clamp( temperature, 1000.0f, 40000.0f ) ) +
		m[ 1 ] ) + m[ 2 ] ), vec3( 0.0f ), vec3( 1.0f ) ), vec3( 1.0f ), glm::smoothstep( 1000.0f, 0.0f, temperature ) );
}

#endif
