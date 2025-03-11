#version 460 compatibility

in vec4 vp_clr;

void main()
{
  gl_FragColor = vec4(vp_clr.rgb, 1);
  //gl_FragColor = vec4(vp_clr.rgb, 1);
}