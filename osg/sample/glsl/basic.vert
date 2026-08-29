#version 330

out vec4 vp_clr;

void main()
{
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  vp_clr = gl_Color;
}