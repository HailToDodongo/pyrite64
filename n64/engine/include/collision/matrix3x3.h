#pragma once

#include "vec_math.h"

namespace P64::Coll {

  struct Matrix3x3 {
    float m[3][3]{};

    static Matrix3x3 identity() {
      Matrix3x3 r{};
      r.m[0][0] = r.m[1][1] = r.m[2][2] = 1.0f;
      return r;
    }
  };

  fm_vec3_t matrix3Vec3Mul(const Matrix3x3 &mat, const fm_vec3_t &v);
  float matrix3Determinant(const Matrix3x3 &matrix);
  Matrix3x3 matrix3Inverse(const Matrix3x3 &matrix);
  Matrix3x3 matrix3Mul(const Matrix3x3 &a, const Matrix3x3 &b);
  Matrix3x3 matrix3Transpose(const Matrix3x3 &m);
  Matrix3x3 quatToMatrix3(const fm_quat_t &q);
  inline Matrix3x3 diagonalMatrix(const fm_vec3_t &diag) {
    Matrix3x3 result{};
    result.m[0][0] = diag.x;
    result.m[1][1] = diag.y;
    result.m[2][2] = diag.z;
    return result;
  }

} // namespace P64::Coll