#version 330 compatibility

in vec2 vertOut;

uniform float time;
uniform ivec2 screenSize;

uniform int cate;
uniform float repNum;
uniform vec2 offset;

float simplex3d(vec3 m);
float simplex3d_fractal(vec3 m);
float simplex3d_fractal_rot(vec3 m);
float simplex3d_fractal_abs(vec3 m);
float simplex3d_fractal_abs_rot(vec3 m);
float simplex3d_fractal_abs_sin(vec3 m);
float simplex3d_fractal_abs_sin_rot(vec3 m);

void main()
{
	vec2 p = gl_FragCoord.xy / screenSize;
	vec3 p3 = vec3(vec3(p, time * 0.025));

	float value;

	if (cate == 0) {
		value = simplex3d(p3 * repNum + vec3(offset, 0));
		value = 0.5 + 0.5 * value;
	} else if (cate == 1) {
		value = simplex3d_fractal(p3 * repNum + vec3(offset, 0));
		value = 0.5 + 0.5 * value;
	} else if (cate == 2) {
		value = simplex3d_fractal_rot(p3 * repNum + vec3(offset, 0));
		value = 0.5 + 0.5 * value;
	} else if (cate == 3)
		value = simplex3d_fractal_abs(p3 * repNum + vec3(offset, 0));
	else if (cate == 4) 
		value = simplex3d_fractal_abs_rot(p3 * repNum + vec3(offset, 0));
	else if (cate == 5) {
		value = simplex3d_fractal_abs_sin(p3 * repNum + vec3(offset, 0));
		value = 0.5 + 0.5 * value;
	} else if (cate == 6) {
		value = simplex3d_fractal_abs_sin_rot(p3 * repNum + vec3(offset, 0));
		value = 0.5 + 0.5 * value;
	}

	//value *= smoothstep(0.0, 0.005, abs(0.6 - p.x)); 

	gl_FragColor = vec4(vec3(value), 1.0);
}
