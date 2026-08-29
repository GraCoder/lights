#version 430 core

out vec4 vp_clr;
out vec4 vp_pos;

void main()
{
  vp_clr = gl_Color;
  vec4 vert = gl_ModelViewProjectionMatrix * gl_Vertex;
  gl_Position = vert;

  gl_PointSize = 3;
}