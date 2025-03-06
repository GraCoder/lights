#version 330

out vec2 vertOut;


void main()
{
	vec4 pos = gl_Vertex;
	gl_Position = pos;
	vertOut = pos.xy / pos.w;
}
