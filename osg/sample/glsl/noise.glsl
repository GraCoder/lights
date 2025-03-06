#version 330


//simplex3d_fractal
//simplex3d_fractal_rot
//simplex3d_fractal_abs
//simplex3d_fractal_abs_rot
//simplex3d_fractal_abs_sin
//simplex3d_fractal_abs_sin


vec3 hash32(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973));
	p3 += dot(p3, p3.yxz + 33.33);
	return fract((p3.xxy + p3.yzz) * p3.zyx);
}

vec3 hash33(vec3 p)
{
	p = fract(p * vec3(.1031, .11369, .13787));
	p += dot(p, p.yxz + 19.19);
	return fract((p.xxy + p.yxx) * p.zyx);

}

vec3 random3(vec3 p)
{
#if defined HASH33
	return hash33(p) - 0.5;
#else
	float j = 4096.0 *
		sin(dot(p, vec3(17.0, 59.4, 15.0)));
	vec3 r; r.z = fract(512.0 * j); j *= .125;
	r.x = fract(512.0 * j); j *= .125;
	r.y = fract(512.0 * j);
	return r - 0.5;
#endif
}

const float F3 = 0.3333333;
const float G3 = 0.1666667;

float simplex3d(vec3 v)
{
	vec3 s = floor(v + dot(v, vec3(F3)));
	vec3 x = v - s + dot(s, vec3(G3));

	vec3 e = step(vec3(0.0), x - x.yzx);
	vec3 i1 = e * (1.0 - e.zxy);
	vec3 i2 = 1.0 - e.zxy * (1.0 - e);

	vec3 x1 = x - i1 + G3;
	vec3 x2 = x - i2 + 2.0 * G3;
	vec3 x3 = x - 1.0 + 3.0 * G3;

	vec4 w, d;

	w.x = dot(x, x);
	w.y = dot(x1, x1);
	w.z = dot(x2, x2);
	w.w = dot(x3, x3);

	w = max(0.6 - w, 0.0);

	d.x = dot(random3(s), x);
	d.y = dot(random3(s + i1), x1);
	d.z = dot(random3(s + i2), x2);
	d.w = dot(random3(s + 1.0), x3);

	w *= w;
	w *= w;
	d *= w;

	return dot(d, vec4(52.0));
}

//out[1-0]
float worley2d_circle(in vec2 v)
{
	vec2 p = floor(v);
	vec2 f = fract(v);

	float ret = 0;
	for (int j = -2; j <= 2; j++)
		for (int i = -2; i <= 2; i++) {
			vec2  g = vec2(float(i), float(j));
			vec3  o = hash32(p + g);
			vec2  r = g - f + o.xy;
			float d = dot(r, r);
			float w = 1.0 - smoothstep(0.0, 0.707, sqrt(d));
			ret = max(ret, w);
		}

	return ret;
}

float worley3d_circle(in vec3 v)
{
	vec3 p = floor(v);
	vec3 f = fract(v);

	float ret = 0;
	for (int j = -2; j <= 2; j++)
		for (int i = -2; i <= 2; i++) {
			for (int k = -2; k <= 2; k++) {
				vec3  g = vec3(float(i), float(j), float(k));
				vec3  o = random3(p + g) * 2;
				vec3  r = g - f + o;
				float d = dot(r, r);
				float w = 1.0 - smoothstep(0.0, 0.866, sqrt(d));
				ret = max(w, ret);
			}
		}

	return ret;
}


float worley2d_poly(in vec2 v)
{
	vec2 p = floor(v);
	vec2 f = fract(v);

	float ret = 0;
	for (int j = -2; j <= 2; j++)
		for (int i = -2; i <= 2; i++) {
			vec2  g = vec2(float(i), float(j));
			vec3  o = hash32(p + g);
			vec2  r = g - f + o.xy;
			float d = dot(r, r);
			float w = 1.0 - smoothstep(0.0, 1.404, sqrt(d));
			ret = max(ret, w);
		}

	return ret;
}

float worley2d_test(vec2 x, float u, float v, float radius)
{
	vec2 p = floor(x);
	vec2 f = fract(x);

	float k = 1.0 + 63.0 * pow(1.0 - v, 4.0);
	float va = 0.0;
	float wt = 0.0;
	for (int j = -2; j <= 2; j++)
		for (int i = -2; i <= 2; i++) {
			vec2  g = vec2(float(i), float(j));
			vec3  o = hash32(p + g) * vec3(u, u, 1.0);
			vec2  r = g - f + o.xy;
			float d = dot(r, r);
			float w = pow(1.0 - smoothstep(0.0, radius * 1.414, sqrt(d)), k);
			va += w * o.z;
			wt += w;
		}

	return va / wt;
}

const mat3 rot1 = mat3(-0.37, 0.36, 0.85, -0.14, -0.93, 0.34, 0.92, 0.01, 0.4);
const mat3 rot2 = mat3(-0.55, -0.39, 0.74, 0.33, -0.91, -0.24, 0.77, 0.12, 0.63);
const mat3 rot3 = mat3(-0.71, 0.52, -0.47, -0.08, -0.72, -0.68, -0.7, -0.45, 0.56);

float simplex3d_fractal(vec3 v)
{
	return 0.5333333 * simplex3d(v)
		+ 0.2666667 * simplex3d(2.0 * v)
		+ 0.1333333 * simplex3d(4.0 * v)
		+ 0.0666667 * simplex3d(8.0 * v);
}

float simplex3d_fractal_rot(vec3 v)
{
	return 0.5333333 * simplex3d(v * rot1)
		+ 0.2666667 * simplex3d(2.0 * v * rot2)
		+ 0.1333333 * simplex3d(4.0 * v * rot3)
		+ 0.0666667 * simplex3d(8.0 * v);
}

float simplex3d_fractal_abs(vec3 v)
{
	return 0.5333333 * abs(simplex3d(v))
		+ 0.2666667 * abs(simplex3d(2.0 * v))
		+ 0.1333333 * abs(simplex3d(4.0 * v))
		+ 0.0666667 * abs(simplex3d(8.0 * v));
}

float simplex3d_fractal_abs_rot(vec3 v)
{
	return 0.5333333 * abs(simplex3d(v * rot1))
		+ 0.2666667 * abs(simplex3d(2.0 * v * rot2))
		+ 0.1333333 * abs(simplex3d(4.0 * v * rot3))
		+ 0.0666667 * abs(simplex3d(8.0 * v));
}

float simplex3d_fractal_abs_sin(vec3 v)
{
	float ret = simplex3d_fractal_abs(v);
	return sin(ret * 3.14159 + v.x * 0.5);
}

float simplex3d_fractal_abs_sin_rot(vec3 v)
{
	float ret = simplex3d_fractal_abs_rot(v);
	return sin(ret * 3.14159 + v.x * 0.5);
}

float worley2d_circle_fractal(vec2 x)
{
	return 0.65 * worley2d_circle(x)
		+ 0.25 * worley2d_circle(2.0 * x)
		+ 0.1 * worley2d_circle(4.0 * x);
}

float worley3d_circle_fractal(vec3 x)
{
	return 0.65 * worley3d_circle(x)
		+ 0.25 * worley3d_circle(2.0 * x)
		+ 0.1 * worley3d_circle(4.0 * x);
}

