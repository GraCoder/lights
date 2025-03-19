#version 460 compatibility

// uniform vec4 blend_factor;

out vec4 vp_clr;

out float vp_Z;

void main()
{
  vp_clr = gl_Color;
  vec4 vert = gl_ModelViewProjectionMatrix * gl_Vertex;

  vec4 vv = gl_ModelViewMatrix * gl_Vertex;

  vp_Z = abs(vv.z);
  gl_Position = vert;
}