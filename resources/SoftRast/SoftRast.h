#ifndef SOFTRAST
#define SOFTRAST

#include "../ModelLoading/TinyOBJLoader/tiny_obj_loader.h"
#include "../engineCode/includes.h"

struct triangle {
	vec3 p0, p1, p2; // per vertex position
	vec3 t0, t1, t2; // per vertex texcoord xy, texture index
	vec3 n0, n1, n2; // per vertex normals
	vec3 c0, c1, c2; // per vertex color

	vec3 t; // tangent
	vec3 b; // bitangent
};

// helper functions
static const float RemapRange ( const float value, const float iMin, const float iMax, const float oMin, const float oMax ) {
	return ( oMin + ( ( oMax - oMin ) / ( iMax - iMin ) ) * ( value - iMin ) );
}

static const rgba RGBAFromVec4( vec4 color ) {
	rgba temp;
	temp.r = uint8_t( RemapRange( color.r, 0.0f, 1.0f, 0.0f, 255.0f ) );
	temp.g = uint8_t( RemapRange( color.g, 0.0f, 1.0f, 0.0f, 255.0f ) );
	temp.b = uint8_t( RemapRange( color.b, 0.0f, 1.0f, 0.0f, 255.0f ) );
	temp.a = uint8_t( RemapRange( color.a, 0.0f, 1.0f, 0.0f, 255.0f ) );
	return temp;
}

static const vec3 BarycentricCoords ( vec3 p0, vec3 p1, vec3 p2, vec3 P ) {
	vec3 s[ 2 ];
	for ( int i = 2; i--; ) {
		s[ i ][ 0 ] = p2[ i ] - p0[ i ];
		s[ i ][ 1 ] = p1[ i ] - p0[ i ];
		s[ i ][ 2 ] = p0[ i ] - P[ i ];
	}
	vec3 u = glm::cross( s[ 0 ], s[ 1 ] );
	if ( std::abs( u[ 2 ] ) > 1e-2f )	 	// If u[ 2 ] is zero then triangle ABC is degenerate
		return vec3( 1.0f - ( u.x + u.y ) / u.z, u.y / u.z, u.x / u.z );
	return vec3( -1.0f, 1.0f, 1.0f );	// in this case generate negative coordinates, it will be thrown away by the rasterizer
}

static const mat3 rotation( vec3 a, float angle ) {
	a = glm::normalize( a ); //a is the axis
	float s = sin( angle );
	float c = cos( angle );
	float oc = 1.0f - c;
	return mat3(
		oc * a.x * a.x + c,         oc * a.x * a.y - a.z * s,  oc * a.z * a.x + a.y * s,
		oc * a.x * a.y + a.z * s,   oc * a.y * a.y + c,        oc * a.y * a.z - a.x * s,
		oc * a.z * a.x - a.y * s,   oc * a.y * a.z + a.x * s,  oc * a.z * a.z + c
	);
}

// if for some reason I need the mat4 version
// static const mat4 rotation( vec3 a, float angle ) {
// 	a = glm::normalize( a ); //a is the axis
// 	float s = sin( angle );
// 	float c = cos( angle );
// 	float oc = 1.0f - c;
// 	return mat4(
// 		oc * a.x * a.x + c,         oc * a.x * a.y - a.z * s,  oc * a.z * a.x + a.y * s, 0.0f,
// 		oc * a.x * a.y + a.z * s,   oc * a.y * a.y + c,        oc * a.y * a.z - a.x * s, 0.0f,
// 		oc * a.z * a.x - a.y * s,   oc * a.y * a.z + a.x * s,  oc * a.z * a.z + c,       0.0f,
// 		0.0f,                       0.0f,                      0.0f,                     1.0f
// 	);
// }


// Plans:
	// something to wrap texture reference, with or without interpolation - start with no interp for now
	// DrawModel, using TinyOBJLoader wrapper + transform
	// wrapper for writing a pixel's color and depth values, and optionally alpha blending
	// helper function for depth testing? it's already pretty short... tbd
	// object for holding triangle parameters, to simplify passing - need positions, need texcoords, need normals

// noisey noisey
constexpr bool verboseLoad = false;
constexpr bool verboseDraw = false;

class SoftRast {
public:
	SoftRast( uint32_t x = 0, uint32_t y = 0 ) : width( x ), height( y ) {
		Color = Image( x, y );
		Depth = ImageF( x, y );
		BlueNoise = Image( "resources/noise/blueNoise.png" ); // for sample jitter, write helper function to return some samples
		// init std::random generator as member variable, for picking blue noise sample point - then sweep along x or y to get low discrepancy sequence
	}

	vec4 BlueNoiseRef ( ivec2 loc ) {
		rgba value = BlueNoise.GetAtXY( loc.x % BlueNoise.width, loc.y % BlueNoise.height );
		return vec4( value.r / 255.0f, value.g / 255.0f, value.b / 255.0f, value.a / 255.0f ) - vec4( 0.5f );
	}

	std::vector<Image> texSet;
	void LoadTex ( string texPath ) {
		if ( !texPath.empty() ) {
			Image temp( texPath );
			temp.FlipVertical();

			// hackity hack hack - pre-squared the chain texture, only non-square texture in the set
			if ( temp.width == 256 ) {
				temp.Resize( 8.0f );
			} else if ( temp.width == 512 ) {
				temp.Resize( 4.0f );
			} else if ( temp.width == 1024 ) {
				temp.Resize( 2.0f );
			}

			if ( verboseLoad ) {
				cout << "    loading ";
				cout << temp.width << "x" << temp.height << " image" << newline;
				cout << "    done" << endl << endl;
			}

			texSet.push_back( temp );
		} else {
			Image temp( 2048, 2048 );

			if ( verboseLoad ) {
				cout << "    image defaulting";
				cout << temp.width << "x" << temp.height << " image" << newline;
				cout << "    done" << endl << endl;
			}

			texSet.push_back( temp );
		}
	}
	vec4 TexRef ( vec2 texCoord, int id ) {
		uint32_t x = uint32_t( texCoord.x * float( texSet[ id ].width ) );
		uint32_t y = uint32_t( texCoord.y * float( texSet[ id ].height ) );
		rgba val = texSet[ id ].GetAtXY( x, y );
		vec4 returnVal = vec4( float( val.r ) / 255.0f, float( val.g ) / 255.0f, float( val.b ) / 255.0f, float( val.a ) / 255.0f );
		// cout << returnVal.x << " " << returnVal.y << " " << returnVal.z << " " << returnVal.w << newline;
		return returnVal;
	}

	const vec3 NDCToPixelCoords ( vec3 NDCCoord ) {
		return vec3(
			RemapRange( NDCCoord.x, -1.0f, 1.0f, 0.0f, float( width - 1.0f ) ),
			RemapRange( NDCCoord.y, -1.0f, 1.0f, 0.0f, float( height - 1.0f ) ),
			NDCCoord.z
		);
	}

	// draw dot - draw smooth? tbd, some kind of gaussian distribution, how do you handle depth?
	void DrawDot ( vec3 position, vec4 color ) {
		position = NDCToPixelCoords( position );
		// TODO: support for alpha blending, based on existing buffer color + input color
		vec2 positionXY = vec2( position.x, position.y );
		if ( glm::clamp( positionXY, vec2( 0.0f ), vec2( width, height ) ) == positionXY && // point is on screen
			Depth.GetAtXY( uint32_t( position.x ), uint32_t( position.y ) ).r > position.z ) { // depth testing
			Color.SetAtXY( position.x, position.y, RGBAFromVec4( color ) );
			Depth.SetAtXY( position.x, position.y, { position.z, 0.0f, 0.0f, 0.0f } );
		}
	}

	// draw line
	void DrawLine ( vec3 p0, vec3 p1, vec4 color ) {
		int x0 = int( RemapRange( p0.x, -1.0f, 1.0f, 0.0f, float( width ) - 1.0f ) );
		int y0 = int( RemapRange( p0.y, -1.0f, 1.0f, 0.0f, float( height ) - 1.0f ) );
		int x1 = int( RemapRange( p1.x, -1.0f, 1.0f, 0.0f, float( width ) - 1.0f ) );
		int y1 = int( RemapRange( p1.y, -1.0f, 1.0f, 0.0f, float( height ) - 1.0f ) );
		float z0 = p0.z;
		float z1 = p1.z;
		bool steep = false;
		if ( std::abs( x0 - x1 ) < std::abs( y0 - y1 ) ) {
			std::swap( x0, y0 );
			std::swap( x1, y1 );
			steep = true;
		}
		if ( x0 > x1 ) {
			std::swap( x0, x1 );
			std::swap( y0, y1 );
			std::swap( z0, z1 );
		}
		int dx = x1 - x0;
		int dy = y1 - y0;
		int derror2 = std::abs( dy ) * 2;
		int error2 = 0;
		int y = y0;
		for ( int x = x0; x <= x1; x++ ) {
			// interpolated depth value
			float depth = RemapRange( float( x ), float( x0 ), float( x1 ), z0, z1 );
			if ( steep ) {
				if ( Depth.GetAtXY( y, x ).r >= depth ) {
					Color.SetAtXY( y, x, RGBAFromVec4( color ) );
					Depth.SetAtXY( y, x, { depth, 0.0f, 0.0f, 0.0f } );
				}
			} else {
				if ( Depth.GetAtXY( x, y ).r >= depth ) {
					Color.SetAtXY( x, y, RGBAFromVec4( color ) );
					Depth.SetAtXY( x, y, { depth, 0.0f, 0.0f, 0.0f } );
				}
			}
			error2 += derror2;
			if ( error2 > dx ) {
				y += ( y1 > y0 ? 1 : -1 );
				error2 -= dx * 2;
			}
		}
	}

	// draw triangle
	void DrawTriangle ( triangle t, const mat3 transform, const vec3 offset  ) {

		// apply transform
		t.p0 = transform * ( t.p0 + offset );
		t.p1 = transform * ( t.p1 + offset );
		t.p2 = transform * ( t.p2 + offset );

		// perspective projection parameters
		t.p0.x *= RemapRange( t.p0.z, -2.0f, 2.0f, 0.9f, 0.75f );
		t.p0.y *= RemapRange( t.p0.z, -2.0f, 2.0f, 0.9f, 0.75f );

		t.p1.x *= RemapRange( t.p1.z, -2.0f, 2.0f, 0.9f, 0.75f );
		t.p1.y *= RemapRange( t.p1.z, -2.0f, 2.0f, 0.9f, 0.75f );

		t.p2.x *= RemapRange( t.p2.z, -2.0f, 2.0f, 0.9f, 0.75f );
		t.p2.y *= RemapRange( t.p2.z, -2.0f, 2.0f, 0.9f, 0.75f );

		// translate x, y of points into screen space
		t.p0 = NDCToPixelCoords( t.p0 );
		t.p1 = NDCToPixelCoords( t.p1 );
		t.p2 = NDCToPixelCoords( t.p2 );

		// clipping for single triangle
		vec2 bboxmin(  std::numeric_limits< float >::max(),  std::numeric_limits< float >::max() );
		vec2 bboxmax( -std::numeric_limits< float >::max(), -std::numeric_limits< float >::max() );
		vec2 clamp( width - 1, height - 1 );

		for ( int j = 0; j < 2; j++ ) {
			bboxmin[ j ] = std::max( 0.0f, std::min( bboxmin[ j ], t.p0[ j ] ) );
			bboxmax[ j ] = std::min( clamp[ j ], std::max( bboxmax[ j ], t.p0[ j ] ) );

			bboxmin[ j ] = std::max( 0.0f, std::min( bboxmin[ j ], t.p1[ j ] ) );
			bboxmax[ j ] = std::min( clamp[ j ], std::max( bboxmax[ j ], t.p1[ j ] ) );

			bboxmin[ j ] = std::max( 0.0f, std::min( bboxmin[ j ], t.p2[ j ] ) );
			bboxmax[ j ] = std::min( clamp[ j ], std::max( bboxmax[ j ], t.p2[ j ] ) );
		}

		if ( verboseDraw ) {
			cout << "Drawing triangle, with:" << newline;
			cout << "[vertex 0]" << newline;
			cout << "  Position: " << t.p0.x << " " << t.p0.y << " " << t.p0.z << newline;
			cout << "  Normal:   " << t.n0.x << " " << t.n0.y << " " << t.n0.z << newline;
			cout << "  TexCoord: " << t.t0.x << " " << t.t0.y << " " << t.t0.z << newline;
			cout << "  Color:    " << t.c0.x << " " << t.c0.y << " " << t.c0.z << newline;
			cout << "[vertex 1]" << newline;
			cout << "  Position: " << t.p1.x << " " << t.p1.y << " " << t.p1.z << newline;
			cout << "  Normal:   " << t.n1.x << " " << t.n1.y << " " << t.n1.z << newline;
			cout << "  TexCoord: " << t.t1.x << " " << t.t1.y << " " << t.t1.z << newline;
			cout << "  Color:    " << t.c1.x << " " << t.c1.y << " " << t.c1.z << newline;
			cout << "[vertex 2]" << newline;
			cout << "  Position: " << t.p2.x << " " << t.p2.y << " " << t.p2.z << newline;
			cout << "  Normal:   " << t.n2.x << " " << t.n2.y << " " << t.n2.z << newline;
			cout << "  TexCoord: " << t.t2.x << " " << t.t2.y << " " << t.t2.z << newline;
			cout << "  Color:    " << t.c2.x << " " << t.c2.y << " " << t.c2.z << newline << newline;
		}

		constexpr bool allowPrimitiveJitter = false;
		ivec2 eval;
		for ( eval.x = bboxmin.x; eval.x <= bboxmax.x; eval.x++ ) {
			for ( eval.y = bboxmin.y; eval.y <= bboxmax.y; eval.y++ ) {

				// for( n ) jittered samples? tbd, will need to do something to get an alpha value from the n samples
				vec4 jitter = allowPrimitiveJitter ? BlueNoiseRef( eval ) : vec4( 0.0f );
				vec3 bc = BarycentricCoords( t.p0, t.p1, t.p2, vec3( float( eval.x ) + jitter.x, float( eval.y ) + jitter.y, 0.0f ) );

				// any barycentric coord being negative means degenerate triangle or sample point outside triangle
				if ( bc.x < 0 || bc.y < 0 || bc.z < 0 ) continue;

				// if ( // interesting experiment, reject samples with certain ranges of the barycentric coords
				// 	( std::fmod( bc.x, 0.5f ) > 0.1618 && std::fmod( bc.y, 0.5f ) > 0.1618 ) ||
				// 	( std::fmod( bc.x, 0.5f ) > 0.1618 && std::fmod( bc.z, 0.5f ) > 0.1618 ) ||
				// 	( std::fmod( bc.z, 0.5f ) > 0.1618 && std::fmod( bc.y, 0.5f ) > 0.1618 )
				// ) continue;

				float depth = 0.0f; // barycentric interpolation of depth
				depth += bc.x * t.p0.z;
				depth += bc.y * t.p1.z;
				depth += bc.z * t.p2.z;

				vec3 texCoord = vec3( 0.0f );
				texCoord += bc.x * vec3( t.t0.x, t.t0.y, 0.0f );
				texCoord += bc.y * vec3( t.t1.x, t.t1.y, 0.0f );
				texCoord += bc.z * vec3( t.t2.x, t.t2.y, 0.0f );
				texCoord.z = t.t0.z; // single material per tri

				vec3 normal = vec3( 0.0f );
				normal += bc.x * vec3( t.n0.x, t.n0.y, t.n0.z );
				normal += bc.y * vec3( t.n1.x, t.n1.y, t.n1.z );
				normal += bc.z * vec3( t.n2.x, t.n2.y, t.n2.z );

				if ( depth < 0.0f ) {
					return; // cheapo clipping plane
				}

				if ( Depth.GetAtXY( eval.x, eval.y ).r > depth ) { // compute the color to write, texturing, etc, etc

					vec4 texRef = TexRef( glm::mod( vec2( texCoord.x, 1.0f - texCoord.y ), vec2( 1.0f ) ), texCoord.z );
					if ( texRef.a == 0.0f ) {
						continue; // reject zero alpha samples - still need to implement blending
					}

					// vec4 color( texCoord.x, texCoord.y, texCoord.z / texSet.size(), 1.0f );
					vec4 color( texRef.x, texRef.y, texRef.z, 1.0f );

					Color.SetAtXY( eval.x, eval.y, RGBAFromVec4( color ) );
					Depth.SetAtXY( eval.x, eval.y, { depth, 0.0f, 0.0f, 0.0f } );
				}
			}
		}
	}


// the interface to TinyOBJLoader has changed significantly, and my wrapper is no longer really relevant at all - this will need to be rewritten to handle the new stuff
	// on the upside - materials become much, much easier to handle - this means that I will be able to more easily handle multi-texture models
		// for example, the sponza model I found here https://github.com/jimmiebergmann/Sponza

	void LoadModel ( string modelPath, string mtlSearchPath ) {
		tinyobj::ObjReaderConfig readerConfig;
		readerConfig.mtl_search_path = mtlSearchPath;

		tinyobj::ObjReader reader;

		Tick();

		// report any errors or warnings
		if ( !reader.ParseFromFile( modelPath, readerConfig ) ) {
			if ( !reader.Error().empty() ) {
				cout << "TinyOBJLoader: " << reader.Error() << newline;
			}
		}

		if ( !reader.Warning().empty() ) {
			cout << "TinyObjLoader: " << reader.Warning() << newline;
		}

		auto& attributes = reader.GetAttrib();
		auto& shapes = reader.GetShapes();
		auto& materials = reader.GetMaterials();

	// eventually I'll implement something for GLTF and have something higher quality to look at, with the
		// full complement of pbr textures ( intel sponza is a nice option, given sufficient VRAM )

		// iterating through the materials
		for ( size_t materialID = 0; materialID < materials.size(); materialID++ ) {

			string diffuseTexname = materials[ materialID ].diffuse_texname;
			string normalTexname = materials[ materialID ].displacement_texname;
			LoadTex( diffuseTexname.empty() ? string() : mtlSearchPath + diffuseTexname );
			LoadTex( normalTexname.empty() ? string() : mtlSearchPath + normalTexname );

			if ( verboseLoad ) {
				cout << "Material " << materialID << " is called " << materials[ materialID ].name << newline;
				cout << "  diffuse texture is: " << diffuseTexname << newline;
				// for some reason they use the displacement texture field
				cout << "  normal texture is: " << normalTexname << newline;
			}
		}


		// iterating through shapes in the file
		for ( size_t shapeID = 0; shapeID < shapes.size(); shapeID++ ) {

			// for indexing into the mesh's index array
			size_t indexOffset = 0;

			// iterating through faces in the mesh
			for ( size_t faceID = 0; faceID < shapes[ shapeID ].mesh.num_face_vertices.size(); faceID++ ) {

				triangle t; // current triangle to be drawn

				// per-face material ( texture select )
				size_t texID = shapes[ shapeID ].mesh.material_ids[ faceID ];

				// this should basically always be 3, with the triangulate flag set ( default setting )
				size_t numFaceVertices = size_t( shapes[ shapeID ].mesh.num_face_vertices[ faceID ] );

				// iterating through vertices in the face
				for ( size_t vertexID = 0; vertexID < numFaceVertices; vertexID++ ) {

				// we got triangles
					// access to vertex position data
					tinyobj::index_t idx = shapes[ shapeID ].mesh.indices[ indexOffset + vertexID ];
					tinyobj::real_t vx, vy, vz;
					vx = attributes.vertices[ 3 * size_t( idx.vertex_index ) + 0 ];
					vy = attributes.vertices[ 3 * size_t( idx.vertex_index ) + 1 ];
					vz = attributes.vertices[ 3 * size_t( idx.vertex_index ) + 2 ];

					tinyobj::real_t nx, ny, nz;
					if ( idx.normal_index >= 0 ) { // Check if `normal_index` is zero or positive. negative = no normal data
						nx = attributes.normals[ 3 * size_t( idx.normal_index ) + 0 ];
						ny = attributes.normals[ 3 * size_t( idx.normal_index ) + 1 ];
						nz = attributes.normals[ 3 * size_t( idx.normal_index ) + 2 ];
					}

					tinyobj::real_t tx, ty;
					if ( idx.texcoord_index >= 0 ) { // Check if `texcoord_index` is zero or positive. negative = no texcoord data
						tx = attributes.texcoords[ 2 * size_t( idx.texcoord_index ) + 0 ];
						ty = attributes.texcoords[ 2 * size_t( idx.texcoord_index ) + 1 ];
						// pack the material id in the third element
					}

					tinyobj::real_t red, green, blue;
					if ( idx.vertex_index >= 0 ) { // Check if `vertex_index` is zero or positive. negative = no vertex color data
						red   = attributes.colors[ 3 * size_t( idx.vertex_index ) + 0 ];
						green = attributes.colors[ 3 * size_t( idx.vertex_index ) + 1 ];
						blue  = attributes.colors[ 3 * size_t( idx.vertex_index ) + 2 ];
					}


					switch ( vertexID ) { // there's a better way to do this
						case 0:
							t.p0 = vec3( vx, vy, vz );
							t.n0 = vec3( nx, ny, nz );
							t.t0 = vec3( tx, ty, texID );
							t.c0 = vec3( red, green, blue );
							break;

						case 1:
							t.p1 = vec3( vx, vy, vz );
							t.n1 = vec3( nx, ny, nz );
							t.t1 = vec3( tx, ty, texID );
							t.c1 = vec3( red, green, blue );
							break;

						case 2:
							t.p2 = vec3( vx, vy, vz );
							t.n2 = vec3( nx, ny, nz );
							t.t2 = vec3( tx, ty, texID );
							t.c2 = vec3( red, green, blue );
							break;

						default: // should not hit this, because of triangulate flag
							cout << "vertex out of range" << newline;
							break;
					}
				}

				// increment index array indexing by ( what should always be 3 )
				indexOffset += numFaceVertices;

				// compute tangent, bitangent
				vec3 edge1 = t.p1 - t.p0;
				vec3 edge2 = t.p2 - t.p0;
				vec2 deltaUV1 = t.t1 - t.t0;
				vec2 deltaUV2 = t.t2 - t.t0;

				float f = 1.0f / ( deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y );
				t.t.x = f * ( deltaUV2.y * edge1.x - deltaUV1.y * edge2.x );
				t.t.y = f * ( deltaUV2.y * edge1.y - deltaUV1.y * edge2.y );
				t.t.z = f * ( deltaUV2.y * edge1.z - deltaUV1.y * edge2.z );
				t.b.x = f * ( -deltaUV2.x * edge1.x + deltaUV1.x * edge2.x );
				t.b.y = f * ( -deltaUV2.x * edge1.y + deltaUV1.x * edge2.y );
				t.b.z = f * ( -deltaUV2.x * edge1.z + deltaUV1.x * edge2.z );

				// do it
				triangles.push_back( t );
			}
		}

		if ( verboseLoad ) {
			cout << "loading took " << Tock() / 1000.0f << "ms" << newline;
		}
	}

	void DrawModel( const mat3 transform, const vec3 offset = vec3( 0.0f ) ) {
		Tick();
		for ( auto& t : triangles ) {
			DrawTriangle( t, transform, offset );
		}
		if ( verboseDraw ) {
			cout << "drawing took " << Tock() / 1000.0f << "ms" << newline;
		}
	}

	void DrawModelWireframe( const mat3 transform, const vec3 offset = vec3( 0.0f ) ) {
		Tick();
		for ( auto& t : triangles ) {
			DrawLine( ( transform * ( t.p0 + offset ) ), ( transform * ( t.p1 + offset ) ), vec4( t.n0, 1.0f ) );
			DrawLine( ( transform * ( t.p1 + offset ) ), ( transform * ( t.p2 + offset ) ), vec4( t.n1, 1.0f ) );
			DrawLine( ( transform * ( t.p2 + offset ) ), ( transform * ( t.p0 + offset ) ), vec4( t.n2, 1.0f ) );
		}
		if ( verboseDraw ) {
			cout << "drawing took " << Tock() / 1000.0f << "ms" << newline;
		}
	}

	std::vector<triangle> triangles;

	// dimensions
	uint32_t width = 0;
	uint32_t height = 0;

	// buffers
	Image Color;
	ImageF Depth;
	Image BlueNoise;
};

#endif
