#pragma once

#include "tmath.h"

class Manipulator{
public:
  Manipulator();
  ~Manipulator();

  void set_home(const tg::vec3 &eye, const tg::vec3 &pos, const tg::vec3 &up);

  void home();

  void rotate(int x, int y);

  void translate(int x, int y);

  void zoom(float in);

  const tg::vec3d & eye() const { return _eye; }

  tg::mat4 view_matrix();

private:

  tg::vec3d _eye;
  tg::vec3d _pos;
  tg::vec3d _up;

  tg::vec3d _home[3];
};