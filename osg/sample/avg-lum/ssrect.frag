#version 430 core

uniform sampler2D clr_tex;

layout(location = 0) out vec4 outColor;

void main()
{
  ivec2 uv = ivec2(gl_FragCoord.xy);
  vec4 clr = texelFetch(clr_tex, uv, 0);

  outColor = clr;
}