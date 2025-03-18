#version 460 compatibility

layout(location = 0) out vec4 accc;
layout(location = 1) out float acca;

in vec4 vp_clr;

void main()
{
  accc.rgb = vp_clr.rgb * vp_clr.a;
  accc.a = vp_clr.a;
  acca = 1;
}