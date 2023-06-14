#pragma once

#include "tvec.h"

#include <vector>

using tg::vec3;
using tg::vec2;

class Sphere {
  vec3 _pos;
  float _rad;

  constexpr static uint32_t x_segs = 64;
  constexpr static uint32_t y_segs = 64;

  std::vector<vec3> _verts, _norms;
  std::vector<vec2> _uvs;
  std::vector<uint16_t> _indices;

public:
  Sphere(const tg::vec3 &pos, float rad) : _pos(pos), _rad(rad) {}

  ~Sphere() {}

  void build()
  {
    _verts.reserve(x_segs * y_segs);
    _norms.reserve(_verts.size());
    _uvs.reserve(_verts.size());
    for (uint32_t y = 0; y < y_segs; y++) {
      for (uint32_t x = 0; x < x_segs; x++) {
        float xSegment = (float)x / (float)x_segs;
        float ySegment = (float)y / (float)y_segs;
        float xPos = _rad * std::cos(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI);
        float yPos = _rad * std::cos(ySegment * M_PI);
        float zPos = _rad * std::sin(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI);

        _verts.push_back(vec3(xPos, yPos, zPos) + _pos);
        _norms.push_back(vec3(xPos, yPos, zPos));
        _uvs.push_back(vec2(xSegment, ySegment));
      }
    }

    _indices.reserve(y_segs * x_segs * 6);
    bool oddRow = false;
    for (unsigned int y = 0; y < y_segs; ++y) {
      if (!oddRow) {
        for (unsigned int x = 0; x <= x_segs; ++x) {
          _indices.push_back((y + 1) * (x_segs + 1) + x);
          _indices.push_back(y * (x_segs + 1) + x);
        }
      } else {
        for (int x = x_segs; x >= 0; --x) {
          _indices.push_back(y * (x_segs + 1) + x);
          _indices.push_back((y + 1) * (x_segs + 1) + x);
        }
      }
      oddRow = !oddRow;
    }
  }

  const std::vector<vec3>& get_vertex() { return _verts; }
  const std::vector<vec3>& get_norms() { return _norms; }
  const std::vector<vec2>& get_uvs() { return _uvs; }
  const std::vector<uint16_t>& get_index() { return _indices; }
};