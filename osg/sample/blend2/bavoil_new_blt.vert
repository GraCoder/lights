#version 460 compatibility

out vec4 vp_clr;

void main()
{
  vp_clr = gl_Color;
  gl_Position = gl_Vertex;
}