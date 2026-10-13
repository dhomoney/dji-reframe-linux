// Geometry setup and CPU sampling for the DJI Reframe port.
//
// Everything here follows the recovered spec in the README. Geometry is built once per
// render and then consumed by either the CPU loop below or the OpenCL kernels,
// so there is a single source of truth for the math.
#pragma once

#include "Mat4.h"

#include <algorithm>
#include <cmath>

namespace DJIReframe {

static const float kPi      = 3.1415927f;
static const float kDeg2Rad = 0.017453292f;
static const float kZNear   = 0.0005f;
static const float kZFar    = 10.0f;

enum BorderMode {
    eBorderTransparent  = 0,
    eBorderColor        = 1,
    eBorderMirrorExtend = 2
};

// Resolution choice index 1..12; 0 is unused (the original yields 0x0 there).
enum { kResolutionSource = 11, kResolutionCustom = 12 };

struct ReframeParams {
    // camera
    double pan = 0.0, tilt = 0.0, roll = 0.0;
    double fov = 60.0, distortion = 0.6;
    // source-file orientation correction
    double srcYaw = 0.0, srcPitch = 0.0, srcRoll = 0.0;
    // output
    int resolution = 1;
    int customWidth = 0, customHeight = 0;
    // background
    int borderMode = eBorderTransparent;
    float borderR = 0.0f, borderG = 0.0f, borderB = 0.0f;
};

inline void outputSizeForChoice(int choice, int srcW, int srcH,
                                int customW, int customH, int* outW, int* outH)
{
    switch (choice) {
        case 1:  *outW = 1920; *outH = 1080; return;
        case 2:  *outW = 1080; *outH = 1920; return;
        case 3:  *outW = 1080; *outH = 1080; return;
        case 4:  *outW = 2538; *outH = 1080; return;
        case 5:  *outW = 1440; *outH = 1080; return;
        case 6:  *outW = 1080; *outH = 1440; return;
        case 7:  *outW = 3840; *outH = 2160; return;
        case 8:  *outW = 2160; *outH = 3840; return;
        case 9:  *outW = 2160; *outH = 2160; return;
        case 10: *outW = 3840; *outH = 1634; return;
        case 11: *outW = srcW; *outH = srcH; return;
        case 12:
            *outW = (customW > 0) ? customW : srcW;
            *outH = (customH > 0) ? customH : srcH;
            return;
        default: *outW = 0; *outH = 0; return;
    }
}

enum RenderMode { eModeSphere, eModeEquirect };

// Fully resolved geometry for one render pass.
struct Geometry {
    RenderMode mode = eModeSphere;

    int srcW = 0, srcH = 0;
    int dstW = 0, dstH = 0;

    float vpX = 0.0f, vpY = 0.0f, vpW = 0.0f, vpH = 0.0f;

    Mat4 invMVP = mat4Identity();   // eModeSphere
    Mat3 rot    = mat3Identity();   // eModeEquirect

    float vScale = 1.0f, vOffset = 0.0f;

    int   borderMode = eBorderTransparent;
    float borderR = 0.0f, borderG = 0.0f, borderB = 0.0f;
};

// srcW/srcH are the source image bounds; dstW/dstH the render window.
inline Geometry buildGeometry(const ReframeParams& p, int srcW, int srcH, int dstW, int dstH)
{
    Geometry g;
    g.srcW = srcW; g.srcH = srcH;
    g.dstW = dstW; g.dstH = dstH;

    // Equirectangular sampling window: a 2:1 source gives scale 1 / offset 0;
    // a 16:9 source is treated as the vertical middle of a partial sphere.
    g.vScale  = 0.5f * static_cast<float>(srcW) / std::max(static_cast<float>(srcH), 1.0f);
    g.vOffset = 0.5f * (1.0f - g.vScale);

    int outW = 0, outH = 0;
    outputSizeForChoice(p.resolution, srcW, srcH, p.customWidth, p.customHeight, &outW, &outH);
    if (outW <= 0 || outH <= 0) { outW = srcW; outH = srcH; }

    g.vpX = (static_cast<float>(dstW) - static_cast<float>(outW)) * 0.5f;
    g.vpY = (static_cast<float>(dstH) - static_cast<float>(outH)) * 0.5f;
    g.vpW = static_cast<float>(outW);
    g.vpH = static_cast<float>(outH);

    // Resolution "Source" keeps the equirect format and only corrects the
    // source orientation -- pan/tilt/roll/FOV/distortion are not used.
    if (p.resolution == kResolutionSource) {
        g.mode = eModeEquirect;
        g.rot = rotZ3(static_cast<float>(p.srcYaw) * kDeg2Rad)
              * rotY3(kPi - static_cast<float>(p.srcPitch) * kDeg2Rad)
              * rotX3(static_cast<float>(p.srcRoll) * kDeg2Rad);
        return g;
    }

    g.mode       = eModeSphere;
    g.borderMode = p.borderMode;
    g.borderR    = p.borderR;
    g.borderG    = p.borderG;
    g.borderB    = p.borderB;

    const float fovRad = static_cast<float>(p.fov) * kDeg2Rad;
    const float fOutW  = static_cast<float>(outW);
    const float fOutH  = static_cast<float>(outH);

    Mat4 proj;
    if (p.borderMode == eBorderMirrorExtend) {
        // Expand the view to cover the whole render window with an off-centre
        // frustum, and let the viewport become the full frame.
        const float halfH = kZNear * std::tan(fovRad * 0.5f);
        const float halfW = halfH * (fOutW / std::max(fOutH, 1.0f));
        const float sx = (static_cast<float>(dstW) * 0.5f - (fOutW * 0.5f + g.vpX)) / (fOutW * 0.5f);
        const float sy = (static_cast<float>(dstH) * 0.5f - (fOutH * 0.5f + g.vpY)) / (fOutH * 0.5f);
        const float ex = halfW * (static_cast<float>(dstW) / fOutW);
        const float ey = halfH * (static_cast<float>(dstH) / fOutH);

        proj = frustum(sx * halfW - ex, sx * halfW + ex,
                       sy * halfH - ey, sy * halfH + ey, kZNear, kZFar);

        g.vpX = 0.0f; g.vpY = 0.0f;
        g.vpW = static_cast<float>(dstW);
        g.vpH = static_cast<float>(dstH);
    } else {
        proj = perspective(fovRad, fOutW / std::max(fOutH, 1.0f), kZNear, kZFar);
    }

    // Distortion is the eye's Z offset from the sphere centre -- this one
    // scalar is what separates Wide / CrystalBall / TinyPlanet.
    const Mat4 view = lookAt(vec3(0.0f, 0.0f, static_cast<float>(p.distortion)),
                             vec3(0.0f, 0.0f, -1.0f),
                             vec3(0.0f, -1.0f, 0.0f));

    Mat4 model = mat4Identity();
    model = rotate(model,  static_cast<float>(p.roll)     * kDeg2Rad, vec3(0, 0, 1));
    model = rotate(model, -static_cast<float>(p.tilt)     * kDeg2Rad, vec3(1, 0, 0));
    model = rotate(model,  static_cast<float>(p.pan)      * kDeg2Rad, vec3(0, 1, 0));
    model = rotate(model,  static_cast<float>(p.srcRoll)  * kDeg2Rad, vec3(0, 0, 1));
    model = rotate(model,  static_cast<float>(p.srcPitch) * kDeg2Rad, vec3(1, 0, 0));
    model = rotate(model,  static_cast<float>(p.srcYaw)   * kDeg2Rad, vec3(0, 1, 0));

    g.invMVP = inverse(proj * view * model);
    return g;
}

// ---------------------------------------------------------------- CPU sampling

struct RGBA { float r, g, b, a; };

// Bilinear sample of an RGBA float image, UV clamped to [0,1].
// srcStride is the distance between rows in floats (srcW*4 when packed), so an
// OFX image can be read in place without repacking.
inline RGBA sampleBilinear(const float* src, int srcW, int srcH, size_t srcStride,
                           float u, float v)
{
    u = std::min(1.0f, std::max(0.0f, u));
    v = std::min(1.0f, std::max(0.0f, v));

    const float fx = u * static_cast<float>(srcW - 1);
    const float fy = v * static_cast<float>(srcH - 1);

    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int x1 = std::min(x0 + 1, srcW - 1);
    const int y1 = std::min(y0 + 1, srcH - 1);

    const float wx = fx - static_cast<float>(x0);
    const float wy = fy - static_cast<float>(y0);

    const float* row0 = src + static_cast<size_t>(y0) * srcStride;
    const float* row1 = src + static_cast<size_t>(y1) * srcStride;
    const float* p00 = row0 + static_cast<size_t>(x0) * 4;
    const float* p10 = row0 + static_cast<size_t>(x1) * 4;
    const float* p01 = row1 + static_cast<size_t>(x0) * 4;
    const float* p11 = row1 + static_cast<size_t>(x1) * 4;

    const float w00 = (1.0f - wx) * (1.0f - wy);
    const float w10 = wx * (1.0f - wy);
    const float w01 = (1.0f - wx) * wy;
    const float w11 = wx * wy;

    RGBA o;
    o.r = w00 * p00[0] + w10 * p10[0] + w01 * p01[0] + w11 * p11[0];
    o.g = w00 * p00[1] + w10 * p10[1] + w01 * p01[1] + w11 * p11[1];
    o.b = w00 * p00[2] + w10 * p10[2] + w01 * p01[2] + w11 * p11[2];
    o.a = w00 * p00[3] + w10 * p10[3] + w01 * p01[3] + w11 * p11[3];
    return o;
}

inline RGBA borderPixel(const Geometry& g)
{
    if (g.borderMode == eBorderColor) return RGBA{g.borderR, g.borderG, g.borderB, 1.0f};
    return RGBA{0.0f, 0.0f, 0.0f, 0.0f};
}

// One output pixel of the perspective path: unproject through invMVP,
// intersect the unit sphere, convert the hit point to equirect UV.
inline RGBA renderSpherePixel(const Geometry& g, const float* src, size_t srcStride,
                              int px, int py)
{
    const float fx = static_cast<float>(px);
    const float fy = static_cast<float>(py);

    const bool inside = (fx >= g.vpX && fx < g.vpX + g.vpW &&
                         fy >= g.vpY && fy < g.vpY + g.vpH);
    if (!inside) return borderPixel(g);

    const float ndcX =  (((fx - g.vpX) / g.vpW) * 2.0f - 1.0f);
    const float ndcY = -(((fy - g.vpY) / g.vpH) * 2.0f - 1.0f);

    const float* M = g.invMVP.m;
    float nearPt[3], farPt[3];
    {
        const float z = -1.0f;
        const float x = M[0] * ndcX + M[4] * ndcY + M[8]  * z + M[12];
        const float y = M[1] * ndcX + M[5] * ndcY + M[9]  * z + M[13];
        const float zz = M[2] * ndcX + M[6] * ndcY + M[10] * z + M[14];
        const float w = M[3] * ndcX + M[7] * ndcY + M[11] * z + M[15];
        nearPt[0] = x / w; nearPt[1] = y / w; nearPt[2] = zz / w;
    }
    {
        const float z = 1.0f;
        const float x = M[0] * ndcX + M[4] * ndcY + M[8]  * z + M[12];
        const float y = M[1] * ndcX + M[5] * ndcY + M[9]  * z + M[13];
        const float zz = M[2] * ndcX + M[6] * ndcY + M[10] * z + M[14];
        const float w = M[3] * ndcX + M[7] * ndcY + M[11] * z + M[15];
        farPt[0] = x / w; farPt[1] = y / w; farPt[2] = zz / w;
    }

    float dir[3] = {farPt[0] - nearPt[0], farPt[1] - nearPt[1], farPt[2] - nearPt[2]};
    const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (len < 1e-10f) return RGBA{0.0f, 0.0f, 0.0f, 0.0f};
    const float invLen = 1.0f / len;
    dir[0] *= invLen; dir[1] *= invLen; dir[2] *= invLen;

    // Unit sphere; direction is normalised so the quadratic's a == 1.
    const float b = 2.0f * (nearPt[0] * dir[0] + nearPt[1] * dir[1] + nearPt[2] * dir[2]);
    const float c = nearPt[0] * nearPt[0] + nearPt[1] * nearPt[1] + nearPt[2] * nearPt[2] - 1.0f;
    const float disc = b * b - 4.0f * c;
    if (disc < 0.0f) return borderPixel(g);

    const float sq = std::sqrt(disc);
    float t = (-b - sq) * 0.5f;
    if (t < 0.0f) t = (-b + sq) * 0.5f;

    const float hx = nearPt[0] + t * dir[0];
    const float hy = nearPt[1] + t * dir[1];
    const float hz = nearPt[2] + t * dir[2];

    const float theta = std::acos(std::min(1.0f, std::max(-1.0f, hy)));
    float phi = std::atan2(hx, hz);
    if (phi < 0.0f) phi += 2.0f * kPi;

    const float u = phi / (2.0f * kPi);
    float v = 1.0f - theta / kPi;
    v = g.vOffset + v * g.vScale;

    return sampleBilinear(src, g.srcW, g.srcH, srcStride, u, v);
}

// One output pixel of the equirect path: build a z-up direction from the output
// UV, rotate it, and convert back to equirect UV.
inline RGBA renderEquirectPixel(const Geometry& g, const float* src, size_t srcStride,
                                int px, int py)
{
    const float fx = static_cast<float>(px);
    const float fy = static_cast<float>(py);

    const bool inside = (fx >= g.vpX && fx < g.vpX + g.vpW &&
                         fy >= g.vpY && fy < g.vpY + g.vpH);
    if (!inside) return RGBA{0.0f, 0.0f, 0.0f, 1.0f};

    const float u = (fx - g.vpX) / g.vpW;
    const float v = 1.0f - (fy - g.vpY) / g.vpH;

    const float thetaRaw = v * kPi;
    const float sinTheta = std::sin(thetaRaw);
    const float phiRaw   = 2.0f * kPi - u * 2.0f * kPi;

    const float sx = sinTheta * std::cos(phiRaw);
    const float sy = sinTheta * std::sin(phiRaw);
    const float sz = std::cos(thetaRaw);

    const float* R = g.rot.m;
    const float rx = R[0] * sx + R[3] * sy + R[6] * sz;
    const float ry = R[1] * sx + R[4] * sy + R[7] * sz;
    const float rz = R[2] * sx + R[5] * sy + R[8] * sz;

    const float theta = std::acos(std::min(1.0f, std::max(-1.0f, rz)));
    float phi = std::atan2(ry, rx) + kPi;
    if (phi < 0.0f) phi += 2.0f * kPi;
    else if (phi > 2.0f * kPi) phi -= 2.0f * kPi;

    const float su = phi / (2.0f * kPi);
    float sv = theta / kPi;
    sv = g.vOffset + sv * g.vScale;

    return sampleBilinear(src, g.srcW, g.srcH, srcStride, su, sv);
}

} // namespace DJIReframe
