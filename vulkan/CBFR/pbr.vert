#version 450

layout(binding = 0) uniform MatrixObject{
    mat4 proj;
    mat4 view;
    mat4 model;
    vec4 cam;
} ubo;

layout(location = 0) in vec3 attr_pos;
layout(location = 1) in vec3 attr_norm;

layout(location = 0) out vec3 vp_pos;
layout(location = 1) out vec3 vp_norm;
layout(location = 2) out int instance_idx;
//layout(location = 2) out vec2 vp_uv;

void main(void)
{
  float row = gl_InstanceIndex / 7 - 3;
  float col = mod(gl_InstanceIndex, 7) - 3;
  vec3 offset = vec3(0, 0, 3) * row + vec3(3, 0, 0) * col;
  gl_Position = ubo.proj * ubo.view * vec4(attr_pos + offset, 1.0);
  //gl_Position.y = -gl_Position.y;
  //gl_Position = vec4(attr_pos, 1);

  vp_pos = attr_pos;
  vp_norm = attr_norm;
  instance_idx = gl_InstanceIndex;
}
