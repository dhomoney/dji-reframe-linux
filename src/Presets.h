// Generated from the DJIReframe.ofx .rdata table at 0x180022580.
// See the README; regenerate with tools/extract_presets.py.
//
// Both dropdown orders below are the appendOption order recovered from
// describeInContext, NOT alphabetical -- getting this wrong silently swaps the
// look of every projection.
#pragma once

namespace DJIReframe {

struct CameraPreset { double pan, tilt, roll, fov, distortion; };

static const int kNumProjections = 5;
static const int kNumResolutions = 12;

// Resolution dropdown is 0-based; the internal size selector is index + 1.
static const int kChoiceSourceImage = 10;
static const int kChoiceCustom      = 11;

static const char* const kProjectionLabels[kNumProjections] = {
    "Wide",
    "Crystal Ball",
    "Tiny Planet",
    "Ultra Wide",
    "Dewarp",
};

static const char* const kResolutionLabels[kNumResolutions] = {
    "1080p 16:9(1920*1080)",
    "1080p 9:16(1080*1920)",
    "1080p 1:1 (1080*1080)",
    "1080p 2.35:1(2538*1080)",
    "1080p 4:3 (1440*1080)",
    "1080p 3:4(1080*1440)",
    "4k 16:9(3840*2160)",
    "4k 9:16(2160*3840)",
    "4k 1:1(2160*2160)",
    "4k 2.35:1(3840*1634)",
    "Source Image",
    "Resolution Custom",
};

// kPresets[projection][resolutionIndex]
static const CameraPreset kPresets[kNumProjections][kNumResolutions] = {
    // Wide
    {
        { 0, 0, 0, 60, 0.6 },  // 1080p 16:9(1920*1080)
        { 0, 0, 0, 90, 0.6 },  // 1080p 9:16(1080*1920)
        { 0, 0, 0, 60, 0.6 },  // 1080p 1:1 (1080*1080)
        { 0, 0, 0, 60, 0.6 },  // 1080p 2.35:1(2538*1080)
        { 0, 0, 0, 60, 0.6 },  // 1080p 4:3 (1440*1080)
        { 0, 0, 0, 72, 0.6 },  // 1080p 3:4(1080*1440)
        { 0, 0, 0, 60, 0.6 },  // 4k 16:9(3840*2160)
        { 0, 0, 0, 90, 0.6 },  // 4k 9:16(2160*3840)
        { 0, 0, 0, 60, 0.6 },  // 4k 1:1(2160*2160)
        { 0, 0, 0, 60, 0.6 },  // 4k 2.35:1(3840*1634)
        { 0, 0, 0, 60, 0.6 },  // Source Image
        { 0, 0, 0, 60, 0.6 },  // Resolution Custom
    },
    // CrystalBall
    {
        { 0, 0, 0, 75, 1.8 },  // 1080p 16:9(1920*1080)
        { 0, 0, 0, 110, 1.8 },  // 1080p 9:16(1080*1920)
        { 0, 0, 0, 75, 1.8 },  // 1080p 1:1 (1080*1080)
        { 0, 0, 0, 75, 1.8 },  // 1080p 2.35:1(2538*1080)
        { 0, 0, 0, 75, 1.8 },  // 1080p 4:3 (1440*1080)
        { 0, 0, 0, 87, 1.8 },  // 1080p 3:4(1080*1440)
        { 0, 0, 0, 75, 1.8 },  // 4k 16:9(3840*2160)
        { 0, 0, 0, 110, 1.8 },  // 4k 9:16(2160*3840)
        { 0, 0, 0, 75, 1.8 },  // 4k 1:1(2160*2160)
        { 0, 0, 0, 75, 1.8 },  // 4k 2.35:1(3840*1634)
        { 0, 0, 0, 75, 1.8 },  // Source Image
        { 0, 0, 0, 75, 1.8 },  // Resolution Custom
    },
    // TinyPlanet
    {
        { 0, -90, 0, 138, 1 },  // 1080p 16:9(1920*1080)
        { 0, -90, 0, 147, 1 },  // 1080p 9:16(1080*1920)
        { 0, -90, 0, 138, 1 },  // 1080p 1:1 (1080*1080)
        { 0, -90, 0, 138, 1 },  // 1080p 2.35:1(2538*1080)
        { 0, -90, 0, 138, 1 },  // 1080p 4:3 (1440*1080)
        { 0, -90, 0, 147, 1 },  // 1080p 3:4(1080*1440)
        { 0, -90, 0, 138, 1 },  // 4k 16:9(3840*2160)
        { 0, -90, 0, 147, 1 },  // 4k 9:16(2160*3840)
        { 0, -90, 0, 138, 1 },  // 4k 1:1(2160*2160)
        { 0, -90, 0, 138, 1 },  // 4k 2.35:1(3840*1634)
        { 0, -90, 0, 138, 1 },  // Source Image
        { 0, -90, 0, 138, 1 },  // Resolution Custom
    },
    // UltraWide
    {
        { 0, 0, 0, 78, 0.5 },  // 1080p 16:9(1920*1080)
        { 0, 0, 0, 110, 0.5 },  // 1080p 9:16(1080*1920)
        { 0, 0, 0, 78, 0.5 },  // 1080p 1:1 (1080*1080)
        { 0, 0, 0, 78, 0.5 },  // 1080p 2.35:1(2538*1080)
        { 0, 0, 0, 78, 0.5 },  // 1080p 4:3 (1440*1080)
        { 0, 0, 0, 95, 0.5 },  // 1080p 3:4(1080*1440)
        { 0, 0, 0, 78, 0.5 },  // 4k 16:9(3840*2160)
        { 0, 0, 0, 110, 0.5 },  // 4k 9:16(2160*3840)
        { 0, 0, 0, 78, 0.5 },  // 4k 1:1(2160*2160)
        { 0, 0, 0, 78, 0.5 },  // 4k 2.35:1(3840*1634)
        { 0, 0, 0, 78, 0.5 },  // Source Image
        { 0, 0, 0, 78, 0.5 },  // Resolution Custom
    },
    // Dewarp
    {
        { 0, 0, 0, 80, 0.2 },  // 1080p 16:9(1920*1080)
        { 0, 0, 0, 112, 0.2 },  // 1080p 9:16(1080*1920)
        { 0, 0, 0, 80, 0.2 },  // 1080p 1:1 (1080*1080)
        { 0, 0, 0, 80, 0.2 },  // 1080p 2.35:1(2538*1080)
        { 0, 0, 0, 80, 0.2 },  // 1080p 4:3 (1440*1080)
        { 0, 0, 0, 97, 0.2 },  // 1080p 3:4(1080*1440)
        { 0, 0, 0, 80, 0.2 },  // 4k 16:9(3840*2160)
        { 0, 0, 0, 112, 0.2 },  // 4k 9:16(2160*3840)
        { 0, 0, 0, 80, 0.2 },  // 4k 1:1(2160*2160)
        { 0, 0, 0, 80, 0.2 },  // 4k 2.35:1(3840*1634)
        { 0, 0, 0, 80, 0.2 },  // Source Image
        { 0, 0, 0, 80, 0.2 },  // Resolution Custom
    },
};

} // namespace DJIReframe
