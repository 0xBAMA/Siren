#version 430 core
layout( local_size_x = 32, local_size_y = 32, local_size_z = 1 ) in;

// render texture, this is written to by this shader
layout( binding = 0, rgba8ui ) uniform uimage2D current;
layout( binding = 1, rgba32f ) uniform image2D accum;

layout( binding = 3 ) uniform sampler2D blue_noise_dither_pattern;

#define M_PI 3.1415926535897932384626433832795

#define MAX_STEPS 300
#define MAX_DIST  5.
#define EPSILON   0.00012 // closest surface distance

#define MAX_BOUNCES 10

#define AA 2

uniform int frame;         // used to cycle the blue noise values over time

// uniform uint wang_seed;
uint seed = 0;
// from demofox
// https://blog.demofox.org/2020/05/25/casual-shadertoy-path-tracing-1-basic-camera-diffuse-emissive/
uint wang_hash(){
	seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
	seed *= uint(9);
	seed = seed ^ (seed >> 4);
	seed *= uint(0x27d4eb2d);
	seed = seed ^ (seed >> 15);
	return seed;
}

float RandomFloat01(){
    return float(wang_hash()) / 4294967296.0;
}



uniform vec3 basic_diffuse;
uniform vec3 sky_color;

uniform int tonemap_mode;
uniform float gamma;
uniform float exposure;

uniform vec3 lightPos1;
uniform vec3 lightPos2;
uniform vec3 lightPos3;


// lens controls
uniform float lens_scale_factor;
uniform float lens_radius_1;
uniform float lens_radius_2;
uniform float lens_thickness;
uniform float lens_rotate;

uniform float lens_ir;

uniform float jitterfactor;
uniform float focusdistance;

uniform float fov;

uniform vec3 ray_origin;
uniform float time;

uniform float depth_scale;
uniform int depth_falloff;

// because this is going to have to be tile-based, we need this local offset
uniform int x_offset;
uniform int y_offset;

ivec2 global_loc = ivec2(gl_GlobalInvocationID.xy) + ivec2(x_offset, y_offset);
ivec2 dimensions = ivec2(imageSize(accum));
vec2 fdimensions = vec2(dimensions);
vec2 pixcoord = (vec2(global_loc.xy)-(fdimensions/2.)) / (fdimensions/2.);

float escape = 0.;

vec3 pal( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d ) {
	return a + b*cos( 6.28318*(c*t+d) );
}

vec3 albedo;
vec3 current_emission;

bool refractive_hit = false;      // this triggers the material behavior for refraction
bool entering_refractive = false; // initially not located inside of refractive shapes - toggled on hit with lens_de

float lens_de(vec3 p){
	// scaling
	p *= lens_scale_factor;
	float dfinal;

	float radius1 = lens_radius_1;
	float radius2 = lens_radius_2;

	float thickness = lens_thickness;

	float center1 = radius1 - thickness/2.;
	float center2 = - radius2 + thickness/2;

	vec3 prot = rotate3D(0.1*lens_rotate, vec3(1.))*p;

	float sphere1 = distance(prot, vec3(0,center1,0)) - radius1;
	float sphere2 = distance(prot, vec3(0,center2,0)) - radius2;

	dfinal = fOpIntersectionRound(sphere1, sphere2, 0.03);
	// dfinal = fOpIntersectionChamfer(sphere1, sphere2, 0.1);
	// dfinal = max(sphere1, sphere2);
	// seed = 6942069;

	pModInterval1(p.x, 0.1, -10., 10.);
	pModInterval1(p.y, 0.25, -10., 10.);
	pModInterval1(p.z, 0.1, -10., 10.);

	if(dfinal < 0)
		for(int i = 0; i < 15; i++)
			dfinal = max(dfinal, -distance(vec3(RandomFloat01(), RandomFloat01(), RandomFloat01())*0.01, p) + 0.002*RandomFloat01());

	return dfinal/lens_scale_factor;
}


float de(vec3 p){

	vec3 porig = p;
	current_emission = vec3(0.);
	albedo = basic_diffuse;

	refractive_hit = false;

	float dlight1 = fPlane(p, vec3(0,1,0), 1.1);
	float dlight2 = fPlane(p, vec3(0,-1,0), 1.1);
	float dlight = min(dlight1, dlight2);

	float width = 0.5;
	vec3 boxsize = vec3(1.618*width, 2.*width/9., width);
	vec3 offset = vec3(0.032);

	float lens_distance = (entering_refractive ? -1. : 1.) * lens_de(p); // if inside, consider the negative
	float dfinal = lens_distance;

	float dfract = deee(rotate3D(1.,vec3(1.))*porig);
	dfinal = min(dfinal, dfract);
	dfinal = min(dfinal, dlight);

	if(dfinal == dfract) {
		albedo = vec3(0.6,0.3,0.1);
	}

	if(dfinal == lens_distance) {
		albedo = vec3(1.,0.99,0.97);
		if(dfinal < EPSILON){
			refractive_hit = true;
			entering_refractive = !entering_refractive;
		}
	}

	if(dfinal == dlight1)
	{
		current_emission = vec3(0.2, 0.5, 0.4)*5;
		albedo = vec3(1.);
	}

	if(dfinal == dlight2)
	{
		current_emission = vec3(0.1, 0.1, 1.)*5;
		albedo = vec3(1.);
	}
	return dfinal;
}



//  ╦═╗┌─┐┌┐┌┌┬┐┌─┐┬─┐┬┌┐┌┌─┐  ╔═╗┌─┐┌┬┐┌─┐
//  ╠╦╝├┤ │││ ││├┤ ├┬┘│││││ ┬  ║  │ │ ││├┤
//  ╩╚═└─┘┘└┘─┴┘└─┘┴└─┴┘└┘└─┘  ╚═╝└─┘─┴┘└─┘
// global state tracking
uint num_steps = 0; // how many steps taken by the raymarch function
float dmin = 1e10; // minimum distance initially large

float side = 1.; // +1 is outside, -1 is inside object - used for refraction

// raymarches to the next hit
float raymarch(vec3 ro, vec3 rd) {
	float d0 = 0.0, d1 = 0.0;
	for(int i = 0; i < MAX_STEPS; i++) {
		vec3 p = ro + rd * d0;      // point for distance query from parametric form
		d1 = side*de(p); d0 += d1;  // increment distance by de evaluated at p
		dmin = min( dmin, d1);      // tracking minimum distance
		num_steps++;                // increment step count
		if(d0 > MAX_DIST || abs(d1) < EPSILON || i == (MAX_STEPS-1)) return d0; // return the final ray distance
	}
}

vec3 norm(vec3 p) { // to get the normal vector for a point in space, this function evaluates the gradient of the distance function
#define METHOD 0
#if METHOD == 0
	// tetrahedron version, unknown source - 4 evaluations
	vec2 e = vec2(1,-1) * EPSILON;
	return normalize(e.xyy*de(p+e.xyy)+e.yyx*de(p+e.yyx)+e.yxy*de(p+e.yxy)+e.xxx*de(p+e.xxx));
#endif
}

vec3 lens_norm(vec3 p){
#if METHOD == 0
	// tetrahedron version, unknown source - 4 evaluations
	vec2 e = vec2(1,-1) * EPSILON;
	return normalize(e.xyy*lens_de(p+e.xyy)+e.yyx*lens_de(p+e.yyx)+e.yxy*lens_de(p+e.yxy)+e.xxx*lens_de(p+e.xxx));
#endif
}

void lens_normal_adjust(inout vec3 p){
    p += (entering_refractive ? -2. : 2. ) * lens_norm(p) * EPSILON;
}


// hash function

// http://www.jcgt.org/published/0009/03/02/
uvec4 pcg4d(vec2 s) {
	uvec4 v = uvec4(s, uint(s.x) ^ uint(s.y), uint(s.x) + uint(s.y));

	v = v * 1664525u + 1013904223u;

	v.x += v.y*v.w;
	v.y += v.z*v.x;
	v.z += v.x*v.y;
	v.w += v.y*v.z;

	v ^= v >> 16u;

	v.x += v.y*v.w;
	v.y += v.z*v.x;
	v.z += v.x*v.y;
	v.w += v.y*v.z;

	return v;
}


vec3 RandomUnitVector(){
	float z = RandomFloat01() * 2.0f - 1.0f;
	float a = RandomFloat01() * 2. * M_PI;
	float r = sqrt(1.0f - z * z);
	float x = r * cos(a);
	float y = r * sin(a);
	return vec3(x, y, z);
}

vec3 RandomInUnitDisk(){
	return vec3(RandomUnitVector().xy, 0.);
}

float reflectance(float cosine, float ref_idx) {
	// Use Schlick's approximation for reflectance.
	float r0 = (1-ref_idx) / (1+ref_idx);
	r0 = r0*r0;
	return r0 + (1-r0)*pow((1 - cosine),5);
}

vec3 get_color_for_ray(vec3 ro_in, vec3 rd_in){
	escape = 0.;
	vec3 ro = ro_in, rd = rd_in;

	// float dresult = raymarch(ro, rd);
	// float escape_result = escape;

	// so first hit location is known in hitpos, and surface normal at that point in the normal

	vec3 final_color = vec3(0.);
	vec3 throughput = vec3(1.);
	for(int i = 0; i < MAX_BOUNCES; i++){
		escape = 0.;
		float dresult = raymarch(ro, rd);
		float escape_result = escape;

		// cache old ro, compute new ray origin
		vec3 old_ro = ro;
		ro = ro+dresult*rd;

		if(refractive_hit) {
			// vec3 normal = norm(ro);
			vec3 normal = lens_norm(ro) * (entering_refractive ? 1 : -1);
			vec3 unit_direction = normalize(ro-old_ro);

			lens_normal_adjust(ro); // bump away from surface hit
			float refraction_ratio = entering_refractive ? 1./lens_ir : lens_ir; // entering or leaving

			float cos_theta = min(dot(-unit_direction, normal), 1.0);
			float sin_theta = sqrt(1.0 - cos_theta*cos_theta);

			// accounting for TIR effects
			bool cannot_refract = refraction_ratio * sin_theta > 1.0;

			if (cannot_refract || reflectance(cos_theta, refraction_ratio) > RandomFloat01())
				rd = reflect(unit_direction, normal);
			else
				rd = refract(unit_direction, normal, refraction_ratio);

		} else { // other material behaviors

			// get normal and bump off surface to avoid self intersection
			vec3 normal = norm(ro);
			ro += 2. * EPSILON * normal;

			vec3 reflected = reflect(ro-old_ro, normal);
			vec3 temp = mix(reflected, RandomUnitVector(), 0.1);
			vec3 randomvector_specular = normalize((1.+EPSILON)*normal + temp);
			vec3 randomvector_diffuse  = normalize((1.+EPSILON)*normal + RandomUnitVector());

			rd = mix(randomvector_diffuse, randomvector_specular, 0.7);

			final_color += throughput*current_emission;
			throughput *= albedo;

			// russian roulette - chance to quit early
			float p = max(throughput.r, max(throughput.g, throughput.b));
			if(RandomFloat01() > p) break;
			// add to compensate for energy lost by randomly terminating paths
			throughput *= 1. / p;

		}
	}
	return final_color;
}

void main () {
	seed = uint(uint(global_loc.x) * uint(1973) + uint(global_loc.y) * uint(9277) + uint(time+frame) * uint(26699)) | uint(1);;

	// check image bounds - on pass, begin checking the ray against the scene representation
	if(global_loc.x < dimensions.x && global_loc.y < dimensions.y){
		vec4 col = vec4(0, 0, 0, 1);
		float dresult_avg = 0.;

		for(int x = 0; x < AA; x++)
		for(int y = 0; y < AA; y++) {

			vec2 offset = vec2(float(x+RandomFloat01()), float(y+RandomFloat01())) / float(AA) - 0.5;

			vec2 pixcoord = (vec2(global_loc.xy + offset)-vec2(imageSize(current)/2.)) / vec2(imageSize(current)/2.);
			vec3 ro = ray_origin;

			// ray gen
			float aspect_ratio = float(dimensions.x) / float(dimensions.y);
			vec3 rd = normalize(aspect_ratio*pixcoord.x*basis_x + pixcoord.y*basis_y + (1./fov)*basis_z);

			// DoF adjust - more correct version - hard to control
			vec3 focuspoint = ro+((rd*focusdistance) / dot(rd, basis_z));
			vec2 disk = RandomInUnitDisk().xy;
			ro += disk.x*jitterfactor*basis_x + disk.y*jitterfactor*basis_y + jitterfactor*RandomFloat01()*basis_z;
			rd = normalize(focuspoint - ro);

			// original method
			// vec3 focuspoint = ro+((rd*focusdistance) / dot(rd, basis_z));
			// ro += jitterfactor*RandomFloat01()*basis_z;
			// rd = normalize(focuspoint - ro);

			dresult_avg += raymarch(ro, rd);


			// color the ray
			col.rgb += get_color_for_ray(ro, rd);

		}
		col.rgb /= float(AA*AA);

		// gamma correction
		col.rgb = pow(col.rgb*(0.1*get_cycled_rgb_blue()+1.0), vec3(1/gamma));
		vec4 read = imageLoad(accum, ivec2(global_loc.xy));
		imageStore(accum, ivec2(global_loc.xy), vec4(mix(read.rgb, col.rgb, 1./(read.a+1.)), read.a+1.));
	}
}
