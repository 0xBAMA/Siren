#include "../../engine/includes.h"

#ifndef FONTRENDERER_H
#define FONTRENDERER_H

// double bar frame
#define TOP_LEFT_DOUBLE_CORNER			201
#define TOP_RIGHT_DOUBLE_CORNER			187
#define BOTTOM_LEFT_DOUBLE_CORNER		200
#define BOTTOM_RIGHT_DOUBLE_CORNER	188
#define VERTICAL_DOUBLE							186
#define HORIZONTAL_DOUBLE						205

// single bar frame
#define TOP_LEFT_SINGLE_CORNER			218
#define TOP_RIGHT_SINGLE_CORNER			191
#define BOTTOM_LEFT_SINGLE_CORNER		192
#define BOTTOM_RIGHT_SINGLE_CORNER	217
#define VERTICAL_SINGLE							179
#define HORIZONTAL_SINGLE						196

// curly scroll thingy
#define CURLY_SCROLL_TOP						244
#define CURLY_SCROLL_BOTTOM					245
#define CURLY_SCROLL_MIDDLE					179

// percentage fill blocks
#define FILL_0											32
#define FILL_25											176
#define FILL_50											177
#define FILL_75											178
#define FILL_100										219

// some colors
#define GOLD	glm::ivec3( 191, 146,  23 )
#define GREEN	glm::ivec3( 100, 186,  20 )
#define BLUE	glm::ivec3(  50, 103, 184 )
#define WHITE	glm::ivec3( 245, 245, 245 )
#define GREY	glm::ivec3( 169, 169, 169 )
#define BLACK	glm::ivec3(  16,  16,  16 )


struct cChar {
	unsigned char data[ 4 ] = { 255, 255, 255, 0 };
	cChar() {}
	cChar( unsigned char c ) {
		data[ 3 ] = c;
	}
	cChar( glm::ivec3 color, unsigned char c ) {
		data[ 0 ] = color.x;
		data[ 1 ] = color.y;
		data[ 2 ] = color.z;
		data[ 3 ] = c;
	}
};

class Layer {
public:
	Layer ( int w, int h ) : width( w ), height( h ) {
		Resize( w, h );
		glGenTextures( 1, &textureHandle );
	}

	void Resize ( int w, int h ) {
		if ( bufferBase != nullptr ) { free( bufferBase ); }
		bufferBase = ( cChar * ) malloc( sizeof( cChar ) * w * h );
		ClearBuffer();
	}

	void Resend () {
		glActiveTexture( GL_TEXTURE2 );
		glBindTexture( GL_TEXTURE_2D, textureHandle );
		if ( bufferDirty ) {
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bufferBase );
			bufferDirty = false;
		}
	}

	void Draw () { // bind the data texture and dispatch
		if ( bufferDirty ) {
			Resend();
		}

		// bind the data texture to slot 1
		glBindImageTexture( 1, textureHandle, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );

		// this is actually very sexy - workgroup is 8x16, same as a glyph's dimensions
		glDispatchCompute( width, height, 1 );
	}

	void ClearBuffer () {
		size_t numBytes = sizeof( cChar ) * width * height;
		memset( ( void * ) bufferBase, 0, numBytes );
	}

	cChar GetCharAt ( glm::uvec2 position ) {
		if ( position.x < width && position.y < height ) // >= 0 is implicit with unsigned
			return *( bufferBase + sizeof( cChar ) * ( position.x + position.y * width ) );
		else
			return cChar();
	}

	void WriteCharAt ( glm::uvec2 position, cChar c ) {
		if ( position.x < width && position.y < height ) {
			int index = position.x + position.y * width;
			bufferBase[ index ] = c;
		}
	}

	void WriteString ( glm::uvec2 min, glm::uvec2 max, std::string str, glm::ivec3 color ) {
		bufferDirty = true;
		glm::uvec2 cursor = min;
		for ( auto c : str ) {
			if ( c == '\t' ) {
				cursor.x += 2;
			} else if ( c == '\n' ) {
				cursor.y++;
				cursor.x = min.x;
				if ( cursor.y >= max.y ) {
					break;
				}
			} else if ( c == 0 ) { // null character, don't draw anything - can use 32 aka space to overwrite with blank
				cursor.x++;
			} else {
				WriteCharAt( cursor, cChar( color, ( unsigned char )( c ) ) );
				cursor.x++;
			}
			if ( cursor.x >= max.x ) {
				cursor.y++;
				cursor.x = min.x;
				if ( cursor.y >= max.y ) {
					break;
				}
			}
		}
	}

	void WriteCCharVector ( glm::uvec2 min, glm::uvec2 max, std::vector< cChar > vec ) {
		bufferDirty = true;
		glm::uvec2 cursor = min;
		for ( unsigned int i = 0; i < vec.size(); i++ ) {
			if ( vec[ i ].data[ 4 ] == '\t' ) {
				cursor.x += 2;
			} else if ( vec[ i ].data[ 4 ] == '\n' ) {
				cursor.y++;
				cursor.x = min.x;
				if ( cursor.y >= max.y ) {
					break;
				}
			} else if ( vec[ i ].data[ 4 ] == 0 ) { // special no-write character
				cursor.x++;
			} else {
				WriteCharAt( cursor, vec[ i ] );
				cursor.x++;
			}
			if ( cursor.x >= max.x ) {
				cursor.y++;
				cursor.x = min.x;
				if ( cursor.y >= max.y ) {
					break;
				}
			}
		}
	}

	void DrawRandomChars ( int n ) {
		bufferDirty = true;
		std::random_device r;
		std::seed_seq s{ r(), r(), r(), r(), r(), r(), r(), r(), r() };
		auto gen = std::mt19937_64( s );
		std::uniform_int_distribution< unsigned char > cDist( 0, 255 );
		std::uniform_int_distribution< unsigned int > xDist( 0, width - 1 );
		std::uniform_int_distribution< unsigned int > yDist( 0, height - 1 );
		for ( int i = 0; i < n; i++ )
			WriteCharAt( glm::uvec2( xDist( gen ), yDist( gen ) ), cChar( glm::ivec3( cDist( gen ), cDist( gen ), cDist( gen ) ), cDist( gen ) ) );
	}

	void DrawDoubleFrame ( glm::uvec2 min, glm::uvec2 max, glm::ivec3 color ) {
		bufferDirty = true;
		WriteCharAt( min, cChar( color, TOP_LEFT_DOUBLE_CORNER ) );
		WriteCharAt( glm::uvec2( max.x, min.y ), cChar( color, TOP_RIGHT_DOUBLE_CORNER ) );
		WriteCharAt( glm::uvec2( min.x, max.y ), cChar( color, BOTTOM_LEFT_DOUBLE_CORNER ) );
		WriteCharAt( max, cChar( color, BOTTOM_RIGHT_DOUBLE_CORNER ) );
		for( unsigned int x = min.x + 1; x < max.x; x++  ){
			WriteCharAt( glm::uvec2( x, min.y ), cChar( color, HORIZONTAL_DOUBLE ) );
			WriteCharAt( glm::uvec2( x, max.y ), cChar( color, HORIZONTAL_DOUBLE ) );
		}
		for( unsigned int y = min.y + 1; y < max.y; y++  ){
			WriteCharAt( glm::uvec2( min.x, y ), cChar( color, VERTICAL_DOUBLE ) );
			WriteCharAt( glm::uvec2( max.x, y ), cChar( color, VERTICAL_DOUBLE ) );
		}
	}

	void DrawSingleFrame ( glm::uvec2 min, glm::uvec2 max, glm::ivec3 color ) {
		bufferDirty = true;
		WriteCharAt( min, cChar( color, TOP_LEFT_SINGLE_CORNER ) );
		WriteCharAt( glm::uvec2( max.x, min.y ), cChar( color, TOP_RIGHT_SINGLE_CORNER ) );
		WriteCharAt( glm::uvec2( min.x, max.y ), cChar( color, BOTTOM_LEFT_SINGLE_CORNER ) );
		WriteCharAt( max, cChar( color, BOTTOM_RIGHT_SINGLE_CORNER ) );
		for( unsigned int x = min.x + 1; x < max.x; x++  ){
			WriteCharAt( glm::uvec2( x, min.y ), cChar( color, HORIZONTAL_SINGLE ) );
			WriteCharAt( glm::uvec2( x, max.y ), cChar( color, HORIZONTAL_SINGLE ) );
		}
		for( unsigned int y = min.y + 1; y < max.y; y++  ){
			WriteCharAt( glm::uvec2( min.x, y ), cChar( color, VERTICAL_SINGLE ) );
			WriteCharAt( glm::uvec2( max.x, y ), cChar( color, VERTICAL_SINGLE ) );
		}
	}

	void DrawCurlyScroll ( glm::uvec2 start, unsigned int length, glm::ivec3 color ) {
		bufferDirty = true;
		WriteCharAt( start, cChar( color, CURLY_SCROLL_TOP ) );
		for ( unsigned int i = 1; i < length; i++ ) {
			WriteCharAt( start + glm::uvec2( 0, i ), cChar ( color, CURLY_SCROLL_MIDDLE ) );
		}
		WriteCharAt( start + glm::uvec2( 0, length ), cChar( color, CURLY_SCROLL_BOTTOM ) );
	}

	void DrawRectRandom ( glm::uvec2 min, glm::uvec2 max, glm::ivec3 color ) {
		bufferDirty = true;
		std::random_device r;
		std::seed_seq s{ r(), r(), r(), r(), r(), r(), r(), r(), r() };
		auto gen = std::mt19937_64( s );
		std::uniform_int_distribution< unsigned char > fDist( 0, 4 );
		const unsigned char fills[ 5 ] = { FILL_0, FILL_25, FILL_50, FILL_75, FILL_100 };

		for( unsigned int x = min.x; x <= max.x; x++ ) {
			for( unsigned int y = min.y; y <= max.y; y++ ) {
				WriteCharAt( glm::uvec2( x, y ), cChar( color, fills[ fDist( gen ) ] ) );
			}
		}
	}

	void DrawRectConstant ( glm::uvec2 min, glm::uvec2 max, cChar c ) {
		bufferDirty = true;
		for( unsigned int x = min.x; x <= max.x; x++ ) {
			for( unsigned int y = min.y; y <= max.y; y++ ) {
				WriteCharAt( glm::uvec2( x, y ), c );
			}
		}
	}

	unsigned int width, height;
	GLuint textureHandle;
	bool bufferDirty;
	cChar * bufferBase = nullptr;
};

class layerManager {
public:
	layerManager () {}
	void Init ( int w, int h, GLuint shader ) {
		width = w;
		height = h;

		// how many complete 8x16px glyphs to cover the image ( x and y )
		numBinsWidth = std::floor( width / 8 );
		numBinsHeight = std::floor( height / 16 );

		// currently just two layers, background and foreground
		layers.push_back( Layer( numBinsWidth, numBinsHeight ) );
		layers.push_back( Layer( numBinsWidth, numBinsHeight ) );

		// get the compiled shader
		fontWriteShader = shader;

		// generate the altas texture - only ever needed in the context of layerManager
		Image fontAtlas( "src/fonts/fontRenderer/whiteOnClear.png", LODEPNG );
		fontAtlas.FlipVertical(); // for some reason loading upside down

		// font atlas GPU setup
		glGenTextures( 1, &atlasTexture );
		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_2D, atlasTexture );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, fontAtlas.width, fontAtlas.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &fontAtlas.data.data()[ 0 ] );
	}

	void Update ( float seconds ) {
		// std::string fps( "60.00 fps " );
		// std::string ms( "16.666 ms " );
		// WriteString( glm::uvec2( width - fps.length(), 1 ), glm::uvec2( width, 1 ), fps, WHITE );
		// WriteString( glm::uvec2( width - ms.length(), 0 ), glm::uvec2( width, 0 ), ms, WHITE );

// little bit of input smoothing, average over NUM_FRAMES_SMOOTHING frames - implementation needs work
		#define NUM_FRAMES_SMOOTHING 5
		float ms = seconds * 1000.0f;
		static std::deque< float > msHistory;
		msHistory.push_back( ms );
		if ( msHistory.size() > NUM_FRAMES_SMOOTHING ) {
			msHistory.pop_front();
		}
		ms = 0.0f;
		for ( unsigned int i = 0; i < msHistory.size(); i++ ) {
			ms += ( msHistory[ i ] / msHistory.size() );
		}

		std::stringstream ss;
		ss << " total: " << std::setw( 10 ) << std::setfill( ' ' ) << std::setprecision( 4 ) << std::fixed << ms << "ms";
		layers[ 0 ].DrawRectConstant( glm::uvec2( layers[ 0 ].width - ss.str().length(), 0 ), glm::uvec2( layers[ 0 ].width, 0 ), cChar( BLACK, FILL_100 ) );
		layers[ 1 ].WriteString( glm::uvec2( layers[ 1 ].width - ss.str().length(), 0 ), glm::uvec2( layers[ 1 ].width, 0 ), ss.str(), WHITE );
	}

	void Draw ( GLuint writeTarget ) {
		glUseProgram( fontWriteShader );
		// bind the appropriate textures ( atlas( 0 ) + write target( 2 ) )
		glBindImageTexture( 0, atlasTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );
		glBindImageTexture( 2, writeTarget, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );
		for ( auto layer : layers ) {
			layer.Draw(); // data texture( 1 ) is bound internal to this function, since it is unique to each layer
		}
	}

	int width, height;
	int numBinsWidth;
	int numBinsHeight;

	GLuint fontWriteShader;
	GLuint atlasTexture;

	// allocation of the textures happens in Layer()
	std::vector< Layer > layers;
};

#endif
