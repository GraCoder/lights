#version 330 compatibility

in vec4 clr;

//layout(location = 0) out vec4 color;
//layout(location = 1) out vec2 edge;

uniform vec4 texture_size;
uniform sampler2D clr_tex;

const float threshold = 0.05f;

void main()
{
	vec2 uv = gl_TexCoord[0].xy;
	vec2 des = texture_size.xy;
	float c = texture(clr_tex, uv).w;	
	float l = abs(texture(clr_tex, uv - vec2(des.x, 0)).w - c);
	float l2 = abs(texture(clr_tex, uv - vec2(des.x * 2, 0)).w - c);
	float r = abs(texture(clr_tex, uv + vec2(des.x, 0)).w - c);

	float t = abs(texture(clr_tex, uv + vec2(0, des.y)).w - c);
	float t2 = abs(texture(clr_tex, uv + vec2(0, des.y * 2)).w - c);
	float b = abs(texture(clr_tex, uv - vec2(0, des.y)).w - c);

	float cmax = max(max(l, r), max(b, t));
	bool el = l > threshold;
	el = el && l > (max(cmax, l2) * 0.5);

	bool et = t > threshold;
	et = et && t > (max(cmax, t2) * 0.5);
	gl_FragColor = vec4(el ? 1: 0, et ? 1: 0, 0, 1);
}

