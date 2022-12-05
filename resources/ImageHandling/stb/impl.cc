// this file is to fulfil the requirement of the STB librarires, that they have
//	the corresponding #define in exactly one .cc file, so that they have a place
//	to compile the actual implementation. This file is compiled as a library in
//	CMakeLists, before the headers are included where the are used, then the link
//	step makes sure that everything gets mashed together and Just Works tm
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

// putting this here for now, since I don't really have a better place
//	not really any issue with keeping it here, it's just not related to
//	the image handling like the rest of the stuff in this file
#define STB_INCLUDE_IMPLEMENTATION
#define STB_INCLUDE_LINE_GLSL
#include "../../engineCode/shaders/lib/stb_include.h"
