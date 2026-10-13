// DJI Reframe -- Linux OFX port.
//
// Reconstructed from the Windows build of DJI's OFX plugin; see
// the README for how each parameter, preset and matrix was recovered.
// The parameter names match the original so project files stay compatible.

#include "ReframeCore.h"
#include "Presets.h"

#include "ofxsImageEffect.h"
#include "ofxsProcessing.h"
#include "ofxsLog.h"

#include <cfloat>
#include <cstring>
#include <string>

#define kPluginName        "DJI Reframe"
#define kPluginGrouping    "DJI"
#define kPluginDescription "Reframe 360 / ultra-wide DJI footage: pan, tilt, roll and " \
                           "field of view from an equirectangular source."
#define kPluginIdentifier  "com.pano360.reframe"
#define kPluginVersionMajor 1
#define kPluginVersionMinor 0

// Parameter names -- kept identical to the Windows plugin.
#define kParamResolution   "resolution"
#define kParamCustomWidth  "custom_width"
#define kParamCustomHeight "custom_height"
#define kParamProjection   "camera_projection_type"
#define kParamPan          "camera_pan"
#define kParamTilt         "camera_tilt"
#define kParamRoll         "camera_rotate"
#define kParamFov          "camera_fov_control"
#define kParamDistortion   "camera_distortion_control"
#define kParamReset        "reset_button"
#define kParamSrcYaw       "source_file_yaw"
#define kParamSrcPitch     "source_file_pitch"
#define kParamSrcRoll      "source_file_roll"
#define kParamBorderMode   "border_mode"
#define kParamBorderColor  "border_color"

#define kGroupResolution   "Group_Resolution"
#define kGroupCamera       "Group_Camera"
#define kGroupSourceFile   "Group_SourceFile"
#define kGroupBackground   "Group_Background"

using namespace DJIReframe;

namespace {

// Border mode order recovered from describeInContext: Transparent, Color,
// Mirror/Extend -- which matches the kernel's borderMode == 1 meaning "colour".
const char* kBorderLabels[] = { "Transparent", "Color", "Mirror / Extend" };

} // namespace

// ----------------------------------------------------------------- processor

class ReframeProcessor : public OFX::ImageProcessor {
public:
    explicit ReframeProcessor(OFX::ImageEffect& instance)
        : OFX::ImageProcessor(instance), _srcImg(nullptr) {}

    void setSrcImg(OFX::Image* v) { _srcImg = v; }
    void setGeometry(const Geometry& g) { _geom = g; }

    void multiThreadProcessImages(OfxRectI window) override
    {
        const OfxRectI dstBounds = _dstImg->getBounds();
        const OfxRectI srcBounds = _srcImg->getBounds();

        // The recovered kernels index both images as origin-relative buffers,
        // so convert OFX coordinates into that space.
        const float* srcBase = reinterpret_cast<const float*>(
            _srcImg->getPixelAddress(srcBounds.x1, srcBounds.y1));
        if (!srcBase) return;
        const size_t srcStride = _srcImg->getRowBytes() / sizeof(float);

        for (int y = window.y1; y < window.y2; ++y) {
            if (_effect.abort()) break;

            float* dstPix = reinterpret_cast<float*>(_dstImg->getPixelAddress(window.x1, y));
            if (!dstPix) continue;

            const int py = y - dstBounds.y1;
            for (int x = window.x1; x < window.x2; ++x) {
                const int px = x - dstBounds.x1;

                const RGBA c = (_geom.mode == eModeEquirect)
                    ? renderEquirectPixel(_geom, srcBase, srcStride, px, py)
                    : renderSpherePixel(_geom, srcBase, srcStride, px, py);

                dstPix[0] = c.r; dstPix[1] = c.g; dstPix[2] = c.b; dstPix[3] = c.a;
                dstPix += 4;
            }
        }
    }

private:
    OFX::Image* _srcImg;
    Geometry    _geom;
};

// -------------------------------------------------------------------- effect

class ReframePlugin : public OFX::ImageEffect {
public:
    explicit ReframePlugin(OfxImageEffectHandle handle)
        : OFX::ImageEffect(handle)
    {
        _dstClip = fetchClip(kOfxImageEffectOutputClipName);
        _srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);

        _resolution   = fetchChoiceParam(kParamResolution);
        _customWidth  = fetchIntParam(kParamCustomWidth);
        _customHeight = fetchIntParam(kParamCustomHeight);
        _projection   = fetchChoiceParam(kParamProjection);
        _pan          = fetchDoubleParam(kParamPan);
        _tilt         = fetchDoubleParam(kParamTilt);
        _roll         = fetchDoubleParam(kParamRoll);
        _fov          = fetchDoubleParam(kParamFov);
        _distortion   = fetchDoubleParam(kParamDistortion);
        _srcYaw       = fetchDoubleParam(kParamSrcYaw);
        _srcPitch     = fetchDoubleParam(kParamSrcPitch);
        _srcRoll      = fetchDoubleParam(kParamSrcRoll);
        _borderMode   = fetchChoiceParam(kParamBorderMode);
        _borderColor  = fetchRGBParam(kParamBorderColor);

        syncEnabledState();
    }

    void render(const OFX::RenderArguments& args) override;
    void changedParam(const OFX::InstanceChangedArgs& args, const std::string& name) override;

private:
    ReframeParams gatherParams(double time);
    void applyPreset();
    void syncEnabledState();

    OFX::Clip* _dstClip;
    OFX::Clip* _srcClip;

    OFX::ChoiceParam* _resolution;
    OFX::IntParam*    _customWidth;
    OFX::IntParam*    _customHeight;
    OFX::ChoiceParam* _projection;
    OFX::DoubleParam* _pan;
    OFX::DoubleParam* _tilt;
    OFX::DoubleParam* _roll;
    OFX::DoubleParam* _fov;
    OFX::DoubleParam* _distortion;
    OFX::DoubleParam* _srcYaw;
    OFX::DoubleParam* _srcPitch;
    OFX::DoubleParam* _srcRoll;
    OFX::ChoiceParam* _borderMode;
    OFX::RGBParam*    _borderColor;
};

ReframeParams ReframePlugin::gatherParams(double time)
{
    ReframeParams p;
    int resChoice = 0;
    _resolution->getValueAtTime(time, resChoice);
    p.resolution = resChoice + 1;
    _customWidth->getValueAtTime(time, p.customWidth);
    _customHeight->getValueAtTime(time, p.customHeight);
    _pan->getValueAtTime(time, p.pan);
    _tilt->getValueAtTime(time, p.tilt);
    _roll->getValueAtTime(time, p.roll);
    _fov->getValueAtTime(time, p.fov);
    _distortion->getValueAtTime(time, p.distortion);
    _srcYaw->getValueAtTime(time, p.srcYaw);
    _srcPitch->getValueAtTime(time, p.srcPitch);
    _srcRoll->getValueAtTime(time, p.srcRoll);
    _borderMode->getValueAtTime(time, p.borderMode);

    double r = 0.0, g = 0.0, b = 0.0;
    _borderColor->getValueAtTime(time, r, g, b);
    p.borderR = static_cast<float>(r);
    p.borderG = static_cast<float>(g);
    p.borderB = static_cast<float>(b);
    return p;
}

void ReframePlugin::render(const OFX::RenderArguments& args)
{
    std::unique_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
    std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
    if (!dst.get() || !src.get()) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
        return;
    }

    if (dst->getPixelDepth() != OFX::eBitDepthFloat ||
        src->getPixelDepth() != OFX::eBitDepthFloat ||
        dst->getPixelComponents() != OFX::ePixelComponentRGBA ||
        src->getPixelComponents() != OFX::ePixelComponentRGBA) {
        OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
        return;
    }

    const OfxRectI srcB = src->getBounds();
    const OfxRectI dstB = dst->getBounds();

    const ReframeParams p = gatherParams(args.time);
    const Geometry geom = buildGeometry(p,
                                        srcB.x2 - srcB.x1, srcB.y2 - srcB.y1,
                                        dstB.x2 - dstB.x1, dstB.y2 - dstB.y1);

    ReframeProcessor proc(*this);
    proc.setDstImg(dst.get());
    proc.setSrcImg(src.get());
    proc.setGeometry(geom);
    proc.setRenderWindow(args.renderWindow);
    proc.process();
}

// Switching projection or resolution pushes the preset values into the camera
// params, exactly as the original's changedParam does.
void ReframePlugin::applyPreset()
{
    int proj = 0, resIdx = 0;
    _projection->getValue(proj);
    _resolution->getValue(resIdx);

    if (proj < 0 || proj >= kNumProjections) proj = 0;
    if (resIdx < 0 || resIdx >= kNumResolutions) resIdx = 0;

    const CameraPreset& preset = kPresets[proj][resIdx];
    _pan->setValue(preset.pan);
    _tilt->setValue(preset.tilt);
    _roll->setValue(preset.roll);
    _fov->setValue(preset.fov);
    _distortion->setValue(preset.distortion);
}

void ReframePlugin::syncEnabledState()
{
    int resChoice = 0;
    _resolution->getValue(resChoice);
    const bool custom = (resChoice == kChoiceCustom);
    _customWidth->setEnabled(custom);
    _customHeight->setEnabled(custom);

    // The equirect path ignores the camera and background controls.
    const bool perspective = (resChoice != kChoiceSourceImage);
    _pan->setEnabled(perspective);
    _tilt->setEnabled(perspective);
    _roll->setEnabled(perspective);
    _fov->setEnabled(perspective);
    _distortion->setEnabled(perspective);
    _projection->setEnabled(perspective);
    _borderMode->setEnabled(perspective);

    int border = eBorderTransparent;
    _borderMode->getValue(border);
    _borderColor->setEnabled(perspective && border == eBorderColor);
}

void ReframePlugin::changedParam(const OFX::InstanceChangedArgs& /*args*/,
                                 const std::string& name)
{
    if (name == kParamResolution) {
        syncEnabledState();
        applyPreset();
    } else if (name == kParamProjection) {
        applyPreset();
    } else if (name == kParamBorderMode) {
        syncEnabledState();
    } else if (name == kParamReset) {
        applyPreset();
        _srcYaw->setValue(0.0);
        _srcPitch->setValue(0.0);
        _srcRoll->setValue(0.0);
    }
}

// ------------------------------------------------------------------- factory

mDeclarePluginFactory(ReframePluginFactory, {}, {});

void ReframePluginFactory::describe(OFX::ImageEffectDescriptor& desc)
{
    desc.setLabels(kPluginName, kPluginName, kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);

    desc.addSupportedContext(OFX::eContextFilter);
    desc.addSupportedContext(OFX::eContextGeneral);
    desc.addSupportedBitDepth(OFX::eBitDepthFloat);

    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(false);
    desc.setSupportsTiles(false);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(false);
}

void ReframePluginFactory::describeInContext(OFX::ImageEffectDescriptor& desc,
                                             OFX::ContextEnum /*context*/)
{
    OFX::ClipDescriptor* srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(false);
    srcClip->setIsMask(false);

    OFX::ClipDescriptor* dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    dstClip->setSupportsTiles(false);

    OFX::PageParamDescriptor* page = desc.definePageParam("Controls");

    // ---- Resolution
    {
        OFX::GroupParamDescriptor* grp = desc.defineGroupParam(kGroupResolution);
        grp->setLabels("Output", "Output", "Output");
        page->addChild(*grp);

        OFX::ChoiceParamDescriptor* res = desc.defineChoiceParam(kParamResolution);
        res->setLabels("Resolution", "Resolution", "Resolution");
        res->setHint("Output frame size.");
        for (int i = 0; i < kNumResolutions; ++i) res->appendOption(kResolutionLabels[i]);
        res->setDefault(0);
        res->setParent(*grp);
        page->addChild(*res);

        OFX::IntParamDescriptor* cw = desc.defineIntParam(kParamCustomWidth);
        cw->setLabels("Custom Width", "Custom Width", "Custom Width");
        cw->setHint("Custom output width.");
        cw->setRange(0, 16384);
        cw->setDisplayRange(0, 8192);
        cw->setDefault(1920);
        cw->setParent(*grp);
        page->addChild(*cw);

        OFX::IntParamDescriptor* ch = desc.defineIntParam(kParamCustomHeight);
        ch->setLabels("Custom Height", "Custom Height", "Custom Height");
        ch->setHint("Custom output height.");
        ch->setRange(0, 16384);
        ch->setDisplayRange(0, 8192);
        ch->setDefault(1080);
        ch->setParent(*grp);
        page->addChild(*ch);
    }

    // ---- Camera
    {
        OFX::GroupParamDescriptor* grp = desc.defineGroupParam(kGroupCamera);
        grp->setLabels("Camera", "Camera", "Camera");
        page->addChild(*grp);

        OFX::ChoiceParamDescriptor* proj = desc.defineChoiceParam(kParamProjection);
        proj->setLabels("Projection", "Projection", "Projection");
        proj->setHint("Preset field of view and distortion.");
        for (int i = 0; i < kNumProjections; ++i) proj->appendOption(kProjectionLabels[i]);
        proj->setDefault(0);
        proj->setParent(*grp);
        page->addChild(*proj);

        struct AngleDef { const char* name; const char* label; const char* hint; };
        const AngleDef angles[] = {
            { kParamPan,  "Pan",  "Rotate the view horizontally." },
            { kParamTilt, "Tilt", "Rotate the view vertically." },
            { kParamRoll, "Roll", "Rotate the view around the viewing axis." },
        };
        for (const AngleDef& a : angles) {
            OFX::DoubleParamDescriptor* d = desc.defineDoubleParam(a.name);
            d->setLabels(a.label, a.label, a.label);
            d->setHint(a.hint);
            d->setDoubleType(OFX::eDoubleTypeAngle);
            d->setRange(-DBL_MAX, DBL_MAX);
            d->setDisplayRange(-360.0, 360.0);
            d->setDefault(0.0);
            d->setParent(*grp);
            page->addChild(*d);
        }

        OFX::DoubleParamDescriptor* fov = desc.defineDoubleParam(kParamFov);
        fov->setLabels("FOV", "FOV", "FOV Control");
        fov->setHint("Vertical field of view in degrees.");
        fov->setRange(-DBL_MAX, DBL_MAX);
        fov->setDisplayRange(1.0, 178.0);
        fov->setDefault(60.0);
        fov->setParent(*grp);
        page->addChild(*fov);

        OFX::DoubleParamDescriptor* dist = desc.defineDoubleParam(kParamDistortion);
        dist->setLabels("Distortion", "Distortion", "Distortion Control");
        dist->setHint("Camera position relative to the sphere centre. Low values give a "
                      "tiny-planet look, high values an ultra-wide look.");
        dist->setRange(-DBL_MAX, DBL_MAX);
        dist->setDisplayRange(0.0, 1.8);
        dist->setDefault(0.6);
        dist->setParent(*grp);
        page->addChild(*dist);

        OFX::PushButtonParamDescriptor* reset = desc.definePushButtonParam(kParamReset);
        reset->setLabels("Reset", "Reset", "Reset");
        reset->setHint("Reset camera parameters.");
        reset->setParent(*grp);
        page->addChild(*reset);
    }

    // ---- Source file orientation
    {
        OFX::GroupParamDescriptor* grp = desc.defineGroupParam(kGroupSourceFile);
        grp->setLabels("Source File", "Source File", "Source File");
        page->addChild(*grp);

        struct AngleDef { const char* name; const char* label; };
        const AngleDef angles[] = {
            { kParamSrcYaw,   "Yaw" },
            { kParamSrcPitch, "Pitch" },
            { kParamSrcRoll,  "Roll" },
        };
        for (const AngleDef& a : angles) {
            OFX::DoubleParamDescriptor* d = desc.defineDoubleParam(a.name);
            d->setLabels(a.label, a.label, a.label);
            d->setHint("Correct the orientation the clip was recorded at.");
            d->setDoubleType(OFX::eDoubleTypeAngle);
            d->setRange(-DBL_MAX, DBL_MAX);
            d->setDisplayRange(-360.0, 360.0);
            d->setDefault(0.0);
            d->setParent(*grp);
            page->addChild(*d);
        }
    }

    // ---- Background
    {
        OFX::GroupParamDescriptor* grp = desc.defineGroupParam(kGroupBackground);
        grp->setLabels("Background", "Background", "Background Settings");
        page->addChild(*grp);

        OFX::ChoiceParamDescriptor* bm = desc.defineChoiceParam(kParamBorderMode);
        bm->setLabels("Border Mode", "Border Mode", "Border Mode");
        bm->setHint("How to fill pixels the sphere does not cover.");
        for (int i = 0; i < 3; ++i) bm->appendOption(kBorderLabels[i]);
        bm->setDefault(eBorderTransparent);
        bm->setParent(*grp);
        page->addChild(*bm);

        OFX::RGBParamDescriptor* bc = desc.defineRGBParam(kParamBorderColor);
        bc->setLabels("Border Color", "Border Color", "Border Color");
        bc->setHint("Fill colour when Border Mode is Color.");
        bc->setDefault(0.0, 0.0, 0.0);
        bc->setParent(*grp);
        page->addChild(*bc);
    }
}

OFX::ImageEffect* ReframePluginFactory::createInstance(OfxImageEffectHandle handle,
                                                       OFX::ContextEnum /*context*/)
{
    return new ReframePlugin(handle);
}

namespace OFX {
namespace Plugin {

void getPluginIDs(OFX::PluginFactoryArray& ids)
{
    static ReframePluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
    ids.push_back(&p);
}

} // namespace Plugin
} // namespace OFX
