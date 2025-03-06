#version 450 compatibility

#pragma import_defines(DIAG_DETECTION)

uniform vec4 texture_size;

uniform sampler2D edge_tex;
uniform sampler2D area_tex;

#define SMAA_SEARCH_STEP 16
#define SMAA_SEARCH_DIAG_STEP 8 
#define SMAA_ROUNDING_FACTOR 0.25
#define SMAA_AREATEX_MAX_DISTANCE 16
#define SMAA_AREATEX_DIAG_MAX_DISTANCE 20
#define SMAA_AREATEX_PIXEL_SIZE (1.0 / vec2(160, 560))
#define SMAA_AREATEX_SUBTEX_SIZE (1.0 / 7.0)
#define SMAA_CORNER_ROUNDING 25
#define SMAA_CORNER_ROUNDING_NORM ((SMAA_CORNER_ROUNDING) / 100.0)

//#define CALCULATE_FACTOR 1
#define SMAA_DEBUG 1

float smaa_search_left(vec2 uv)
{
  int i = 0;
  float e = 0;
  uv -= vec2(1.5 * texture_size.x, 0);
  for(i = 0; i < SMAA_SEARCH_STEP; i++)
  {
    e = texture(edge_tex, uv).g; 
    if(e < 0.9) break;
    uv -= vec2(texture_size.x * 2, 0);
  }
  return min(2 * (i + e), 2 * SMAA_SEARCH_STEP);
}

float smaa_search_right(vec2 uv)
{
  int i = 0;
  float e = 0;
  uv += vec2(1.5 * texture_size.x, 0);
  for(i = 0; i < SMAA_SEARCH_STEP; i++)
  {
    e = texture(edge_tex, uv).g; 
    if(e < 0.9) break;
    uv += vec2(texture_size.x * 2, 0);
  }
  return min(2 * (i + e), 2 * SMAA_SEARCH_STEP);
}

float smaa_search_up(vec2 uv)
{
  int i = 0;
  float e = 0;
  uv += vec2(0, 1.5 * texture_size.y);
  for(i = 0; i < SMAA_SEARCH_STEP; i++)
  {
    e = texture(edge_tex, uv).r; 
    if(e < 0.9) break;
    uv += vec2(0, texture_size.y * 2);
  }
  return min(2 * (i + e), 2 * SMAA_SEARCH_STEP);
}

float smaa_search_down(vec2 uv)
{
  int i = 0;
  float e = 0;
  uv -= vec2(0, 1.5 * texture_size.y);
  for(i = 0; i < SMAA_SEARCH_STEP; i++)
  {
    e = texture(edge_tex, uv).r; 
    if(e < 0.9) break;
    uv -= vec2(0, texture_size.y * 2);
  }
  return min(2 * (i + e), 2 * SMAA_SEARCH_STEP);
}

#ifdef CALCULATE_FACTOR

bvec2 mode_of_single(float value)
{
  bvec2 res = bvec2(false);
  if(value > 0.875)
    res = bvec2(true, true);
  else if(value > 0.5)
    res.y = true;
  else if(value > 0.125)
    res.x = true;
  return res;
}

bvec4 mode_of_pair(float v1, float v2)
{
  bvec4 ret;
  ret.xy = mode_of_single(v1);
  ret.zw = mode_of_single(v2);
  return ret;
}

float l_n_shape(float d, float m)
{
  float s = 0;
  float l = d * 0.5;
  if(l > (m + 0.5))
    s = (l - m) * 0.5 / l;
  else if(l > (m - 0.5))
  {
    float a = l - m + 0.5;
    float s = a * a * 0.25 / l;
  }
  return s;
}

float l_l_s_shape(float d1, float d2)
{
  float d = d1 + d2;
  float s1 = l_n_shape(d, d1);
  float s2 = l_n_shape(d, d2);
  return s1 + s2;
}

float l_l_d_shape(float d1, float d2)
{
  float d = d1 + d2;
  float s1 = l_n_shape(d, d1);
  float s2 = l_n_shape(d, d2);
  return s1 - s2;
}

float cal_area(vec2 d, bvec2 left, bvec2 right)
{
  float result = 0;
  if(left.x && left.y){
    if(right.x && !right.y)
      result = -l_l_d_shape(d.x + 0.5, d.y + 0.5);
    else if(!right.x && right.y)
      result = l_l_d_shape(d.x + 0.5, d.y + 0.5);
  }
  else if(left.x && !left.y){
    if(right.y)
      result = l_l_d_shape(d.x + 0.5, d.y + 0.5);
    else if(!right.x)
      result = l_n_shape(d.y + d.x + 1, d.x + 0.5);
    else 
      result = l_l_s_shape(d.x + 0.5, d.y + 0.5);
  }
  else if(!left.x && left.y){
    if(right.x)
      result = -l_l_d_shape(d.x + 0.5, d.y + 0.5);
    else if(!right.y)
      result = -l_n_shape(d.y + d.x + 1, d.x + 0.5);
    else 
      result = -l_l_s_shape(d.x + 0.5, d.y + 0.5);
  }
  else{
    if(right.x && !right.y)
      result = l_n_shape(d.x + d.y + 1, d.y + 0.5);
    else if(!right.x && right.y)
      result = -l_n_shape(d.x + d.y + 1, d.y + 0.5);
  }

  return result;
}

#else

vec2 smaa_area(vec2 dist, float e1, float e2)
{
  vec2 texcoord = vec2(SMAA_AREATEX_MAX_DISTANCE) * round(4.0 * vec2(e1, e2)) + dist;
  texcoord = SMAA_AREATEX_PIXEL_SIZE * (texcoord + 0.5);
  //texcoord.y += SMAA_AREATEX_SUBTEX_SIZE * offset;
  //texcoord.y = 1 - texcoord.y;
  //if(texcoord.x < 0 || texcoord.x > 1)
  //  return vec2(0, 1);
  //return texcoord;
  return textureLod(area_tex, texcoord, 0).rg;
}

vec2 smaa_diag_area(vec2 dist, vec2 e)
{
  vec2 texcoord = vec2(SMAA_AREATEX_DIAG_MAX_DISTANCE) * e + dist;
  texcoord = SMAA_AREATEX_PIXEL_SIZE * (texcoord + 0.5);
  texcoord.x += 0.5;
  return textureLod(area_tex, texcoord, 0).rg;
}

vec2 smaa_hor_corner_pattern(vec2 w, vec4 texcoord, vec2 d)
{
  vec2 lr = step(d.xy, d.yx); 
  vec2 rd = (1 - SMAA_CORNER_ROUNDING_NORM) * lr;
  rd /= lr.x + lr.y;
  vec2 factor = vec2(1, 1);
  factor.x -= rd.x * textureLodOffset(edge_tex, texcoord.xy, 0, ivec2(0, -1)).r;
  factor.x -= rd.y * textureLodOffset(edge_tex, texcoord.zw, 0, ivec2(0, -1)).r;
  factor.y -= rd.x * textureLodOffset(edge_tex, texcoord.xy, 0, ivec2(0,  2)).r;
  factor.y -= rd.y * textureLodOffset(edge_tex, texcoord.zw, 0, ivec2(0,  2)).r;
  w *= clamp(factor, 0, 1);
  return w;
}

vec2 smaa_ver_corner_pattern(vec2 w, vec4 texcoord, vec2 d)
{
  vec2 ud = step(d.xy, d.yx); 
  vec2 rd = (1 - SMAA_CORNER_ROUNDING_NORM) * ud;
  rd /= ud.x + ud.y;
  vec2 factor = vec2(1, 1);
  factor.x -= rd.x * textureLodOffset(edge_tex, texcoord.xy, 0, ivec2( 1, 0)).g;
  factor.x -= rd.y * textureLodOffset(edge_tex, texcoord.zw, 0, ivec2( 1, 0)).g;
  factor.y -= rd.x * textureLodOffset(edge_tex, texcoord.xy, 0, ivec2(-2, 0)).g;
  factor.y -= rd.y * textureLodOffset(edge_tex, texcoord.zw, 0, ivec2(-2, 0)).g;
  w *= clamp(factor, 0, 1);
  return w;
}

vec2 smaa_decode_diag_bil_access(vec2 c)
{
  c.r = c.r * abs(5 * c.r - 5 * 0.75);
  return round(c);
}

vec4 smaa_decode_diag_bil_access(vec4 c)
{
  c.rb = c.rb * abs(5 * c.rb - 5 * 0.75);
  return round(c);
}

void smaa_movc(bvec2 cond, inout vec2 c, vec2 v)
{
  if(cond.x) c.x = v.x;
  if(cond.y) c.y = v.y;
}

void smaa_movc(bvec4 cond, inout vec4 c, vec4 v)
{
  smaa_movc(cond.xy, c.xy, v.xy);
  smaa_movc(cond.zw, c.zw, v.zw); 
}

vec2 smaa_search_diag1(vec2 uv, vec2 dir, out vec2 edge)
{
  int i = 0; float w = 1;
  while(i < SMAA_SEARCH_DIAG_STEP && w > 0.9)
  {
    uv += texture_size.xy * dir;
    edge = textureLod(edge_tex, uv, 0).rg;
    w = dot(edge, vec2(0.5, 0.5));
    i++;
  }
  return vec2(i - 1, w);
}

vec2 smaa_search_diag2(vec2 uv, vec2 dir, out vec2 edge)
{
  int i = 0; float w = 1;
  uv.x += 0.25 * texture_size.x;
  // while(i < SMAA_SEARCH_DIAG_STEP && w >0.9)
  // {
  //   uv += texture_size.xy * dir;
  //   edge = textureLod(edge_tex, uv, 0).rg;
  //   edge = smaa_decode_diag_bil_access(edge);
  //   w = dot(edge, vec2(0.5, 0.5));
  //   i++;
  // }
  uv.x += 0.5 * texture_size.x;
  while(i < SMAA_SEARCH_DIAG_STEP && w >0.9)
  {
    uv += texture_size.xy * dir;
    edge = textureLod(edge_tex, uv, 0).rg;
    w = edge.r + edge.g;
    i++;
  }

  return vec2(i - 1, w);
}

vec2 smaa_cal_diag_weight(vec2 uv, vec2 edge)
{
  vec4 d; vec2 end;
  vec2 weight = vec2(0);
  if(edge.r > 0){
    d.xz = smaa_search_diag1(uv, vec2(-1, -1), end);
    d.x += float(end.y > 0.9);
  }
  else
    d.xz = vec2(0);
  d.yw = smaa_search_diag1(uv, vec2(1, 1), end);
  if(d.x + d.y > 2.0)
  {
    vec4 c;
    vec4 coords = vec4(-d.x + 0.25, -d.x, d.y, d.y + 0.25) * texture_size.xyxy + uv.xyxy;
    c.xy = textureLodOffset(edge_tex, coords.xy, 0, ivec2(-1, 0)).rg;
    c.zw = textureLodOffset(edge_tex, coords.zw, 0, ivec2(1, 0)).rg;
    c.yxwz = smaa_decode_diag_bil_access(c);

    vec2 cc = vec2(2) * c.xz + c.yw;
    smaa_movc(bvec2(step(vec2(0.9), d.zw)), cc, vec2(0, 0));
    weight += smaa_diag_area(d.xy, cc); 
  }

  d.xz = smaa_search_diag2(uv, vec2(-1, 1), end);
  if(textureLodOffset(edge_tex, uv, 0, ivec2(1, 0)).r > 0)
  {
    d.yw = smaa_search_diag2(uv, vec2(1, -1), end);
    d.y += float(end.y > 0.9);
  }
  else
    d.yw = vec2(0);

  if(d.x + d.y > 2.0)
  {
    vec4 c;
    vec4 coords = vec4(-d.x, d.x, d.y, -d.y) * texture_size.xyxy + uv.xyxy;
    c.x = textureLodOffset(edge_tex, coords.xy, 0, ivec2(-1, 0)).g;
    c.y = textureLodOffset(edge_tex, coords.xy, 0, ivec2(0, 1)).r;
    c.zw = textureLodOffset(edge_tex, coords.zw, 0, ivec2(1, 0)).gr;
    vec2 cc = vec2(2) * c.xz + c.yw;
    smaa_movc(bvec2(step(vec2(0.9), d.zw)), cc, vec2(0, 0));
    weight += smaa_diag_area(d.xy, cc).gr;
  }
  return weight;
}

#endif

void main()
{
  vec2 uv = gl_TexCoord[0].xy;
  vec2 edge = texture(edge_tex, uv).rg;
  vec4 result = vec4(0);
  if(edge.g > 0){
    #ifdef DIAG_DETECTION
    result.xy = smaa_cal_diag_weight(uv, edge);
    if(result.x != -result.y){
      gl_FragColor = result;
      return;
    }
    #endif
    float lt = smaa_search_left(uv);
    float rt = smaa_search_right(uv);
    vec2 lfo = vec2(-lt * texture_size.x, 0.25 * texture_size.y);
    vec2 rto = vec2((rt + 1) * texture_size.x, 0.25 * texture_size.y);
    vec2 lfuv = uv + lfo, rtuv = uv + rto;
    float ltv = texture(edge_tex, lfuv).r;
    float rtv = texture(edge_tex, rtuv).r;
    
    #ifdef CALCULATE_FACTOR
      bvec2 l = mode_of_single(ltv);
      bvec2 r = mode_of_single(rtv);
      float value = cal_area(vec2(lt, rt), l, r);
      result.xy = vec2(-value, value);
    #else
      vec2 sqrtd = sqrt(vec2(lt, rt));
      result.xy = smaa_area(sqrtd, ltv, rtv);
      result.xy = smaa_hor_corner_pattern(result.xy, vec4(lfuv, rtuv), vec2(lt, rt));
    #endif
  }

  if(edge.r > 0)
  {
    float up = smaa_search_up(uv);
    float dn = smaa_search_down(uv);
    vec2 upo = vec2(-0.25 * texture_size.x, up * texture_size.y);
    vec2 dno = vec2(-0.25 * texture_size.x, -(dn + 1) * texture_size.y);
    vec2 upuv = uv + upo, dnuv = uv + dno;
    float upv = texture(edge_tex, upuv).g;
    float dnv = texture(edge_tex, dnuv).g;

    #ifdef CALCULATE_FACTOR
      bvec2 u = mode_of_single(upv);
      bvec2 d = mode_of_single(dnv);
      float value = cal_area(vec2(up, dn), u, d);
      result.zw = vec2(-value, value);
    #else
      vec2 sqrtd = sqrt(round(vec2(up , dn)));
      result.zw = smaa_area(sqrtd, upv, dnv);
      result.zw = smaa_ver_corner_pattern(result.zw, vec4(upuv, dnuv), vec2(up, dn));
    #endif
  }
  gl_FragColor = result;
}