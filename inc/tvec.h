#ifndef __TVEC_INC__
#define __TVEC_INC__

#define NOMINMAX 1

#include <cmath>
#include <cstdint>

namespace tg {
// template <typename T, const int32_t w, const int32_t h> class matNM;
// template <typename T, const int32_t n> class vecN;
// template <typename T> class Tquat;

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFIENS
#define M_E 2.71828182845904523536        // e
#define M_LOG2E 1.44269504088896340736    // log2(e)
#define M_LOG10E 0.434294481903251827651  // log10(e)
#define M_LN2 0.693147180559945309417     // ln(2)
#define M_LN10 2.30258509299404568402     // ln(10)
#define M_PI 3.14159265358979323846       // pi
#define M_PI_2 1.57079632679489661923     // pi/2
#define M_PI_4 0.785398163397448309616    // pi/4
#define M_1_PI 0.318309886183790671538    // 1/pi
#define M_2_PI 0.636619772367581343076    // 2/pi
#define M_2_SQRTPI 1.12837916709551257390 // 2/sqrt(pi)
#define M_SQRT2 1.41421356237309504880    // sqrt(2)
#define M_SQRT1_2 0.707106781186547524401 // 1/sqrt(2)
#endif

template <typename T> struct teps {
  static constexpr T eps = 0;
};

template <> struct teps<float> {
  static constexpr float eps = 1e-6;
};

template <> struct teps<double> {
  static constexpr double eps = 1e-15;
};

template <typename T> inline T degrees(T angleInRadians) { return angleInRadians * static_cast<T>(180.0 / M_PI); }

template <typename T> inline T radians(T angleInDegrees) { return angleInDegrees * static_cast<T>(M_PI / 180.0); }

template <typename T, int32_t n> class vecN {
public:
  using ele_type = T;
  using this_type = vecN<T, n>;

  inline vecN() {}

  explicit inline vecN(const this_type &that) { assign(that); }

  explicit inline vecN(T s) { set(s); }

  template <typename U, int m> vecN(const vecN<U, m> &that)
  {
    constexpr int s = n < m ? n : m;
    for (int32_t i = 0; i < s; i++) {
      data_[i] = that[i];
    }
  }

  template <typename U> inline void set(U *ptr) { assign(ptr); }

  inline void set(T t)
  {
    for (int32_t i = 0; i < n; i++) {
      data_[i] = t;
    }
  }

  inline vecN<T, n> &operator=(const vecN &that)
  {
    assign(that);
    return *this;
  }

  inline vecN<T, n> &operator=(const T &that)
  {
    for (int32_t i = 0; i < n; i++) {
      data_[i] = that;
    }
    return *this;
  }

  template <typename U, const int32_t m> inline vecN<T, n> &operator=(const vecN<U, m> &that)
  {
    constexpr int32_t sz = n < m ? n : m;
    for (int32_t i = 0; i < sz; i++)
      data_[i] = that[i];
    return *this;
  }

  inline vecN operator+(const vecN &that) const
  {
    this_type result;
    for (int32_t i = 0; i < n; i++)
      result.data_[i] = data_[i] + that.data_[i];
    return result;
  }

  inline vecN &operator+=(const vecN &that) { return (*this = *this + that); }

  inline vecN operator-() const
  {
    this_type result;
    for (int32_t i = 0; i < n; i++)
      result.data_[i] = -data_[i];
    return result;
  }

  inline vecN operator-(const vecN &that) const
  {
    this_type result;
    for (int32_t i = 0; i < n; i++)
      result.data_[i] = data_[i] - that.data_[i];
    return result;
  }

  inline vecN &operator-=(const vecN &that) { return (*this = *this - that); }

  inline vecN operator*(const vecN &that) const
  {
    this_type result;
    for (int32_t i = 0; i < n; i++)
      result.data_[i] = data_[i] * that.data_[i];
    return result;
  }

  inline vecN &operator*=(const vecN &that) { return (*this = *this * that); }

  inline vecN operator*(const T &that) const
  {
    this_type result;
    for (int32_t i = 0; i < n; i++)
      result.data_[i] = data_[i] * that;
    return result;
  }

  inline vecN &operator*=(const T &that)
  {
    assign(*this * that);

    return *this;
  }

  inline vecN operator/(const vecN &that) const
  {
    this_type result;
    for (int32_t i = 0; i < n; i++)
      result.data_[i] = data_[i] / that.data_[i];
    return result;
  }

  inline vecN &operator/=(const vecN &that)
  {
    assign(*this / that);
    return *this;
  }

  inline vecN operator/(const T &that) const
  {
    this_type result;
    for (int32_t i = 0; i < n; i++)
      result.data_[i] = data_[i] / that;
    return result;
  }

  inline vecN &operator/=(const T &that)
  {
    assign(*this / that);
    return *this;
  }

  inline T &operator[](int32_t i) { return data_[i]; }
  inline const T &operator[](int32_t i) const { return data_[i]; }

  inline const T *data() const { return static_cast<const T *>(data_); }

  inline static int32_t size(void) { return n; }

protected:
  T data_[n] = {};

  inline void assign(const vecN &that) { memcpy(data_, that.data_, sizeof(T) * n); }

  template <typename U> inline void assign(U *ptr)
  {
    for (int32_t i = 0; i < n; i++) {
      data_[i] = ptr[i];
    }
  }
};

template <typename T, int32_t n> inline bool operator==(const vecN<T, n> &v1, const vecN<T, n> &v2)
{
  for (int32_t i = 0; i < n; i++) {
    if (fabs(v1[i] - v2[i]) > teps<T>::eps)
      return false;
  }
  return true;
}

template <typename T> class Tvec2 : public vecN<T, 2> {
public:
  typedef vecN<T, 2> base;
  typedef Tvec2<T> this_type;

  inline Tvec2() {}

  explicit inline Tvec2(const this_type &v)
  {
    base::data_[0] = v[0];
    base::data_[1] = v[1];
  }

  explicit inline Tvec2(const base &v)
    : base(v)
  {
  }

  inline Tvec2(T x, T y)
  {
    base::data_[0] = x;
    base::data_[1] = y;
  }

  template <typename U>
  inline Tvec2(const vecN<U, 2> &that)
    : base(that)
  {
  }

  template <typename U, int32_t n> inline this_type operator=(const vecN<U, n> &that)
  {
    base::operator=(that);
    return *this;
  }

  inline void operator=(const T &t)
  {
    base::data_[0] = t;
    base::data_[1] = t;
  }

  inline T &x() { return base::data_[0]; }
  inline T &y() { return base::data_[1]; }

  inline const T &x() const { return base::data_[0]; }
  inline const T &y() const { return base::data_[1]; }
};

template <typename T> class Tvec3 : public vecN<T, 3> {
public:
  using base = vecN<T, 3>;
  using this_type = Tvec3<T>;

  inline Tvec3()
    : base(0)
  {
  }

  explicit inline Tvec3(T t)
    : base(t)
  {
  }

  explicit inline Tvec3(const this_type &v)
    : base(v)
  {
  }

  inline Tvec3(const base &v)
    : base(v)
  {
  }

  inline Tvec3(T x, T y, T z)
    : base()
  {
    base::data_[0] = x;
    base::data_[1] = y;
    base::data_[2] = z;
  }

  inline Tvec3(const Tvec2<T> &v, T z)
    : base()
  {
    base::data_[0] = v[0];
    base::data_[1] = v[1];
    base::data_[2] = z;
  }

  inline Tvec3(T x, const Tvec2<T> &v)
    : base()
  {
    base::data_[0] = x;
    base::data_[1] = v[0];
    base::data_[2] = v[1];
  }

  inline Tvec3(const vecN<T, 4> &v)
    : base()
  {
    base::data_[0] = v[0];
    base::data_[1] = v[1];
    base::data_[2] = v[2];
  }

  inline this_type operator=(const T &t)
  {
    base::data_[0] = t;
    base::data_[1] = t;
    base::data_[2] = t;
    return *this;
  }

  template <typename U> inline Tvec3(const U *ptr) { base::assign(ptr); }

  template <typename U>
  inline Tvec3(const vecN<U, 3> &that)
    : base(that)
  {
  }

  template <typename U, int32_t n> inline this_type operator=(vecN<U, n> vec)
  {
    base::operator=(vec);
    return *this;
  }

  inline T &x() { return base::data_[0]; }
  inline T &y() { return base::data_[1]; }
  inline T &z() { return base::data_[2]; }

  inline const T &x() const { return base::data_[0]; }
  inline const T &y() const { return base::data_[1]; }
  inline const T &z() const { return base::data_[2]; }

  inline void set(const T &x, const T &y, const T &z)
  {
    base::data_[0] = x;
    base::data_[1] = y;
    base::data_[2] = z;
  }
};

template <typename T> class Tvec4 : public vecN<T, 4> {
public:
  typedef vecN<T, 4> base;
  typedef Tvec4<T> this_type;

  inline Tvec4() {}

  explicit inline Tvec4(const this_type &v)
  {
    base::data_[0] = v[0];
    base::data_[1] = v[1];
    base::data_[2] = v[2];
    base::data_[3] = v[3];
  }

  template <typename U>
  inline Tvec4(const Tvec4<U> &that)
    : base(that)
  {
  }

  template <typename U> inline Tvec4(const U *ptr) { assign(ptr); }

  inline Tvec4(T x, T y, T z, T w)
  {
    base::data_[0] = x;
    base::data_[1] = y;
    base::data_[2] = z;
    base::data_[3] = w;
  }

  inline Tvec4(const Tvec2<T> &v, T z, T w)
  {
    base::data_[0] = v[0];
    base::data_[1] = v[1];
    base::data_[2] = z;
    base::data_[3] = w;
  }

  inline Tvec4(T x, const Tvec2<T> &v, T w)
  {
    base::data_[0] = x;
    base::data_[1] = v[0];
    base::data_[2] = v[1];
    base::data_[3] = w;
  }

  inline Tvec4(T x, T y, const Tvec2<T> &v)
  {
    base::data_[0] = x;
    base::data_[1] = y;
    base::data_[2] = v[0];
    base::data_[3] = v[1];
  }

  inline Tvec4(const Tvec2<T> &u, const Tvec2<T> &v)
  {
    base::data_[0] = u[0];
    base::data_[1] = u[1];
    base::data_[2] = v[0];
    base::data_[3] = v[1];
  }

  inline Tvec4(const Tvec3<T> &v, T w)
  {
    base::data_[0] = v[0];
    base::data_[1] = v[1];
    base::data_[2] = v[2];
    base::data_[3] = w;
  }

  inline Tvec4(T x, const Tvec3<T> &v)
  {
    base::data_[0] = x;
    base::data_[1] = v[0];
    base::data_[2] = v[1];
    base::data_[3] = v[2];
  }

  explicit inline Tvec4(const Tvec3<T> &v)
  {
    base::data_[0] = v[0];
    base::data_[1] = v[1];
    base::data_[2] = v[2];
    base::data_[3] = 0;
  }

  template <typename U>
  inline Tvec4(const vecN<U, 4> &that)
    : base(that)
  {
  }

  template <typename U, int32_t n> inline this_type operator=(vecN<U, n> vec)
  {
    base::operator=(vec);
    return *this;
  }

  inline this_type &operator=(const this_type &v)
  {
    base::data_[0] = v[0];
    base::data_[1] = v[1];
    base::data_[2] = v[2];
    base::data_[3] = v[3];
    return *this;
  }

  inline void operator=(const T &t)
  {
    base::data_[0] = t;
    base::data_[1] = t;
    base::data_[2] = t;
    base::data_[3] = t;
  }

  inline operator Tvec3<T>() { return Tvec3<T>(base::data_[0], base::data_[1], base::data_[2]); }

  inline T &x() { return base::data_[0]; }
  inline T &y() { return base::data_[1]; }
  inline T &z() { return base::data_[2]; }
  inline T &w() { return base::data_[3]; }

  inline const T &x() const { return base::data_[0]; }
  inline const T &y() const { return base::data_[1]; }
  inline const T &z() const { return base::data_[2]; }
  inline const T &w() const { return base::data_[3]; }

  inline void set(const T &x, const T &y, const T &z, const T &w)
  {
    base::data_[0] = x;
    base::data_[1] = y;
    base::data_[2] = z;
    base::data_[3] = w;
  }
};

typedef vecN<float, 1> vec1;
typedef vecN<int32_t, 1> vec1i;
typedef vecN<uint32_t, 1> vec1u;
typedef vecN<double, 1> vec1d;

typedef Tvec2<float> vec2;
typedef Tvec2<double> vec2d;
typedef Tvec2<int32_t> vec2i;
typedef Tvec2<uint32_t> vec2u;

typedef Tvec3<float> vec3;
typedef Tvec3<double> vec3d;
typedef Tvec3<int8_t> vec3b;
typedef Tvec3<uint8_t> vec3ub;
typedef Tvec3<int32_t> vec3i;
typedef Tvec3<uint32_t> vec3u;
typedef Tvec3<int16_t> vec3s;
typedef Tvec3<uint16_t> vec3us;

typedef Tvec4<float> vec4;
typedef Tvec4<double> vec4d;
typedef Tvec4<int8_t> vec4b;
typedef Tvec4<uint8_t> vec4ub;
typedef Tvec4<int32_t> vec4i;
typedef Tvec4<uint32_t> vec4u;

template <typename T, int32_t n> static inline const vecN<T, n> operator*(T x, const vecN<T, n> &v) { return v * x; }

template <typename T> static inline const Tvec2<T> operator/(T x, const Tvec2<T> &v)
{
  return Tvec2<T>(x / v[0], x / v[1]);
}

template <typename T> static inline const Tvec3<T> operator/(T x, const Tvec3<T> &v)
{
  return Tvec3<T>(x / v[0], x / v[1], x / v[2]);
}

template <typename T> static inline const Tvec4<T> operator/(T x, const Tvec4<T> &v)
{
  return Tvec4<T>(x / v[0], x / v[1], x / v[2], x / v[3]);
}

template <typename T, int32_t n> static inline T dot(const vecN<T, n> &a, const vecN<T, n> &b)
{
  T total(0);
  for (int32_t i = 0; i < n; i++) {
    total += a[i] * b[i];
  }
  return total;
}

template <typename T> static inline vecN<T, 3> cross(const vecN<T, 3> &a, const vecN<T, 3> &b)
{
  return Tvec3<T>(a[1] * b[2] - b[1] * a[2], a[2] * b[0] - b[2] * a[0], a[0] * b[1] - b[0] * a[1]);
}

template <typename T, int32_t n> static inline T pow(const vecN<T, n> &v, int32_t num)
{
  T result(0);
  for (int32_t i = 0; i < n; i++) {
    T res = v[i];
    for (int32_t j = 1; j < num; j++)
      res *= v[i];
    result += res;
  }
  return result;
}

template <typename T, int32_t n> static inline T length(const vecN<T, n> &v)
{
  double result = 0;
  for (int32_t i = 0; i < n; ++i) {
    const T &t = v[i];
    result += t * t;
  }
  return (T)sqrt(result);
}

template <typename T, int32_t n> static inline T square(const vecN<T, n> &v)
{
  T result(0);
  for (int32_t i = 0; i < n; ++i) {
    const T &t = v[i];
    result += t * t;
  }
  return result;
}

template <typename T, int32_t n> static inline vecN<T, n> normalize(const vecN<T, n> &v) { return v / length(v); }

template <typename T, int32_t n> static inline T distance(const vecN<T, n> &a, const vecN<T, n> &b)
{
  return length(b - a);
}

template <typename T, int32_t n> static inline T angle(const vecN<T, n> &a, const vecN<T, n> &b)
{
  return arccos(dot(a, b));
}

template <typename T, int32_t n> static inline vecN<T, n> abs(const vecN<T, n> &a)
{
  vecN<T, n> result;
  for (int32_t i = 0; i < n; i++) {
    result[i] = fabs(a[i]);
  }
  return result;
}

// Quaternion///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T> class Tquat {
  template <typename T> friend class Tmat3;

public:
  inline Tquat() {}

  inline Tquat(const Tquat &q)
    : s_(q.s_)
    , v_(q.v_)
  {
  }

  inline Tquat(T s)
    : s_(s)
    , v_(T(0))
  {
  }

  inline Tquat(T s, const Tvec3<T> &v)
    : s_(s)
    , v_(v)
  {
  }

  inline Tquat(const Tvec4<T> &v)
    : s_(v[3])
    , v_(v[0], v[1], v[2])
  {
  }

  inline Tquat(T w, T x, T y, T z)
    : s_(w)
    , v_(x, y, z)
  {
  }

  inline explicit Tquat(const Tvec3<T> &v)
    : v_(v)
    , s_(0)
  {
  }

  inline T &operator[](int32_t n) { return data_[n]; }

  inline const T &operator[](int32_t n) const { return data_[n]; }

  inline Tquat operator+(const Tquat &q) const { return quat(s_ + q.s_, v_ + q.v_); }

  inline Tquat &operator+=(const Tquat &q)
  {
    s_ += q.s_;
    v_ += q.v_;
    return *this;
  }

  inline Tquat operator-(const Tquat &q) const { return quat(s_ - q.s_, v_ - q.v_); }

  inline Tquat &operator-=(const Tquat &q)
  {
    s_ -= q.s_;
    v_ -= q.v_;

    return *this;
  }

  inline Tquat operator-() const { return Tquat(-s_, -v_); }

  inline Tquat operator*(const T s) const { return Tquat(data_[0] * s, data_[1] * s, data_[2] * s, data_[3] * s); }

  inline Tquat &operator*=(const T s)
  {
    s_ *= s;
    v_ *= s;
    return *this;
  }

  inline Tquat operator*(const Tquat &q) const
  {
    const T &w1 = data_[3];
    const T &x1 = data_[0];
    const T &y1 = data_[1];
    const T &z1 = data_[2];
    const T &w2 = q.data_[3];
    const T &x2 = q.data_[0];
    const T &y2 = q.data_[1];
    const T &z2 = q.data_[2];

    return Tquat(w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2, w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
                 w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2, w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2);
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

  static Tquat<T> rotate(const T &rad, const T &x, const T &y, const T &z) { return rotate(rad, Tvec3<T>(x, y, z)); }

  static Tquat<T> rotate(const T &rad, const Tvec3<T> &axis)
  {
    return Tquat<T>(cos(rad / T(2.0)), normalize(axis) * sin(rad / T(2.0)));
  }

  inline Tquat<T> conjugate() const { return Tquat<T>(s_, -v_); }

private:
  union {
    struct {
      Tvec3<T> v_;
      T s_;
    };
    struct {
      T x_;
      T y_;
      T z_;
      T w_;
    };
    T data_[4];
  };
};

typedef Tquat<float> quat;
typedef Tquat<int32_t> quati;
typedef Tquat<uint32_t> quatu;
typedef Tquat<double> quatd;

template <typename T> static inline Tquat<T> operator*(T a, const Tquat<T> &b) { return b * a; }

template <typename T> static inline Tquat<T> operator/(T a, const Tquat<T> &b)
{
  return Tquat<T>(a / b[0], a / b[1], a / b[2], a / b[3]);
}

template <typename T> static inline Tquat<T> normalize(const Tquat<T> &q) { return q / length(vecN<T, 4>(q)); }

template <typename T, const int32_t n, const int32_t m> class matNM {
public:
  typedef class matNM<T, n, m> this_type;
  typedef class vecN<T, n> vector_type;

  inline matNM() {}

  // Copy constructor
  inline matNM(const matNM &that) { assign(that); }

  explicit inline matNM(T f)
  {
    for (int32_t i = 0; i < m; i++) {
      data_[i] = f;
    }
  }

  template <typename U> matNM(const matNM<U, n, m> &that)
  {
    for (int32_t i = 0; i < m; i++) {
      data_[i] = that[i];
    }
  }

  template <const int32_t u, const int32_t v> matNM(const matNM<T, u, v> &that)
  {
    constexpr int32_t col = m < m ? m : m;
    constexpr int32_t row = n < n ? n : n;
    for (int32_t i = 0; i < col; i++)
      for (int32_t j = 0; j < row; j++)
        data_[i][j] = that[i][j];
  }

  explicit inline matNM(const vector_type &v)
  {
    for (int32_t i = 0; i < m; i++) {
      data_[i] = v;
    }
  }

  inline matNM &operator=(const this_type &that)
  {
    assign(that);
    return *this;
  }

  template <typename U> matNM &operator=(const matNM<U, n, m> &that)
  {
    for (int32_t i = 0; i < m; i++) {
      data_[i] = that[i];
    }
    return *this;
  }

  inline matNM operator+(const this_type &that) const
  {
    this_type result;
    for (int32_t i = 0; i < m; i++)
      result.data_[i] = data_[i] + that.data_[i];
    return result;
  }

  inline this_type &operator+=(const this_type &that) { return (*this = *this + that); }

  inline this_type operator-(const this_type &that) const
  {
    this_type result;
    for (int32_t i = 0; i < m; i++)
      result.data_[i] = data_[i] - that.data_[i];
    return result;
  }

  inline this_type &operator-=(const this_type &that) { return (*this = *this - that); }

  inline this_type operator*(const T &that) const
  {
    this_type result;
    for (int32_t i = 0; i < m; i++)
      result.data_[i] = data_[i] * that;
    return result;
  }

  inline this_type &operator*=(const T &that)
  {
    for (int32_t i = 0; i < m; i++)
      data_[i] = data_[i] * that;
    return *this;
  }

  template <int32_t u> inline matNM<T, n, u> operator*(const matNM<T, m, u> &that) const
  {
    matNM<T, n, u> result(T(0));
    for (int32_t i = 0; i < n; i++) {
      for (int32_t j = 0; j < u; j++) {
        T sum(0);
        const vecN<T, m> &v = that.data_[j];
        for (int32_t k = 0; k < m; k++) {
          sum += data_[k][i] * v[k];
        }
        result[j][i] = sum;
      }
    }
    return result;
  }

  inline this_type &operator*=(const this_type &that) { return (*this = *this * that); }

  inline vector_type &operator[](int32_t n) { return data_[n]; }
  inline const vector_type &operator[](int32_t n) const { return data_[n]; }
  inline operator T *() { return static_cast<T *>(data_); }
  inline operator const T *() const { return static_cast<T *>(data_); }

  inline matNM<T, m, n> transpose() const
  {
    matNM<T, m, n> result;
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < m; j++) {
        result.data_[i][j] = data_[j][i];
      }
    }
    return result;
  }

  inline void identity()
  {
    memset(data_, 0, sizeof(this_type));
    constexpr int32_t u = m < n ? m : n;
    for (int32_t i = 0; i < u; i++) {
      data_[i][i] = 1;
    }
  }

  static inline int32_t width(void) { return m; }
  static inline int32_t height(void) { return n; }

  template <typename U> inline void set(const U *ele)
  {
    std::size_t off = 0;
    for (int32_t i = 0; i < m; i++) {
      data_[i].assign(ele + off);
      off += sizeof(U) * n;
    }
  }

  inline void set(const T *ele) { memcpy(data_, ele, sizeof(T) * m * n); }

  inline void set(T v)
  {
    for (int32_t i = 0; i < m; i++)
      data_[i].set(v);
  }

  inline bool reduce()
  {
    bool res = true;
    vecN<T, m> arr[n];
    vecN<T, m> *rarr[n];
    for (int32_t i = 0; i < n; i++) {
      for (int32_t j = 0; j < m; j++) {
        arr[i][j] = data_[j][i];
      }
      rarr[i] = (arr + i);
    }
    for (int32_t i = 0; i < n; i++) {
      for (;;) {
        vecN<T, m> &carr = *rarr[i];
        for (int32_t k = 0; k < i; k++) {
          if (carr[k]) {
            carr /= carr[k];
            carr -= *rarr[k];
          }
        }
        if (carr[i] == 0) {
          int32_t j = i + 1;
          for (; j < n; j++) {
            if (rarr[j][i]) {
              auto tmp = rarr[i];
              rarr[i] = rarr[j];
              rarr[j] = tmp;
              break;
            }
          }
          if (j == n) {
            res = false;
            break;
          }
        } else {
          carr /= carr[i];
          break;
        }
      }
    }
    return res;
  }

protected:
  vecN<T, n> data_[m] = {};

  inline void assign(const matNM &that)
  {
    for (int32_t i = 0; i < m; i++)
      data_[i] = that.data_[i];
  }
};

template <typename T> class Tmat2 : public matNM<T, 2, 2> {
public:
  typedef matNM<T, 2, 2> base;
  typedef Tmat2<T> this_type;

  inline Tmat2() {}
  inline Tmat2(const this_type &that)
    : base(that)
  {
  }
  inline Tmat2(const base &that)
    : base(that)
  {
  }
  inline Tmat2(const vecN<T, 2> &v)
    : base(v)
  {
  }
  inline Tmat2(const vecN<T, 2> &v0, const vecN<T, 2> &v1)
  {
    base::data[0] = v0;
    base::data[1] = v1;
  }

  template <typename U>
  Tmat2(const matNM<U, 2, 2> &that)
    : base(that)
  {
  }

  template <typename U> this_type &operator=(const matNM<U, 2, 2> &that)
  {
    base::operator=(that);
    return *this;
  }
};
typedef Tmat2<float> mat2;

template <typename T> class Tmat3 : public matNM<T, 3, 3> {
public:
  typedef matNM<T, 3, 3> base;
  typedef Tmat3<T> this_type;

  inline Tmat3() {}
  inline Tmat3(const this_type &that)
    : base(that)
  {
  }
  inline Tmat3(const vecN<T, 3> &v)
    : base(v)
  {
  }
  inline Tmat3(const vecN<T, 3> &v0, const vecN<T, 3> &v1, const vecN<T, 3> &v2)
  {
    base::data[0] = v0;
    base::data[1] = v1;
    base::data[2] = v2;
  }

  Tmat3(const Tquat<T> &quat)
  {
    Tvec4<T> v(quat);
    // const T ww = v.w() * v.w();
    const T xx = v.x() * v.x();
    const T yy = v.y() * v.y();
    const T zz = v.z() * v.z();
    const T xy = v.x() * v.y();
    const T xz = v.x() * v.z();
    const T xw = v.x() * v.w();
    const T yz = v.y() * v.z();
    const T yw = v.y() * v.w();
    const T zw = v.z() * v.w();

    auto &m = base::data_;

    m[0][0] = T(1) - T(2) * (yy + zz);
    m[0][1] = T(2) * (xy + zw);
    m[0][2] = T(2) * (xz - yw);

    m[1][0] = T(2) * (xy - zw);
    m[1][1] = T(1) - T(2) * (xx + zz);
    m[1][2] = T(2) * (yz + xw);

    m[2][0] = T(2) * (xz + yw);
    m[2][1] = T(2) * (yz - xw);
    m[2][2] = T(1) - T(2) * (xx + yy);
  }

  template <typename U>
  Tmat3(const matNM<U, 3, 3> &that)
    : base(that)
  {
  }

  template <int32_t u, int32_t v>
  Tmat3(const matNM<T, u, v> &that)
    : base(that)
  {
  }

  template <typename U> this_type &operator=(const matNM<U, 3, 3> &that)
  {
    base::operator=(that);
    return *this;
  }
};
typedef Tmat3<float> mat3;
typedef Tmat3<int32_t> imat3;
typedef Tmat3<uint32_t> umat3;
typedef Tmat3<double> dmat3;

template <typename T> class Tmat4 : public matNM<T, 4, 4> {
public:
  typedef matNM<T, 4, 4> base;
  typedef Tmat4<T> this_type;

  inline Tmat4() {}
  inline Tmat4(const this_type &that)
    : base(that)
  {
  }
  explicit inline Tmat4(const vecN<T, 4> &v)
    : base(v)
  {
  }
  inline Tmat4(const vecN<T, 4> &v0, const vecN<T, 4> &v1, const vecN<T, 4> &v2, const vecN<T, 4> &v3)
  {
    base::data_[0] = v0;
    base::data_[1] = v1;
    base::data_[2] = v2;
    base::data_[3] = v3;
  }

  inline Tmat4(const Tmat3<T> &that)
    : base(that)
  {
    base::data_[0][3] = T(0);
    base::data_[1][3] = T(0);
    base::data_[2][3] = T(0);

    base::data_[3][0] = T(0);
    base::data_[3][1] = T(0);
    base::data_[3][2] = T(0);
    base::data_[3][3] = T(1);
  }

  template <typename U>
  Tmat4(const matNM<U, 4, 4> &that)
    : base(that)
  {
  }

  template <typename U> this_type &operator=(const matNM<U, 4, 4> &that)
  {
    base::operator=(that);
    return *this;
  }
};

typedef Tmat4<float> mat4;
typedef Tmat4<int32_t> mat4i;
typedef Tmat4<uint32_t> mat4u;
typedef Tmat4<double> mat4d;

template <typename T, const int32_t w, const int32_t h> Tquat<T> matquat(const matNM<T, w, h> &m)
{
  Tquat<T> q;
  T tq[4];
  tq[0] = 1 + m[0][0] + m[1][1] + m[2][2];
  tq[1] = 1 + m[0][0] - m[1][1] - m[2][2];
  tq[2] = 1 - m[0][0] + m[1][1] - m[2][2];
  tq[3] = 1 - m[0][0] - m[1][1] + m[2][2];
  int32_t i = 0, j = 0;
  for (i = 1; i < 4; i++)
    j = (tq[i] > tq[j]) ? i : j;
  if (j == 0) {
    q.w_ = tq[0];
    q.x_ = m[1][2] - m[2][1];
    q.y_ = m[2][0] - m[0][2];
    q.z_ = m[0][1] - m[1][0];
  } else if (j == 1) {
    q.w_ = m[1][2] - m[2][1];
    q.x_ = tq[1];
    q.y_ = m[0][1] + m[1][0];
    q.z_ = m[2][0] + m[0][2];
  } else if (j == 2) {
    q.w_ = m[2][0] - m[0][2];
    q.x_ = m[0][1] + m[1][0];
    q.y_ = tq[2];
    q.z_ = m[1][2] + m[2][1];
  } else /* if (j==3) */
  {
    q.w_ = m[0][1] - m[1][0];
    q.x_ = m[2][0] + m[0][2];
    q.y_ = m[1][2] + m[2][1];
    q.z_ = tq[3];
  }

  T s = sqrt(0.25 / tq[j]);
  q.w_ *= s;
  q.x_ *= s;
  q.y_ *= s;
  q.z_ *= s;
  return q;
}

}; // namespace tg

#endif /* __TVEC_INC__ */
