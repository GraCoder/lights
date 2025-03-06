#include "Manipulator.h"
#include "tvec.h"

#include <stdio.h>

using tg::vec3;
using tg::vec4;
using tg::vec3d;
using tg::vec4d;

Manipulator::Manipulator()
{
  set_home(vec3(0, -5, 0), vec3(0), vec3(0, 0, 1));

  vec3d v(1, 1, 1);
  vec3 t(v);
  vec3 x = v;
  //v = tg::normalize(v);
  //auto m = tg::rotate<double>(45.0 / 180 * M_PI, tg::vec3(0, 0, 1));
  //auto q = tg::matquat(m);
  //float l1 = tg::length(tg::vec4d(q));

  //auto v1 = q * v * q.conjugate();

  //auto q1 = tg::quatd::rotate(45.0 / 180 * M_PI, vec3d(0, 0, 1));
  //float l2 = tg::length(vec4d(q1));

  //auto v2 = q1 * v * q1.conjugate();
  //printf("");
}

Manipulator::~Manipulator()
{
}

void Manipulator::set_home(const tg::vec3& eye, const tg::vec3 &pos, const tg::vec3 &up)
{
  _eye = eye;
  _pos = pos;
  _up = tg::cross(eye - pos, tg::cross(up, eye - pos));
  _up = tg::normalize(_up);

  _home[0] = _eye;
  _home[1] = _pos;
  _home[2] = _up;
}

void Manipulator::home()
{
  _eye = _home[0];
  _pos = _home[1];
  _up = _home[2];
}

void Manipulator::rotate(int x, int y)
{
  vec3d tmp = _eye - _pos;
  vec3d rt = tg::cross(_up, tmp);
  if (x) {
    float xrad = x / 100.f;
    auto q = tg::quatd::rotate(-xrad, _up);
    auto xtmp = q * tg::quatd(tmp) * q.conjugate();
    tmp = xtmp;
    rt = tg::normalize(tg::cross(_up, tmp));
  }

  if (y) {
    float yrad = y / 100.f;
    auto q = tg::quatd::rotate(-yrad, rt);
    auto ytmp = q * tg::quatd(tmp) * q.conjugate();
    //_up = q * tg::quat(_up) * q.conjugate();
    tmp = ytmp;
  }

  _eye = _pos + tmp;
  _up = tg::normalize(tg::cross(tmp, rt));

  if (tg::length(tg::cross(tmp, vec3d(0, 0, 1))) > 1e-4) {
    rt = tg::cross(vec3d(0, 0, 1), tmp);
    _up = tg::normalize(tg::cross(tmp, rt));
  } else
    _up = tg::normalize(tg::cross(tmp, rt));
}

void Manipulator::translate(int x, int y)
{
    auto tmp = tg::normalize(_eye - _pos);
    auto rt = tg::cross(_up, tmp);
    rt *= -x * 0.01;
    auto up = _up * -y * 0.01;
    _eye += rt;
    _eye += up;
    _pos += rt;
    _pos += up;
}

void Manipulator::zoom(float in)
{
  auto tmp = _eye - _pos;
  if (in > 0)
    tmp *= 1.1;
  else
    tmp *= 0.9;
  auto len = tg::length(tmp);
  if (len < 1 || len > 1000)
    return;
  _eye = _pos + tmp;
}

tg::mat4 Manipulator::view_matrix()
{
  auto m = tg::lookat(_eye, _pos, _up);

  return m;
}
