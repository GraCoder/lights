#version 330

uniform vec4 blend_factor;

out vec4 vp_clr;

out vec4 vp_pos;

void main()
{
  vp_clr = gl_Color;
  vec4 vert = gl_ModelViewProjectionMatrix * gl_Vertex;

  vp_pos.x = (vert.x / vert.w + 1) * 0.5 * blend_factor.x;
  vp_pos.y = (vert.y / vert.w + 1) * 0.5 * blend_factor.y;

  float sz = 128;
  vp_pos.w = sz / 2.0;

  gl_PointSize = sz;
  gl_Position = vert;
}