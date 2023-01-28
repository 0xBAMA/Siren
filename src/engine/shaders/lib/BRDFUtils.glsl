vec3 ggx_S(vec3 d, float a){
	float r1 = normalizedRandomFloat();
	float r2 = normalizedRandomFloat();

	float phi = r1*3.14159*2.;
	float theta = atan(a*sqrt(r2/(1.0-r2)));

	float x = cos(phi)*sin(theta);
	float y = sin(phi)*sin(theta);
	float z = cos(theta);

	vec3 N = d;
	vec3 W = (abs(N.x) > 0.99)?vec3(0.,1.,0.):vec3(1.,0.,0.);
	vec3 T = normalize(cross(N,W));
	vec3 B = normalize(cross(N,T));

	return normalize(T*x + B*y + z*N);
}

float ggx_D(float cost, float a){
	float as = a*a;
	float of = 3.14159*pow((a*a-1.)*cost*cost + 1.0,2.);
	return as/of;
}

float ggx_pdf(float cost, float a){
	float as = a*a*cost;
	float of = 3.14159*pow((a*a-1.)*cost*cost + 1.0,2.);
	return as/of;
}

float cookTorranceG(vec3 n, vec3 h, vec3 v, vec3 l){
	return min(1., min((2.*max(dot(n,h),0.)*max(dot(n,v),0.))/max(dot(v,h),0.001),
	(2.*max(dot(n,h),0.)*max(dot(n,l),0.))/max(dot(v,h),0.001)));
}

vec3 Schlick(vec3 F0, float cost){
	return F0 + (1.0-F0)*pow(1.0-cost,5.);
}
