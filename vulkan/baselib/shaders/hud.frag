#version 450

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 vp_uv;

layout(location = 0) out vec4 frag_color;

void main()
{
  float v = texture(tex, vp_uv).r;
  frag_color = vec4(vec3(v), 1);
}
