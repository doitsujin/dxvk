#pragma once

#include "util_vector.h"

namespace dxvk {

  class Matrix4 {

    public:

    // Identity
    inline Matrix4() {
      data[0] = { 1, 0, 0, 0 };
      data[1] = { 0, 1, 0, 0 };
      data[2] = { 0, 0, 1, 0 };
      data[3] = { 0, 0, 0, 1 };
    }

    // Produces a scalar matrix, x * Identity
    inline explicit Matrix4(float x) {
      data[0] = { x, 0, 0, 0 };
      data[1] = { 0, x, 0, 0 };
      data[2] = { 0, 0, x, 0 };
      data[3] = { 0, 0, 0, x };
    }

    inline Matrix4(
      const Vector4& v0,
      const Vector4& v1,
      const Vector4& v2,
      const Vector4& v3) {
      data[0] = v0;
      data[1] = v1;
      data[2] = v2;
      data[3] = v3;
    }

    inline Matrix4(const float matrix[4][4]) {
      data[0] = Vector4(matrix[0]);
      data[1] = Vector4(matrix[1]);
      data[2] = Vector4(matrix[2]);
      data[3] = Vector4(matrix[3]);
    }

    Matrix4(const Matrix4& other) = default;
    Matrix4& operator=(const Matrix4& other) = default;

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

  static_assert(sizeof(Matrix4) == sizeof(Vector4) * 4);

  inline Matrix4 operator*(float scalar, const Matrix4& m) { return m * scalar; }

  Matrix4 transpose(const Matrix4& m);

  float determinant(const Matrix4& m);

  Matrix4 inverse(const Matrix4& m);

  Matrix4 hadamardProduct(const Matrix4& a, const Matrix4& b);

  std::ostream& operator<<(std::ostream& os, const Matrix4& m);

}