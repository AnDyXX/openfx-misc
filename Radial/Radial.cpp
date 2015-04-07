/*
 OFX Radial plugin.
 
 Copyright (C) 2014 INRIA
 
 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
 
 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.
 
 Neither the name of the {organization} nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 INRIA
 Domaine de Voluceau
 Rocquencourt - B.P. 105
 78153 Le Chesnay Cedex - France
 
 
 The skeleton for this source file is from:
 OFX Basic Example plugin, a plugin that illustrates the use of the OFX Support library.
 
 Copyright (C) 2004-2005 The Open Effects Association Ltd
 Author Bruno Nicoletti bruno@thefoundry.co.uk
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 * Neither the name The Open Effects Association Ltd, nor the names of its
 contributors may be used to endorse or promote products derived from this
 software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 The Open Effects Association Ltd
 1 Wardour St
 London W1D 6PA
 England
 
 */
#include "Radial.h"

#include <cmath>
#include <algorithm>

#include "ofxsProcessing.H"
#include "ofxsMerging.h"
#include "ofxsMaskMix.h"
#include "ofxsRectangleInteract.h"
#include "ofxsMacros.h"

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#define POINT_TOLERANCE 6
#define POINT_SIZE 5


#define kPluginName "RadialOFX"
#define kPluginGrouping "Draw"
#define kPluginDescription \
"Radial ramp.\n" \
"The ramp is composited with the source image using the 'over' operator."
#define kPluginIdentifier "net.sf.openfx.Radial"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kSupportsTiles 1
#define kSupportsMultiResolution 1
#define kSupportsRenderScale 1
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths false
#define kRenderThreadSafety eRenderFullySafe


#define kParamProcessR      "r"
#define kParamProcessRLabel "R"
#define kParamProcessRHint  "Process red component"
#define kParamProcessG      "g"
#define kParamProcessGLabel "G"
#define kParamProcessGHint  "Process green component"
#define kParamProcessB      "b"
#define kParamProcessBLabel "B"
#define kParamProcessBHint  "Process blue component"
#define kParamProcessA      "a"
#define kParamProcessALabel "A"
#define kParamProcessAHint  "Process alpha component"

#define kParamSoftness "softness"
#define kParamSoftnessLabel "Softness"
#define kParamSoftnessHint "Softness of the radial ramp. 0 is a hard edge."

#define kParamPLinear "plinear"
#define kParamPLinearLabel "Perceptually Linear"
#define kParamPLinearHint "Make the radial ramp look more linear to the eye."

#define kParamColor0 "color0"
#define kParamColor0Label "Color 0"

#define kParamColor1 "color1"
#define kParamColor1Label "Color 1"

#define kParamExpandRoD "expandRoD"
#define kParamExpandRoDLabel "Expand RoD"
#define kParamExpandRoDHint "Expand the source region of definition by the shape RoD (if Source is connected and color0.a=0)."

//RGBA checkbox are host side if true
static bool gHostHasNativeRGBACheckbox;

namespace {
    struct RGBAValues {
        double r,g,b,a;
        RGBAValues(double v) : r(v), g(v), b(v), a(v) {}
        RGBAValues() : r(0), g(0), b(0), a(0) {}
    };
}

static inline
double
rampSmooth(double t)
{
    t *= 2.;
    if (t < 1) {
        return t * t / (2.);
    } else {
        t -= 1.;
        return -0.5 * (t * (t - 2) - 1);
    }
}

using namespace OFX;

class RadialProcessorBase : public OFX::ImageProcessor
{
   
    
protected:
    const OFX::Image *_srcImg;
    const OFX::Image *_maskImg;
    bool   _doMasking;
    double _mix;
    bool _maskInvert;
    bool _processR, _processG, _processB, _processA;

    OfxPointD _btmLeft, _size;
    double _softness;
    bool _plinear;
    RGBAValues _color0, _color1;

public:
    RadialProcessorBase(OFX::ImageEffect &instance)
    : OFX::ImageProcessor(instance)
    , _srcImg(0)
    , _maskImg(0)
    , _doMasking(false)
    , _mix(1.)
    , _maskInvert(false)
    , _processR(false)
    , _processG(false)
    , _processB(false)
    , _processA(false)
    , _softness(1.)
    , _plinear(false)
    {
        _btmLeft.x = _btmLeft.y = _size.x = _size.y = 0.;
        _color0.r = _color0.g = _color0.b = _color0.a = 0.;
        _color1.r = _color1.g = _color1.b = _color1.a = 0.;
    }

    /** @brief set the src image */
    void setSrcImg(const OFX::Image *v)
    {
        _srcImg = v;
    }

    void setMaskImg(const OFX::Image *v, bool maskInvert)
    {
        _maskImg = v;
        _maskInvert = maskInvert;
    }

    void doMasking(bool v) {
        _doMasking = v;
    }

    void setValues(const OfxPointD& btmLeft,
                   const OfxPointD& size,
                   double softness,
                   bool plinear,
                   const RGBAValues& color0,
                   const RGBAValues& color1,
                   double mix,
                   bool processR,
                   bool processG,
                   bool processB,
                   bool processA)
    {
        _btmLeft = btmLeft;
        _size = size;
        _softness = std::max(0.,std::min(softness,1.));
        _plinear = plinear;
        _color0 = color0;
        _color1 = color1;
        _mix = mix;
        _processR = processR;
        _processG = processG;
        _processB = processB;
        _processA = processA;
    }
 };


template <class PIX, int nComponents, int maxValue>
class RadialProcessor : public RadialProcessorBase
{
public:
    RadialProcessor(OFX::ImageEffect &instance)
    : RadialProcessorBase(instance)
    {
    }

private:
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        
        int todo = ((_processR ? 0xf000 : 0) | (_processG ? 0x0f00 : 0) | (_processB ? 0x00f0 : 0) | (_processA ? 0x000f : 0));
        if (nComponents == 1) {
            switch (todo) {
                case 0x0000:
                case 0x00f0:
                case 0x0f00:
                case 0x0ff0:
                case 0xf000:
                case 0xf0f0:
                case 0xff00:
                case 0xfff0:
                    return process<false,false,false,false>(procWindow);
                case 0x000f:
                case 0x00ff:
                case 0x0f0f:
                case 0x0fff:
                case 0xf00f:
                case 0xf0ff:
                case 0xff0f:
                case 0xffff:
                    return process<false,false,false,true >(procWindow);
            }
        } else if (nComponents == 3) {
            switch (todo) {
                case 0x0000:
                case 0x000f:
                    return process<false,false,false,false>(procWindow);
                case 0x00f0:
                case 0x00ff:
                    return process<false,false,true ,false>(procWindow);
                case 0x0f00:
                case 0x0f0f:
                    return process<false,true ,false,false>(procWindow);
                case 0x0ff0:
                case 0x0fff:
                    return process<false,true ,true ,false>(procWindow);
                case 0xf000:
                case 0xf00f:
                    return process<true ,false,false,false>(procWindow);
                case 0xf0f0:
                case 0xf0ff:
                    return process<true ,false,true ,false>(procWindow);
                case 0xff00:
                case 0xff0f:
                    return process<true ,true ,false,false>(procWindow);
                case 0xfff0:
                case 0xffff:
                    return process<true ,true ,true ,false>(procWindow);
            }
        } else if (nComponents == 4) {
            switch (todo) {
                case 0x0000:
                    return process<false,false,false,false>(procWindow);
                case 0x000f:
                    return process<false,false,false,true >(procWindow);
                case 0x00f0:
                    return process<false,false,true ,false>(procWindow);
                case 0x00ff:
                    return process<false,false,true, true >(procWindow);
                case 0x0f00:
                    return process<false,true ,false,false>(procWindow);
                case 0x0f0f:
                    return process<false,true ,false,true >(procWindow);
                case 0x0ff0:
                    return process<false,true ,true ,false>(procWindow);
                case 0x0fff:
                    return process<false,true ,true ,true >(procWindow);
                case 0xf000:
                    return process<true ,false,false,false>(procWindow);
                case 0xf00f:
                    return process<true ,false,false,true >(procWindow);
                case 0xf0f0:
                    return process<true ,false,true ,false>(procWindow);
                case 0xf0ff:
                    return process<true ,false,true, true >(procWindow);
                case 0xff00:
                    return process<true ,true ,false,false>(procWindow);
                case 0xff0f:
                    return process<true ,true ,false,true >(procWindow);
                case 0xfff0:
                    return process<true ,true ,true ,false>(procWindow);
                case 0xffff:
                    return process<true ,true ,true ,true >(procWindow);
            }
        }
    }

    
private:
    
    template<bool processR, bool processG, bool processB, bool processA>
    void process(const OfxRectI& procWindow)
    {
        assert((!processR && !processG && !processB) || (nComponents == 3 || nComponents == 4));
        assert(!processA || (nComponents == 1 || nComponents == 4));

        float tmpPix[4];

        for (int y = procWindow.y1; y < procWindow.y2; ++y) {
            if (_effect.abort()) {
                break;
            }
            
            PIX *dstPix = (PIX *) _dstImg->getPixelAddress(procWindow.x1, y);
            
            for (int x = procWindow.x1; x < procWindow.x2; ++x, dstPix += nComponents) {
                const PIX *srcPix = (const PIX *)  (_srcImg ? _srcImg->getPixelAddress(x, y) : 0);
                OfxPointI p_pixel;
                OfxPointD p;
                p_pixel.x = x;
                p_pixel.y = y;
                OFX::MergeImages2D::toCanonical(p_pixel, _dstImg->getRenderScale(), _dstImg->getPixelAspectRatio(), &p);

                double dx = (p.x - (_btmLeft.x + (_btmLeft.x + _size.x)) / 2) / (_size.x/2);
                double dy = (p.y - (_btmLeft.y + (_btmLeft.y + _size.y)) / 2) / (_size.y/2);

                if (dx >= 1 || dy >= 1) {
                    tmpPix[0] = (float)_color0.r;
                    tmpPix[1] = (float)_color0.g;
                    tmpPix[2] = (float)_color0.b;
                    tmpPix[3] = (float)_color0.a;
                } else {
                    double dsq = dx*dx + dy*dy;

                    if (dsq >= 1) {
                        tmpPix[0] = (float)_color0.r;
                        tmpPix[1] = (float)_color0.g;
                        tmpPix[2] = (float)_color0.b;
                        tmpPix[3] = (float)_color0.a;
                    } else if (dsq <= 0 || _softness == 0) {
                        tmpPix[0] = (float)_color1.r;
                        tmpPix[1] = (float)_color1.g;
                        tmpPix[2] = (float)_color1.b;
                        tmpPix[3] = (float)_color1.a;
                    } else {
                        float t = (1.f - (float)std::sqrt(dsq)) / (float)_softness;
                        if (t >= 1) {
                            tmpPix[0] = (float)_color1.r;
                            tmpPix[1] = (float)_color1.g;
                            tmpPix[2] = (float)_color1.b;
                            tmpPix[3] = (float)_color1.a;
                        } else {
                            t = (float)rampSmooth(t);

                            if (_plinear) {
                                // it seems to be the way Nuke does it... I could understand t*t, but why t*t*t?
                                t = t*t*t;
                            }
                            tmpPix[0] = (float)_color0.r * (1.f - t) + (float)_color1.r * t;
                            tmpPix[1] = (float)_color0.g * (1.f - t) + (float)_color1.g * t;
                            tmpPix[2] = (float)_color0.b * (1.f - t) + (float)_color1.b * t;
                            tmpPix[3] = (float)_color0.a * (1.f - t) + (float)_color1.a * t;
                        }
                    }
                }
                float a = tmpPix[3];

                // ofxsMaskMixPix takes non-normalized values
                tmpPix[0] *= maxValue;
                tmpPix[1] *= maxValue;
                tmpPix[2] *= maxValue;
                tmpPix[3] *= maxValue;
                float srcPixRGBA[4] = {0, 0, 0, 0};
                if (srcPix) {
                    if (nComponents >= 3) {
                        srcPixRGBA[0] = srcPix[0];
                        srcPixRGBA[1] = srcPix[1];
                        srcPixRGBA[2] = srcPix[2];
                    }
                    if (nComponents == 1 || nComponents == 4) {
                        srcPixRGBA[3] = srcPix[nComponents-1];
                    }
                }
                if (processR) {
                    tmpPix[0] = tmpPix[0] + srcPixRGBA[0]*(1.f-a);
                } else {
                    tmpPix[0] = srcPixRGBA[0];
                }
                if (processG) {
                    tmpPix[1] = tmpPix[1] + srcPixRGBA[1]*(1.f-a);
                } else {
                    tmpPix[1] = srcPixRGBA[1];
                }
                if (processB) {
                    tmpPix[2] = tmpPix[2] + srcPixRGBA[2]*(1.f-a);
                } else {
                    tmpPix[2] = srcPixRGBA[2];
                }
                if (processA) {
                    tmpPix[3] = tmpPix[3] + srcPixRGBA[3]*(1.f-a);
                } else {
                    tmpPix[3] = srcPixRGBA[3];
                }
                ofxsMaskMixPix<PIX, nComponents, maxValue, true>(tmpPix, x, y, srcPix, _doMasking, _maskImg, (float)_mix, _maskInvert, dstPix);
            }
        }
    }

};





////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class RadialPlugin : public OFX::ImageEffect
{
public:
    /** @brief ctor */
    RadialPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , _dstClip(0)
    , _srcClip(0)
    , _processR(0)
    , _processG(0)
    , _processB(0)
    , _processA(0)
    , _btmLeft(0)
    , _size(0)
    , _color0(0)
    , _color1(0)
    , _expandRoD(0)
    {
        _dstClip = fetchClip(kOfxImageEffectOutputClipName);
        assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentAlpha || _dstClip->getPixelComponents() == ePixelComponentRGB || _dstClip->getPixelComponents() == ePixelComponentRGBA));
        _srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
        assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentAlpha || _srcClip->getPixelComponents() == ePixelComponentRGB || _srcClip->getPixelComponents() == ePixelComponentRGBA));
        _maskClip = getContext() == OFX::eContextFilter ? NULL : fetchClip(getContext() == OFX::eContextPaint ? "Brush" : "Mask");
<<<<<<< HEAD
		assert(!_maskClip || _maskClip->getPixelComponents() == ePixelComponentAlpha || _maskClip->getPixelComponents() == ePixelComponentRGBA);

        _processR = fetchBooleanParam(kParamProcessR);
        _processG = fetchBooleanParam(kParamProcessG);
        _processB = fetchBooleanParam(kParamProcessB);
        _processA = fetchBooleanParam(kParamProcessA);

        assert(_processR && _processG && _processB && _processA);
=======
        assert(!_maskClip || _maskClip->getPixelComponents() == ePixelComponentAlpha);
        
        if (!gHostHasNativeRGBACheckbox) {
            _processR = fetchBooleanParam(kParamProcessR);
            _processG = fetchBooleanParam(kParamProcessG);
            _processB = fetchBooleanParam(kParamProcessB);
            _processA = fetchBooleanParam(kParamProcessA);
            
            assert(_processR && _processG && _processB && _processA);
        }
>>>>>>> 81bd61045866842963281bfc99cad1ac90c1534e
        _btmLeft = fetchDouble2DParam(kParamRectangleInteractBtmLeft);
        _size = fetchDouble2DParam(kParamRectangleInteractSize);
        _softness = fetchDoubleParam(kParamSoftness);
        _plinear = fetchBooleanParam(kParamPLinear);
        _color0 = fetchRGBAParam(kParamColor0);
        _color1 = fetchRGBAParam(kParamColor1);
        _expandRoD = fetchBooleanParam(kParamExpandRoD);
        assert(_btmLeft && _size && _softness && _plinear && _color0 && _color1 && _expandRoD);

        _mix = fetchDoubleParam(kParamMix);
        _maskInvert = fetchBooleanParam(kParamMaskInvert);
        assert(_mix && _maskInvert);
    }

private:
    /* override is identity */
    virtual bool isIdentity(const OFX::IsIdentityArguments &args, OFX::Clip * &identityClip, double &identityTime) OVERRIDE FINAL;

    /* Override the clip preferences */
    void getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;

    virtual bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;

    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    
    template <int nComponents>
    void renderInternal(const OFX::RenderArguments &args, OFX::BitDepthEnum dstBitDepth);

    /* set up and run a processor */
    void setupAndProcess(RadialProcessorBase &, const OFX::RenderArguments &args);
    
private:
    
    // do not need to delete these, the ImageEffect is managing them for us
    Clip *_dstClip;
    Clip *_srcClip;
    Clip *_maskClip;

    BooleanParam* _processR;
    BooleanParam* _processG;
    BooleanParam* _processB;
    BooleanParam* _processA;
    Double2DParam* _btmLeft;
    Double2DParam* _size;
    DoubleParam* _softness;
    BooleanParam* _plinear;
    RGBAParam* _color0;
    RGBAParam* _color1;
    BooleanParam* _expandRoD;
    OFX::DoubleParam* _mix;
    OFX::BooleanParam* _maskInvert;
};

////////////////////////////////////////////////////////////////////////////////
/** @brief render for the filter */

////////////////////////////////////////////////////////////////////////////////
// basic plugin render function, just a skelington to instantiate templates from

/* set up and run a processor */
void
RadialPlugin::setupAndProcess(RadialProcessorBase &processor, const OFX::RenderArguments &args)
{
    std::auto_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
    if (!dst.get()) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    OFX::BitDepthEnum         dstBitDepth    = dst->getPixelDepth();
    OFX::PixelComponentEnum   dstComponents  = dst->getPixelComponents();
    if (dstBitDepth != _dstClip->getPixelDepth() ||
        dstComponents != _dstClip->getPixelComponents()) {
        setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong depth or components");
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    if (dst->getRenderScale().x != args.renderScale.x ||
        dst->getRenderScale().y != args.renderScale.y ||
        (dst->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && dst->getField() != args.fieldToRender)) {
        setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    assert(_srcClip);
    std::auto_ptr<const OFX::Image> src((_srcClip && _srcClip->isConnected()) ?
                                        _srcClip->fetchImage(args.time) : 0);
    if (src.get()) {
        if (src->getRenderScale().x != args.renderScale.x ||
            src->getRenderScale().y != args.renderScale.y ||
            (src->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && src->getField() != args.fieldToRender)) {
            setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
            OFX::throwSuiteStatusException(kOfxStatFailed);
        }
        OFX::BitDepthEnum    srcBitDepth      = src->getPixelDepth();
        OFX::PixelComponentEnum srcComponents = src->getPixelComponents();
        if (srcBitDepth != dstBitDepth || srcComponents != dstComponents) {
            OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
        }
    }
    std::auto_ptr<const OFX::Image> mask((getContext() != OFX::eContextFilter && _maskClip && _maskClip->isConnected()) ?
                                         _maskClip->fetchImage(args.time) : 0);
    if (getContext() != OFX::eContextFilter && _maskClip && _maskClip->isConnected()) {
        if (mask.get()) {
            if (mask->getRenderScale().x != args.renderScale.x ||
                mask->getRenderScale().y != args.renderScale.y ||
                (mask->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && mask->getField() != args.fieldToRender)) {
                setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
                OFX::throwSuiteStatusException(kOfxStatFailed);
            }
        }
        bool maskInvert;
        _maskInvert->getValueAtTime(args.time, maskInvert);
        processor.doMasking(true);
        processor.setMaskImg(mask.get(), maskInvert);
    }

    if (src.get() && dst.get()) {
        OFX::BitDepthEnum    srcBitDepth      = src->getPixelDepth();
        OFX::PixelComponentEnum srcComponents = src->getPixelComponents();
        OFX::BitDepthEnum dstBitDepth       = dst->getPixelDepth();
        OFX::PixelComponentEnum dstComponents  = dst->getPixelComponents();

        // see if they have the same depths and bytes and all
        if (srcBitDepth != dstBitDepth || srcComponents != dstComponents)
            OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
    }

    // set the images
    processor.setDstImg(dst.get());
    processor.setSrcImg(src.get());

    // set the render window
    processor.setRenderWindow(args.renderWindow);

    OfxPointD btmLeft, size;
    _btmLeft->getValueAtTime(args.time, btmLeft.x, btmLeft.y);
    _size->getValueAtTime(args.time, size.x, size.y);

    double softness;
    _softness->getValueAtTime(args.time, softness);
    bool plinear;
    _plinear->getValueAtTime(args.time, plinear);
    
    RGBAValues color0, color1;
    _color0->getValueAtTime(args.time, color0.r, color0.g, color0.b, color0.a);
    _color1->getValueAtTime(args.time, color1.r, color1.g, color1.b, color1.a);
    
    bool processR, processG, processB, processA;
    if (!gHostHasNativeRGBACheckbox) {
        _processR->getValue(processR);
        _processG->getValue(processG);
        _processB->getValue(processB);
        _processA->getValue(processA);
    } else {
        processR = processG = processB = processA = true;
    }
    
    double mix;
    _mix->getValueAtTime(args.time, mix);
    
    processor.setValues(btmLeft, size,
                        softness, plinear,
                        color0, color1,
                        mix,
                        processR, processG, processB, processA);
    // Call the base class process member, this will call the derived templated process code
    processor.process();
}


// the internal render function
template <int nComponents>
void
RadialPlugin::renderInternal(const OFX::RenderArguments &args, OFX::BitDepthEnum dstBitDepth)
{
    switch (dstBitDepth) {
        case OFX::eBitDepthUByte: {
            RadialProcessor<unsigned char, nComponents, 255> fred(*this);
            setupAndProcess(fred, args);
        }   break;
        case OFX::eBitDepthUShort: {
            RadialProcessor<unsigned short, nComponents, 65535> fred(*this);
            setupAndProcess(fred, args);
        }   break;
        case OFX::eBitDepthFloat: {
            RadialProcessor<float, nComponents, 1> fred(*this);
            setupAndProcess(fred, args);
        }   break;
        default:
            OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
    }
}

// the overridden render function
void
RadialPlugin::render(const OFX::RenderArguments &args)
{
    
    // instantiate the render code based on the pixel depth of the dst clip
    OFX::BitDepthEnum       dstBitDepth    = _dstClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents  = _dstClip->getPixelComponents();

    assert(kSupportsMultipleClipPARs   || !_srcClip || _srcClip->getPixelAspectRatio() == _dstClip->getPixelAspectRatio());
    assert(kSupportsMultipleClipDepths || !_srcClip || _srcClip->getPixelDepth()       == _dstClip->getPixelDepth());
    assert(dstComponents == OFX::ePixelComponentRGB || dstComponents == OFX::ePixelComponentRGBA || dstComponents == OFX::ePixelComponentAlpha);
    if (dstComponents == OFX::ePixelComponentRGBA) {
        renderInternal<4>(args, dstBitDepth);
    } else if (dstComponents == OFX::ePixelComponentRGB) {
        renderInternal<3>(args, dstBitDepth);
    } else {
        assert(dstComponents == OFX::ePixelComponentAlpha);
        renderInternal<1>(args, dstBitDepth);
    }
}

bool
RadialPlugin::isIdentity(const OFX::IsIdentityArguments &args,
                       OFX::Clip * &identityClip,
                       double &/*identityTime*/)
{
    double mix;
    _mix->getValueAtTime(args.time, mix);

    if (mix == 0. /*|| (!processR && !processG && !processB && !processA)*/) {
        identityClip = _srcClip;
        return true;
    }
    
    if (!gHostHasNativeRGBACheckbox) {
        bool processR;
        bool processG;
        bool processB;
        bool processA;
        _processR->getValueAtTime(args.time, processR);
        _processG->getValueAtTime(args.time, processG);
        _processB->getValueAtTime(args.time, processB);
        _processA->getValueAtTime(args.time, processA);
        if (!processR && !processG && !processB && !processA) {
            identityClip = _srcClip;
            return true;
        }
    }

    RGBAValues color0, color1;
    _color0->getValueAtTime(args.time, color0.r, color0.g, color0.b, color0.a);
    _color1->getValueAtTime(args.time, color1.r, color1.g, color1.b, color1.a);
    if (color0.a == 0. && color1.a == 0.) {
        identityClip = _srcClip;
        return true;
    }

    return false;
}

/* Override the clip preferences */
void
RadialPlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    // set the premultiplication of _dstClip if alpha is affected and source is Opaque
    bool alpha;
    if (!gHostHasNativeRGBACheckbox) {
        _processA->getValue(alpha);
    } else {
        alpha = true;
    }
    if (alpha && _srcClip->getPreMultiplication() == eImageOpaque) {
        clipPreferences.setOutputPremultiplication(eImageUnPreMultiplied);
    }
}

bool
RadialPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    double mix;
    _mix->getValueAtTime(args.time, mix);
    if (mix == 0.) {
        if (_srcClip->isConnected()) {
            // nothing to draw: return default region of definition
            return false;
        } else {
            // empty RoD
            rod.x1 = rod.y1 = rod.x2 = rod.y2 = 0.;
            return true;
        }
    }
    RGBAValues color0;
    _color0->getValueAtTime(args.time, color0.r, color0.g, color0.b, color0.a);
    if (color0.a != 0.) {
        // something has to be drawn outside of the rectangle
        // return default RoD.
        return false;
        //// Other option: RoD could be union(defaultRoD,inputsRoD)
        //// Natron does this if the RoD is infinite
        //rod.x1 = rod.y1 = kOfxFlagInfiniteMin;
        //rod.x2 = rod.y2 = kOfxFlagInfiniteMax;
    }
    RGBAValues color1;
    _color1->getValueAtTime(args.time, color1.r, color1.g, color1.b, color1.a);
    if (color1.a == 0.) {
        if (_srcClip->isConnected()) {
            // nothing to draw: return default region of definition
            return false;
        } else {
            // empty RoD
            rod.x1 = rod.y1 = rod.x2 = rod.y2 = 0.;
            return true;
        }
    }
    bool expandRoD;
    _expandRoD->getValueAtTime(args.time, expandRoD);
    if (_srcClip && _srcClip->isConnected() && !expandRoD) {
        return false;
    }
    OfxPointD btmLeft, size;
    _btmLeft->getValueAtTime(args.time, btmLeft.x, btmLeft.y);
    _size->getValueAtTime(args.time, size.x, size.y);
    rod.x1 = btmLeft.x;
    rod.y1 = btmLeft.y;
    rod.x2 = rod.x1 + size.x;
    rod.y2 = rod.y1 + size.y;
    if (_srcClip && _srcClip->isConnected()) {
        // something has to be drawn outside of the rectangle: return union of input RoD and rectangle
        OfxRectD srcRoD = _srcClip->getRegionOfDefinition(args.time);
        rod.x1 = std::min(rod.x1, srcRoD.x1);
        rod.x2 = std::max(rod.x2, srcRoD.x2);
        rod.y1 = std::min(rod.y1, srcRoD.y1);
        rod.y2 = std::max(rod.y2, srcRoD.y2);
    }

    return true;
}

mDeclarePluginFactory(RadialPluginFactory, {}, {});

void RadialPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    // basic labels
    desc.setLabel(kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);

    desc.addSupportedContext(eContextGeneral);
    desc.addSupportedContext(eContextGenerator);

    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);
    
    
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(true);
    desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
    desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
    desc.setRenderThreadSafety(kRenderThreadSafety);
    
    desc.setSupportsTiles(kSupportsTiles);
    
    // in order to support multiresolution, render() must take into account the pixelaspectratio and the renderscale
    // and scale the transform appropriately.
    // All other functions are usually in canonical coordinates.
    desc.setSupportsMultiResolution(kSupportsMultiResolution);
    desc.setOverlayInteractDescriptor(new RectangleOverlayDescriptor);
    
#ifdef OFX_EXTENSIONS_NATRON
    if (OFX::getImageEffectHostDescription()->isNatron) {
        gHostHasNativeRGBACheckbox = true;
    } else {
        gHostHasNativeRGBACheckbox = false;
    }
#else
    gHostHasNativeRGBACheckbox = false;
#endif
}



OFX::ImageEffect* RadialPluginFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum /*context*/)
{
    return new RadialPlugin(handle);
}




void RadialPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
{
    // Source clip only in the filter context
    // create the mandated source clip
    // always declare the source clip first, because some hosts may consider
    // it as the default input clip (e.g. Nuke)
    ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setIsMask(false);
    srcClip->setOptional(true);

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setSupportsTiles(kSupportsTiles);

    if (context == eContextGeneral || context == eContextPaint) {
        ClipDescriptor *maskClip = context == eContextGeneral ? desc.defineClip("Mask") : desc.defineClip("Brush");
        maskClip->addSupportedComponent(ePixelComponentAlpha);
        maskClip->setTemporalClipAccess(false);
        if (context == eContextGeneral) {
            maskClip->setOptional(true);
        }
        maskClip->setSupportsTiles(kSupportsTiles);
        maskClip->setIsMask(true);
    }

    // make some pages and to things in
    PageParamDescriptor *page = desc.definePageParam("Controls");
    
    if (!gHostHasNativeRGBACheckbox) {
        {
            OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamProcessR);
            param->setLabel(kParamProcessRLabel);
            param->setHint(kParamProcessRHint);
            param->setDefault(true);
            param->setLayoutHint(eLayoutHintNoNewLine);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamProcessG);
            param->setLabel(kParamProcessGLabel);
            param->setHint(kParamProcessGHint);
            param->setDefault(true);
            param->setLayoutHint(eLayoutHintNoNewLine);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamProcessB);
            param->setLabel(kParamProcessBLabel);
            param->setHint(kParamProcessBHint);
            param->setDefault(true);
            param->setLayoutHint(eLayoutHintNoNewLine);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamProcessA);
            param->setLabel(kParamProcessALabel);
            param->setHint(kParamProcessAHint);
            param->setDefault(true);
            if (page) {
                page->addChild(*param);
            }
        }
    }
    
    // btmLeft
    {
        Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamRectangleInteractBtmLeft);
        param->setLabel(kParamRectangleInteractBtmLeftLabel);
        param->setDoubleType(OFX::eDoubleTypeXYAbsolute);
        param->setDefaultCoordinateSystem(OFX::eCoordinatesNormalised);
        param->setDefault(0.25, 0.25);
        param->setIncrement(1.);
        param->setHint("Coordinates of the bottom left corner of the effect rectangle.");
        param->setDigits(0);
        if (page) {
            page->addChild(*param);
        }
    }

    // size
    {
        Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamRectangleInteractSize);
        param->setLabel(kParamRectangleInteractSizeLabel);
        param->setDoubleType(OFX::eDoubleTypeXY);
        param->setDefaultCoordinateSystem(OFX::eCoordinatesNormalised);
        param->setDefault(0.5, 0.5);
        param->setIncrement(1.);
        param->setDimensionLabels(kParamRectangleInteractSizeDim1, kParamRectangleInteractSizeDim2);
        param->setHint("Width and height of the effect rectangle.");
        param->setDigits(0);
        if (page) {
            page->addChild(*param);
        }
    }

    // interactive
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamRectangleInteractInteractive);
        param->setLabel(kParamRectangleInteractInteractiveLabel);
        param->setHint(kParamRectangleInteractInteractiveHint);
        param->setEvaluateOnChange(false);
        if (page) {
            page->addChild(*param);
        }
    }

    // softness
    {
        DoubleParamDescriptor* param = desc.defineDoubleParam(kParamSoftness);
        param->setLabel(kParamSoftnessLabel);
        param->setHint(kParamSoftnessHint);
        param->setDefault(1.);
        param->setIncrement(0.01);
        param->setRange(0., 1.);
        param->setDisplayRange(0., 1.);
        param->setDigits(2);
        param->setLayoutHint(OFX::eLayoutHintNoNewLine);
        if (page) {
            page->addChild(*param);
        }
    }

    // plinear
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamPLinear);
        param->setLabel(kParamPLinearLabel);
        param->setHint(kParamPLinearHint);
        if (page) {
            page->addChild(*param);
        }
    }

    // color0
    {
        RGBAParamDescriptor* param = desc.defineRGBAParam(kParamColor0);
        param->setLabel(kParamColor0Label);
        param->setDefault(0, 0, 0, 0);
        if (page) {
            page->addChild(*param);
        }
    }

    // color1
    {
        RGBAParamDescriptor* param = desc.defineRGBAParam(kParamColor1);
        param->setLabel(kParamColor1Label);
        param->setDefault(1., 1., 1., 1. );
        if (page) {
            page->addChild(*param);
        }
    }

    // expandRoD
    {
        OFX::BooleanParamDescriptor *param = desc.defineBooleanParam(kParamExpandRoD);
        param->setLabel(kParamExpandRoDLabel);
        param->setHint(kParamExpandRoDHint);
        param->setDefault(true);
        if (page) {
            page->addChild(*param);
        }
    }

    ofxsMaskMixDescribeParams(desc, page);
}

void getRadialPluginID(OFX::PluginFactoryArray &ids)
{
    static RadialPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
    ids.push_back(&p);
}

