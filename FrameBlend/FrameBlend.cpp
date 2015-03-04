/*
 OFX FrameBlend plugin.

 Copyright (C) 2014 INRIA
 Author: Frederic Devernay <frederic.devernay@inria.fr>

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


 This plugin is based on:

 OFX retimer example plugin, a plugin that illustrates the use of the OFX Support library.

 This will not work very well on fielded imagery.

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

// TODO:
// - fetch Images and process them one by one
// - compute count only if necessary (i.e. it is asked on output or operation is average)
// - show progress

#include "FrameBlend.h"

#include <cmath> // for floor
#include <climits> // for INT_MAX
#include <cassert>

#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"

#include "ofxsPixelProcessor.h"
#include "ofxsMaskMix.h"
#include "ofxsMacros.h"

#define kPluginName "FrameBlendOFX"
#define kPluginGrouping "Time"
#define kPluginDescription \
"Blend frames of the input clip.\n" \
"If a foreground matte is connected, only pixels with a negative or zero (<= 0) foreground value are taken into account.\n" \
"The number of values used to compute each pixel can be output to the alpha channel."

#define kPluginIdentifier "net.sf.openfx.FrameBlend"
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

#define kParamFrameRangeName  "frameRange"
#define kParamFrameRangeLabel "Frame Range"
#define kParamFrameRangeHint  "Range of frames which are to be blended together. Frame range is absolute if \"absolute\" is checked, else relative."

#define kParamAbsoluteName  "absolute"
#define kParamAbsoluteLabel "Absolute"
#define kParamAbsoluteHint  "Use an absolute frame range. If the frame range is not animated or is not an expression, then all output images will be the same."

#define kParamInputRangeName  "inputRange"
#define kParamInputRangeLabel "Input Range"
#define kParamInputRangeHint  "Set the frame range to the input range. This can be used, combined with a foreground matte, to produce a clean background plate."

#define kParamFrameIntervalName  "frameInterval"
#define kParamFrameIntervalLabel "Frame Interval"
#define kParamFrameIntervalHint  "Interval (in frames) between frames to process. 1 means to process every frame in the range. The first frame processed is the lower bound of the range. Can be used to reduce processing time or memory usage."

#define kParamOperation "operation"
#define kParamOperationLabel "Operation"
#define kParamOperationHint \
"The operation used to compute the output image."
#define kParamOperationOptionAverage "Average"
#define kParamOperationOptionAverageHint "Output is the average of selected frames."
#define kParamOperationOptionMin "Min"
#define kParamOperationOptionMinHint "Output is the minimum of selected frames."
#define kParamOperationOptionMax "Max"
#define kParamOperationOptionMaxHint "Output is the maximum of selected frames."
#define kParamOperationOptionSum "Sum"
#define kParamOperationOptionSumHint "Output is the sum/addition of selected frames."
#define kParamOperationOptionProduct "Product"
#define kParamOperationOptionProductHint "Output is the product/multiplication of selected frames."
#define kParamOperationDefault eOperationAverage
enum OperationEnum {
    eOperationAverage,
    eOperationMin,
    eOperationMax,
    eOperationSum,
    eOperationProduct,
};


#define kParamOutputCountName  "outputCount"
#define kParamOutputCountLabel "Output Count to Alpha"
#define kParamOutputCountHint  "Output image count at each pixel to alpha."

#define kClipFgMName "FgM"

using namespace OFX;

class FrameBlendProcessorBase : public OFX::PixelProcessor
{
protected:
    const OFX::Image *_srcImg;
    std::vector<const OFX::Image*> _srcImgs;
    std::vector<const OFX::Image*> _fgMImgs;
    const OFX::Image *_maskImg;
    bool _processR;
    bool _processG;
    bool _processB;
    bool _processA;
    bool _outputCount;
    bool   _doMasking;
    double _mix;
    bool _maskInvert;

public:

    FrameBlendProcessorBase(OFX::ImageEffect &instance)
    : OFX::PixelProcessor(instance)
    , _srcImg(0)
    , _srcImgs(0)
    , _fgMImgs(0)
    , _maskImg(0)
    , _processR(true)
    , _processG(true)
    , _processB(true)
    , _processA(false)
    , _outputCount(false)
    , _doMasking(false)
    , _mix(1.)
    , _maskInvert(false)
    {
    }

    void setSrcImgs(const OFX::Image *src, const std::vector<const OFX::Image*> &v) {_srcImg = src; _srcImgs = v;}
    void setFgMImgs(const std::vector<const OFX::Image*> &v) {_fgMImgs = v;}

    void setMaskImg(const OFX::Image *v, bool maskInvert) { _maskImg = v; _maskInvert = maskInvert; }

    void doMasking(bool v) {_doMasking = v;}

    void setValues(bool processR,
                   bool processG,
                   bool processB,
                   bool processA,
                   bool outputCount,
                   double mix)
    {
        _processR = processR;
        _processG = processG;
        _processB = processB;
        _processA = processA;
        _outputCount = outputCount;
        _mix = mix;
    }

private:
};



template <class PIX, int nComponents, int maxValue, OperationEnum operation>
class FrameBlendProcessor : public FrameBlendProcessorBase
{
public:
    FrameBlendProcessor(OFX::ImageEffect &instance)
    : FrameBlendProcessorBase(instance)
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

    template<bool processR, bool processG, bool processB, bool processA>
    void process(const OfxRectI& procWindow)
    {
        assert(nComponents == 1 || nComponents == 3 || nComponents == 4);
        assert(_dstPixelData);
        assert(_srcImgs.size() == _fgMImgs.size());
        float tmpPix[nComponents];
        for (int y = procWindow.y1; y < procWindow.y2; y++) {
            if (_effect.abort()) {
                break;
            }

            PIX *dstPix = (PIX *) getDstPixelAddress(procWindow.x1, y);
            assert(dstPix);
            if (!dstPix) {
                // coverity[dead_error_line]
                continue;
            }
            float initVal = 0.;
            switch (operation) {
                case eOperationAverage:
                    initVal = 0.;
                    break;
                case eOperationMin:
                    initVal = std::numeric_limits<float>::infinity();
                    break;
                case eOperationMax:
                    initVal = -std::numeric_limits<float>::infinity();
                    break;
                case eOperationSum:
                    initVal = 0.;
                    break;
                case eOperationProduct:
                    initVal = 1.;
                    break;
            }

            for (int x = procWindow.x1; x < procWindow.x2; x++) {
                const PIX *srcPix = (const PIX *)  (_srcImg ? _srcImg->getPixelAddress(x, y) : 0);
                int count = 0;
                std::fill(tmpPix, tmpPix+nComponents, initVal);
                // accumulate
                for (unsigned i = 0; i < _srcImgs.size(); ++i) {
                    const PIX *fgMPix = (const PIX *)  (_fgMImgs[i] ? _fgMImgs[i]->getPixelAddress(x, y) : 0);
                    if (!fgMPix || *fgMPix <= 0) {
                        const PIX *srcPixi = (const PIX *)  (_srcImgs[i] ? _srcImgs[i]->getPixelAddress(x, y) : 0);
                        if (srcPixi) {
                            for (int c = 0; c < nComponents; ++c) {
                                switch (operation) {
                                    case eOperationAverage:
                                        tmpPix[c] += srcPixi[c];
                                        break;
                                    case eOperationMin:
                                        tmpPix[c] = std::min(tmpPix[c], (float)srcPixi[c]);
                                        break;
                                    case eOperationMax:
                                        tmpPix[c] = std::max(tmpPix[c], (float)srcPixi[c]);
                                        break;
                                    case eOperationSum:
                                        tmpPix[c] += srcPixi[c];
                                        break;
                                    case eOperationProduct:
                                        tmpPix[c] *= srcPixi[c];
                                        break;
                                }
                            }
                        }
                        ++count;
                    }
                }
                // copy back original values from unprocessed channels
                if (nComponents == 1) {
                    int c = 0;
                    if (_outputCount) {
                        tmpPix[c] = count;
                    } else {
                        tmpPix[c] = (operation == eOperationAverage) ? (count ? (tmpPix[c] / count) : 0) : tmpPix[c];
                    }
                } else if (3 <= nComponents && nComponents <= 4) {
                    for (int c = 0; c < 3; ++c) {
                        tmpPix[c] = (operation == eOperationAverage) ? (count ? (tmpPix[c] / count) : 0) : tmpPix[c];
                    }
                    if (nComponents >= 4) {
                        int c = nComponents - 1;
                        if (_outputCount) {
                            tmpPix[c] = count;
                        } else {
                            tmpPix[c] = (operation == eOperationAverage) ? (count ? (tmpPix[c] / count) : 0) : tmpPix[c];
                        }
                    }
                }
                // tmpPix is not normalized, it is within [0,maxValue]
                ofxsMaskMixPix<PIX,nComponents,maxValue,true>(tmpPix, x, y, srcPix, _doMasking,
                                                              _maskImg, _mix, _maskInvert,
                                                              dstPix);
                // copy back original values from unprocessed channels
                if (nComponents == 1) {
                    if (!processA) {
                        dstPix[0] = srcPix ? srcPix[0] : PIX();
                    }
                } else if (nComponents == 3 || nComponents == 4) {
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
class FrameBlendPlugin : public OFX::ImageEffect
{
public:
    /** @brief ctor */
    FrameBlendPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , _dstClip(0)
    , _srcClip(0)
    , _maskClip(0)
    , _fgMClip(0)
    , _processR(0)
    , _processG(0)
    , _processB(0)
    , _processA(0)
    , _frameRange(0)
    , _absolute(0)
    , _inputRange(0)
    , _frameInterval(0)
    , _operation(0)
    , _outputCount(0)
    , _mix(0)
    , _maskInvert(0)
    {
        _dstClip = fetchClip(kOfxImageEffectOutputClipName);
        assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB || _dstClip->getPixelComponents() == ePixelComponentRGBA));
        _srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
        assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB || _srcClip->getPixelComponents() == ePixelComponentRGBA));
        _maskClip = getContext() == OFX::eContextFilter ? NULL : fetchClip(getContext() == OFX::eContextPaint ? "Brush" : "Mask");
        assert(!_maskClip || _maskClip->getPixelComponents() == ePixelComponentAlpha);
        _fgMClip = fetchClip(kClipFgMName);
        assert(!_fgMClip || _fgMClip->getPixelComponents() == ePixelComponentAlpha);
        _processR = fetchBooleanParam(kParamProcessR);
        _processG = fetchBooleanParam(kParamProcessG);
        _processB = fetchBooleanParam(kParamProcessB);
        _processA = fetchBooleanParam(kParamProcessA);
        assert(_processR && _processG && _processB && _processA);
        _frameRange = fetchInt2DParam(kParamFrameRangeName);
        _absolute = fetchBooleanParam(kParamAbsoluteName);
        _inputRange = fetchPushButtonParam(kParamInputRangeName);
        _frameInterval = fetchIntParam(kParamFrameIntervalName);
        _operation = fetchChoiceParam(kParamOperation);
        _outputCount = fetchBooleanParam(kParamOutputCountName);
        assert(_frameRange && _absolute && _inputRange && _operation && _outputCount);
        _mix = fetchDoubleParam(kParamMix);
        _maskInvert = fetchBooleanParam(kParamMaskInvert);
        assert(_mix && _maskInvert);
    }

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

    /* set up and run a processor */
    void setupAndProcess(FrameBlendProcessorBase &, const OFX::RenderArguments &args);

    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime) OVERRIDE FINAL;

    /** Override the get frames needed action */
    virtual void getFramesNeeded(const OFX::FramesNeededArguments &args, OFX::FramesNeededSetter &frames) OVERRIDE FINAL;

    /** @brief called when a param has just had its value changed */
    virtual void changedParam(const InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

private:

    template<int nComponents>
    void renderForComponents(const OFX::RenderArguments &args);

    template <class PIX, int nComponents, int maxValue>
    void renderForBitDepth(const OFX::RenderArguments &args);

    template <class PIX, int nComponents, int maxValue, OperationEnum operation>
    void renderForOperation(const OFX::RenderArguments &args);

    // do not need to delete these, the ImageEffect is managing them for us
    OFX::Clip *_dstClip;
    OFX::Clip *_srcClip;
    OFX::Clip *_maskClip;
    OFX::Clip *_fgMClip;
    OFX::BooleanParam* _processR;
    OFX::BooleanParam* _processG;
    OFX::BooleanParam* _processB;
    OFX::BooleanParam* _processA;
    Int2DParam* _frameRange;
    BooleanParam* _absolute;
    PushButtonParam* _inputRange;
    IntParam* _frameInterval;
    ChoiceParam* _operation;
    BooleanParam* _outputCount;
    OFX::DoubleParam* _mix;
    OFX::BooleanParam* _maskInvert;
};


////////////////////////////////////////////////////////////////////////////////
/** @brief render for the filter */

////////////////////////////////////////////////////////////////////////////////
// basic plugin render function, just a skelington to instantiate templates from

namespace {
// Since we cannot hold a std::auto_ptr in the vector we must hold a raw pointer.
// To ensure that images are always freed even in case of exceptions, use a RAII class.
struct OptionalImagesHolder_RAII
{
    std::vector<const OFX::Image*> images;
    
    OptionalImagesHolder_RAII()
    : images()
    {
        
    }
    
    ~OptionalImagesHolder_RAII()
    {
        for (unsigned int i = 0; i < images.size(); ++i) {
            delete images[i];
        }
    }
};
}

/* set up and run a processor */
void
FrameBlendPlugin::setupAndProcess(FrameBlendProcessorBase &processor, const OFX::RenderArguments &args)
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
    // compute range
    bool absolute;
    _absolute->getValueAtTime(time, absolute);
    int min, max;
    _frameRange->getValueAtTime(time, min, max);
    if (min > max) {
        std::swap(min, max);
    }
    int interval;
    _frameInterval->getValueAtTime(time, interval);
    interval = std::max(1,interval);

    int n = (max + 1 - min) / interval;
    if (!absolute) {
        min += time;
        max += time;
    }
    // fetch the source images
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
    OptionalImagesHolder_RAII srcImgs;
    for (int i = 0; i < n; ++i) {
        if (abort()) {
            throwSuiteStatusException(kOfxStatFailed);
        }
        const OFX::Image* src = _srcClip ? _srcClip->fetchImage(min + i*interval) : 0;
        if (src) {
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
        srcImgs.images.push_back(src);
    }
    // fetch the foreground mattes
    OptionalImagesHolder_RAII fgMImgs;
    for (int i = 0; i < n; ++i) {
        if (abort()) {
            throwSuiteStatusException(kOfxStatFailed);
        }
        const OFX::Image* mask = (_fgMClip && _fgMClip->isConnected()) ? _fgMClip->fetchImage(min + i*interval) : 0;
        if (mask) {
            assert(_fgMClip->isConnected());
            if (mask->getRenderScale().x != args.renderScale.x ||
                mask->getRenderScale().y != args.renderScale.y ||
                (mask->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && mask->getField() != args.fieldToRender)) {
                setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
                OFX::throwSuiteStatusException(kOfxStatFailed);
            }
        }
        fgMImgs.images.push_back(mask);
    }
    // fetch the mask
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
    processor.setSrcImgs(src.get(), srcImgs.images);
    processor.setFgMImgs(fgMImgs.images);
    // set the render window
    processor.setRenderWindow(args.renderWindow);

    bool processR, processG, processB, processA;
    _processR->getValueAtTime(time, processR);
    _processG->getValueAtTime(time, processG);
    _processB->getValueAtTime(time, processB);
    _processA->getValueAtTime(time, processA);
    bool outputCount;
    _outputCount->getValueAtTime(time, outputCount);
    double mix;
    _mix->getValueAtTime(time, mix);
    processor.setValues(processR, processG, processB, processA,
                        outputCount, mix);

    // Call the base class process member, this will call the derived templated process code
    processor.process();
}

// the overridden render function
void
FrameBlendPlugin::render(const OFX::RenderArguments &args)
{
    OFX::PixelComponentEnum dstComponents  = _dstClip->getPixelComponents();

    assert(kSupportsMultipleClipPARs   || !_srcClip || _srcClip->getPixelAspectRatio() == _dstClip->getPixelAspectRatio());
    assert(kSupportsMultipleClipDepths || !_srcClip || _srcClip->getPixelDepth()       == _dstClip->getPixelDepth());
    assert(dstComponents == OFX::ePixelComponentAlpha || dstComponents == OFX::ePixelComponentRGB || dstComponents == OFX::ePixelComponentRGBA);
    if (dstComponents == OFX::ePixelComponentRGBA) {
        renderForComponents<4>(args);
    } else if (dstComponents == OFX::ePixelComponentAlpha) {
        renderForComponents<1>(args);
    } else {
        assert(dstComponents == OFX::ePixelComponentRGB);
        renderForComponents<3>(args);
    }
}

template<int nComponents>
void
FrameBlendPlugin::renderForComponents(const OFX::RenderArguments &args)
{
    OFX::BitDepthEnum dstBitDepth    = _dstClip->getPixelDepth();
    switch (dstBitDepth) {
        case OFX::eBitDepthUByte:
            renderForBitDepth<unsigned char, nComponents, 255>(args);
            break;

        case OFX::eBitDepthUShort:
            renderForBitDepth<unsigned short, nComponents, 65535>(args);
            break;

        case OFX::eBitDepthFloat:
            renderForBitDepth<float, nComponents, 1>(args);
            break;
        default:
            OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
    }
}

template <class PIX, int nComponents, int maxValue>
void
FrameBlendPlugin::renderForBitDepth(const OFX::RenderArguments &args)
{
    int operation_i;
    _operation->getValueAtTime(args.time, operation_i);
    OperationEnum operation = (OperationEnum)operation_i;

    switch (operation) {
        case eOperationAverage: {
            FrameBlendProcessor<PIX, nComponents, maxValue, eOperationAverage> fred(*this);
            setupAndProcess(fred, args);
            break;
        }
        case eOperationMin: {
            FrameBlendProcessor<PIX, nComponents, maxValue, eOperationMin> fred(*this);
            setupAndProcess(fred, args);
            break;
        }
        case eOperationMax: {
            FrameBlendProcessor<PIX, nComponents, maxValue, eOperationMax> fred(*this);
            setupAndProcess(fred, args);
            break;
        }
        case eOperationSum: {
            FrameBlendProcessor<PIX, nComponents, maxValue, eOperationSum> fred(*this);
            setupAndProcess(fred, args);
            break;
        }
        case eOperationProduct: {
            FrameBlendProcessor<PIX, nComponents, maxValue, eOperationProduct> fred(*this);
            setupAndProcess(fred, args);
            break;
        }
    }
}

template <class PIX, int nComponents, int maxValue, OperationEnum operation>
void
FrameBlendPlugin::renderForOperation(const OFX::RenderArguments &args)
{
    FrameBlendProcessor<PIX, nComponents, maxValue, eOperationAverage> fred(*this);
    setupAndProcess(fred, args);
}

bool
FrameBlendPlugin::isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime)
{
    const double time = args.time;
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

    if (_fgMClip && _fgMClip->isConnected()) {
        // FgM may contain anything
        return false;
    }

    bool outputCount;
    _outputCount->getValueAtTime(time, outputCount);
    if (outputCount) {
        return false;
    }

    bool absolute;
    _absolute->getValueAtTime(time, absolute);
    OfxRangeD range;
    int min, max;
    _frameRange->getValueAtTime(time, min, max);
    if (!absolute) {
        min += time;
        max += time;
    }
    range.min = min;
    range.max = max;

    if (range.min == range.max) {
        identityClip = _srcClip;
        identityTime = time;
        return true;
    }

    return false;
}

void
FrameBlendPlugin::getFramesNeeded(const OFX::FramesNeededArguments &args,
                                   OFX::FramesNeededSetter &frames)
{
    const double time = args.time;
    bool absolute;
    _absolute->getValueAtTime(time, absolute);
    OfxRangeD range;
    int min, max;
    _frameRange->getValueAtTime(time, min, max);
    if (min > max) {
        std::swap(min, max);
    }
    if (!absolute) {
        min += time;
        max += time;
    }
    int interval;
    _frameInterval->getValueAtTime(time, interval);
    if (interval <= 1) {
        range.min = min;
        range.max = max;
        frames.setFramesNeeded(*_srcClip, range);
    } else {
        for (int i = min; i <= max; i += interval) {
            range.min = range.max = i;
            frames.setFramesNeeded(*_srcClip, range);
        }
    }
}

/** @brief called when a param has just had its value changed */
void
FrameBlendPlugin::changedParam(const InstanceChangedArgs &args, const std::string &paramName)
{
    if (paramName == kParamInputRangeName && args.reason == eChangeUserEdit) {
        OfxRangeD range;
        if ( _srcClip && _srcClip->isConnected() ) {
            range = _srcClip->getFrameRange();
        } else {
            timeLineGetBounds(range.min, range.max);
        }
        _frameRange->setValue((int)range.min, (int)range.max);
        _absolute->setValue(true);
    }
}


mDeclarePluginFactory(FrameBlendPluginFactory, {}, {});

void FrameBlendPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    // basic labels
    desc.setLabel(kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);

    desc.addSupportedContext(eContextFilter);
    desc.addSupportedContext(eContextGeneral);
    desc.addSupportedContext(eContextPaint);
    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);

    // set a few flags
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(kSupportsMultiResolution);
    desc.setSupportsTiles(kSupportsTiles);
    desc.setTemporalClipAccess(true);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
    desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
    desc.setRenderThreadSafety(kRenderThreadSafety);
}

void FrameBlendPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
{
    // Source clip only in the filter context
    // create the mandated source clip
    ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setTemporalClipAccess(true);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setIsMask(false);

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setSupportsTiles(kSupportsTiles);

    if (context == eContextGeneral || context == eContextPaint) {
        ClipDescriptor *maskClip = context == eContextGeneral ? desc.defineClip("Mask") : desc.defineClip("Brush");
        maskClip->addSupportedComponent(ePixelComponentAlpha);
        maskClip->setTemporalClipAccess(true);
        if (context == eContextGeneral)
            maskClip->setOptional(true);
        maskClip->setSupportsTiles(kSupportsTiles);
        maskClip->setIsMask(true);
    }

    ClipDescriptor *fgM = desc.defineClip(kClipFgMName);
    fgM->addSupportedComponent(ePixelComponentAlpha);
    fgM->setTemporalClipAccess(true);
    fgM->setOptional(true);
    fgM->setSupportsTiles(kSupportsTiles);
    fgM->setIsMask(true);

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
        Int2DParamDescriptor *param = desc.defineInt2DParam(kParamFrameRangeName);
        param->setLabel(kParamFrameRangeLabel);
        param->setHint(kParamFrameRangeHint);
        param->setDimensionLabels("min", "max");
        param->setDefault(-5, 0);
        param->setAnimates(true); // can animate
        param->setLayoutHint(eLayoutHintNoNewLine);
        if (page) {
            page->addChild(*param);
        }
    }

    {
        BooleanParamDescriptor *param = desc.defineBooleanParam(kParamAbsoluteName);
        param->setLabel(kParamAbsoluteLabel);
        param->setHint(kParamAbsoluteHint);
        param->setDefault(false);
        param->setAnimates(true); // can animate
        param->setLayoutHint(eLayoutHintNoNewLine);
        if (page) {
            page->addChild(*param);
        }
    }

    {
        PushButtonParamDescriptor *param = desc.definePushButtonParam(kParamInputRangeName);
        param->setLabel(kParamInputRangeLabel);
        param->setHint(kParamInputRangeHint);
        if (page) {
            page->addChild(*param);
        }
    }

    {
        IntParamDescriptor *param = desc.defineIntParam(kParamFrameIntervalName);
        param->setLabel(kParamFrameIntervalLabel);
        param->setHint(kParamFrameIntervalHint);
        param->setRange(1, INT_MAX);
        param->setDisplayRange(1, 10);
        param->setDefault(1);
        param->setAnimates(true); // can animate
        if (page) {
            page->addChild(*param);
        }
    }

    {
        ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamOperation);
        param->setLabel(kParamOperationLabel);
        param->setHint(kParamOperationHint);
        assert(param->getNOptions() == (int)eOperationAverage);
        param->appendOption(kParamOperationOptionAverage, kParamOperationOptionAverageHint);
        assert(param->getNOptions() == (int)eOperationMin);
        param->appendOption(kParamOperationOptionMin, kParamOperationOptionMinHint);
        assert(param->getNOptions() == (int)eOperationMax);
        param->appendOption(kParamOperationOptionMax, kParamOperationOptionMaxHint);
        assert(param->getNOptions() == (int)eOperationSum);
        param->appendOption(kParamOperationOptionSum, kParamOperationOptionSumHint);
        assert(param->getNOptions() == (int)eOperationProduct);
        param->appendOption(kParamOperationOptionProduct, kParamOperationOptionProductHint);
        param->setDefault((int)kParamOperationDefault);
        if (page) {
            page->addChild(*param);
        }
    }

    {
        BooleanParamDescriptor *param = desc.defineBooleanParam(kParamOutputCountName);
        param->setLabel(kParamOutputCountLabel);
        param->setHint(kParamOutputCountHint);
        if (page) {
            page->addChild(*param);
        }
    }

    ofxsMaskMixDescribeParams(desc, page);
}

OFX::ImageEffect* FrameBlendPluginFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum /*context*/)
{
    return new FrameBlendPlugin(handle);
}

void getFrameBlendPluginID(OFX::PluginFactoryArray &ids)
{
    static FrameBlendPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
    ids.push_back(&p);
}

