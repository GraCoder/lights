#pragma once

#include "tmath.h"

class Manipulator
{
public:
  Manipulator();
  ~Manipulator();

  void home();
  void resetHome();
  void setHome(const tg::vec3 &eye, const tg::vec3 &pos, const tg::vec3 &up);

  void rotate(int x, int y);

  void translate(int x, int y);

  void zoom(float in);

  tg::vec3d eye() const;

  tg::mat4 viewMatrix() const;

private:
  tg::vec3d _target{0, 0, 0};
  tg::quatd _rotation;
  double _distance = 30;

  tg::vec3d _homeTarget;
  tg::quatd _homeRotation;
  double _homeDistance = 30;
};
