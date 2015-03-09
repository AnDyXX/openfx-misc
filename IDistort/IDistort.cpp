/*
 OFX IDistort plugin.
 
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

/*
   Although the indications from nuke/fnOfxExtensions.h were followed, and the
   kFnOfxImageEffectActionGetTransform action was implemented in the Support
   library, that action is never called by the Nuke host, so it cannot be tested.
   The code is left here for reference or for further extension.

   There is also an open question about how the last plugin in a transform chain
   may get the concatenated transform from upstream, the untransformed source image,
   concatenate its own transform and apply the resulting transform in its render
   action. Should the host be doing this instead?
*/
// This node concatenates transforms upstream.

#include "IDistort.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "ofxsImageEffect.h"
#include "ofxsProcessing.H"
#include "ofxsMerging.h"
#include "ofxsMaskMix.h"
#include "ofxsFilter.h"
#include "ofxsMatrix2D.h"
#include "ofxsCopier.h"
#include "ofxsMacros.h"

#define kPluginIDistortName "IDistortOFX"
#define kPluginIDistortGrouping "Transform"
#define kPluginIDistortDescription \
"Distort an image, based on UV channels.\n" \
"The U and V channels give the offset in pixels in the destination image to the pixel where the color is taken. " \
"For example, if at pixel (45,12) the UV value is (-1.5,3.2), then the color at this pixel is taken from (43.5,15.2) in the source image. " \
"This plugin concatenates transforms upstream, so that if the nodes upstream output a 3x3 transform " \
"(e.g. Transform, CornerPin, Dot, NoOp, Switch), the original image is sampled only once." \

#define kPluginIDistortIdentifier "net.sf.openfx.IDistort"

#define kPluginSTMapName "STMapOFX"
#define kPluginSTMapGrouping "Transform"
#define kPluginSTMapDescription \
"Move pixels around an image, based on a UVmap.\n" \
"The U and V channels give, for each pixel in the destination image, the normalized position of the pixel where the color is taken. " \
"(0,0) is the bottom left corner of the input image, while (1,1) is the top right corner. " \
"This plugin concatenates transforms upstream, so that if the nodes upstream output a 3x3 transform " \
"(e.g. Transform, CornerPin, Dot, NoOp, Switch), the original image is sampled only once." \

#define kPluginSTMapIdentifier "net.sf.openfx.STMap"


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

#define kParamChannelU "channelU"
#define kParamChannelULabel "U Channel"
#define kParamChannelUHint "Input channel for U from UV"

#define kParamChannelV "channelV"
#define kParamChannelVLabel "V Channel"
#define kParamChannelVHint "Input channel for V from UV"

#define kParamChannelOptionR "UV.r"
#define kParamChannelOptionRHint "R channel from UV"
#define kParamChannelOptionG "UV.g"
#define kParamChannelOptionGHint "G channel from UV"
#define kParamChannelOptionB "UV.b"
#define kParamChannelOptionBHint "B channel from UV"
#define kParamChannelOptionA "UV.a"
#define kParamChannelOptionAHint "A channel from UV"
#define kParamChannelOption0 "0"
#define kParamChannelOption0Hint "0 constant channel"
#define kParamChannelOption1 "1"
#define kParamChannelOption1Hint "1 constant channel"

enum InputChannelEnum {
    eInputChannelR = 0,
    eInputChannelG,
    eInputChannelB,
    eInputChannelA,
    eInputChannel0,
    eInputChannel1,
};

#define kParamWrapU "wrapU"
#define kParamWrapULabel "U Wrap Mode"
#define kParamWrapUHint "Wrap mode for U coordinate."

#define kParamWrapV "wrapV"
#define kParamWrapVLabel "V Wrap Mode"
#define kParamWrapVHint "Wrap mode for V coordinate."

#define kParamWrapOptionClamp "Clamp"
#define kParamWrapOptionClampHint "Texture edges are stretched indefinitely."
#define kParamWrapOptionRepeat "Repeat"
#define kParamWrapOptionRepeatHint "Texture is repeated."
#define kParamWrapOptionMirror "Mirror"
#define kParamWrapOptionMirrorHint "Texture is mirrored alternatively."

enum WrapEnum {
    eWrapClamp = 0,
    eWrapRepeat,
    eWrapMirror,
};

#define kClipUV "UV"

#define kParamUVOffset "uvOffset"
#define kParamUVOffsetLabel "UV Offset"
#define kParamUVOffsetHint "Offset to apply to the U and V channel (useful if these were stored in a file that cannot handle negative numbers)"

#define kParamUVScale "uvScale"
#define kParamUVScaleLabel "UV Scale"
#define kParamUVScaleHint "Scale factor to apply to the U and V channel (useful if these were stored in a file that can only store integer values)"

using namespace OFX;

class IDistortProcessorBase : public OFX::ImageProcessor
{
protected:
    const OFX::Image *_srcImg;
    const OFX::Image *_uvImg;
    const OFX::Image *_maskImg;
    bool _processR;
    bool _processG;
    bool _processB;
    bool _processA;
    bool _transformIsIdentity;
    OFX::Matrix3x3 _srcTransformInverse;
    InputChannelEnum _uChannel;
    InputChannelEnum _vChannel;
    double _uOffset;
    double _vOffset;
    double _uScale;
    double _vScale;
    WrapEnum _uWrap;
    WrapEnum _vWrap;
    bool _blackOutside;
    bool _doMasking;
    double _mix;
    bool _maskInvert;

public:

    IDistortProcessorBase(OFX::ImageEffect &instance)
    : OFX::ImageProcessor(instance)
    , _srcImg(0)
    , _maskImg(0)
    , _processR(true)
    , _processG(true)
    , _processB(true)
    , _processA(false)
    , _transformIsIdentity(true)
    , _srcTransformInverse()
    , _uOffset(0.)
    , _vOffset(0.)
    , _uScale(1.)
    , _vScale(1.)
    , _uWrap(eWrapClamp)
    , _vWrap(eWrapClamp)
    , _blackOutside(false)
    , _doMasking(false)
    , _mix(1.)
    , _maskInvert(false)
    {
    }

    void setSrcImgs(const OFX::Image *src, const OFX::Image *uv) {_srcImg = src; _uvImg = uv;}

    void setMaskImg(const OFX::Image *v, bool maskInvert) { _maskImg = v; _maskInvert = maskInvert; }

    void doMasking(bool v) {_doMasking = v;}

    void setValues(bool processR,
                   bool processG,
                   bool processB,
                   bool processA,
                   bool transformIsIdentity,
                   const OFX::Matrix3x3 &srcTransformInverse,
                   InputChannelEnum uChannel,
                   InputChannelEnum vChannel,
                   double uOffset,
                   double vOffset,
                   double uScale,
                   double vScale,
                   WrapEnum uWrap,
                   WrapEnum vWrap,
                   bool blackOutside,
                   double mix)
    {
        _processR = processR;
        _processG = processG;
        _processB = processB;
        _processA = processA;
        _transformIsIdentity = transformIsIdentity;
        _srcTransformInverse = srcTransformInverse;
        _uChannel = uChannel;
        _vChannel = vChannel;
        _uOffset = uOffset;
        _vOffset = vOffset;
        _uScale = uScale;
        _vScale = vScale;
        _uWrap = uWrap;
        _vWrap = vWrap;
        _blackOutside = blackOutside;
        _mix = mix;
    }

private:
};



// The "filter" and "clamp" template parameters allow filter-specific optimization
// by the compiler, using the same generic code for all filters.
template <class PIX, int nComponents, int maxValue, bool isSTMap, FilterEnum filter, bool clamp>
class IDistortProcessor : public IDistortProcessorBase
{
public:
    IDistortProcessor(OFX::ImageEffect &instance)
    : IDistortProcessorBase(instance)
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

    void
    compFromChannel(InputChannelEnum channel, OFX::Image const **img, int *comp)
    {
        switch (channel) {
            case eInputChannelR:
                if (_uvImg) {
                    if (nComponents >= 3) {
                        *img = _uvImg;
                        *comp = 0;
                    }
                }
                break;
            case eInputChannelG:
                if (_uvImg) {
                    if (nComponents >= 3) {
                        *img = _uvImg;
                        *comp = 1;
                    }
                }
                break;
            case eInputChannelB:
                if (_uvImg) {
                    if (nComponents >= 3) {
                        *img = _uvImg;
                        *comp = 2;
                    }
                }
                break;
            case eInputChannelA:
                if (_uvImg) {
                    if (nComponents >= 4) {
                        *img = _uvImg;
                        *comp = 3;
                    } else if (nComponents == 1) {
                        *img = _uvImg;
                        *comp = 0;
                    }
                }
                break;
            case eInputChannel0:
                *img = 0;
                *comp = 0;
                break;
            case eInputChannel1:
                *img = 0;
                *comp = 1;
                break;
        }
    }

    static inline double wrap(double x, WrapEnum wrap)
    {
        switch(wrap) {
            case eWrapClamp:
                return x;
            case eWrapRepeat:
                return x - std::floor(x);
            case eWrapMirror: {
                double x2 = x/2 - std::floor(x/2);
                return (x2 <= 0.5) ? (2 * x2) : (2 - 2 * x2);
            }
        }
    }

    template<bool processR, bool processG, bool processB, bool processA>
    void process(const OfxRectI& procWindow)
    {
        assert(nComponents == 1 || nComponents == 3 || nComponents == 4);
        assert(_dstImg);
        const OFX::Image* uImg = 0;
        const OFX::Image* vImg = 0;
        int uComp = 0;
        int vComp = 0;
        compFromChannel(_uChannel, &uImg, &uComp);
        compFromChannel(_vChannel, &vImg, &vComp);
        int srcx1 = 0, srcx2 = 1, srcy1 = 0, srcy2 = 0;
        if (isSTMap && _srcImg) {
            const OfxRectI& srcBounds = _srcImg->getBounds();
            srcx1 = srcBounds.x1;
            srcx2 = srcBounds.x2;
            srcy1 = srcBounds.y1;
            srcy2 = srcBounds.y2;
        }
        float tmpPix[4];
        for (int y = procWindow.y1; y < procWindow.y2; y++) {
            if (_effect.abort()) {
                break;
            }

            PIX *dstPix = (PIX *) _dstImg->getPixelAddress(procWindow.x1, y);

            for (int x = procWindow.x1; x < procWindow.x2; x++) {
                const PIX *uvPix = (const PIX *)  (_uvImg ? _uvImg->getPixelAddress(x, y) : 0);
                double sx, sy;
                if (isSTMap) {
                    double u = ((uImg ? (uvPix ? uvPix[uComp] : PIX()) : uComp) - _uOffset) * _uScale;
                    double v = ((vImg ? (uvPix ? uvPix[vComp] : PIX()) : vComp) - _vOffset) * _vScale;
                    // TODO: compute gradients before wrapping, scale gradients by (srcx2 - srcx1)
                    // for each gradient, take the smallest of the left and right derivatives

                    u = wrap(u, _uWrap);
                    v = wrap(v, _vWrap);
                    sx = srcx1 + u * (srcx2 - srcx1) - 0.5;
                    sy = srcy1 + v * (srcy2 - srcy1) - 0.5;
                } else {
                    sx = x + ((uImg ? (uvPix ? uvPix[uComp] : PIX()) : uComp) - _uOffset) * _uScale;
                    sy = y + ((vImg ? (uvPix ? uvPix[vComp] : PIX()) : vComp) - _vOffset) * _vScale;
                }
                // add 0.5 to get the coords of the pixel center
                double fx, fy;
                if (_transformIsIdentity) {
                    fx = sx + 0.5;
                    fy = sy + 0.5;
                } else {
                    double fz = _srcTransformInverse.g*sx + _srcTransformInverse.h*sy + _srcTransformInverse.i;
                    if (fz == 0) {
                        fx = std::numeric_limits<double>::infinity();
                        fy = std::numeric_limits<double>::infinity();
                    } else {
                        fx = (_srcTransformInverse.a*sx + _srcTransformInverse.b*sy + _srcTransformInverse.c)/fz + 0.5;
                        fy = (_srcTransformInverse.d*sx + _srcTransformInverse.e*sy + _srcTransformInverse.f)/fz + 0.5;
                    }
                }

                // TODO: ofxsFilterInterpolate2DSuper
                ofxsFilterInterpolate2D<PIX,nComponents,filter,clamp>(fx, fy, _srcImg, _blackOutside, tmpPix);
                ofxsMaskMix<PIX, nComponents, maxValue, true>(tmpPix, x, y, _srcImg, _doMasking, _maskImg, (float)_mix, _maskInvert, dstPix);
                // copy back original values from unprocessed channels
                if (nComponents == 1) {
                    if (!processA) {
                        const PIX *srcPix = (const PIX *)  (_srcImg ? _srcImg->getPixelAddress(x, y) : 0);
                        dstPix[0] = srcPix ? srcPix[0] : PIX();
                    }
                } else if (nComponents == 3 || nComponents == 4) {
                    const PIX *srcPix = 0;
                    if (!processR || !processG || !processB || (!processA && nComponents == 4)) {
                        srcPix = (const PIX *)  (_srcImg ? _srcImg->getPixelAddress(x, y) : 0);
                    }
                    if (!processR) {
                        dstPix[0] = srcPix ? srcPix[0] : PIX();
                    }
                    if (!processG) {
                        dstPix[1] = srcPix ? srcPix[1] : PIX();
                    }
                    if (!processB) {
                        dstPix[2] = srcPix ? srcPix[2] : PIX();
                    }
                    if (!processA && nComponents == 4) {
                        dstPix[3] = srcPix ? srcPix[3] : PIX();
                    }
                }
                // increment the dst pixel
                dstPix += nComponents;
            }
        }
    }
};


////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class IDistortPlugin : public OFX::ImageEffect
{
public:
    /** @brief ctor */
    IDistortPlugin(OfxImageEffectHandle handle, bool isSTMap)
    : ImageEffect(handle)
    , _dstClip(0)
    , _srcClip(0)
    , _uvClip(0)
    , _maskClip(0)
    , _processR(0)
    , _processG(0)
    , _processB(0)
    , _processA(0)
    , _uChannel(0)
    , _vChannel(0)
    , _uvOffset(0)
    , _uvScale(0)
    , _uWrap(0)
    , _vWrap(0)
    , _filter(0)
    , _clamp(0)
    , _blackOutside(0)
    , _mix(0)
    , _maskInvert(0)
    , _isSTMap(isSTMap)
    {
        _dstClip = fetchClip(kOfxImageEffectOutputClipName);
        assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB || _dstClip->getPixelComponents() == ePixelComponentRGBA || _dstClip->getPixelComponents() == ePixelComponentAlpha));
        _srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
        assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB || _srcClip->getPixelComponents() == ePixelComponentRGBA|| _srcClip->getPixelComponents() == ePixelComponentAlpha));
        _uvClip = fetchClip(kClipUV);
        assert(_uvClip && (_uvClip->getPixelComponents() == ePixelComponentRGB || _uvClip->getPixelComponents() == ePixelComponentRGBA || _uvClip->getPixelComponents() == ePixelComponentAlpha));
        _maskClip = getContext() == OFX::eContextFilter ? NULL : fetchClip(getContext() == OFX::eContextPaint ? "Brush" : "Mask");
        assert(!_maskClip || _maskClip->getPixelComponents() == ePixelComponentAlpha);
        _processR = fetchBooleanParam(kParamProcessR);
        _processG = fetchBooleanParam(kParamProcessG);
        _processB = fetchBooleanParam(kParamProcessB);
        _processA = fetchBooleanParam(kParamProcessA);
        assert(_processR && _processG && _processB && _processA);
        _uChannel = fetchChoiceParam(kParamChannelU);
        _vChannel = fetchChoiceParam(kParamChannelV);
        _uvOffset = fetchDouble2DParam(kParamUVOffset);
        _uvScale = fetchDouble2DParam(kParamUVScale);
        assert(_uChannel && _vChannel && _uvOffset && _uvScale);
        if (_isSTMap) {
            _uWrap = fetchChoiceParam(kParamWrapU);
            _vWrap = fetchChoiceParam(kParamWrapV);
            assert(_uWrap && _vWrap);
        }
        _filter = fetchChoiceParam(kParamFilterType);
        _clamp = fetchBooleanParam(kParamFilterClamp);
        _blackOutside = fetchBooleanParam(kParamFilterBlackOutside);
        assert(_filter && _clamp && _blackOutside);
        _mix = fetchDoubleParam(kParamMix);
        _maskInvert = fetchBooleanParam(kParamMaskInvert);
        assert(_mix && _maskInvert);
    }

private:
    // override the roi call
    virtual void getRegionsOfInterest(const OFX::RegionsOfInterestArguments &args, OFX::RegionOfInterestSetter &rois) OVERRIDE FINAL;

    virtual bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;

    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

    /* internal render function */
    template <class PIX, int nComponents, int maxValue, bool isSTMap>
    void renderInternalForBitDepth(const OFX::RenderArguments &args);

    template <int nComponents, bool isSTMap>
    void renderInternal(const OFX::RenderArguments &args, OFX::BitDepthEnum dstBitDepth);
    
    /* set up and run a processor */
    void setupAndProcess(IDistortProcessorBase &, const OFX::RenderArguments &args);

    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime) OVERRIDE FINAL;

private:
    // do not need to delete these, the ImageEffect is managing them for us
    OFX::Clip *_dstClip;
    OFX::Clip *_srcClip;
    OFX::Clip *_uvClip;
    OFX::Clip *_maskClip;
    OFX::BooleanParam* _processR;
    OFX::BooleanParam* _processG;
    OFX::BooleanParam* _processB;
    OFX::BooleanParam* _processA;
    OFX::ChoiceParam* _uChannel;
    OFX::ChoiceParam* _vChannel;
    OFX::Double2DParam *_uvOffset;
    OFX::Double2DParam *_uvScale;
    OFX::ChoiceParam* _uWrap;
    OFX::ChoiceParam* _vWrap;
    OFX::ChoiceParam* _filter;
    OFX::BooleanParam* _clamp;
    OFX::BooleanParam* _blackOutside;
    OFX::DoubleParam* _mix;
    OFX::BooleanParam* _maskInvert;
    bool _isSTMap;
};


////////////////////////////////////////////////////////////////////////////////
/** @brief render for the filter */

////////////////////////////////////////////////////////////////////////////////
// basic plugin render function, just a skelington to instantiate templates from

/* set up and run a processor */
void
IDistortPlugin::setupAndProcess(IDistortProcessorBase &processor, const OFX::RenderArguments &args)
{
    const double time = args.time;
    std::auto_ptr<OFX::Image> dst(_dstClip->fetchImage(time));
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
    std::auto_ptr<const OFX::Image> src((_srcClip && _srcClip->isConnected()) ?
                                        _srcClip->fetchImage(time) : 0);
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

    std::auto_ptr<const OFX::Image> uv((_uvClip && _uvClip->isConnected()) ?
                                        _uvClip->fetchImage(time) : 0);
    if (uv.get()) {
        if (uv->getRenderScale().x != args.renderScale.x ||
            uv->getRenderScale().y != args.renderScale.y ||
            (uv->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && uv->getField() != args.fieldToRender)) {
            setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
            OFX::throwSuiteStatusException(kOfxStatFailed);
        }
        OFX::BitDepthEnum    uvBitDepth      = uv->getPixelDepth();
        OFX::PixelComponentEnum uvComponents = uv->getPixelComponents();
        if (uvBitDepth != dstBitDepth || uvComponents != dstComponents) {
            OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
        }
    }
    std::auto_ptr<const OFX::Image> mask((getContext() != OFX::eContextFilter && _maskClip && _maskClip->isConnected()) ?
                                         _maskClip->fetchImage(time) : 0);
    // do we do masking
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
        _maskInvert->getValueAtTime(time, maskInvert);
        processor.doMasking(true);
        processor.setMaskImg(mask.get(), maskInvert);
    }

    // set the images
    processor.setDstImg(dst.get());
    processor.setSrcImgs(src.get(), uv.get());
    // set the render window
    processor.setRenderWindow(args.renderWindow);

    bool processR, processG, processB, processA;
    _processR->getValueAtTime(time, processR);
    _processG->getValueAtTime(time, processG);
    _processB->getValueAtTime(time, processB);
    _processA->getValueAtTime(time, processA);
    int uChannel_i, vChannel_i;
    _uChannel->getValueAtTime(time, uChannel_i);
    _vChannel->getValueAtTime(time, vChannel_i);
    InputChannelEnum uChannel = (InputChannelEnum)uChannel_i;
    InputChannelEnum vChannel = (InputChannelEnum)vChannel_i;
    double uOffset, vOffset;
    _uvOffset->getValueAtTime(time, uOffset, vOffset);
    double uScale, vScale;
    _uvScale->getValueAtTime(time, uScale, vScale);
    int uWrap_i;
    _uWrap->getValueAtTime(time, uWrap_i);
    WrapEnum uWrap = (WrapEnum)uWrap_i;
    int vWrap_i;
    _vWrap->getValueAtTime(time, vWrap_i);
    WrapEnum vWrap = (WrapEnum)vWrap_i;
    bool blackOutside;
    _blackOutside->getValueAtTime(time, blackOutside);
    double mix;
    _mix->getValueAtTime(time, mix);

    bool transformIsIdentity = src.get() ? src->getTransformIsIdentity() : true;
    OFX::Matrix3x3 srcTransformInverse;
    if (!transformIsIdentity) {
        double srcTransform[9]; // transform to apply to the source image, in pixel coordinates, from source to destination
        src->getTransform(srcTransform);
        OFX::Matrix3x3 srcTransformMat;
        srcTransformMat.a = srcTransform[0];
        srcTransformMat.b = srcTransform[1];
        srcTransformMat.c = srcTransform[2];
        srcTransformMat.d = srcTransform[3];
        srcTransformMat.e = srcTransform[4];
        srcTransformMat.f = srcTransform[5];
        srcTransformMat.g = srcTransform[6];
        srcTransformMat.h = srcTransform[7];
        srcTransformMat.i = srcTransform[8];
        // invert it
        double det = ofxsMatDeterminant(srcTransformMat);
        if (det != 0.) {
            srcTransformInverse = ofxsMatInverse(srcTransformMat, det);
        } else {
            transformIsIdentity = true; // no transform
        }
    }
    if (!_isSTMap) {
        uScale *= args.renderScale.x;
        vScale *= args.renderScale.y;
    }
    processor.setValues(processR, processG, processB, processA,
                        transformIsIdentity, srcTransformInverse,
                        uChannel, vChannel,
                        uOffset, vOffset,
                        uScale, vScale,
                        uWrap, vWrap,
                        blackOutside, mix);

    // Call the base class process member, this will call the derived templated process code
    processor.process();
}

template <class PIX, int nComponents, int maxValue, bool isSTMap>
void
IDistortPlugin::renderInternalForBitDepth(const OFX::RenderArguments &args)
{
    const double time = args.time;
    int filter = eFilterCubic;
    if (_filter) {
        _filter->getValueAtTime(time, filter);
    }
    bool clamp = false;
    if (_clamp) {
        _clamp->getValueAtTime(time, clamp);
    }

    // as you may see below, some filters don't need explicit clamping, since they are
    // "clamped" by construction.
    switch ( (FilterEnum)filter ) {
        case eFilterImpulse: {
            IDistortProcessor<PIX, nComponents, maxValue, isSTMap, eFilterImpulse, false> fred(*this);
            setupAndProcess(fred, args);
            break;
        }
        case eFilterBilinear: {
            IDistortProcessor<PIX, nComponents, maxValue, isSTMap, eFilterBilinear, false> fred(*this);
            setupAndProcess(fred, args);
            break;
        }
        case eFilterCubic: {
            IDistortProcessor<PIX, nComponents, maxValue, isSTMap, eFilterCubic, false> fred(*this);
            setupAndProcess(fred, args);
            break;
        }
        case eFilterKeys:
            if (clamp) {
                IDistortProcessor<PIX, nComponents, maxValue, isSTMap, eFilterKeys, true> fred(*this);
                setupAndProcess(fred, args);
            } else {
                IDistortProcessor<PIX, nComponents, maxValue, isSTMap, eFilterKeys, false> fred(*this);
                setupAndProcess(fred, args);
            }
            break;
        case eFilterSimon:
            if (clamp) {
                IDistortProcessor<PIX, nComponents, maxValue, isSTMap, eFilterSimon, true> fred(*this);
                setupAndProcess(fred, args);
            } else {
                IDistortProcessor<PIX, nComponents, maxValue, isSTMap, eFilterSimon, false> fred(*this);
                setupAndProcess(fred, args);
            }
            break;
        case eFilterRifman:
            if (clamp) {
                IDistortProcessor<PIX, nComponents, maxValue, isSTMap, eFilterRifman, true> fred(*this);
                setupAndProcess(fred, args);
            } else {
                IDistortProcessor<PIX, nComponents, maxValue, isSTMap, eFilterRifman, false> fred(*this);
                setupAndProcess(fred, args);
            }
            break;
        case eFilterMitchell:
            if (clamp) {
                IDistortProcessor<PIX, nComponents, maxValue, isSTMap, eFilterMitchell, true> fred(*this);
                setupAndProcess(fred, args);
            } else {
                IDistortProcessor<PIX, nComponents, maxValue, isSTMap, eFilterMitchell, false> fred(*this);
                setupAndProcess(fred, args);
            }
            break;
        case eFilterParzen: {
            IDistortProcessor<PIX, nComponents, maxValue, isSTMap, eFilterParzen, false> fred(*this);
            setupAndProcess(fred, args);
            break;
        }
        case eFilterNotch: {
            IDistortProcessor<PIX, nComponents, maxValue, isSTMap, eFilterNotch, false> fred(*this);
            setupAndProcess(fred, args);
            break;
        }
    } // switch
} // renderInternalForBitDepth

// the internal render function
template <int nComponents, bool isSTMap>
void
IDistortPlugin::renderInternal(const OFX::RenderArguments &args,
                                   OFX::BitDepthEnum dstBitDepth)
{
    switch (dstBitDepth) {
        case OFX::eBitDepthUByte:
            renderInternalForBitDepth<unsigned char, nComponents, 255, isSTMap>(args);
            break;
        case OFX::eBitDepthUShort:
            renderInternalForBitDepth<unsigned short, nComponents, 65535, isSTMap>(args);
            break;
        case OFX::eBitDepthFloat:
            renderInternalForBitDepth<float, nComponents, 1, isSTMap>(args);
            break;
        default:
            OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
    }
}

// the overridden render function
void
IDistortPlugin::render(const OFX::RenderArguments &args)
{

    // instantiate the render code based on the pixel depth of the dst clip
    OFX::BitDepthEnum       dstBitDepth    = _dstClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents  = _dstClip->getPixelComponents();

    assert(kSupportsMultipleClipPARs   || !_srcClip || _srcClip->getPixelAspectRatio() == _dstClip->getPixelAspectRatio());
    assert(kSupportsMultipleClipDepths || !_srcClip || _srcClip->getPixelDepth()       == _dstClip->getPixelDepth());
    assert(dstComponents == OFX::ePixelComponentAlpha || dstComponents == OFX::ePixelComponentRGB || dstComponents == OFX::ePixelComponentRGBA);
    if (dstComponents == OFX::ePixelComponentRGBA) {
        if (_isSTMap) {
            renderInternal<4, true>(args, dstBitDepth);
        } else {
            renderInternal<4, false>(args, dstBitDepth);
        }
    } else if (dstComponents == OFX::ePixelComponentRGB) {
        if (_isSTMap) {
            renderInternal<3, true>(args, dstBitDepth);
        } else {
            renderInternal<3, false>(args, dstBitDepth);
        }
    } else {
        assert(dstComponents == OFX::ePixelComponentAlpha);
        if (_isSTMap) {
            renderInternal<1, true>(args, dstBitDepth);
        } else {
            renderInternal<1, false>(args, dstBitDepth);
        }
    }
}


bool
IDistortPlugin::isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &/*identityTime*/)
{
    const double time = args.time;
    if (!_uvClip || !_uvClip->isConnected()) {
        identityClip = _srcClip;
        return true;
    }

    double mix;
    _mix->getValueAtTime(time, mix);

    if (mix == 0. /*|| (!processR && !processG && !processB && !processA)*/) {
        identityClip = _srcClip;
        return true;
    }

    bool processR, processG, processB, processA;
    _processR->getValueAtTime(time, processR);
    _processG->getValueAtTime(time, processG);
    _processB->getValueAtTime(time, processB);
    _processA->getValueAtTime(time, processA);
    if (!processR && !processG && !processB && !processA) {
        identityClip = _srcClip;
        return true;
    }
    return false;
}

// override the roi call
// Required if the plugin requires a region from the inputs which is different from the rendered region of the output.
// (this is the case here)
void
IDistortPlugin::getRegionsOfInterest(const OFX::RegionsOfInterestArguments &args,
                                     OFX::RegionOfInterestSetter &rois)
{
    const double time = args.time;
    if (!_srcClip) {
        return;
    }
    // ask for full RoD of srcClip
    const OfxRectD& srcRod = _srcClip->getRegionOfDefinition(time);
    rois.setRegionOfInterest(*_srcClip, srcRod);
    // only ask for the renderWindow (intersected with the RoD) from uvClip
    if (_uvClip) {
        OfxRectD uvRoI = _uvClip->getRegionOfDefinition(time);
        MergeImages2D::rectIntersection(uvRoI, args.regionOfInterest, &uvRoI);
        rois.setRegionOfInterest(*_uvClip, uvRoI);
    }
}


bool
IDistortPlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    const double time = args.time;
    if (_isSTMap) {
        if (_uvClip) {
            // IDistort: RoD is the same as uv map
            rod = _uvClip->getRegionOfDefinition(time);
            return true;
        }
    } else {
        if (_srcClip) {
            // IDistort: RoD is the same as srcClip
            rod = _srcClip->getRegionOfDefinition(time);
            return true;
        }
    }
    return false;
}

//mDeclarePluginFactory(IDistortPluginFactory, {}, {});
template<bool isSTMap>
class IDistortPluginFactory : public OFX::PluginFactoryHelper<IDistortPluginFactory<isSTMap> >
{
public:
    IDistortPluginFactory<isSTMap>(const std::string& id, unsigned int verMaj, unsigned int verMin):OFX::PluginFactoryHelper<IDistortPluginFactory>(id, verMaj, verMin){}
    virtual void describe(OFX::ImageEffectDescriptor &desc);
    virtual void describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context);
    virtual OFX::ImageEffect* createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context);
};

template<bool isSTMap>
void IDistortPluginFactory<isSTMap>::describe(OFX::ImageEffectDescriptor &desc)
{
    // basic labels
    if (isSTMap) {
        desc.setLabel(kPluginSTMapName);
        desc.setPluginGrouping(kPluginSTMapGrouping);
        desc.setPluginDescription(kPluginSTMapDescription);
    } else {
        desc.setLabel(kPluginIDistortName);
        desc.setPluginGrouping(kPluginIDistortGrouping);
        desc.setPluginDescription(kPluginIDistortDescription);
    }

    //desc.addSupportedContext(eContextFilter);
    desc.addSupportedContext(eContextGeneral);
    //desc.addSupportedContext(eContextPaint);
    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);

    // set a few flags
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(kSupportsMultiResolution);
    desc.setSupportsTiles(kSupportsTiles);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
    desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
    desc.setRenderThreadSafety(kRenderThreadSafety);
}

static void
addInputChannelOptions(ChoiceParamDescriptor* channel, InputChannelEnum def)
{
    assert(channel->getNOptions() == eInputChannelR);
    channel->appendOption(kParamChannelOptionR, kParamChannelOptionRHint);
    assert(channel->getNOptions() == eInputChannelG);
    channel->appendOption(kParamChannelOptionG, kParamChannelOptionGHint);
    assert(channel->getNOptions() == eInputChannelB);
    channel->appendOption(kParamChannelOptionB, kParamChannelOptionBHint);
    assert(channel->getNOptions() == eInputChannelA);
    channel->appendOption(kParamChannelOptionA, kParamChannelOptionAHint);
    assert(channel->getNOptions() == eInputChannel0);
    channel->appendOption(kParamChannelOption0, kParamChannelOption0Hint);
    assert(channel->getNOptions() == eInputChannel1);
    channel->appendOption(kParamChannelOption1, kParamChannelOption1Hint);
    channel->setDefault(def);
}

static void
addWrapOptions(ChoiceParamDescriptor* channel, WrapEnum def)
{
    assert(channel->getNOptions() == eWrapClamp);
    channel->appendOption(kParamWrapOptionClamp, kParamWrapOptionClampHint);
    assert(channel->getNOptions() == eWrapRepeat);
    channel->appendOption(kParamWrapOptionRepeat, kParamWrapOptionRepeatHint);
    assert(channel->getNOptions() == eWrapMirror);
    channel->appendOption(kParamWrapOptionMirror, kParamWrapOptionMirrorHint);
    channel->setDefault(def);
}

template<bool isSTMap>
void IDistortPluginFactory<isSTMap>::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
{
    // create the mandated source clip
    ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setCanTransform(true);
    srcClip->setIsMask(false);

    // create the uv clip
    ClipDescriptor *uvClip = desc.defineClip(kClipUV);
    uvClip->addSupportedComponent(ePixelComponentRGBA);
    uvClip->addSupportedComponent(ePixelComponentRGB);
    uvClip->addSupportedComponent(ePixelComponentAlpha);
    uvClip->setTemporalClipAccess(false);
    uvClip->setSupportsTiles(kSupportsTiles);
    uvClip->setIsMask(false);

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
        if (context == eContextGeneral)
            maskClip->setOptional(true);
        maskClip->setSupportsTiles(kSupportsTiles);
        maskClip->setIsMask(true);
    }

    // make some pages and to things in
    PageParamDescriptor *page = desc.definePageParam("Controls");

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

    {
        ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamChannelU);
        param->setLabel(kParamChannelULabel);
        param->setHint(kParamChannelUHint);
        param->setLayoutHint(eLayoutHintNoNewLine);
        addInputChannelOptions(param, eInputChannelR);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamChannelV);
        param->setLabel(kParamChannelVLabel);
        param->setHint(kParamChannelVHint);
        addInputChannelOptions(param, eInputChannelG);
        if (page) {
            page->addChild(*param);
        }
    }

    {
        Double2DParamDescriptor *param = desc.defineDouble2DParam(kParamUVOffset);
        param->setLabel(kParamUVOffsetLabel);
        param->setHint(kParamUVOffsetHint);
        param->setDefault(0., 0.);
        param->setDisplayRange(0., 0., 1., 1.);
        param->setDimensionLabels("U", "V");
        if (page) {
            page->addChild(*param);
        }
    }
    {
        Double2DParamDescriptor *param = desc.defineDouble2DParam(kParamUVScale);
        param->setLabel(kParamUVScaleLabel);
        param->setHint(kParamUVScaleHint);
        param->setDoubleType(eDoubleTypeScale);
        param->setDefault(1., 1.);
        param->setDisplayRange(0., 0., 100., 100.);
        param->setDimensionLabels("U", "V");
        if (page) {
            page->addChild(*param);
        }
    }

    if (isSTMap) {
        {
            ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamWrapU);
            param->setLabel(kParamWrapULabel);
            param->setHint(kParamWrapUHint);
            param->setLayoutHint(eLayoutHintNoNewLine);
            addWrapOptions(param, eWrapClamp);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamWrapV);
            param->setLabel(kParamWrapVLabel);
            param->setHint(kParamWrapVHint);
            addWrapOptions(param, eWrapClamp);
            if (page) {
                page->addChild(*param);
            }
        }
    }

    ofxsFilterDescribeParamsInterpolate2D(desc, page, false);
    ofxsMaskMixDescribeParams(desc, page);
}

template<bool isSTMap>
OFX::ImageEffect* IDistortPluginFactory<isSTMap>::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum /*context*/)
{
    return new IDistortPlugin(handle, isSTMap);
}

void getIDistortPluginID(OFX::PluginFactoryArray &ids)
{
    {
        static IDistortPluginFactory<false> p(kPluginIDistortIdentifier, kPluginVersionMajor, kPluginVersionMinor);
        ids.push_back(&p);
    }
    {
        static IDistortPluginFactory<true> p(kPluginSTMapIdentifier, kPluginVersionMajor, kPluginVersionMinor);
        ids.push_back(&p);
    }
}
