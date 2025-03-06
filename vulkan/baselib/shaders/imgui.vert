#version 450

layout(location = 0) in vec2 attr_pos;
layout(location = 1) in vec2 attr_uv;
layout(location = 2) in vec4 attr_color;

layout(push_constant) uniform PushConstant{
  vec2 scale;
  vec2 translate;
} push_constant;

layout(location = 0) out vec2 vp_uv;
layout(location = 1) out vec4 vp_color;

out gl_PerVertex{
  vec4 gl_Position;
};

void main()
{
  vp_uv = attr_uv;
  vp_color = attr_color;
  gl_Position = vec4(attr_pos * push_constant.scale + push_constant.translate, 0.0, 1.0);
}