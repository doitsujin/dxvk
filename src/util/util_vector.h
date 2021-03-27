#pragma once

#include <iostream>

#include "util_bit.h"
#include "util_math.h"

namespace dxvk {

  template <typename T>
  struct Vector4Base {
    Vector4Base()
      : x{ }, y{ }, z{ }, w{ } { }

    Vector4Base(T splat)
      : x(splat), y(splat), z(splat), w(splat) { }

    Vector4Base(T x, T y, T z, T w)
      : x(x), y(y), z(z), w(w) { }

    Vector4Base(const T xyzw[4])
      : x(xyzw[0]), y(xyzw[1]), z(xyzw[2]), w(xyzw[3]) { }

    Vector4Base(const Vector4Base<T>& other) = default;

    inline       float& operator[](size_t index)       { return data[index]; }
    inline const float& operator[](size_t index) const { return data[index]; }

    bool operator==(const Vector4Base<T>& other) const {
      for (uint32_t i = 0; i < 4; i++) {
        if (data[i] != other.data[i])
        return false;
      }

      return true;
    }

    bool operator!=(const Vector4Base<T>& other) const {
      return !operator==(other);
    }

    Vector4Base operator-() const { return {-x, -y, -z, -w}; }

    Vector4Base operator+(const Vector4Base<T>& other) const {
      return {x + other.x, y + other.y, z + other.z, w + other.w};
    }

    Vector4Base operator-(const Vector4Base<T>& other) const {
      return {x - other.x, y - other.y, z - other.z, w - other.w};
    }

    Vector4Base operator*(T scalar) const {
      return {scalar * x, scalar * y, scalar * z, scalar * w};
    }

    Vector4Base operator*(const Vector4Base<T>& other) const {
      Vector4Base result;
      for (uint32_t i = 0; i < 4; i++)
        result[i] = data[i] * other.data[i];
      return result;
    }

    Vector4Base operator/(const Vector4Base<T>& other) const {
      Vector4Base result;
      for (uint32_t i = 0; i < 4; i++)
        result[i] = data[i] / other.data[i];
      return result;
    }

    Vector4Base operator/(T scalar) const {
      return {x / scalar, y / scalar, z / scalar, w / scalar};
    }

    Vector4Base& operator+=(const Vector4Base<T>& other) {
      x += other.x;
      y += other.y;
      z += other.z;
      w += other.w;

      return *this;
    }

    Vector4Base& operator-=(const Vector4Base<T>& other) {
      x -= other.x;
      y -= other.y;
      z -= other.z;
      w -= other.w;

      return *this;
    }

    Vector4Base& operator*=(T scalar) {
      x *= scalar;
      y *= scalar;
      z *= scalar;
      w *= scalar;

      return *this;
    }

    Vector4Base& operator/=(T scalar) {
      x /= scalar;
      y /= scalar;
      z /= scalar;
      w /= scalar;

      return *this;
    }

    union alignas(16) {
      T data[4];
      struct {
        T x, y, z, w;
      };
      struct {
        T r, g, b, a;
      };
    };

  };

  template <typename T>
  inline Vector4Base<T> operator*(T scalar, const Vector4Base<T>& vector) {
    return vector * scalar;
  }

  template <typename T>
  float dot(const Vector4Base<T>& a, const Vector4Base<T>& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  }

  template <typename T>
  T lengthSqr(const Vector4Base<T>& a) { return dot(a, a); }

  template <typename T>
  float length(const Vector4Base<T>& a) { return std::sqrt(float(lengthSqr(a))); }

  template <typename T>
  Vector4Base<T> normalize(const Vector4Base<T>& a) { return a * T(1.0f / length(a)); }

  template <typename T>
  std::ostream& operator<<(std::ostream& os, const Vector4Base<T>& v) {
    return os << "Vector4(" << v[0] << ", " << v[1] << ", " << v[2] << ", " << v[3] << ")";
  }

  using Vector4i = Vector4Base<int>;

  static_assert(sizeof(Vector4i) == sizeof(int)   * 4);

#ifdef __SSE2__
  struct Vector4 {
    inline Vector4() {
      const __m128 zero = _mm_setzero_ps();
      this->store(zero);
    }

    inline Vector4(float splat) {
      const __m128 value = _mm_set_ps1(splat);
      this->store(value);
    }

    inline Vector4(float x, float y, float z, float w)
      : x(x), y(y), z(z), w(w) { }

    inline Vector4(const float xyzw[4]) {
      const __m128 value = _mm_loadu_ps(xyzw);
      this->store(value);
    }

    inline Vector4(__m128 value) {
      this->store(value);
    }

    inline void store(__m128 value) {
      _mm_store_ps(this->data, value);
    }

    inline __m128 load() const {
      return _mm_load_ps(this->data);
    }

    inline Vector4(const Vector4& other) = default;

    inline       float& operator[](size_t index)       { return data[index]; }
    inline const float& operator[](size_t index) const { return data[index]; }

    inline bool operator==(const Vector4& other) const {
      const __m128  value     = this->load();
      const __m128  toCompare = other.load();
      const __m128  mask      = _mm_cmpeq_ps(value, toCompare);
      const __m128i imask     = _mm_castps_si128(mask);
      const int     reduced   = _mm_movemask_epi8(imask);
      return !reduced;
    }

    inline bool operator!=(const Vector4& other) const {
      return !operator==(other);
    }

    inline Vector4 operator-() const {
      const __m128 zero     = _mm_setzero_ps();
      const __m128 value    = this->load();
      const __m128 newValue = _mm_sub_ps(zero, value);
      return { newValue };
    }

    inline Vector4 operator+(const Vector4& o) const {
      const __m128 value  = this->load();
      const __m128 other  = o.load();
      const __m128 result = _mm_add_ps(value, other);
      return { result };
    }

    inline Vector4 operator-(const Vector4& o) const {
      const __m128 value  = this->load();
      const __m128 other  = o.load();
      const __m128 result = _mm_sub_ps(value, other);
      return { result };
    }

    inline Vector4 operator*(float scalar) const {
      const __m128 value  = this->load();
      const __m128 other  = _mm_set_ps1(scalar);
      const __m128 result = _mm_mul_ps(value, other);
      return { result };
    }

    inline Vector4 operator*(const Vector4& o) const {
      const __m128 value  = this->load();
      const __m128 other  = o.load();
      const __m128 result = _mm_mul_ps(value, other);
      return { result };
    }

    inline Vector4 operator/(float scalar) const {
      const __m128 value  = this->load();
      const __m128 other  = _mm_set_ps1(scalar);
      const __m128 result = _mm_div_ps(value, other);
      return { result };
    }

    inline Vector4 operator/(const Vector4& o) const {
      const __m128 value  = this->load();
      const __m128 other  = o.load();
      const __m128 result = _mm_div_ps(value, other);
      return { result };
    }

    inline Vector4& operator+=(const Vector4& o) {
      const __m128 value  = this->load();
      const __m128 other  = o.load();
      const __m128 result = _mm_add_ps(value, other);
      this->store(result);

      return *this;
    }

    inline Vector4& operator-=(const Vector4& o) {
      const __m128 value  = this->load();
      const __m128 other  = o.load();
      const __m128 result = _mm_sub_ps(value, other);
      this->store(result);

      return *this;
    }

    inline Vector4& operator*=(int scalar) {
      const __m128 value  = this->load();
      const __m128 other  = _mm_set_ps1(scalar);
      const __m128 result = _mm_mul_ps(value, other);
      this->store(result);

      return *this;
    }

    inline Vector4& operator/=(int scalar) {
      const __m128 value  = this->load();
      const __m128 other  = _mm_set_ps1(scalar);
      const __m128 result = _mm_div_ps(value, other);
      this->store(result);

      return *this;
    }


    union alignas(16) {
      float data[4];
      struct {
        float x, y, z, w;
      };
      struct {
        float r, g, b, a;
      };
    };
  };

  inline Vector4 operator*(float scalar, const Vector4& vector) {
    return vector * scalar;
  }

  inline float dot(const Vector4& a, const Vector4& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  }

  inline float lengthSqr(const Vector4& a) { return dot(a, a); }

  inline float length(const Vector4& a) { return std::sqrt(float(lengthSqr(a))); }

  inline Vector4 normalize(const Vector4& a) { return a * float(1.0f / length(a)); }

  inline std::ostream& operator<<(std::ostream& os, const Vector4& v) {
    return os << "Vector4(" << v[0] << ", " << v[1] << ", " << v[2] << ", " << v[3] << ")";
  }

  inline Vector4 replaceNaN(Vector4 a) {
    const __m128 value = a.load();
    const __m128 mask  = _mm_cmpeq_ps(value, value);
    const __m128 result = _mm_and_ps(value, mask);
    return { result };
  }
#else
  using Vector4 = Vector4Base<float>;

  inline Vector4 replaceNaN(Vector4 a) {
    Vector4 result;

    for (unsigned i = 0; i < 4; ++i) {
        result.data[i] = a.data[i] == a.data[i] ? a.data[i] : 0.0f;
    }

    return result;
  }
#endif

  static_assert(sizeof(Vector4)  == sizeof(float) * 4);
}