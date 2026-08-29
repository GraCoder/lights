#version 460 compatibility

in vec4 vp_clr;

uniform sampler2D c0_tex; //opaque texture

void main()
{
  //vec4 c0 = texelFetch(c0_tex, ivec2(gl_FragCoord.xy), 0);
  //gl_FragColor = c0;
  vec4 c0 = vec4(0, 0, 0, 0);
  gl_FragColor = vec4(vp_clr.rgb * vp_clr.a - vp_clr.a * c0.rgb, 1.0);
}