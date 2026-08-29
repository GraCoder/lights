#version 430 core

in vec4 vp_clr;
in vec4 vp_pos;

layout(location = 0) out vec4 outColor;

void main()
{
  outColor = vp_clr;
}