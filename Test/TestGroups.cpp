/*
 OFX TestGroups plugin.

 Copyright (C) 2015 INRIA

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

 */

#include "TestGroups.h"

#ifdef _WINDOWS
#include <windows.h>
#endif

#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"

#include "ofxsProcessing.H"
#include "ofxsMaskMix.h"
#include "ofxsMerging.h"
#include "ofxsCopier.h"
#include "ofxsMacros.h"


#define kPluginName "TestGroupsOFX"
#define kPluginGrouping "Other/Test"
#define kPluginDescription \
"Test parameter groups. See https://github.com/MrKepzie/Natron/issues/521"

#define kPluginIdentifier "net.sf.openfx.TestGroups"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths false

#define kParamColor0      "color0"
#define kParamColor0Label "Color 0"

#define kParamClipInfo      "clipInfo"
#define kParamClipInfoLabel "Clip Info..."
#define kParamClipInfoHint  "Display information about the inputs"

#define kParamForceCopy "forceCopy"
#define kParamForceCopyLabel "Force Copy"
#define kParamForceCopyHint "Force copy from input to output"

using namespace OFX;


////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class TestGroupsPlugin : public OFX::ImageEffect
{
public:
    /** @brief ctor */
    TestGroupsPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , _dstClip(0)
    , _srcClip(0)
    {
        _dstClip = fetchClip(kOfxImageEffectOutputClipName);
        assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB || _dstClip->getPixelComponents() == ePixelComponentRGBA || _dstClip->getPixelComponents() == ePixelComponentAlpha));
        _srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
        assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB || _srcClip->getPixelComponents() == ePixelComponentRGBA || _srcClip->getPixelComponents() == ePixelComponentAlpha));
        _maskClip = getContext() == OFX::eContextFilter ? NULL : fetchClip(getContext() == OFX::eContextPaint ? "Brush" : "Mask");
        assert(!_maskClip || _maskClip->getPixelComponents() == ePixelComponentAlpha);

        _color = fetchRGBAParam(kParamColor0);
        assert(_color);

        _forceCopy = fetchBooleanParam(kParamForceCopy);
        assert(_forceCopy);

        _mix = fetchDoubleParam(kParamMix);
        _maskInvert = fetchBooleanParam(kParamMaskInvert);
        assert(_mix && _maskInvert);
    }

private:
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime) OVERRIDE FINAL;

    virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

private:
    // do not need to delete these, the ImageEffect is managing them for us
    OFX::Clip *_dstClip;
    OFX::Clip *_srcClip;
    OFX::Clip *_maskClip;

    OFX::RGBAParam* _color;

    OFX::BooleanParam *_forceCopy;
    OFX::DoubleParam* _mix;
    OFX::BooleanParam* _maskInvert;
};



// the overridden render function
void
TestGroupsPlugin::render(const OFX::RenderArguments &args)
{
    bool forceCopy;
    _forceCopy->getValue(forceCopy);

    if (!forceCopy) {
        setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host should not render");
        throwSuiteStatusException(kOfxStatFailed);
    }

    assert(kSupportsMultipleClipPARs   || !_srcClip || _srcClip->getPixelAspectRatio() == _dstClip->getPixelAspectRatio());
    assert(kSupportsMultipleClipDepths || !_srcClip || _srcClip->getPixelDepth()       == _dstClip->getPixelDepth());
    // do the rendering
    std::auto_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
    if (!dst.get()) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    if (dst->getRenderScale().x != args.renderScale.x ||
        dst->getRenderScale().y != args.renderScale.y ||
        (dst->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && dst->getField() != args.fieldToRender)) {
        setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    OFX::BitDepthEnum dstBitDepth       = dst->getPixelDepth();
    OFX::PixelComponentEnum dstComponents  = dst->getPixelComponents();
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
    copyPixels(*this, args.renderWindow, src.get(), dst.get());
}

bool
TestGroupsPlugin::isIdentity(const IsIdentityArguments &/*args*/, Clip * &identityClip, double &/*identityTime*/)
{
    bool forceCopy;
    _forceCopy->getValue(forceCopy);

    if (!forceCopy) {
        identityClip = _srcClip;
        return true;
    } else {
        return false;
    }
}


static const char*
bitDepthString(BitDepthEnum bitDepth)
{
    switch (bitDepth) {
        case OFX::eBitDepthUByte:
            return "8u";
        case OFX::eBitDepthUShort:
            return "16u";
        case OFX::eBitDepthHalf:
            return "16f";
        case OFX::eBitDepthFloat:
            return "32f";
        case OFX::eBitDepthCustom:
            return "x";
        case OFX::eBitDepthNone:
            return "0";
#ifdef OFX_EXTENSIONS_VEGAS
        case eBitDepthUByteBGRA:
            return "8uBGRA";
        case eBitDepthUShortBGRA:
            return "16uBGRA";
        case eBitDepthFloatBGRA:
            return "32fBGRA";
#endif
        default:
            return "[unknown bit depth]";
    }
}

static std::string
pixelComponentString(const std::string& p)
{
    const std::string prefix = "OfxImageComponent";
    std::string s = p;
    return s.replace(s.find(prefix),prefix.length(),"");
}

static const char*
premultString(PreMultiplicationEnum e)
{
    switch (e) {
        case eImageOpaque:
            return "Opaque";
        case eImagePreMultiplied:
            return "PreMultiplied";
        case eImageUnPreMultiplied:
            return "UnPreMultiplied";
        default:
            return "[unknown premult]";
    }
}

#ifdef OFX_EXTENSIONS_VEGAS
static const char*
pixelOrderString(PixelOrderEnum e)
{
    switch (e) {
        case ePixelOrderRGBA:
            return "RGBA";
        case ePixelOrderBGRA:
            return "BGRA";
        default:
            return "[unknown pixel order]";
    }
}
#endif

static const char*
fieldOrderString(FieldEnum e)
{
    switch (e) {
        case eFieldNone:
            return "None";
        case eFieldBoth:
            return "Both";
        case eFieldLower:
            return "Lower";
        case eFieldUpper:
            return "Upper";
        case eFieldSingle:
            return "Single";
        case eFieldDoubled:
            return "Doubled";
        default:
            return "[unknown field order]";
    }
}

void
TestGroupsPlugin::changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName)
{
    if (paramName == kParamClipInfo) {
        std::ostringstream oss;
        oss << "Clip Info:\n\n";
        oss << "Input: ";
        if (!_srcClip) {
            oss << "N/A";
        } else {
            OFX::Clip &c = *_srcClip;
            oss << pixelComponentString(c.getPixelComponentsProperty());
            oss << bitDepthString(c.getPixelDepth());
            oss << " (unmapped: ";
            oss << pixelComponentString(c.getUnmappedPixelComponentsProperty());
            oss << bitDepthString(c.getUnmappedPixelDepth());
            oss << ")\npremultiplication: ";
            oss << premultString(c.getPreMultiplication());
#ifdef OFX_EXTENSIONS_VEGAS
            oss << "\npixel order: ";
            oss << pixelOrderString(c.getPixelOrder());
#endif
            oss << "\nfield order: ";
            oss << fieldOrderString(c.getFieldOrder());
            oss << "\n";
            oss << (c.isConnected() ? "connected" : "not connected");
            oss << "\n";
            oss << (c.hasContinuousSamples() ? "continuous samples" : "discontinuous samples");
            oss << "\npixel aspect ratio: ";
            oss << c.getPixelAspectRatio();
            oss << "\nframe rate: ";
            oss << c.getFrameRate();
            oss << " (unmapped: ";
            oss << c.getUnmappedFrameRate();
            oss << ")";
            OfxRangeD range = c.getFrameRange();
            oss << "\nframe range: ";
            oss << range.min << "..." << range.max;
            oss << " (unmapped: ";
            range = c.getUnmappedFrameRange();
            oss << range.min << "..." << range.max;
            oss << ")";
            oss << "\nregion of definition: ";
            OfxRectD rod = c.getRegionOfDefinition(args.time);
            oss << rod.x1 << ' ' << rod.y1 << ' ' << rod.x2 << ' ' << rod.y2;
        }
        oss << "\n\n";
        oss << "Output: ";
        if (!_dstClip) {
            oss << "N/A";
        } else {
            OFX::Clip &c = *_dstClip;
            oss << pixelComponentString(c.getPixelComponentsProperty());
            oss << bitDepthString(c.getPixelDepth());
            oss << " (unmapped: ";
            oss << pixelComponentString(c.getUnmappedPixelComponentsProperty());
            oss << bitDepthString(c.getUnmappedPixelDepth());
            oss << ")\npremultiplication: ";
            oss << premultString(c.getPreMultiplication());
#ifdef OFX_EXTENSIONS_VEGAS
            oss << "\npixel order: ";
            oss << pixelOrderString(c.getPixelOrder());
#endif
            oss << "\nfield order: ";
            oss << fieldOrderString(c.getFieldOrder());
            oss << "\n";
            oss << (c.isConnected() ? "connected" : "not connected");
            oss << "\n";
            oss << (c.hasContinuousSamples() ? "continuous samples" : "discontinuous samples");
            oss << "\npixel aspect ratio: ";
            oss << c.getPixelAspectRatio();
            oss << "\nframe rate: ";
            oss << c.getFrameRate();
            oss << " (unmapped: ";
            oss << c.getUnmappedFrameRate();
            oss << ")";
            OfxRangeD range = c.getFrameRange();
            oss << "\nframe range: ";
            oss << range.min << "..." << range.max;
            oss << " (unmapped: ";
            range = c.getUnmappedFrameRange();
            oss << range.min << "..." << range.max;
            oss << ")";
            oss << "\nregion of definition: ";
            OfxRectD rod = c.getRegionOfDefinition(args.time);
            oss << rod.x1 << ' ' << rod.y1 << ' ' << rod.x2 << ' ' << rod.y2;
        }
        oss << "\n\n";
        oss << "time: " << args.time << ", renderscale: " << args.renderScale.x << 'x' << args.renderScale.y << '\n';

        sendMessage(OFX::Message::eMessageMessage, "", oss.str());
    }
}


mDeclarePluginFactory(TestGroupsPluginFactory, {}, {});

using namespace OFX;

void
TestGroupsPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    // basic labels
    desc.setLabel(kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);

    // add the supported contexts
    desc.addSupportedContext(eContextFilter);
    desc.addSupportedContext(eContextGeneral);
    desc.addSupportedContext(eContextPaint);
    desc.addSupportedContext(eContextGenerator);

    // add supported pixel depths
    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);

    // set a few flags
    desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
    desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
}

void
TestGroupsPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
{
    // Source clip only in the filter context
    // create the mandated source clip
    ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setTemporalClipAccess(false);
    srcClip->setIsMask(false);

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentAlpha);

    if (context == eContextGeneral || context == eContextPaint) {
        ClipDescriptor *maskClip = context == eContextGeneral ? desc.defineClip("Mask") : desc.defineClip("Brush");
        maskClip->addSupportedComponent(ePixelComponentAlpha);
        maskClip->setTemporalClipAccess(false);
        if (context == eContextGeneral) {
            maskClip->setOptional(true);
        }
        maskClip->setIsMask(true);
    }

    // make some pages and to things in
    //PageParamDescriptor *page = NULL;
    PageParamDescriptor *page = desc.definePageParam("Controls");

    // color0
    {
        RGBAParamDescriptor *param = desc.defineRGBAParam(kParamColor0);
        param->setLabel(kParamColor0Label);
        param->setDefault(0.0, 1.0, 1.0, 1.0);
        param->setAnimates(true); // can animate
        if (page) {
            page->addChild(*param);
        }
    }

    // forceCopy
    {
        BooleanParamDescriptor *param = desc.defineBooleanParam(kParamForceCopy);
        param->setLabel(kParamForceCopyLabel);
        param->setHint(kParamForceCopyHint);
        param->setDefault(false);
        param->setAnimates(false);
        if (page) {
            page->addChild(*param);
        }
    }

    // clipInfo
    {
        PushButtonParamDescriptor *param = desc.definePushButtonParam(kParamClipInfo);
        param->setLabel(kParamClipInfoLabel);
        param->setHint(kParamClipInfoHint);
        if (page) {
            page->addChild(*param);
        }
    }

    // Groups
    OFX::GroupParamDescriptor* formatGroup = desc.defineGroupParam( "kParamFormatGroup" );
    OFX::GroupParamDescriptor* videoGroup  = desc.defineGroupParam( "kParamVideoGroup" );
    formatGroup->setLabel( "Format" );
    videoGroup->setLabel( "Video" );

    formatGroup->setAsTab( );
    videoGroup->setAsTab( );

    /// FORMAT PARAMETERS
    //avtranscoder::FormatContext formatContext( AV_OPT_FLAG_DECODING_PARAM );
    //avtranscoder::OptionArray formatOptions = formatContext.getOptions();
    //common::addOptionsToGroup( desc, formatGroup, formatOptions, common::kPrefixFormat );
    {
        OFX::ParamDescriptor* param = NULL;
        OFX::BooleanParamDescriptor* boolParam = desc.defineBooleanParam( "opt1" );
        boolParam->setDefault( true );
        param = boolParam;
        param->setLabel( "Opt1" );
        param->setHint( "Opt1 help" );
        param->setParent( *formatGroup );
    }

    {
        OFX::ParamDescriptor* param = NULL;
        OFX::IntParamDescriptor* intParam = desc.defineIntParam( "int" );
        param = intParam;
        param->setLabel( "Int1" );
        param->setHint( "Int1 help" );
        param->setParent( *formatGroup );
    }
    
    OFX::GroupParamDescriptor* formatDetailledGroup = desc.defineGroupParam( "kParamFormatDetailledGroup" );
    formatDetailledGroup->setLabel( "Detailled" );
    formatDetailledGroup->setAsTab( );
    formatDetailledGroup->setParent( *formatGroup );

    //avtranscoder::OptionArrayMap formatDetailledGroupOptions = avtranscoder::getOutputFormatOptions();
    //common::addOptionsToGroup( desc, formatDetailledGroup, formatDetailledGroupOptions, common::kPrefixFormat );
    {
        OFX::ParamDescriptor* param = NULL;
        OFX::BooleanParamDescriptor* boolParam = desc.defineBooleanParam( "opt2" );
        boolParam->setDefault( true );
        param = boolParam;
        param->setLabel( "Opt2" );
        param->setHint( "Opt2 help" );
        param->setParent( *formatDetailledGroup );
    }

    /// VIDEO PARAMETERS
    OFX::BooleanParamDescriptor* useCustomSAR = desc.defineBooleanParam( "kParamUseCustomSAR" );
    useCustomSAR->setLabel( "Override SAR" );
    useCustomSAR->setDefault( false );
    useCustomSAR->setHint( "Override the file SAR (Storage Aspect Ratio) with a custom SAR value." );
    useCustomSAR->setParent( *videoGroup );

    OFX::DoubleParamDescriptor* customSAR = desc.defineDoubleParam( "kParamCustomSAR" );
    customSAR->setLabel( "Custom SAR" );
    customSAR->setDefault( 1.0 );
    customSAR->setDisplayRange( 0., 3. );
    customSAR->setRange( 0., 10. );
    customSAR->setHint( "Choose a custom value to override the file SAR (Storage Aspect Ratio). Maximum value: 10." );
    customSAR->setParent( *videoGroup );

    OFX::IntParamDescriptor* streamIndex = desc.defineIntParam( "kParamVideoStreamIndex" );
    streamIndex->setLabel( "kParamVideoStreamIndexLabel" );
    streamIndex->setDefault( 0 );
    streamIndex->setDisplayRange( 0., 16. );
    streamIndex->setRange( 0., 100. );
    streamIndex->setHint( "Choose a custom value to decode the video stream you want. Maximum value: 100." );
    streamIndex->setParent( *videoGroup );

    OFX::GroupParamDescriptor* videoDetailledGroup  = desc.defineGroupParam( "kParamVideoDetailledGroup" );
    videoDetailledGroup->setLabel( "Detailled" );
    videoDetailledGroup->setAsTab( );
    videoDetailledGroup->setParent( *videoGroup );

    //avtranscoder::OptionArrayMap videoDetailledGroupOptions =  avtranscoder::getVideoCodecOptions();
    //common::addOptionsToGroup( desc, videoDetailledGroup, videoDetailledGroupOptions, common::kPrefixVideo );
    {
        OFX::ParamDescriptor* param = NULL;
        OFX::BooleanParamDescriptor* boolParam = desc.defineBooleanParam( "opt3" );
        boolParam->setDefault( true );
        param = boolParam;
        param->setLabel( "Op3" );
        param->setHint( "Opt3 help" );
        param->setParent( *videoDetailledGroup );
    }

    /// VERBOSE
    OFX::BooleanParamDescriptor* useVerbose = desc.defineBooleanParam( "kParamVerbose" );
    useVerbose->setLabel( "Set to verbose" );
    useVerbose->setDefault( false );
    useVerbose->setHint( "Set plugin to verbose to get debug informations." );

    ofxsMaskMixDescribeParams(desc, page);
}

OFX::ImageEffect* TestGroupsPluginFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum /*context*/)
{
    return new TestGroupsPlugin(handle);
}


void getTestGroupsPluginID(OFX::PluginFactoryArray &ids)
{
    {
        std::string id = kPluginIdentifier;
        static TestGroupsPluginFactory p(id, kPluginVersionMajor, kPluginVersionMinor);
        ids.push_back(&p);
    }
}

