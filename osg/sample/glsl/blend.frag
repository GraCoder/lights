#version 330 core

in vec4 vp_clr;
in vec4 vp_pos;

void main()
{
  vec2 uv = gl_FragCoord.xy;
  float d = length(uv - vp_pos.xy);
  if(d > vp_pos.w)
    discard;

  float w = smoothstep(vp_pos.w, 0, d);

  gl_FragColor = vp_clr;
  gl_FragColor.w = w;
  //gl_FragColor.w = w * 0.5 + 0.4;
  //gl_FragColor.w = 1;
  //gl_FragColor = vec4(1, 1, 1, 1);
}