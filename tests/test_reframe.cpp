// Standalone harness: renders the reconstructed geometry against a synthetic
// equirectangular source so the math can be checked without a host.
//
//   test_reframe <out_dir>
//
// Writes PPM files plus a PASS/FAIL summary of the invariants we can assert
// numerically (identity round-trip, pan symmetry, border behaviour).

#include "../src/ReframeCore.h"
#include "../src/Presets.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace DJIReframe;

// A latitude/longitude grid with coloured quadrants and distinct poles, so that
// orientation errors are obvious by eye and measurable numerically.
static std::vector<float> makeEquirect(int w, int h)
{
    std::vector<float> img(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        const float v = (h > 1) ? static_cast<float>(y) / (h - 1) : 0.0f;  // 0 = row 0
        for (int x = 0; x < w; ++x) {
            const float u = (w > 1) ? static_cast<float>(x) / (w - 1) : 0.0f;
            float* p = &img[(static_cast<size_t>(y) * w + x) * 4];

            // Quadrant base colour: longitude in 4 bands, latitude in 2.
            const int lon = static_cast<int>(u * 4.0f) & 3;
            const bool upper = v > 0.5f;
            const float base[4][3] = {
                {0.80f, 0.15f, 0.15f},   // red
                {0.15f, 0.70f, 0.20f},   // green
                {0.15f, 0.25f, 0.85f},   // blue
                {0.85f, 0.75f, 0.10f},   // yellow
            };
            float r = base[lon][0], g = base[lon][1], b = base[lon][2];
            if (upper) { r = r * 0.45f + 0.25f; g = g * 0.45f + 0.25f; b = b * 0.45f + 0.25f; }

            // Grid lines every 15 degrees.
            const float lonDeg = u * 360.0f, latDeg = v * 180.0f;
            const float lonFrac = lonDeg / 15.0f - static_cast<int>(lonDeg / 15.0f);
            const float latFrac = latDeg / 15.0f - static_cast<int>(latDeg / 15.0f);
            if (lonFrac < 0.04f || latFrac < 0.04f) { r = g = b = 1.0f; }

            // Pole markers: row 0 magenta, last row cyan.
            if (y < h / 40)          { r = 1.0f; g = 0.0f; b = 1.0f; }
            if (y > h - 1 - h / 40)  { r = 0.0f; g = 1.0f; b = 1.0f; }

            p[0] = r; p[1] = g; p[2] = b; p[3] = 1.0f;
        }
    }
    return img;
}

// A smoothly varying source. The forward map divides by the viewport height
// while the sampler multiplies by (srcH - 1), so a round-trip is only identical
// to within a fraction of a pixel -- the original behaves the same way. On a
// smooth image that sub-pixel difference is negligible, so any real mapping
// error still shows up clearly.
static std::vector<float> makeSmooth(int w, int h)
{
    std::vector<float> img(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        const float v = (h > 1) ? static_cast<float>(y) / (h - 1) : 0.0f;
        for (int x = 0; x < w; ++x) {
            const float u = (w > 1) ? static_cast<float>(x) / (w - 1) : 0.0f;
            float* p = &img[(static_cast<size_t>(y) * w + x) * 4];
            p[0] = 0.5f + 0.5f * std::sin(u * 2.0f * kPi);
            p[1] = v;
            p[2] = 0.5f + 0.5f * std::cos(v * kPi);
            p[3] = 1.0f;
        }
    }
    return img;
}

static std::vector<float> renderFrame(const ReframeParams& p, const std::vector<float>& src,
                                      int srcW, int srcH, int dstW, int dstH)
{
    const Geometry g = buildGeometry(p, srcW, srcH, dstW, dstH);
    const size_t srcStride = static_cast<size_t>(srcW) * 4;
    std::vector<float> out(static_cast<size_t>(dstW) * dstH * 4);

    for (int py = 0; py < dstH; ++py) {
        for (int px = 0; px < dstW; ++px) {
            const RGBA c = (g.mode == eModeEquirect)
                ? renderEquirectPixel(g, src.data(), srcStride, px, py)
                : renderSpherePixel(g, src.data(), srcStride, px, py);
            float* o = &out[(static_cast<size_t>(py) * dstW + px) * 4];
            o[0] = c.r; o[1] = c.g; o[2] = c.b; o[3] = c.a;
        }
    }
    return out;
}

static void writePPM(const std::string& path, const std::vector<float>& img, int w, int h)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path.c_str()); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    std::vector<unsigned char> row(static_cast<size_t>(w) * 3);
    // PPM is top-down; our buffers are row-0-first, so emit as-is.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float* p = &img[(static_cast<size_t>(y) * w + x) * 4];
            for (int c = 0; c < 3; ++c) {
                float v = p[c];
                v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
                row[static_cast<size_t>(x) * 3 + c] = static_cast<unsigned char>(v * 255.0f + 0.5f);
            }
        }
        fwrite(row.data(), 1, row.size(), f);
    }
    fclose(f);
}

// Worst per-pixel difference, with the row it occurred on.
static double maxAbsDiffAt(const std::vector<float>& a, const std::vector<float>& b,
                           int w, int h, int* worstRow)
{
    double m = 0.0; *worstRow = -1;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < 4; ++c) {
                const size_t i = (static_cast<size_t>(y) * w + x) * 4 + c;
                const double d = std::fabs(static_cast<double>(a[i]) - b[i]);
                if (d > m) { m = d; *worstRow = y; }
            }
        }
    }
    return m;
}

// Same, ignoring `margin` rows at each pole where longitude is undefined.
static double maxAbsDiffInterior(const std::vector<float>& a, const std::vector<float>& b,
                                 int w, int h, int margin)
{
    double m = 0.0;
    for (int y = margin; y < h - margin; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < 4; ++c) {
                const size_t i = (static_cast<size_t>(y) * w + x) * 4 + c;
                const double d = std::fabs(static_cast<double>(a[i]) - b[i]);
                if (d > m) m = d;
            }
    return m;
}

static int g_fail = 0;
static void check(bool ok, const char* what, const char* detail = "")
{
    printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", what, detail);
    if (!ok) ++g_fail;
}

static double maxAbsDiff(const std::vector<float>& a, const std::vector<float>& b)
{
    double m = 0.0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
        const double d = std::fabs(static_cast<double>(a[i]) - b[i]);
        if (d > m) m = d;
    }
    return m;
}

int main(int argc, char** argv)
{
    const std::string outDir = (argc > 1) ? argv[1] : ".";

    const int srcW = 1024, srcH = 512;            // full 2:1 equirect
    const std::vector<float> src = makeEquirect(srcW, srcH);
    writePPM(outDir + "/00_source_equirect.ppm", src, srcW, srcH);

    const int dstW = 640, dstH = 360;

    printf("Geometry invariants\n");

    // 1. A 2:1 source must sample the whole sphere.
    {
        ReframeParams p;
        const Geometry g = buildGeometry(p, srcW, srcH, dstW, dstH);
        char buf[128];
        snprintf(buf, sizeof(buf), "(vScale=%.6f vOffset=%.6f)", g.vScale, g.vOffset);
        check(std::fabs(g.vScale - 1.0f) < 1e-6f && std::fabs(g.vOffset) < 1e-6f,
              "2:1 source -> vScale 1, vOffset 0", buf);
    }

    // 2. A 16:9 source must map to the vertical middle of a partial sphere.
    {
        ReframeParams p;
        const Geometry g = buildGeometry(p, 3840, 2160, dstW, dstH);
        char buf[128];
        snprintf(buf, sizeof(buf), "(vScale=%.6f vOffset=%.6f)", g.vScale, g.vOffset);
        const bool ok = std::fabs(g.vScale - 0.8888889f) < 1e-5f &&
                        std::fabs(g.vOffset - 0.0555556f) < 1e-5f;
        check(ok, "16:9 source -> vScale 8/9, vOffset 1/18", buf);
    }

    // 3. Output size table and centred viewport.
    {
        ReframeParams p; p.resolution = 7;                // 3840x2160
        const Geometry g = buildGeometry(p, srcW, srcH, 3840, 2160);
        check(g.vpW == 3840.0f && g.vpH == 2160.0f && g.vpX == 0.0f && g.vpY == 0.0f,
              "resolution 7 fills a 3840x2160 render window");

        const Geometry g2 = buildGeometry(p, srcW, srcH, 4000, 2300);
        check(g2.vpX == 80.0f && g2.vpY == 70.0f,
              "output rect is centred in a larger render window");
    }

    // 4. Resolution 11 selects the equirect path and ignores the camera.
    {
        ReframeParams a; a.resolution = kResolutionSource;
        ReframeParams b = a; b.pan = 123.0; b.tilt = 45.0; b.fov = 150.0; b.distortion = 1.2;
        const std::vector<float> ra = renderFrame(a, src, srcW, srcH, srcW, srcH);
        const std::vector<float> rb = renderFrame(b, src, srcW, srcH, srcW, srcH);
        check(buildGeometry(a, srcW, srcH, srcW, srcH).mode == eModeEquirect,
              "resolution 11 -> equirect path");
        check(maxAbsDiff(ra, rb) == 0.0,
              "equirect path ignores pan/tilt/FOV/distortion");

        // With zero source angles the rotation chain must collapse to identity.
        // This pins down the "pi - pitch" term and both UV conventions at once.
        const std::vector<float> smooth = makeSmooth(srcW, srcH);
        const std::vector<float> rs = renderFrame(a, smooth, srcW, srcH, srcW, srcH);
        int worstRow = -1;
        const double full = maxAbsDiffAt(rs, smooth, srcW, srcH, &worstRow);
        const double interior = maxAbsDiffInterior(rs, smooth, srcW, srcH, srcH / 64);
        char buf[192];
        snprintf(buf, sizeof(buf),
                 "(interior %.4g; whole frame %.4g, worst at row %d of %d)",
                 interior, full, worstRow, srcH);
        check(interior < 0.01,
              "equirect path at zero angles reproduces the source", buf);
        writePPM(outDir + "/01_equirect_identity.ppm", ra, srcW, srcH);
    }

    // 5. Pan by a full turn must be a no-op.
    {
        ReframeParams a; a.resolution = 1;
        ReframeParams b = a; b.pan = 360.0;
        const std::vector<float> ra = renderFrame(a, src, srcW, srcH, dstW, dstH);
        const std::vector<float> rb = renderFrame(b, src, srcW, srcH, dstW, dstH);
        char buf[96];
        snprintf(buf, sizeof(buf), "(max diff %.4g)", maxAbsDiff(ra, rb));
        check(maxAbsDiff(ra, rb) < 0.02, "pan 0 == pan 360", buf);
    }

    // 6. Border modes.
    {
        ReframeParams p; p.resolution = 1;              // 1920x1080 into a bigger window
        p.borderMode = eBorderColor;
        p.borderR = 1.0f; p.borderG = 0.0f; p.borderB = 0.0f;
        const int rw = 2000, rh = 1200;
        const std::vector<float> img = renderFrame(p, src, srcW, srcH, rw, rh);
        const float* corner = &img[0];                   // outside the centred 1920x1080
        check(corner[0] == 1.0f && corner[1] == 0.0f && corner[2] == 0.0f && corner[3] == 1.0f,
              "border mode Color fills outside the output rect");

        ReframeParams t = p; t.borderMode = eBorderTransparent;
        const std::vector<float> img2 = renderFrame(t, src, srcW, srcH, rw, rh);
        check(img2[3] == 0.0f, "border mode Transparent leaves alpha 0 outside the rect");

        ReframeParams m = p; m.borderMode = eBorderMirrorExtend;
        const Geometry gm = buildGeometry(m, srcW, srcH, rw, rh);
        check(gm.vpX == 0.0f && gm.vpY == 0.0f &&
              gm.vpW == static_cast<float>(rw) && gm.vpH == static_cast<float>(rh),
              "border mode Mirror/Extend expands the viewport to the whole frame");
        const std::vector<float> img3 = renderFrame(m, src, srcW, srcH, rw, rh);
        check(img3[3] != 0.0f, "border mode Mirror/Extend fills the corner");
        writePPM(outDir + "/06_mirror_extend.ppm", img3, rw, rh);
    }

    // 7. Every projection preset renders something non-degenerate, and the
    //    presets differ from one another.
    {
        // Dropdown order, not alphabetical -- see Presets.h.
        const char* names[] = {"wide", "crystalball", "tinyplanet", "ultrawide", "dewarp"};
        std::vector<std::vector<float>> frames;
        for (int proj = 0; proj < kNumProjections; ++proj) {
            const CameraPreset& pr = kPresets[proj][0];   // 1920x1080 column
            ReframeParams p;
            p.resolution = 1;
            p.pan = pr.pan; p.tilt = pr.tilt; p.roll = pr.roll;
            p.fov = pr.fov; p.distortion = pr.distortion;

            const std::vector<float> img = renderFrame(p, src, srcW, srcH, dstW, dstH);
            char path[256];
            snprintf(path, sizeof(path), "%s/1%d_%s.ppm", outDir.c_str(), proj, names[proj]);
            writePPM(path, img, dstW, dstH);

            double sum = 0.0;
            for (size_t i = 0; i < img.size(); i += 4) sum += img[i] + img[i + 1] + img[i + 2];
            char buf[160];
            snprintf(buf, sizeof(buf), "(fov=%.0f dist=%.1f tilt=%.0f mean=%.4f)",
                     pr.fov, pr.distortion, pr.tilt, sum / (img.size() / 4 * 3));
            check(sum > 0.0, names[proj], buf);
            frames.push_back(img);
        }
        bool allDistinct = true;
        for (size_t i = 0; i < frames.size() && allDistinct; ++i)
            for (size_t j = i + 1; j < frames.size(); ++j)
                if (maxAbsDiff(frames[i], frames[j]) < 1e-6) allDistinct = false;
        check(allDistinct, "the five projection presets all render differently");
    }

    printf("\n%s (%d failure%s)\n", g_fail ? "FAILED" : "ALL CHECKS PASSED",
           g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
