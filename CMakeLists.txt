cmake_minimum_required( VERSION 3.16 )

project( NQADE VERSION 1.2 LANGUAGES C CXX )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED True )
set( CMAKE_CXX_FLAGS "-std=c++17 -lstdc++fs -O4" )
set( CMAKE_REQUIRED_FLAGS -lstdc++fs )

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

# this makes SDL2 work
set( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )

find_package( SDL2 REQUIRED )
add_library( sdl2 INTERFACE )
target_include_directories( sdl2
	INTERFACE
		${SDL2_INCLUDE_DIR}
)
target_link_libraries( sdl2
	INTERFACE
		${SDL2_LIBRARY}
		${CMAKE_DL_LIBS}
)

# this makes OpenGL work
set( OpenGL_GL_PREFERENCE "GLVND" )
find_package( OpenGL REQUIRED )
add_library( opengl INTERFACE )
target_link_libraries( opengl INTERFACE OpenGL::GL )

# FastNoise2
add_subdirectory( ${PROJECT_SOURCE_DIR}/resources/noise/FastNoise2 )

# Tracy Profiler
option(TRACY_ENABLE "" ON)
option(TRACY_ON_DEMAND "" ON)
add_subdirectory( resources/tracy )

# assimp model loader
add_subdirectory( ${PROJECT_SOURCE_DIR}/resources/ModelLoading/assimp )

# this is for ImGUI
add_library( imgui
	resources/ImGUI/imgui_impl_sdl.cc
	resources/ImGUI/imgui_impl_opengl3.cc
	resources/ImGUI/imgui.cc
	resources/ImGUI/imgui_demo.cc
	resources/ImGUI/imgui_draw.cc
	resources/ImGUI/imgui_widgets.cc
	resources/ImGUI/imgui_tables.cc
	resources/ImGUI/imgui_gl3w.cc
	resources/ImGUI/TextEditor.cc
	resources/ImGUI/LanguageDefinitions.cc
)

target_link_libraries( imgui PUBLIC sdl2 opengl )

target_compile_options( imgui PUBLIC -I/usr/include/SDL2 )
target_compile_definitions( imgui PUBLIC -D IMGUI_IMPL_OPENGL_LOADER_GL3W -D_REENTRANT )

# this is the bigint library to support the VAT code
add_library( BigInt
	resources/noise/VAT/BigInt/BigUnsigned.cc
	resources/noise/VAT/BigInt/BigInteger.cc
	resources/noise/VAT/BigInt/BigIntegerAlgorithms.cc
	resources/noise/VAT/BigInt/BigUnsignedInABase.cc
	resources/noise/VAT/BigInt/BigIntegerUtils.cc
)

target_compile_options( BigInt PUBLIC -Wno-deprecated )

add_library( tinyXML2 resources/Serialization/tinyXML2/tinyxml2.cc )

add_library( STB_ImageUtilsWrapper resources/ImageHandling/stb/impl.cc )

add_library( TinyOBJLoader resources/ModelLoading/TinyOBJLoader/tiny_obj_loader.cc )

add_library( CompilerFlags INTERFACE )
target_compile_options( CompilerFlags INTERFACE -Wall -O3 -std=c++17 -lGL -lstdc++fs -lSDL2 -ldl -Wno-maybe-uninitialized -Wno-unused-function ) # suppresses warnings for Tracy

# this builds the final executable
add_executable( exe
	resources/tracy/public/TracyClient.cpp
	resources/noise/perlin.cc
	resources/engineCode/main.cc
	resources/engineCode/engine.cc
	resources/engineCode/engineUtils.cc
	resources/engineCode/engineInit.cc
	resources/engineCode/engineImguiUtils.cc
	resources/ImageHandling/LodePNG/lodepng.cc
)

target_link_libraries( exe
	PUBLIC
	imgui
	BigInt
	tinyXML2
	opengl
	sdl2
	assimp
	STB_ImageUtilsWrapper
	stdc++fs
	FastNoise
	Tracy::TracyClient
	TinyOBJLoader
	CompilerFlags
)
