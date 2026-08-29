
#version 430 compatibility

in vec4 clr;

layout(location = 0) out vec4 color;
layout(location = 1) out vec2 velocity;

struct VelocityStep{
	vec4 pre_pos;
	vec4 cur_pos;
};

in VelocityStep vel_out;


void main()
{
	vec2 uv = gl_TexCoord[0].xy;
	//float r = texture(dep_tex, uv).r;
	//gl_FragColor = vec4(vec3(r), 1);
	color = clr;
	vec2 cur_pos = (vel_out.cur_pos.xy / vel_out.cur_pos.w) * 0.5;
	vec2 pre_pos = (vel_out.pre_pos.xy / vel_out.pre_pos.w) * 0.5;
	//velocity = cur_pos;
	velocity = cur_pos - pre_pos;
}
