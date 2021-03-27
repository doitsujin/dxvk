#include "util_matrix.h"

namespace dxvk {

        Vector4& Matrix4::operator[](size_t index)       { return data[index]; }
  const Vector4& Matrix4::operator[](size_t index) const { return data[index]; }

  bool Matrix4::operator==(const Matrix4& m2) const {
    const Matrix4& m1 = *this;
    for (uint32_t i = 0; i < 4; i++) {
      if (m1[i] != m2[i])
        return false;
    }
    return true;
  }

  bool Matrix4::operator!=(const Matrix4& m2) const { return !operator==(m2); }

  Matrix4 Matrix4::operator+(const Matrix4& other) const {
    Matrix4 mat;
    for (uint32_t i = 0; i < 4; i++)
      mat[i] = data[i] + other.data[i];
    return mat;
  }

  Matrix4 Matrix4::operator-(const Matrix4& other) const {
    Matrix4 mat;
    for (uint32_t i = 0; i < 4; i++)
      mat[i] = data[i] - other.data[i];
    return mat;
  }

  Matrix4 Matrix4::operator*(const Matrix4& m2) const {
    const Matrix4& m1 = *this;

    const Vector4 srcA0 = { m1[0] };
    const Vector4 srcA1 = { m1[1] };
    const Vector4 srcA2 = { m1[2] };
    const Vector4 srcA3 = { m1[3] };

    const Vector4 srcB0 = { m2[0] };
    const Vector4 srcB1 = { m2[1] };
    const Vector4 srcB2 = { m2[2] };
    const Vector4 srcB3 = { m2[3] };

    Matrix4 result;
    result[0] = srcA0 * srcB0[0] + srcA1 * srcB0[1] + srcA2 * srcB0[2] + srcA3 * srcB0[3];
    result[1] = srcA0 * srcB1[0] + srcA1 * srcB1[1] + srcA2 * srcB1[2] + srcA3 * srcB1[3];
    result[2] = srcA0 * srcB2[0] + srcA1 * srcB2[1] + srcA2 * srcB2[2] + srcA3 * srcB2[3];
    result[3] = srcA0 * srcB3[0] + srcA1 * srcB3[1] + srcA2 * srcB3[2] + srcA3 * srcB3[3];
    return result;
  }

  Vector4 Matrix4::operator*(const Vector4& v) const {
    const Matrix4& m = *this;

    const Vector4 mul0 = { m[0] * v[0] };
    const Vector4 mul1 = { m[1] * v[1] };
    const Vector4 mul2 = { m[2] * v[2] };
    const Vector4 mul3 = { m[3] * v[3] };

    const Vector4 add0 = { mul0 + mul1 };
    const Vector4 add1 = { mul2 + mul3 };

    return add0 + add1;
  }

  Matrix4 Matrix4::operator*(float scalar) const {
    Matrix4 mat;
    for (uint32_t i = 0; i < 4; i++)
      mat[i] = data[i] * scalar;
    return mat;
  }

  Matrix4 Matrix4::operator/(float scalar) const {
    Matrix4 mat;
    for (uint32_t i = 0; i < 4; i++)
      mat[i] = data[i] / scalar;
    return mat;
  }

  Matrix4& Matrix4::operator+=(const Matrix4& other) {
    for (uint32_t i = 0; i < 4; i++)
      data[i] += other.data[i];
    return *this;
  }

  Matrix4& Matrix4::operator-=(const Matrix4& other) {
    for (uint32_t i = 0; i < 4; i++)
      data[i] -= other.data[i];
    return *this;
  }

  Matrix4& Matrix4::operator*=(const Matrix4& other) {
    return (*this = (*this) * other);
  }

  Matrix4 transpose(const Matrix4& m) {
    Matrix4 result;

    for (uint32_t i = 0; i < 4; i++) {
      for (uint32_t j = 0; j < 4; j++)
        result[i][j] = m.data[j][i];
    }
    return result;
  }

  float determinant(const Matrix4& m) {
    float coef00    =  m[2][2] * m[3][3] - m[3][2] * m[2][3];
    float coef02    =  m[1][2] * m[3][3] - m[3][2] * m[1][3];
    float coef03    =  m[1][2] * m[2][3] - m[2][2] * m[1][3];

    float coef04    =  m[2][1] * m[3][3] - m[3][1] * m[2][3];
    float coef06    =  m[1][1] * m[3][3] - m[3][1] * m[1][3];
    float coef07    =  m[1][1] * m[2][3] - m[2][1] * m[1][3];

    float coef08    =  m[2][1] * m[3][2] - m[3][1] * m[2][2];
    float coef10    =  m[1][1] * m[3][2] - m[3][1] * m[1][2];
    float coef11    =  m[1][1] * m[2][2] - m[2][1] * m[1][2];

    float coef12    =  m[2][0] * m[3][3] - m[3][0] * m[2][3];
    float coef14    =  m[1][0] * m[3][3] - m[3][0] * m[1][3];
    float coef15    =  m[1][0] * m[2][3] - m[2][0] * m[1][3];

    float coef16    =  m[2][0] * m[3][2] - m[3][0] * m[2][2];
    float coef18    =  m[1][0] * m[3][2] - m[3][0] * m[1][2];
    float coef19    =  m[1][0] * m[2][2] - m[2][0] * m[1][2];

    float coef20    =  m[2][0] * m[3][1] - m[3][0] * m[2][1];
    float coef22    =  m[1][0] * m[3][1] - m[3][0] * m[1][1];
    float coef23    =  m[1][0] * m[2][1] - m[2][0] * m[1][1];

    Vector4 fac0    = { coef00, coef00, coef02, coef03 };
    Vector4 fac1    = { coef04, coef04, coef06, coef07 };
    Vector4 fac2    = { coef08, coef08, coef10, coef11 };
    Vector4 fac3    = { coef12, coef12, coef14, coef15 };
    Vector4 fac4    = { coef16, coef16, coef18, coef19 };
    Vector4 fac5    = { coef20, coef20, coef22, coef23 };

    Vector4 vec0    = { m[1][0], m[0][0], m[0][0], m[0][0] };
    Vector4 vec1    = { m[1][1], m[0][1], m[0][1], m[0][1] };
    Vector4 vec2    = { m[1][2], m[0][2], m[0][2], m[0][2] };
    Vector4 vec3    = { m[1][3], m[0][3], m[0][3], m[0][3] };

    Vector4 inv0    = { vec1 * fac0 - vec2 * fac1 + vec3 * fac2 };
    Vector4 inv1    = { vec0 * fac0 - vec2 * fac3 + vec3 * fac4 };
    Vector4 inv2    = { vec0 * fac1 - vec1 * fac3 + vec3 * fac5 };
    Vector4 inv3    = { vec0 * fac2 - vec1 * fac4 + vec2 * fac5 };

    Vector4 signA   = { +1, -1, +1, -1 };
    Vector4 signB   = { -1, +1, -1, +1 };
    Matrix4 inverse = { inv0 * signA, inv1 * signB, inv2 * signA, inv3 * signB };

    Vector4 row0    = { inverse[0][0], inverse[1][0], inverse[2][0], inverse[3][0] };

    Vector4 dot0    = { m[0] * row0 };

    return (dot0.x + dot0.y) + (dot0.z + dot0.w);
  }

  Matrix4 inverse(const Matrix4& m)
  {
    float coef00    = m[2][2] * m[3][3] - m[3][2] * m[2][3];
    float coef02    = m[1][2] * m[3][3] - m[3][2] * m[1][3];
    float coef03    = m[1][2] * m[2][3] - m[2][2] * m[1][3];
    float coef04    = m[2][1] * m[3][3] - m[3][1] * m[2][3];
    float coef06    = m[1][1] * m[3][3] - m[3][1] * m[1][3];
    float coef07    = m[1][1] * m[2][3] - m[2][1] * m[1][3];
    float coef08    = m[2][1] * m[3][2] - m[3][1] * m[2][2];
    float coef10    = m[1][1] * m[3][2] - m[3][1] * m[1][2];
    float coef11    = m[1][1] * m[2][2] - m[2][1] * m[1][2];
    float coef12    = m[2][0] * m[3][3] - m[3][0] * m[2][3];
    float coef14    = m[1][0] * m[3][3] - m[3][0] * m[1][3];
    float coef15    = m[1][0] * m[2][3] - m[2][0] * m[1][3];
    float coef16    = m[2][0] * m[3][2] - m[3][0] * m[2][2];
    float coef18    = m[1][0] * m[3][2] - m[3][0] * m[1][2];
    float coef19    = m[1][0] * m[2][2] - m[2][0] * m[1][2];
    float coef20    = m[2][0] * m[3][1] - m[3][0] * m[2][1];
    float coef22    = m[1][0] * m[3][1] - m[3][0] * m[1][1];
    float coef23    = m[1][0] * m[2][1] - m[2][0] * m[1][1];
  
    Vector4 fac0    = { coef00, coef00, coef02, coef03 };
    Vector4 fac1    = { coef04, coef04, coef06, coef07 };
    Vector4 fac2    = { coef08, coef08, coef10, coef11 };
    Vector4 fac3    = { coef12, coef12, coef14, coef15 };
    Vector4 fac4    = { coef16, coef16, coef18, coef19 };
    Vector4 fac5    = { coef20, coef20, coef22, coef23 };
  
    Vector4 vec0    = { m[1][0], m[0][0], m[0][0], m[0][0] };
    Vector4 vec1    = { m[1][1], m[0][1], m[0][1], m[0][1] };
    Vector4 vec2    = { m[1][2], m[0][2], m[0][2], m[0][2] };
    Vector4 vec3    = { m[1][3], m[0][3], m[0][3], m[0][3] };
  
    Vector4 inv0    = { vec1 * fac0 - vec2 * fac1 + vec3 * fac2 };
    Vector4 inv1    = { vec0 * fac0 - vec2 * fac3 + vec3 * fac4 };
    Vector4 inv2    = { vec0 * fac1 - vec1 * fac3 + vec3 * fac5 };
    Vector4 inv3    = { vec0 * fac2 - vec1 * fac4 + vec2 * fac5 };
  
    Vector4 signA   = { +1, -1, +1, -1 };
    Vector4 signB   = { -1, +1, -1, +1 };
    Matrix4 inverse = { inv0 * signA, inv1 * signB, inv2 * signA, inv3 * signB };

    Vector4 row0    = { inverse[0][0], inverse[1][0], inverse[2][0], inverse[3][0] };

    Vector4 dot0    = { m[0] * row0 };
    float dot1      = (dot0.x + dot0.y) + (dot0.z + dot0.w);

    return inverse * (1.0f / dot1);
  }

  Matrix4 hadamardProduct(const Matrix4& a, const Matrix4& b) {
    Matrix4 result;

    for (uint32_t i = 0; i < 4; i++)
      result[i] = a[i] * b[i];

    return result;
  }

  std::ostream& operator<<(std::ostream& os, const Matrix4& m) {
    os << "Matrix4(";
    for (uint32_t i = 0; i < 4; i++) {
      os << "\n\t" << m[i];
      if (i < 3)
        os << ", ";
    }
    os << "\n)";

    return os;
  }

}