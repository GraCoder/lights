#ifndef __TVEC_INC__
#define __TVEC_INC__

namespace tg {
// template <typename T, const int w, const int h> class matNM;
// template <typename T, const int n> class vecN;
// template <typename T> class Tquat;

#ifndef _USE_MATH_DEFINES
  #define _USE_MATH_DEFINES
  #define M_E 2.71828182845904523536        // e
  #define M_LOG2E 1.44269504088896340736    // log2(e)
  #define M_LOG10E 0.434294481903251827651  // log10(e)
  #define M_LN2 0.693147180559945309417     // ln(2)
  #define M_LN10 2.30258509299404568402     // ln(10)
  #ifndef M_PI
    #define M_PI 3.14159265358979323846     // pi
  #endif
  #define M_PI_2 1.57079632679489661923     // pi/2
  #define M_PI_4 0.785398163397448309616    // pi/4
  #define M_1_PI 0.318309886183790671538    // 1/pi
  #define M_2_PI 0.636619772367581343076    // 2/pi
  #define M_2_SQRTPI 1.12837916709551257390 // 2/sqrt(pi)
  #define M_SQRT2 1.41421356237309504880    // sqrt(2)
  #define M_SQRT1_2 0.707106781186547524401 // 1/sqrt(2)
#endif

template <typename T>
struct teps
{
  static constexpr T eps = 0;
};

template <>
struct teps<float>
{
  static constexpr float eps = float(1e-6);
};

template <>
struct teps<double>
{
  static constexpr double eps = 1e-15;
};

template <typename T>
inline T degrees(T angleInRadians)
{
  return angleInRadians * static_cast<T>(180.0 / M_PI);
}

template <typename T>
inline T radians(T angleInDegrees)
{
  return angleInDegrees * static_cast<T>(M_PI / 180.0);
}

template <typename T, int n>
class vecN
{
public:
  using type = vecN<T, n>;

  inline vecN() {}

  explicit inline vecN(const type &that)
  {
    for (int i = 0; i < n; i++)
      data_[i] = that[i];
  }

  explicit inline vecN(T s) { set(s); }

  template <typename U, int m>
  vecN(const vecN<U, m> &that)
  {
    constexpr int s = n < m ? n : m;
    for (int i = 0; i < s; i++)
      data_[i] = that[i];
  }

  template <typename U>
  inline void set(const U *u)
  {
    for (int i = 0; i < n; i++)
      data_[i] = static_cast<T>(u[i]);
  }

  inline void set(T t)
  {
    for (int i = 0; i < n; i++)
      data_[i] = t;
  }

  inline vecN<T, n> &operator=(const vecN &that)
  {
    for (int i = 0; i < n; i++)
      data_[i] = that.data_[i];
    return *this;
  }

  inline vecN<T, n> &operator=(const T &that)
  {
    for (int i = 0; i < n; i++)
      data_[i] = that;
    return *this;
  }

  template <typename U, const int m>
  inline vecN<T, n> &operator=(const vecN<U, m> &that)
  {
    constexpr int d = n < m ? n : m;
    for (int i = 0; i < d; i++)
      data_[i] = that[i];
    return *this;
  }

  inline T &operator[](int i) { return data_[i]; }
  inline const T &operator[](int i) const { return data_[i]; }

  inline vecN operator+(const vecN &that) const
  {
    type result;
    for (int i = 0; i < n; i++)
      result.data_[i] = data_[i] + that.data_[i];
    return result;
  }

  inline vecN &operator+=(const vecN &that) { return (*this = *this + that); }

  inline vecN operator-() const
  {
    type result;
    for (int i = 0; i < n; i++)
      result.data_[i] = -data_[i];
    return result;
  }

  inline vecN operator-(const vecN &that) const
  {
    type result;
    for (int i = 0; i < n; i++)
      result.data_[i] = data_[i] - that.data_[i];
    return result;
  }

  inline vecN &operator-=(const vecN &that) { return (*this = *this - that); }

  inline vecN operator*(const vecN &that) const
  {
    type result;
    for (int i = 0; i < n; i++)
      result.data_[i] = data_[i] * that.data_[i];
    return result;
  }

  inline vecN &operator*=(const vecN &that) { return (*this = *this * that); }

  inline vecN operator*(const T &that) const
  {
    type result;
    for (int i = 0; i < n; i++)
      result.data_[i] = data_[i] * that;
    return result;
  }

  inline vecN &operator*=(const T &that)
  {
    for (int i = 0; i < n; i++)
      data_[i] *= that;
    return *this;
  }

  inline vecN operator/(const vecN &that) const
  {
    type result;
    for (int i = 0; i < n; i++)
      result.data_[i] = data_[i] / that.data_[i];
    return result;
  }

  inline vecN operator/(const T &that) const
  {
    type result;
    for (int i = 0; i < n; i++)
      result.data_[i] = data_[i] / that;
    return result;
  }

  inline vecN &operator/=(const T &that)
  {
    for (int i = 0; i < n; i++)
      data_[i] /= that;
    return *this;
  }

  inline vecN &operator/=(const vecN &that)
  {
    for (int i = 0; i < n; i++)
      data_[i] /= that.data_[i];
    return *this;
  }

  inline T *data() { return static_cast<T *>(data_); }
  inline const T *data() const { return static_cast<const T *>(data_); }

  inline static int size(void) { return n; }

protected:
  T data_[n] = {};
};

template <typename T>
class vecN<T, 2>
{
public:
  using type = vecN<T, 2>;

  inline vecN() {}
  explicit inline vecN(const type &that) { set(that.data_); }
  explicit inline vecN(T s) { set(s); }
  inline vecN(T x, T y) { set(x, y); }

  template <typename U, int m>
  vecN(const vecN<U, m> &that)
  {
    constexpr int s = 2 < m ? 2 : m;
    for (int i = 0; i < s; i++)
      data_[i] = static_cast<T>(that[i]);
  }

  template <typename U>
  inline void set(const U *u)
  {
    data_[0] = static_cast<T>(u[0]);
    data_[1] = static_cast<T>(u[1]);
  }

  inline void set(T t)
  {
    data_[0] = t;
    data_[1] = t;
  }

  inline void set(const T &x, const T &y)
  {
    data_[0] = x;
    data_[1] = y;
  }

  inline vecN &operator=(const vecN &that)
  {
    data_[0] = that.data_[0];
    data_[1] = that.data_[1];
    return *this;
  }

  inline vecN &operator=(const T &that)
  {
    set(that);
    return *this;
  }

  template <typename U, const int m>
  inline vecN &operator=(const vecN<U, m> &that)
  {
    constexpr int d = 2 < m ? 2 : m;
    for (int i = 0; i < d; i++)
      data_[i] = static_cast<T>(that[i]);
    return *this;
  }

  inline vecN operator+(const vecN &that) const { return vecN(data_[0] + that.data_[0], data_[1] + that.data_[1]); }
  inline vecN &operator+=(const vecN &that) { return (*this = *this + that); }
  inline vecN operator-() const { return vecN(-data_[0], -data_[1]); }
  inline vecN operator-(const vecN &that) const { return vecN(data_[0] - that.data_[0], data_[1] - that.data_[1]); }
  inline vecN &operator-=(const vecN &that) { return (*this = *this - that); }
  inline vecN operator*(const vecN &that) const { return vecN(data_[0] * that.data_[0], data_[1] * that.data_[1]); }
  inline vecN &operator*=(const vecN &that) { return (*this = *this * that); }
  inline vecN operator*(const T &that) const { return vecN(data_[0] * that, data_[1] * that); }
  inline vecN &operator*=(const T &that)
  {
    data_[0] *= that;
    data_[1] *= that;
    return *this;
  }
  inline vecN operator/(const vecN &that) const { return vecN(data_[0] / that.data_[0], data_[1] / that.data_[1]); }
  inline vecN operator/(const T &that) const { return vecN(data_[0] / that, data_[1] / that); }
  inline vecN &operator/=(const T &that)
  {
    data_[0] /= that;
    data_[1] /= that;
    return *this;
  }
  inline vecN &operator/=(const vecN &that)
  {
    data_[0] /= that.data_[0];
    data_[1] /= that.data_[1];
    return *this;
  }

  inline T &operator[](int i) { return data_[i]; }
  inline const T &operator[](int i) const { return data_[i]; }
  inline T *data() { return static_cast<T *>(data_); }
  inline const T *data() const { return static_cast<const T *>(data_); }
  inline static int size(void) { return 2; }
  inline T &x() { return data_[0]; }
  inline T &y() { return data_[1]; }
  inline const T &x() const { return data_[0]; }
  inline const T &y() const { return data_[1]; }

protected:
  T data_[2] = {};
};

template <typename T>
class vecN<T, 3>
{
public:
  using type = vecN<T, 3>;

  inline vecN() { set(T(0)); }
  explicit inline vecN(T t) { set(t); }
  explicit inline vecN(const type &v) { set(v.data_); }
  inline vecN(T x, T y, T z) { set(x, y, z); }
  inline vecN(const vecN<T, 2> &v, T z) { set(v[0], v[1], z); }
  inline vecN(T x, const vecN<T, 2> &v) { set(x, v[0], v[1]); }

  template <typename U>
  inline vecN(const U *ptr)
  {
    set(ptr);
  }

  template <typename U, int m>
  vecN(const vecN<U, m> &that)
  {
    constexpr int s = 3 < m ? 3 : m;
    for (int i = 0; i < s; i++)
      data_[i] = static_cast<T>(that[i]);
  }

  template <typename U>
  inline void set(const U *u)
  {
    data_[0] = static_cast<T>(u[0]);
    data_[1] = static_cast<T>(u[1]);
    data_[2] = static_cast<T>(u[2]);
  }

  inline void set(T t)
  {
    data_[0] = t;
    data_[1] = t;
    data_[2] = t;
  }

  inline void set(const T &x, const T &y, const T &z)
  {
    data_[0] = x;
    data_[1] = y;
    data_[2] = z;
  }

  inline vecN &operator=(const vecN &that)
  {
    set(that.data_);
    return *this;
  }

  inline vecN &operator=(const T &that)
  {
    set(that);
    return *this;
  }

  template <typename U, const int m>
  inline vecN &operator=(const vecN<U, m> &that)
  {
    constexpr int d = 3 < m ? 3 : m;
    for (int i = 0; i < d; i++)
      data_[i] = static_cast<T>(that[i]);
    return *this;
  }

  inline vecN operator+(const vecN &that) const { return vecN(data_[0] + that.data_[0], data_[1] + that.data_[1], data_[2] + that.data_[2]); }
  inline vecN &operator+=(const vecN &that) { return (*this = *this + that); }
  inline vecN operator-() const { return vecN(-data_[0], -data_[1], -data_[2]); }
  inline vecN operator-(const vecN &that) const { return vecN(data_[0] - that.data_[0], data_[1] - that.data_[1], data_[2] - that.data_[2]); }
  inline vecN &operator-=(const vecN &that) { return (*this = *this - that); }
  inline vecN operator*(const vecN &that) const { return vecN(data_[0] * that.data_[0], data_[1] * that.data_[1], data_[2] * that.data_[2]); }
  inline vecN &operator*=(const vecN &that) { return (*this = *this * that); }
  inline vecN operator*(const T &that) const { return vecN(data_[0] * that, data_[1] * that, data_[2] * that); }
  inline vecN &operator*=(const T &that)
  {
    data_[0] *= that;
    data_[1] *= that;
    data_[2] *= that;
    return *this;
  }
  inline vecN operator/(const vecN &that) const { return vecN(data_[0] / that.data_[0], data_[1] / that.data_[1], data_[2] / that.data_[2]); }
  inline vecN operator/(const T &that) const { return vecN(data_[0] / that, data_[1] / that, data_[2] / that); }
  inline vecN &operator/=(const T &that)
  {
    data_[0] /= that;
    data_[1] /= that;
    data_[2] /= that;
    return *this;
  }
  inline vecN &operator/=(const vecN &that)
  {
    data_[0] /= that.data_[0];
    data_[1] /= that.data_[1];
    data_[2] /= that.data_[2];
    return *this;
  }

  inline T &operator[](int i) { return data_[i]; }
  inline const T &operator[](int i) const { return data_[i]; }
  inline T *data() { return static_cast<T *>(data_); }
  inline const T *data() const { return static_cast<const T *>(data_); }
  inline static int size(void) { return 3; }
  inline T &x() { return data_[0]; }
  inline T &y() { return data_[1]; }
  inline T &z() { return data_[2]; }
  inline const T &x() const { return data_[0]; }
  inline const T &y() const { return data_[1]; }
  inline const T &z() const { return data_[2]; }

protected:
  T data_[3] = {};
};

template <typename T>
class vecN<T, 4>
{
public:
  using type = vecN<T, 4>;

  inline vecN() {}
  explicit inline vecN(const type &v) { set(v.data_); }
  explicit inline vecN(T t) { set(t); }

  template <typename U, int m>
  vecN(const vecN<U, m> &that)
  {
    constexpr int s = 4 < m ? 4 : m;
    for (int i = 0; i < s; i++)
      data_[i] = static_cast<T>(that[i]);
  }

  template <typename U>
  inline vecN(const U *ptr)
  {
    set(ptr);
  }

  inline vecN(T x, T y, T z, T w) { set(x, y, z, w); }
  inline vecN(const vecN<T, 2> &v, T z, T w) { set(v[0], v[1], z, w); }
  inline vecN(T x, const vecN<T, 2> &v, T w) { set(x, v[0], v[1], w); }
  inline vecN(T x, T y, const vecN<T, 2> &v) { set(x, y, v[0], v[1]); }
  inline vecN(const vecN<T, 2> &u, const vecN<T, 2> &v) { set(u[0], u[1], v[0], v[1]); }
  inline vecN(const vecN<T, 3> &v, T w) { set(v[0], v[1], v[2], w); }
  inline vecN(T x, const vecN<T, 3> &v) { set(x, v[0], v[1], v[2]); }
  explicit inline vecN(const vecN<T, 3> &v) { set(v[0], v[1], v[2], T(0)); }

  template <typename U>
  inline void set(const U *u)
  {
    data_[0] = static_cast<T>(u[0]);
    data_[1] = static_cast<T>(u[1]);
    data_[2] = static_cast<T>(u[2]);
    data_[3] = static_cast<T>(u[3]);
  }

  inline void set(T t)
  {
    data_[0] = t;
    data_[1] = t;
    data_[2] = t;
    data_[3] = t;
  }

  inline void set(const T &x, const T &y, const T &z, const T &w)
  {
    data_[0] = x;
    data_[1] = y;
    data_[2] = z;
    data_[3] = w;
  }

  inline vecN &operator=(const vecN &that)
  {
    set(that.data_);
    return *this;
  }

  inline vecN &operator=(const T &that)
  {
    set(that);
    return *this;
  }

  template <typename U, const int m>
  inline vecN &operator=(const vecN<U, m> &that)
  {
    constexpr int d = 4 < m ? 4 : m;
    for (int i = 0; i < d; i++)
      data_[i] = static_cast<T>(that[i]);
    return *this;
  }

  inline vecN operator+(const vecN &that) const { return vecN(data_[0] + that[0], data_[1] + that[1], data_[2] + that[2], data_[3] + that[3]); }
  inline vecN &operator+=(const vecN &that) { return (*this = *this + that); }
  inline vecN operator-() const { return vecN(-data_[0], -data_[1], -data_[2], -data_[3]); }
  inline vecN operator-(const vecN &that) const { return vecN(data_[0] - that[0], data_[1] - that[1], data_[2] - that[2], data_[3] - that[3]); }
  inline vecN &operator-=(const vecN &that) { return (*this = *this - that); }
  inline vecN operator*(const vecN &that) const { return vecN(data_[0] * that[0], data_[1] * that[1], data_[2] * that[2], data_[3] * that[3]); }
  inline vecN &operator*=(const vecN &that) { return (*this = *this * that); }
  inline vecN operator*(const T &that) const { return vecN(data_[0] * that, data_[1] * that, data_[2] * that, data_[3] * that); }
  inline vecN &operator*=(const T &that)
  {
    data_[0] *= that;
    data_[1] *= that;
    data_[2] *= that;
    data_[3] *= that;
    return *this;
  }
  inline vecN operator/(const vecN &that) const { return vecN(data_[0] / that.data_[0], data_[1] / that.data_[1], data_[2] / that.data_[2], data_[3] / that.data_[3]); }
  inline vecN operator/(const T &that) const { return vecN(data_[0] / that, data_[1] / that, data_[2] / that, data_[3] / that); }
  inline vecN &operator/=(const T &that)
  {
    data_[0] /= that;
    data_[1] /= that;
    data_[2] /= that;
    data_[3] /= that;
    return *this;
  }
  inline vecN &operator/=(const vecN &that)
  {
    data_[0] /= that.data_[0];
    data_[1] /= that.data_[1];
    data_[2] /= that.data_[2];
    data_[3] /= that.data_[3];
    return *this;
  }

  inline operator vecN<T, 3>() { return vecN<T, 3>(data_[0], data_[1], data_[2]); }
  inline T &operator[](int i) { return data_[i]; }
  inline const T &operator[](int i) const { return data_[i]; }
  inline T *data() { return static_cast<T *>(data_); }
  inline const T *data() const { return static_cast<const T *>(data_); }
  inline static int size(void) { return 4; }
  inline T &x() { return data_[0]; }
  inline T &y() { return data_[1]; }
  inline T &z() { return data_[2]; }
  inline T &w() { return data_[3]; }
  inline const T &x() const { return data_[0]; }
  inline const T &y() const { return data_[1]; }
  inline const T &z() const { return data_[2]; }
  inline const T &w() const { return data_[3]; }

protected:
  T data_[4] = {};
};

template <typename T>
using Tvec2 = vecN<T, 2>;

template <typename T>
using Tvec3 = vecN<T, 3>;

template <typename T>
using Tvec4 = vecN<T, 4>;

using vec2 = Tvec2<float>;
using vec2d = Tvec2<double>;
using vec2i = Tvec2<int>;
using vec2u = Tvec2<unsigned int>;

using vec3 = Tvec3<float>;
using vec3d = Tvec3<double>;
using vec3b = Tvec3<char>;
using vec3ub = Tvec3<unsigned char>;
using vec3i = Tvec3<int>;
using vec3u = Tvec3<unsigned int>;

using vec4 = Tvec4<float>;
using vec4d = Tvec4<double>;
using vec4b = Tvec4<char>;
using vec4ub = Tvec4<unsigned char>;
using vec4i = Tvec4<int>;
using vec4u = Tvec4<unsigned int>;

template <typename T, int n>
static inline const vecN<T, n> operator*(T x, const vecN<T, n> &v)
{
  return v * x;
}

template <typename T>
static inline const Tvec2<T> operator/(T x, const Tvec2<T> &v)
{
  return Tvec2<T>(x / v[0], x / v[1]);
}

template <typename T>
static inline const Tvec3<T> operator/(T x, const Tvec3<T> &v)
{
  return Tvec3<T>(x / v[0], x / v[1], x / v[2]);
}

template <typename T>
static inline const Tvec4<T> operator/(T x, const Tvec4<T> &v)
{
  return Tvec4<T>(x / v[0], x / v[1], x / v[2], x / v[3]);
}

template <typename T, int n>
static inline T dot(const vecN<T, n> &a, const vecN<T, n> &b)
{
  T total(0);
  for (int i = 0; i < n; i++) {
    total += a[i] * b[i];
  }
  return total;
}

template <typename T>
static inline vecN<T, 3> cross(const vecN<T, 3> &a, const vecN<T, 3> &b)
{
  return Tvec3<T>(a[1] * b[2] - b[1] * a[2], a[2] * b[0] - b[2] * a[0], a[0] * b[1] - b[0] * a[1]);
}

// Quaternion///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class Tquat
{
public:
  inline Tquat() : s_(T(1)), v_(T(0)) {}

  inline Tquat(const Tquat &q) : s_(q.s_), v_(q.v_) {}

  inline Tquat(const Tvec4<T> &v) : s_(v[3]), v_(v[0], v[1], v[2]) {}

  inline Tquat(const Tvec3<T> &v, T s) : v_(v), s_(s) {}

  inline Tquat(T x, T y, T z, T w) : s_(w), v_(x, y, z) {}

  inline Tquat &operator=(const Tquat &q)
  {
    s_ = q.s_;
    v_ = q.v_;
    return *this;
  }

  inline T &operator[](int n) { return data_[n]; }

  inline const T &operator[](int n) const { return data_[n]; }

  inline Tquat operator+(const Tquat &q) const { return Tquat(v_ + q.v_, s_ + q.s_); }

  inline Tquat &operator+=(const Tquat &q)
  {
    s_ += q.s_;
    v_ += q.v_;
    return *this;
  }

  inline Tquat operator-(const Tquat &q) const { return Tquat(v_ - q.v_, s_ - q.s_); }

  inline Tquat &operator-=(const Tquat &q)
  {
    s_ -= q.s_;
    v_ -= q.v_;

    return *this;
  }

  inline Tquat operator-() const { return Tquat(-v_, -s_); }

  inline Tquat operator*(const T s) const { return Tquat(data_[0] * s, data_[1] * s, data_[2] * s, data_[3] * s); }

  inline Tquat &operator*=(const T s)
  {
    s_ *= s;
    v_ *= s;
    return *this;
  }

  inline Tquat operator*(const Tquat &q) const
  {
    const T &x1 = data_[0];
    const T &y1 = data_[1];
    const T &z1 = data_[2];
    const T &w1 = data_[3];
    const T &x2 = q.data_[0];
    const T &y2 = q.data_[1];
    const T &z2 = q.data_[2];
    const T &w2 = q.data_[3];

    return Tquat(w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2, w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2, 
      w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2, w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2);
  }

  inline Tvec3<T> operator*(const Tvec3<T> &v) const
  {
    Tvec3<T> uv = cross(v_, v);
    Tvec3<T> uuv = cross(v_, uv);
    uv *= (static_cast<T>(2) * s_);
    uuv *= static_cast<T>(2);
    return v + uv + uuv;
  }

  inline Tquat operator/(const T s) const { return Tquat(data_[0] / s, data_[1] / s, data_[2] / s, data_[3] / s); }

  inline Tquat &operator/=(const T t)
  {
    s_ /= t;
    v_ /= t;
    return *this;
  }

  inline operator Tvec4<T> &() { return *(Tvec4<T> *)data_; }

  inline operator const Tvec4<T> &() const { return *(const Tvec4<T> *)data_; }

  inline operator Tvec3<T> &() { return v_; }

  inline operator const Tvec3<T> &() const { return v_; }

  inline bool operator==(const Tquat &q) const { return (s_ == q.s_) && (v_ == q.v_); }

  inline bool operator!=(const Tquat &q) const { return (s_ != q.s_) || (v_ != q.v_); }

  inline Tquat<T> conjugate() const { return Tquat<T>(Tvec4<T>(-v_, s_)); }

private:
  union {
    struct
    {
      Tvec3<T> v_;
      T s_;
    };
    struct
    {
      T x_;
      T y_;
      T z_;
      T w_;
    };
    T data_[4];
  };
};

using quat = Tquat<float>;
using quati = Tquat<int>;
using quatd = Tquat<double>;

template <typename T>
static inline Tquat<T> operator*(T a, const Tquat<T> &b)
{
  return b * a;
}

template <typename T>
static inline Tquat<T> operator/(T a, const Tquat<T> &b)
{
  return Tquat<T>(a / b[0], a / b[1], a / b[2], a / b[3]);
}

template <typename T, int m, int n>
class matNM
{
public:
  using vecT = vecN<T, m>;  

  inline matNM() {}

  explicit inline matNM(T f)
  {
    for (int i = 0; i < n; i++) {
      data_[i] = f;
    }
  }

  template <typename U, const int s, const int t>
  matNM(const matNM<U, s, t> &that)
  {
    for (int i = 0; i < n; i++)
      data_[i] = {};
    constexpr int col = n < t ? n : t;
    for (int i = 0; i < col; i++)
      data_[i] = that[i];
  }

  inline matNM &operator=(const matNM &that)
  {
    for (int i = 0; i < n; i++)
      data_[i] = that[i];
    return *this;
  }

  template <typename U>
  matNM &operator=(const matNM<U, m, n> &that)
  {
    for (int i = 0; i < n; i++)
      data_[i] = that[i];
    return *this;
  }

  inline vecT &operator[](int i) { return data_[i]; }
  inline const vecT &operator[](int i) const { return data_[i]; }
  inline operator T *() { return &data_[0][0]; }
  inline operator const T *() const { return &data_[0][0]; }

  inline matNM operator+(const matNM &that) const
  {
    matNM result;
    for (int i = 0; i < n; i++)
      result.data_[i] = data_[i] + that.data_[i];
    return result;
  }

  inline matNM &operator+=(const matNM &that) { return (*this = *this + that); }

  inline matNM operator-(const matNM &that) const
  {
    matNM result;
    for (int i = 0; i < n; i++)
      result.data_[i] = data_[i] - that.data_[i];
    return result;
  }

  inline matNM &operator-=(const matNM &that) { return (*this = *this - that); }

  inline matNM operator*(const T &that) const
  {
    matNM result;
    for (int i = 0; i < n; i++)
      result.data_[i] = data_[i] * that;
    return result;
  }

  inline matNM &operator*=(const T &that)
  {
    for (int i = 0; i < n; i++)
      data_[i] = data_[i] * that;
    return *this;
  }

  inline matNM<T, n, m> transpose() const
  {
    matNM<T, n, m> result;
    for (int i = 0; i < n; i++)
      for (int j = 0; j < m; j++)
        result[j][i] = data_[i][j];
    return result;
  }

  inline void identity() requires (m == n)
  {
    for (int i = 0; i < n; i++) {
      data_[i] = {};
      data_[i][i] = 1;
    }
  }

  template <typename U>
  inline void set(const U *ele)
  {
    for (int i = 0; i < n; i++) {
      data_[i].set(ele + m * i);
    }
  }

  inline void set(T v)
  {
    for (int i = 0; i < n; i++)
      data_[i].set(v);
  }

protected:
  vecT data_[n] = {};
};

template <typename T>
class Tmat2 : public matNM<T, 2, 2>
{
public:
  using base = matNM<T, 2, 2>;
  using base::base;
  using base::operator=;
  using vecT = typename base::vecT;

  Tmat2() = default;
  Tmat2(const base &that) : base(that) {}
  Tmat2(const vecT &v0, const vecT &v1) : base()
  {
    (*this)[0] = v0;
    (*this)[1] = v1;
  }

  Tmat2 &operator=(const base &that)
  {
    base::operator=(that);
    return *this;
  }
};

template <typename T>
class Tmat3 : public matNM<T, 3, 3>
{
public:
  using base = matNM<T, 3, 3>;
  using base::base;
  using base::operator=;
  using vecT = typename base::vecT;

  Tmat3() = default;
  Tmat3(const base &that) : base(that) {}
  Tmat3(const vecT &v0, const vecT &v1, const vecT &v2) : base() { (*this)[0] = v0; (*this)[1] = v1; (*this)[2] = v2; }

  Tmat3(const Tquat<T> &quat) : base()
  {
    Tvec4<T> v(quat);
    const T xx = v.x() * v.x();
    const T yy = v.y() * v.y();
    const T zz = v.z() * v.z();
    const T xy = v.x() * v.y();
    const T xz = v.x() * v.z();
    const T xw = v.x() * v.w();
    const T yz = v.y() * v.z();
    const T yw = v.y() * v.w();
    const T zw = v.z() * v.w();

    (*this)[0][0] = T(1) - T(2) * (yy + zz);
    (*this)[0][1] = T(2) * (xy + zw);
    (*this)[0][2] = T(2) * (xz - yw);
    (*this)[1][0] = T(2) * (xy - zw);
    (*this)[1][1] = T(1) - T(2) * (xx + zz);
    (*this)[1][2] = T(2) * (yz + xw);
    (*this)[2][0] = T(2) * (xz + yw);
    (*this)[2][1] = T(2) * (yz - xw);
    (*this)[2][2] = T(1) - T(2) * (xx + yy);
  }

  Tmat3 &operator=(const base &that)
  {
    base::operator=(that);
    return *this;
  }
};

template <typename T>
class Tmat4 : public matNM<T, 4, 4>
{
public:
  using base = matNM<T, 4, 4>;
  using base::base;
  using base::operator=;
  using vecT = typename base::vecT;

  Tmat4() = default;
  Tmat4(const base &that) : base(that) {}
  Tmat4(const vecT &v0, const vecT &v1, const vecT &v2, const vecT &v3) : base() { (*this)[0] = v0; (*this)[1] = v1; (*this)[2] = v2; (*this)[3] = v3; }

  Tmat4(const matNM<T, 3, 3> &that)
    : base()
  {
    for (int i = 0; i < 3; i++)
      (*this)[i] = that[i];
    (*this)[3][3] = T(1);
  }

  Tmat4 &operator=(const base &that)
  {
    base::operator=(that);
    return *this;
  }
};

using mat2 = Tmat2<float>;
using mat3 = Tmat3<float>;
using imat3 = Tmat3<int>;
using dmat3 = Tmat3<double>;
using mat4 = Tmat4<float>;
using mat4i = Tmat4<int>;
using mat4u = Tmat4<unsigned int>;
using mat4d = Tmat4<double>;

}; // namespace tg

#endif /* __TVEC_INC__ */
