
#version 330 compatibility

uniform sampler2D clr_tex;
uniform sampler2D dep_tex;
uniform sampler2D blend_tex;
uniform sampler2D edge_tex;

uniform vec4 texture_size;

void main()
{
  vec2 uv = gl_TexCoord[0].xy;
  ivec2 st = ivec2(gl_FragCoord.xy);
  vec4 bl = texelFetch(blend_tex, st, 0);
  float br = texelFetch(blend_tex, st + ivec2(1, 0), 0).w;
  float bt = texelFetch(blend_tex, st + ivec2(0, -1), 0).y;
  //vec4 bl = texture(blend_tex, uv, 0);
  //float br = texture(blend_tex, uv + vec2(texture_size.x, 0)).w;
  //float bt = texture(blend_tex, uv + vec2(0, texture_size.y)).y;
  vec4 a = vec4(bl.x, bt, bl.z, br);
  vec4 w = a * a * a;
  float sum = w.x + w.y + w.z + w.w;
  if (sum > 1e-5) {
    vec4 o = a * texture_size.yyxx;
    vec4 clr = vec4(0);
    clr = texture(clr_tex, uv + vec2(0, o.x)) * w.x + clr;
    clr = texture(clr_tex, uv + vec2(0, -o.y)) * w.y + clr;
    clr = texture(clr_tex, uv + vec2(-o.z, 0)) * w.z + clr;
    clr = texture(clr_tex, uv + vec2(o.w, 0)) * w.w + clr;
    gl_FragColor = clr / sum;
    //gl_FragColor.w = 1;
    //gl_FragColor = vec4(0, sum, 0, 1);
    //gl_FragColor = vec4(0, 1, 0, 1);
  }
  else
    gl_FragColor = texture(clr_tex, uv);

  //gl_FragColor = texelFetch(edge_tex, st, 0);
  //gl_FragColor = abs(texelFetch(blend_tex, st, 0));
  //gl_FragColor.w = 1;
}