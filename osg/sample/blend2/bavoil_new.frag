#version 460 compatibility

layout(location = 0) out vec4 accc;
layout(location = 1) out vec4 acca;

in vec4 vp_clr;
in float vp_Z;

void main()
{
  float a = vp_clr.a;
  float w = a * max(0.01, min(3000, 0.03 / (1e-5 + pow(vp_Z / 200.0, 4)))); 

  accc.rgb = vp_clr.rgb * a;
  accc.a = a;
  accc *= w;
  acca = vec4(a);
}