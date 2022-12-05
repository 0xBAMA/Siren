#ifndef SHADER_H
#define SHADER_H

// OpenGL function loader
#include "../../../ImGUI/gl3w.h"

// #include processing for shader files
#include "stb_include.h"

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>

constexpr int numCharsReport = 4096;

// using std statements
using std::cin;
using std::cout;
using std::cerr;
using std::flush;
using std::endl;
using std::string;
using std::vector;
using std::ifstream;
using std::stringstream;

/*==============================================================================
Take in a string, potentially containing one or more #include statements, and
return a string which contains all the header stuff in place of these statements
==============================================================================*/
static string ProcessIncludeString ( string source ) {
	char includeError[ 256 ];
	char * inject = nullptr;
	char * filename = nullptr;
	char * cstrCode = stb_include_string( source.c_str(), inject, const_cast<char*>( "resources/engineCode/shaders/lib" ), filename, includeError );
	return string( cstrCode );
}

/*==============================================================================
Take in a path to a file, read it - report with path if the load fails,
otherwise return the loaded string
==============================================================================*/
static string LoadStringFromFile ( string path, bool &success ) {
	string src;
	ifstream shaderFile;
	// set up to catch exceptions
	shaderFile.exceptions( ifstream::badbit );
	try { // to read the file
		shaderFile.open( path );
		stringstream shaderStream;
		shaderStream << shaderFile.rdbuf();
		shaderFile.close();
		src = shaderStream.str();
	} catch ( std::ifstream::failure &e ) {
		success = false;
		cout << "shader at " << path << " failed to open." << endl;
	}
	return src;
}

/*==============================================================================
Get a string associated with the shader type specified by shaderType
==============================================================================*/
static string GetStringForEnum ( GLenum shaderType ) {
	switch ( shaderType ){
		case GL_VERTEX_SHADER: return string( "vertex shader" ); break;
		case GL_FRAGMENT_SHADER: return string( "fragment shader" ); break;
		case GL_COMPUTE_SHADER: return string( "compute shader" ); break;
	}
	return string(); // fuck you, compiler
}

/*==============================================================================
Create and compile shader and report any errors - return shader handle
==============================================================================*/
static GLuint ShaderCompile ( const char * source, GLenum shaderType, bool &result, stringstream &report ) {
	if ( !result ) return 0;
	GLuint shader = glCreateShader( shaderType );
	glShaderSource( shader, 1, &source, NULL );
	glCompileShader( shader );
	GLint success;
	GLchar infoLog[ numCharsReport ];
	glGetShaderiv( shader, GL_COMPILE_STATUS, &success );
	result = true;
	if ( !success ) {
		result = false;
		glGetShaderInfoLog( shader, numCharsReport, NULL, infoLog );
		cout << "Shader compilation failed during " << GetStringForEnum( shaderType )
			<< " compilation ... " << endl << infoLog << endl;
		report << "Shader compilation failed during " << GetStringForEnum( shaderType )
			<< " compilation ... " << endl << infoLog << endl;
	} else {
		report << "Shader compilation succeded ( " << GetStringForEnum( shaderType ) << " )" << endl;
	}
	return shader;
}

/*==============================================================================
program becomes the linked shader program, or reports failure
==============================================================================*/
static void AttachAndLink ( GLuint& program, vector<GLuint> shaders, bool &result, stringstream &report ) {
	if ( !result ) return;
	program = glCreateProgram();
	for ( auto& shader : shaders )
		glAttachShader( program, shader );
	GLint success;
	GLchar infoLog[ numCharsReport ];
	glLinkProgram( program );
	glGetProgramiv( program, GL_LINK_STATUS, &success );
	result = true;
	if ( !success ) {
		result = false;
		glGetProgramInfoLog( program, numCharsReport, NULL, infoLog );
		cout << "Linking failed: " << endl << infoLog << endl;
		report << "Linking failed: " << endl << infoLog << endl;
	} else {
		report << "Shader linking succeded." << endl;
	}
	for ( auto& shader : shaders )
		glDeleteShader( shader );
}

/*==============================================================================
Construct a standard vertex+fragment pair from the given input strings
==============================================================================*/
class regularShader {
public:
	bool success = true;
	GLuint shaderHandle;
	stringstream report;
	regularShader ( string pathV, string pathF ) {
		// read the source
		string codeV = ProcessIncludeString( LoadStringFromFile( pathV, success ) );
		string codeF = ProcessIncludeString( LoadStringFromFile( pathF, success ) );
		// compile it
		GLuint shaderV = ShaderCompile( codeV.c_str(), GL_VERTEX_SHADER, success, report );
		GLuint shaderF = ShaderCompile( codeF.c_str(), GL_FRAGMENT_SHADER, success, report );
		AttachAndLink( shaderHandle, { shaderV, shaderF }, success, report );
	}
};

/*==============================================================================
Create a compute shader - options are the following:
	- computeShader::shaderSource::fromFile
	- computeShader::shaderSource::fromString
depending on usage, should be fairly self explanatory
==============================================================================*/
class computeShader {
public:
	bool success = true;
	GLuint shaderHandle;
	stringstream report;
	enum class shaderSource { fromFile, fromString };
	computeShader ( string input, shaderSource source = shaderSource::fromFile ) {
		switch ( source ) {
		case shaderSource::fromFile:
			// input becomes the shader source, loaded from the path
			input = LoadStringFromFile( input, success );
			[[fallthrough]];
		case shaderSource::fromString:
			// compile with "input" treated as the program source
			input = ProcessIncludeString( input );
			GLuint shaderC = ShaderCompile( input.c_str(), GL_COMPUTE_SHADER, success, report );
			AttachAndLink( shaderHandle, { shaderC }, success, report );
			break;
		}
	}
};

#endif
