#pragma once

#include "vecMath.h"

namespace P64::Coll {

  struct Matrix3x3 {
    float m[3][3]{};

    static Matrix3x3 identity() {
      Matrix3x3 r{};
      r.m[0][0] = r.m[1][1] = r.m[2][2] = 1.0f;
      return r;
    }
  };

  inline fm_vec3_t matrix3Vec3Mul(const Matrix3x3 &mat, const fm_vec3_t &v) {
    return fm_vec3_t{{
      mat.m[0][0] * v.x + mat.m[0][1] * v.y + mat.m[0][2] * v.z,
      mat.m[1][0] * v.x + mat.m[1][1] * v.y + mat.m[1][2] * v.z,
      mat.m[2][0] * v.x + mat.m[2][1] * v.y + mat.m[2][2] * v.z
    }};
  }

  inline Matrix3x3 matrix3Mul(const Matrix3x3 &a, const Matrix3x3 &b) {
    Matrix3x3 r{};
    for(int i = 0; i < 3; ++i) {
      for(int j = 0; j < 3; ++j) {
        r.m[i][j] = a.m[i][0] * b.m[0][j]
                   + a.m[i][1] * b.m[1][j]
                   + a.m[i][2] * b.m[2][j];
      }
    }
    return r;
  }

  inline Matrix3x3 matrix3Transpose(const Matrix3x3 &m) {
    Matrix3x3 r{};
    for(int i = 0; i < 3; ++i) {
      for(int j = 0; j < 3; ++j) {
        r.m[i][j] = m.m[j][i];
      }
    }
    return r;
  }

  inline Matrix3x3 quatToMatrix3(const fm_quat_t &q) {
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    Matrix3x3 r{};
    r.m[0][0] = 1.0f - 2.0f * (yy + zz);
    r.m[0][1] = 2.0f * (xy - wz);
    r.m[0][2] = 2.0f * (xz + wy);

    r.m[1][0] = 2.0f * (xy + wz);
    r.m[1][1] = 1.0f - 2.0f * (xx + zz);
    r.m[1][2] = 2.0f * (yz - wx);

    r.m[2][0] = 2.0f * (xz - wy);
    r.m[2][1] = 2.0f * (yz + wx);
    r.m[2][2] = 1.0f - 2.0f * (xx + yy);
    return r;
  }

  float matrix3Determinant(const Matrix3x3 &matrix);
  Matrix3x3 matrix3Inverse(const Matrix3x3 &matrix);
  inline Matrix3x3 diagonalMatrix(const fm_vec3_t &diag) {
    Matrix3x3 result{};
    result.m[0][0] = diag.x;
    result.m[1][1] = diag.y;
    result.m[2][2] = diag.z;
    return result;
  }

} // namespace P64::Coll