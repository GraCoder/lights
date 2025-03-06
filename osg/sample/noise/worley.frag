#version 330

in vec2 vertOut;

uniform float time;
uniform ivec2 screenSize;

uniform int cate;
uniform float repNum;
uniform vec3 uvw;

float worley2d_test(vec2, float, float, float);
float worley2d_circle(vec2);
float worley2d_poly(vec2);
float worley2d_circle_fractal(vec2);
float worley3d_circle(vec3);
float worley3d_circle_fractal(vec3);

void main()
{
	vec2 p = gl_FragCoord.xy / screenSize.y;
	vec3 p3 = vec3(vec3(p, time * 0.025));

	float value;

	if (cate == 0) {
		value = worley2d_test(p * vec2(repNum), uvw.x * cos(time), uvw.y, uvw.z);
	} else if (cate == 1 ){
		value = worley2d_circle(p * vec2(repNum));
	} else if (cate == 2) {
		value = worley2d_poly(p * vec2(repNum));
	}
	else if (cate == 3) {
		value = worley2d_circle_fractal(p * vec2(repNum));
	}
	else if (cate == 4) {
		value = worley3d_circle(vec3(p * vec2(repNum), 0));
	}
	else if (cate == 5) {
		value = worley3d_circle_fractal(vec3(p * vec2(repNum), 0));
	}

	//value *= smoothstep(0.0, 0.005, abs(0.6 - p.x)); 

	gl_FragColor = vec4(vec3(value), 1.0);
}
