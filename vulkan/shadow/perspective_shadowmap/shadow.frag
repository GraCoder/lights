#version 450

layout(location = 0) in vec3 vp_pos;
layout(location = 1) in vec3 vp_norm;
layout(location = 2) in vec2 vp_uv;

layout(location = 0) out vec4 frag_color;

layout(set = 0, binding = 0) uniform MatrixObject
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

layout(set = 2, binding = 0) uniform Material
{
  float ao;
  float metallic;
  float roughness;
  vec4 albedo;
} material;

layout(set = 3, binding = 0) uniform sampler2D tex;

layout(set = 4, binding = 0) uniform ShadowMatrix{
  vec4 light;
  mat4 proj;
  mat4 view;
  mat4 mvp;
  mat4 pers;
} shadow_matrix;

layout(set = 5, binding = 0) uniform sampler2D shadow_tex;

const float pi = 3.14159265359;

vec3 fresnel_schlick(float cosTheta, vec3 f0)
{
  return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

float distribution_GGX(vec3 n, vec3 h, float roughness)
{
  float a = roughness * roughness;
  float a2 = a * a;
  float ndot_h = max(dot(n, h), 0.0);
  float ndot_h2 = ndot_h * ndot_h;

  float denom = ndot_h2 * (a2 - 1.0) + 1.0;
  denom = pi * denom * denom;

  return a2 / max(denom, 0.0000001);
}

float schlick_GGX(float ndotv, float roughness)
{
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float denom = ndotv * (1.0 - k) + k;

  return ndotv / denom;
}

float smith_Geometry(vec3 n, vec3 v, vec3 l, float roughness)
{
  float ndotv = max(dot(n, v), 0.0);
  float ndotl = max(dot(n, l), 0.0);
  float ggx2 = schlick_GGX(ndotv, roughness);
  float ggx1 = schlick_GGX(ndotl, roughness);

  return ggx1 * ggx2;
}

void main(void)
{
  frag_color = texture(tex, vp_uv);
  if (frag_color.a == 0.f)
    discard;

  vec3 mate_albedo = frag_color.rgb;
  float mate_roughness = material.roughness;
  float mate_metallic = material.metallic;
  float mate_ao = material.ao;

  vec3 eye = mvp.eye.xyz;
  vec3 n = normalize(vp_norm);
  vec3 v = normalize(eye - vp_pos);

  vec3 f0 = vec3(0.04);
  f0 = mix(f0, mate_albedo, mate_metallic);
  vec3 lo = vec3(0.0);

  vec3 l = normalize(light.light_dir.xyz);
  vec3 h = normalize(v + l);
  vec3 radiance = light.light_color.rgb;

  float nv = distribution_GGX(n, h, mate_roughness);
  float gv = smith_Geometry(n, v, l, mate_roughness);
  vec3 fv = fresnel_schlick(clamp(dot(h, v), 0.0, 1.0), f0);

  vec3 nominator = nv * gv * fv;
  float denominator = 4 * max(dot(n, v), 0) * max(dot(n, l), 0.0);
  vec3 specular = nominator / max(denominator, 0.000001);

  vec3 ks = fv;
  vec3 kd = vec3(1.0) - ks;
  kd *= (1.0 - mate_metallic);

  float ndotl = max(dot(n, l), 0);

  lo += (kd * mate_albedo / pi + specular) * radiance * ndotl;

  vec3 ambient = vec3(0.03) * mate_albedo * mate_ao;
  vec3 color = ambient + lo;

  color = color / (color + vec3(1.0));
  //color = pow(color, vec3(1.0 / 2.2));

  {
    vec4 vtmp = shadow_matrix.pers * vec4(vp_pos, 1);
    vtmp.xyz /= vtmp.w; vtmp.w = 1;
    vtmp = shadow_matrix.mvp * vtmp;
    vtmp.xyz /= vtmp.w;
    vtmp.xy = (vtmp.xy + vec2(1.0, 1.0)) / 2.0;

    vec2 suv = vtmp.xy;
    suv.y = 1 - suv.y;
    float dep = texture(shadow_tex, suv).r;
    float depbias = (tan(acos(dot(n, l))) + 1) * 0.0001;
    if(vtmp.z > dep + depbias)
    {
      color = color * dep;
    }
  }

  frag_color = vec4(color, 1.0);
}
