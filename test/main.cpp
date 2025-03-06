#include "tvec.h"
#include "tmath.h"
#include <filesystem>

using tg::vec3;
using tg::vec4;
using tg::mat4;


int main()
{
  mat4 m1 = tg::lookat(vec3(10, 0, 0));
  mat4 m2 = tg::perspective(90, 1, 1, 100);
  auto m = m2 * m1;

  vec3 v1 = m * vec3(1, 1, 1);
  auto m3 = tg::inverse(m);
  auto m5 = m * *m3;
  auto m4 = m3->transpose();

  auto v3 = m4 * vec4(1, 1, 1, 1);

  {
    auto pt2 = m * vec3(2, 2, -5);
    vec4 xx(pt2, 1);
    auto ret = tg::dot(xx, v3);
    printf("");
  }

  vec3 v31 = m * vec3(2, 2, -5);
  vec3 v32 = m * vec3(3, 3, -7);

  vec3 dir = v32 - v31;
  dir = tg::normalize(dir);

  auto xx = tg::dot(dir, tg::vec3(v3));

  printf("");
}