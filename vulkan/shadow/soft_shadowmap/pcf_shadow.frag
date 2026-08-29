#version 450

layout(location = 0) in vec3 vp_pos;
layout(location = 1) in vec3 vp_norm;
layout(location = 2) in vec2 vp_uv;
layout(location = 3) in vec3 vp_suv; 

layout(location = 0) out vec4 frag_color;

layout(binding = 0) uniform MatrixObject
{
  vec4 eye;
  mat4 proj;
  mat4 view;
} mvp;

layout(set = 1, binding = 0) uniform ParallelLight
{
  vec4 light_dir;
  vec4 light_color;
} light;

layout(set = 2, binding = 0) uniform sampler2D tex;

layout(set = 3, binding = 0) uniform ShadowMatrix{
  vec4 light;
  mat4 proj;
  mat4 view;
  mat4 mvp;
} shadow_matrix;

layout(set = 3, binding = 1) uniform sampler2D shadow_tex;

#define SHADOW_SAMPLING_PCF_HARD 0
#define SHADOW_SAMPLING_PCF_LOW 1

float shadow_depth(vec2 uv)
{
  return texture(shadow_tex, uv).r;
}

float shadow_visible(vec2 uv, float depth, float bias)
{
  return step(0.0, shadow_depth(uv) + bias - depth);
}

float ShadowSample_PCF_Hard(vec2 suv, float depth, float bias)
{
  return shadow_visible(suv, depth, bias);
}

// 4-tap Gaussian approximation ported from Filament's ShadowSample_PCF_Low.
float ShadowSample_PCF_Low(vec2 suv, float depth, float bias)
{
  vec2 size = vec2(textureSize(shadow_tex, 0));
  vec2 texelSize = vec2(1.0) / size;

  vec2 offset = vec2(0.5);
  vec2 uv = (suv * size) + offset;
  vec2 base = (floor(uv) - offset) * texelSize;
  vec2 st = fract(uv);

  vec2 uw = vec2(3.0 - 2.0 * st.x, 1.0 + 2.0 * st.x);
  vec2 vw = vec2(3.0 - 2.0 * st.y, 1.0 + 2.0 * st.y);

  vec2 u = vec2((2.0 - st.x) / uw.x - 1.0, st.x / uw.y + 1.0);
  vec2 v = vec2((2.0 - st.y) / vw.x - 1.0, st.y / vw.y + 1.0);

  u *= texelSize.x;
  v *= texelSize.y;

  float w0 = uw.x * vw.x;
  float w1 = uw.y * vw.x;
  float w2 = uw.x * vw.y;
  float w3 = uw.y * vw.y;

  vec2 uv0 = base + vec2(u.x, v.x);
  vec2 uv1 = base + vec2(u.y, v.x);
  vec2 uv2 = base + vec2(u.x, v.y);
  vec2 uv3 = base + vec2(u.y, v.y);

  uv0 = clamp(uv0, vec2(0.0), vec2(1.0));
  uv1 = clamp(uv1, vec2(0.0), vec2(1.0));
  uv2 = clamp(uv2, vec2(0.0), vec2(1.0));
  uv3 = clamp(uv3, vec2(0.0), vec2(1.0));

  float sum = 0.0;
  sum += w0 * shadow_visible(uv0, depth, bias);
  sum += w1 * shadow_visible(uv1, depth, bias);
  sum += w2 * shadow_visible(uv2, depth, bias);
  sum += w3 * shadow_visible(uv3, depth, bias);

  return sum * 0.0625;
}

void main(void)
{
  vec4 base_color = texture(tex, vp_uv);
  if (base_color.a == 0.0)
    discard;

  vec3 n = normalize(vp_norm);
  vec3 v = normalize(mvp.eye.xyz - vp_pos);
  vec3 l = normalize(light.light_dir.xyz);
  vec3 radiance = light.light_color.rgb;

  float ndotl = max(dot(n, l), 0.0);
  vec3 reflected = reflect(-l, n);
  float specular_factor = 0.0;
  if (ndotl > 0.0)
    specular_factor = pow(max(dot(v, reflected), 0.0), 32.0);

  float visibility = 1.0;

  if (vp_suv.x > -1.0 && vp_suv.x < 1.0 &&
      vp_suv.y > -1.0 && vp_suv.y < 1.0 &&
      vp_suv.z >= 0.0 && vp_suv.z <= 1.0)
  {
    vec2 suv = vp_suv.xy;
    suv = (suv + vec2(1.0)) * 0.5;
    suv.y = 1.0 - suv.y;

    float light_angle = clamp(dot(n, normalize(shadow_matrix.light.xyz)), 0.0, 1.0);
    float depbias = tan(acos(light_angle)) * 0.0002;
    depbias = clamp(depbias, 0.00002, 0.002);
    if (shadow_matrix.light.w >= 0.5)
      visibility = ShadowSample_PCF_Low(suv, vp_suv.z, depbias);
    else
      visibility = ShadowSample_PCF_Hard(suv, vp_suv.z, depbias);
  }

  vec3 ambient = base_color.rgb * 0.05;
  vec3 diffuse = base_color.rgb * radiance * ndotl;
  vec3 specular = radiance * 0.2 * specular_factor;
  vec3 color = ambient + visibility * (diffuse + specular);

  color = color / (color + vec3(1.0));
  frag_color = vec4(color, 1.0);
}
