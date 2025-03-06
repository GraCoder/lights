
#version 330 compatibility

uniform sampler2D clr_tex;
uniform sampler2D dep_tex;
uniform sampler2D vel_tex;

void main()
{
	vec2 uv = gl_TexCoord[0].xy;
	//float r = texture(dep_tex, uv).r;
	//gl_FragColor = vec4(vec3(r), 1);
	gl_FragColor = vec4(texture(clr_tex, uv).rgb, 1);

  //vec2 vel = texture(vel_tex, uv).rg;
	//gl_FragColor = vec4(abs(vel) * 10, 0, 1);
}