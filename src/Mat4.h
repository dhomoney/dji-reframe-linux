// Minimal column-major matrix math matching glm's right-handed, depth -1..1
// conventions. The original plugin was built against glm, so the constructions
// here reproduce glm_perspective/frustum/lookAt/rotate/inverse exactly; the
// recovered kernels consume the result as 16 floats in column-major order.
#pragma once

#include <cmath>

namespace DJIReframe {

struct Vec3 {
    float x, y, z;
};

inline Vec3 vec3(float x, float y, float z) { return Vec3{x, y, z}; }

inline Vec3 normalize(const Vec3& v)
{
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len <= 0.0f) return Vec3{0.0f, 0.0f, 0.0f};
    const float inv = 1.0f / len;
    return Vec3{v.x * inv, v.y * inv, v.z * inv};
}

inline Vec3 cross(const Vec3& a, const Vec3& b)
{
    return Vec3{a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
}

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Column-major: m[col * 4 + row], i.e. m[c][r] in glm terms.
struct Mat4 {
    float m[16];

    float& at(int c, int r) { return m[c * 4 + r]; }
    float at(int c, int r) const { return m[c * 4 + r]; }
};

inline Mat4 mat4Zero()
{
    Mat4 r;
    for (int i = 0; i < 16; ++i) r.m[i] = 0.0f;
    return r;
}

inline Mat4 mat4Identity()
{
    Mat4 r = mat4Zero();
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

inline Mat4 operator*(const Mat4& a, const Mat4& b)
{
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a.at(k, row) * b.at(c, k);
            r.at(c, row) = s;
        }
    }
    return r;
}

// glm::perspective(fovy, aspect, zNear, zFar)
inline Mat4 perspective(float fovy, float aspect, float zNear, float zFar)
{
    const float t = std::tan(fovy * 0.5f);
    Mat4 r = mat4Zero();
    r.at(0, 0) = 1.0f / (aspect * t);
    r.at(1, 1) = 1.0f / t;
    r.at(2, 2) = -(zFar + zNear) / (zFar - zNear);
    r.at(2, 3) = -1.0f;
    r.at(3, 2) = -(2.0f * zFar * zNear) / (zFar - zNear);
    return r;
}

// glm::frustum(left, right, bottom, top, zNear, zFar)
inline Mat4 frustum(float left, float right, float bottom, float top, float zNear, float zFar)
{
    Mat4 r = mat4Zero();
    r.at(0, 0) = (2.0f * zNear) / (right - left);
    r.at(1, 1) = (2.0f * zNear) / (top - bottom);
    r.at(2, 0) = (right + left) / (right - left);
    r.at(2, 1) = (top + bottom) / (top - bottom);
    r.at(2, 2) = -(zFar + zNear) / (zFar - zNear);
    r.at(2, 3) = -1.0f;
    r.at(3, 2) = -(2.0f * zFar * zNear) / (zFar - zNear);
    return r;
}

// glm::lookAt(eye, center, up)
inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up)
{
    const Vec3 f = normalize(vec3(center.x - eye.x, center.y - eye.y, center.z - eye.z));
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);

    Mat4 r = mat4Identity();
    r.at(0, 0) = s.x;  r.at(1, 0) = s.y;  r.at(2, 0) = s.z;
    r.at(0, 1) = u.x;  r.at(1, 1) = u.y;  r.at(2, 1) = u.z;
    r.at(0, 2) = -f.x; r.at(1, 2) = -f.y; r.at(2, 2) = -f.z;
    r.at(3, 0) = -dot(s, eye);
    r.at(3, 1) = -dot(u, eye);
    r.at(3, 2) = dot(f, eye);
    return r;
}

// glm::rotate(m, angle, axis) -- post-multiplies m by the rotation.
inline Mat4 rotate(const Mat4& src, float angle, const Vec3& axisIn)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const Vec3 a = normalize(axisIn);
    const Vec3 t = vec3((1.0f - c) * a.x, (1.0f - c) * a.y, (1.0f - c) * a.z);

    float rot[9];
    rot[0] = c + t.x * a.x;
    rot[1] = t.x * a.y + s * a.z;
    rot[2] = t.x * a.z - s * a.y;
    rot[3] = t.y * a.x - s * a.z;
    rot[4] = c + t.y * a.y;
    rot[5] = t.y * a.z + s * a.x;
    rot[6] = t.z * a.x + s * a.y;
    rot[7] = t.z * a.y - s * a.x;
    rot[8] = c + t.z * a.z;

    Mat4 r;
    for (int row = 0; row < 4; ++row) {
        r.at(0, row) = src.at(0, row) * rot[0] + src.at(1, row) * rot[1] + src.at(2, row) * rot[2];
        r.at(1, row) = src.at(0, row) * rot[3] + src.at(1, row) * rot[4] + src.at(2, row) * rot[5];
        r.at(2, row) = src.at(0, row) * rot[6] + src.at(1, row) * rot[7] + src.at(2, row) * rot[8];
        r.at(3, row) = src.at(3, row);
    }
    return r;
}

// Determinant of the 3x3 obtained by deleting column `skipC` and row `skipR`.
inline float minor3(const Mat4& in, int skipC, int skipR)
{
    float s[9];
    int n = 0;
    for (int c = 0; c < 4; ++c) {
        if (c == skipC) continue;
        for (int r = 0; r < 4; ++r) {
            if (r == skipR) continue;
            s[n++] = in.at(c, r);
        }
    }
    // s is column-major 3x3: s[c*3 + r]
    return s[0] * (s[4] * s[8] - s[7] * s[5])
         - s[3] * (s[1] * s[8] - s[7] * s[2])
         + s[6] * (s[1] * s[5] - s[4] * s[2]);
}

// glm::inverse(mat4) -- adjugate over determinant.
inline Mat4 inverse(const Mat4& in)
{
    float cof[16];
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            const float sign = ((c + r) & 1) ? -1.0f : 1.0f;
            cof[c * 4 + r] = sign * minor3(in, c, r);
        }
    }

    // det via expansion along column 0
    float det = 0.0f;
    for (int r = 0; r < 4; ++r) det += in.at(0, r) * cof[0 * 4 + r];
    if (det == 0.0f) return mat4Identity();
    const float invDet = 1.0f / det;

    // inverse = transpose(cofactor) / det
    Mat4 out;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            out.at(c, r) = cof[r * 4 + c] * invDet;
    return out;
}

// 3x3 column-major helpers for the equirect path.
struct Mat3 {
    float m[9];
    float& at(int c, int r) { return m[c * 3 + r]; }
    float at(int c, int r) const { return m[c * 3 + r]; }
};

inline Mat3 mat3Identity()
{
    Mat3 r;
    for (int i = 0; i < 9; ++i) r.m[i] = 0.0f;
    r.m[0] = r.m[4] = r.m[8] = 1.0f;
    return r;
}

inline Mat3 operator*(const Mat3& a, const Mat3& b)
{
    Mat3 r;
    for (int c = 0; c < 3; ++c) {
        for (int row = 0; row < 3; ++row) {
            float s = 0.0f;
            for (int k = 0; k < 3; ++k) s += a.at(k, row) * b.at(c, k);
            r.at(c, row) = s;
        }
    }
    return r;
}

inline Mat3 rotX3(float a)
{
    const float c = std::cos(a), s = std::sin(a);
    Mat3 r = mat3Identity();
    r.at(1, 1) = c;  r.at(1, 2) = s;
    r.at(2, 1) = -s; r.at(2, 2) = c;
    return r;
}

inline Mat3 rotY3(float a)
{
    const float c = std::cos(a), s = std::sin(a);
    Mat3 r = mat3Identity();
    r.at(0, 0) = c;  r.at(0, 2) = -s;
    r.at(2, 0) = s;  r.at(2, 2) = c;
    return r;
}

inline Mat3 rotZ3(float a)
{
    const float c = std::cos(a), s = std::sin(a);
    Mat3 r = mat3Identity();
    r.at(0, 0) = c;  r.at(0, 1) = s;
    r.at(1, 0) = -s; r.at(1, 1) = c;
    return r;
}

} // namespace DJIReframe
