#pragma once

#include "util_vector.h"

namespace dxvk {

  class Matrix4 {

    public:

    Matrix4(); // Identity

    explicit Matrix4(float x); // Produces a scalar matrix, x * Identity

    Matrix4(
      const Vector4& v0,
      const Vector4& v1,
      const Vector4& v2,
      const Vector4& v3);

    Matrix4(const Matrix4& other) = default;

    Vector4& operator[](size_t index);
    const Vector4& operator[](size_t index) const;

    bool operator==(const Matrix4& m2) const;
    bool operator!=(const Matrix4& m2) const;

    Matrix4 operator+(const Matrix4& other) const;
    Matrix4 operator-(const Matrix4& other) const;

    Matrix4 operator*(const Matrix4& m2) const;
    Vector4 operator*(const Vector4& v) const;
    Matrix4 operator*(float scalar) const;

    Matrix4 operator/(float scalar) const;

    Matrix4& operator+=(const Matrix4& other);
    Matrix4& operator-=(const Matrix4& other);

    Matrix4& operator*=(const Matrix4& other);

    Vector4 data[4];

  };

  inline Matrix4 operator*(float scalar, const Matrix4& m) { return m * scalar; }

  Matrix4 transpose(const Matrix4& m);

  float determinant(const Matrix4& m);

  Matrix4 inverse(const Matrix4& m);

  Matrix4 hadamardProduct(const Matrix4& a, const Matrix4& b);

  std::ostream& operator<<(std::ostream& os, const Matrix4& m);

}