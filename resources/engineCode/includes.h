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

// pi definition - definitely sufficient precision
constexpr double pi = 3.14159265358979323846;

// MSAA count - effects OpenGL geometry evaluation
constexpr int MSAACount = 1;

// vector math library GLM
#define GLM_FORCE_SWIZZLE
#define GLM_SWIZZLE_XYZW
#include "../GLM/glm.hpp"														// general vector types
#include "../GLM/gtc/matrix_transform.hpp"					// for glm::ortho
#include "../GLM/gtc/type_ptr.hpp"									// to send matricies gpu-side
#include "../GLM/gtx/rotate_vector.hpp"
#include "../GLM/gtx/transform.hpp"
#include "../GLM/gtx/quaternion.hpp"
#include "../GLM/gtx/string_cast.hpp"								// to_string for glm types

// not sure as to the utility of this
#define GLX_GLEXT_PROTOTYPES

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

// png loading library - very powerful
#include "../LodePNG/lodepng.h"

// wrapper for TinyOBJLoader
#include "../TinyOBJLoader/objLoader.h"

// shader compilation wrapper
#include "shaders/shader.h"

// coloring of CLI output
#include "../fonts/colors.h"

// diamond square heightmap generation
#include "../noise/diamondSquare/diamond_square.h"

// Brent Werness' Voxel Automata Terrain
#include "../noise/VAT/VAT.h"

// more general noise solution
#include "../noise/FastNoise2/include/FastNoise/FastNoise.h"

// Niels Lohmann - JSON for Modern C++
#include "../JSON/json.hpp"
using json = nlohmann::json;

// #define WIDTH 640
// #define HEIGHT 480

#define WIDTH  1920 / 2											// width of the texture that the tiles render to
#define HEIGHT 1080 / 2										// height of the texture that the tiles render to

#define PERFORMANCEHISTORY 250								// how many datapoints to keep for tile count / fps// #define TILESIZE 16										// play around with this - higher seems to be higher perf, but more frame latency - set to 256 or more on desktop and let er rip
#define TILESIZE 256

struct hostParameters {
	int screenshotDim = WIDTH; 								// width of the screenshot - the code maintains the aspect ratio of HEIGHT/WIDTH
	int tilePerFrameCap = 128;								// max number of tiles allowed to be executed in a frame update
	int numSamplesScreenshot = 128; 						// how many samples to take when rendering the screenshot
};

struct coreParameters {
	glm::ivec2 tileOffset = glm::ivec2( 0, 0 ); 			// x, y of current tile
	glm::ivec2 noiseOffset = glm::ivec2( 0, 0 );			// updated once a frame, offset blue noise texture so there isn't Voraldo's stroke pattern in v1.2
	int maxSteps = 100;										// max raymarch steps
	int maxBounces = 10;									// max pathtrace bounces
	float maxDistance = 100.0;								// max raymarch distance
	float epsilon = 0.001;									// raymarch surface epsilon
	float exposure = 0.98;									// scale factor for the final color result
	float focusDistance = 10.0;								// used for the thin lens approximation ( include an intensity scalar to this as well ( resize jitter disk ) )
	float thinLensIntensity = 1.0;							// scales the disk offset for the thin lens approximation ( scales the intensity of the effect )
	int normalMethod = 1;									// method for calculating the surface normal for the SDF geometry
	float FoV = 0.618;										// FoV for the rendering - higher is wider
	glm::vec3 viewerPosition = glm::vec3( 0.0f );	// location of the viewer
	glm::vec3 basisX = glm::vec3( 1.0f, 0.0f, 0.0f );			// basis vectors are used to control viewer movement and rotation, as well as create the camera
	glm::vec3 basisY = glm::vec3( 0.0f, 1.0f, 0.0f );
	glm::vec3 basisZ = glm::vec3( 0.0f, 0.0f, 1.0f );
};

struct lensParameters {
	float lensScaleFactor = 1.0;							// size of the lens object - vessica pices, in 3d ( intersection of two offset spheres )
	float lensRadius1 = 10.0;								// radius of the first side of the lens
	float lensRadius2 = 3.0;								// radius of the second side of the lens
	float lensThickness = 0.3;								// amount by which the spheres are offset from one another, before intersection
	float lensRotate = 0.0;									// rotates the lens - should this be done this way, or two points? maybe a quaternion
	float lensIOR = 1.2;									// index of refraction of the lens material
};

struct postParameters {
	int ditherMode = 0;										// colorspace
	int ditherMethod = 0;									// bitcrush bitcount or exponential scalar
	int ditherPattern = 0;									// pattern used to dither the output
	int tonemapMode = 0;									// tonemap curve to use
	int depthMode = 0;										// depth fog method
	float depthScale = 0.0;									// scalar for depth term, when computing depth effects ( fog )
	float gamma = 1.6;										// gamma correction term for the color result
	int displayType = 0;									// mode selector - show normals, show depth, show color, show postprocessed version
};
#endif
