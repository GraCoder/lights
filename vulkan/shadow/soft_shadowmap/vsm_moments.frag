#version 450

layout(location = 0) out vec2 moments;

void main(void)
{
  float depth = gl_FragCoord.z;
  float dx = dFdx(depth);
  float dy = dFdy(depth);
  moments = vec2(depth, depth * depth + 0.25 * (dx * dx + dy * dy));
}
