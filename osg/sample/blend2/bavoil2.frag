#version 460 compatibility

in vec4 vp_clr;

uniform sampler2D c0_tex;
uniform sampler2D c1_tex;

void main()
{
  vec4 accum = texelFetch(c0_tex, ivec2(gl_FragCoord.xy), 0);
  float n = max(1.0, texelFetch(c1_tex, ivec2(gl_FragCoord.xy), 0).r);
  gl_FragColor = vec4(accum.rgb / max(accum.a, 0.0001), pow(max(0.0, 1.0 - accum.a / n), n));
}