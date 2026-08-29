#include "Manipulator.h"
#include "tmath.h"
#include "tvec.h"

using tg::quatd;
using tg::vec3;
using tg::vec3d;

Manipulator::Manipulator()
{
  setHome(tg::vec3(0, 0, 30), tg::vec3(0), tg::vec3(0, 1, 0));
}

Manipulator::~Manipulator() {}

void Manipulator::home()
{
  _target = _homeTarget;
  _rotation = _homeRotation;
  _distance = _homeDistance;
}

void Manipulator::setHome(const tg::vec3 &eye, const tg::vec3 &pos, const tg::vec3 &up)
{
  _target = pos;
  _distance = tg::length(tg::vec3d(eye) - _target);

  tg::vec3d forward = tg::normalize(tg::vec3d(eye) - _target);
  tg::vec3d upv = tg::normalize(tg::vec3d(up));
  tg::vec3d rt = tg::normalize(tg::cross(upv, forward));

  tg::dmat3 m(rt, tg::cross(forward, rt), forward);
  _rotation = tg::normalize(tg::quaternion(m));

  _homeTarget = _target;
  _homeRotation = _rotation;
  _homeDistance = _distance;
}

void Manipulator::rotate(int x, int y)
{
  if (!x && !y)
    return;

  double dx = x / 200.0;
  double dy = y / 200.0;

  auto upAxis = tg::normalize(_homeRotation * tg::vec3d(0, 1, 0));
  tg::quatd qx = tg::quaternion<double>(-dx, upAxis);
  auto right = qx * _rotation * tg::vec3d(1, 0, 0);
  tg::quatd qy = tg::quaternion<double>(-dy, right);
  _rotation = tg::normalize(qy * qx * _rotation);
}

void Manipulator::translate(int x, int y)
{
  auto up = tg::normalize(_rotation * tg::vec3d(0, 1, 0));
  auto dir = tg::normalize(_target - eye());
  auto rt = tg::normalize(tg::cross(dir, up));
  double scale = _distance * 0.001;
  _target += (rt * -x + up * -y) * scale;
}

void Manipulator::zoom(float in)
{
  float scale = 1.f + in * 0.2f;
  scale = tg::clamp(scale, 0.1f, 1e6f);
  _distance *= scale;
  _distance = tg::clamp(_distance, 1.0, 1000.0);
}

tg::vec3d Manipulator::eye() const
{
  return _target + _rotation * tg::vec3d(0, 0, _distance);
}

tg::mat4 Manipulator::viewMatrix() const
{
  auto up = tg::normalize(_rotation * tg::vec3d(0, 1, 0));
  return tg::lookat(eye(), _target, up);
}
