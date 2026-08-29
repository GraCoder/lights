#pragma once

#include "tvec.h"

#include <vector>

using tg::vec3;
using tg::vec2;

class Shape{
public:
  const std::vector<vec3>& getVertex() { return _verts; }
  const std::vector<vec3>& getNorms() { return _norms; }
  const std::vector<vec2>& getUvs() { return _uvs; }
  const std::vector<uint16_t>& getIndex() { return _indices; }

protected:
  std::vector<vec3> _verts, _norms;
  std::vector<vec2> _uvs;
  std::vector<uint16_t> _indices;
};

template<uint32_t xSegCount = 64, uint32_t ySegCount = 32>
class Sphere : public Shape{
  vec3 _pos;
  float _rad;

  constexpr static uint32_t xSegs = xSegCount;
  constexpr static uint32_t ySegs = ySegCount;

public:
  Sphere(const tg::vec3 &pos, float rad) : _pos(pos), _rad(rad) {}

  ~Sphere() {}

  void build()
  {
    _verts.reserve(xSegs * (ySegs + 1));
    _norms.reserve(_verts.size());
    _uvs.reserve(_verts.size());
    for (uint32_t y = 0; y <= ySegs; y++) {
      float ySegment = (float)y / (float)ySegs;
      float yrad = ySegment * M_PI - M_PI_2;
      for (uint32_t x = 0; x < xSegs; x++) {
        float xSegment = (float)x / (float)xSegs;
        float xrad = xSegment * M_PI * 2;
        float htmp = std::cos(yrad);
        float xPos = std::cos(xrad) * htmp;
        float yPos = std::sin(xrad) * htmp;
        float zPos = std::sin(yrad);

        _verts.push_back(vec3(xPos, yPos, zPos) * _rad + _pos);
        _norms.push_back(vec3(xPos, yPos, zPos));
        _uvs.push_back(vec2(xSegment, ySegment));
      }
    }

    _indices.reserve((xSegs << 1) * (ySegs + 1));

    bool oddCol = false;
    for (int i = 0; i < xSegs; i++) {
      int idx1, idx2 = 0;
      if (oddCol) {
        idx1 = ySegs * xSegs  + (i + 1) % xSegs;
        idx2 = ySegs * xSegs + i;
      } else {
        idx1 = i;
        idx2 = (i + 1) % xSegs;
      }
      for (int j = 0; j <= ySegs; j++) {
        _indices.push_back(idx1);
        _indices.push_back(idx2);
        if (oddCol) {
          idx1 -= xSegs;
          idx2 -= xSegs;
        } else {
          idx1 += xSegs;
          idx2 += xSegs;
        }
      }
      oddCol = !oddCol;
    }
  }
};

typedef Sphere<4, 2> Octahedron;


class Box : public Shape{
  vec3 _pos;
  vec3 _size;

public:
  Box(const vec3 &pos, const vec3 &sz)
    : _pos(pos), _size(sz)
  { }


  void build()
  {
    _verts.reserve(24); _norms.reserve(24);
    float xlen = _size.x() / 2.0, ylen = _size.y() / 2.0, zlen = _size.z() / 2.0;
    _verts.push_back({xlen, ylen, zlen}); _verts.push_back({xlen, -ylen, zlen});
    _verts.push_back({xlen, -ylen, -zlen}); _verts.push_back({xlen, ylen, -zlen});
    _verts.push_back({-xlen, -ylen, zlen}); _verts.push_back({-xlen, ylen, zlen});
    _verts.push_back({-xlen, ylen, -zlen}); _verts.push_back({-xlen, -ylen, -zlen});

    _verts.push_back({-xlen, ylen, zlen}); _verts.push_back({xlen, ylen, zlen});
    _verts.push_back({xlen, ylen, -zlen}); _verts.push_back({-xlen, ylen, -zlen});
    _verts.push_back({xlen, -ylen, zlen}); _verts.push_back({-xlen, -ylen, zlen});
    _verts.push_back({-xlen, -ylen, -zlen}); _verts.push_back({xlen, -ylen, -zlen});

    _verts.push_back({xlen, ylen, zlen}); _verts.push_back({-xlen, ylen, zlen});
    _verts.push_back({-xlen, -ylen, zlen}); _verts.push_back({xlen, -ylen, zlen});
    _verts.push_back({xlen, ylen, -zlen}); _verts.push_back({xlen, -ylen, -zlen});
    _verts.push_back({-xlen, -ylen, -zlen}); _verts.push_back({-xlen, ylen, -zlen});

    vec3 norm[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int i = 0; i < 24; i++)
      _norms.push_back(norm[i / 4]);

    _uvs.assign(_verts.size(), {0.5, 0.5});

    _indices.reserve(36);
    int count = 0;
    for(int i = 0; i < 6; i++) {
      _indices.push_back(count + 0); _indices.push_back(count + 1); _indices.push_back(count + 2);
      _indices.push_back(count + 0); _indices.push_back(count + 2); _indices.push_back(count + 3);
      count += 4;
    }
  }
};
