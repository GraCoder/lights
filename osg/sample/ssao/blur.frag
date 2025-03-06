#version 330 compatibility

out float fragColor;

uniform sampler2D ssaoTex;

void main()
{
	vec2 uv = gl_TexCoord[0].st;
	vec2 texelSize = 1.0 / vec2(textureSize(ssaoTex, 0));
	float result = 0.0;
	for (int x = -2; x < 2; ++x) {
		for (int y = -2; y < 2; ++y) {
			vec2 offset = vec2(float(x), float(y)) * texelSize;
			result += texture(ssaoTex, uv + offset).r;
		}
	}
	fragColor = result / (4.0 * 4.0);
}