#version 450

layout(location = 0) in vec2 attr_pos;
layout(location = 1) in vec2 attr_uv;

layout(push_constant) uniform PushConstant{
  mat4 m;
} push_constant;

layout(location = 0) out vec2 vp_uv;

out gl_PerVertex{
  vec4 gl_Position;
};

void main()
{
  vp_uv = attr_uv;
  gl_Position = push_constant.m * vec4(attr_pos, 0.0, 1.0);
}