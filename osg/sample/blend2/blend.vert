#version 460 compatibility

//uniform vec4 blend_factor;

out vec4 vp_clr;

void main()
{
  vp_clr = gl_Color;
  vec4 vert = gl_ModelViewProjectionMatrix * gl_Vertex;

  gl_Position = vert;
}