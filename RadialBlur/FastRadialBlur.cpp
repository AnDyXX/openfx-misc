/*
AX Radial blur plugin.

Copyright (C) 2015 AnDyX


*/

#include "FastRadialBlur.h"

#include <cmath>
#include <cstring>
#ifdef _WINDOWS
#include <windows.h>
#endif

#include "ofxsProcessing.H"
#include "ofxsMaskMix.h"
#include "ofxsMacros.h"

#define kPluginName "FastRadialBlurAX"
#define kPluginGrouping "Blur"
#define kPluginDescription "Fast radial blur."
#define kPluginIdentifier "org.andyx.FastRadialBlurPlugin"
#define kPluginVersionMajor 0 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 1 // Increment this when you have fixed a bug or made it faster.

#define kSupportsTiles 0
#define kSupportsMultiResolution 1
#define kSupportsRenderScale 1
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths false
#define kRenderThreadSafety eRenderFullySafe

#define kParamCenterPoint "centerPoint"
#define kParamCenterPointLabel "Center Point"

#define kParamUsePx "usePx"
#define kParamUsePxLabel "Center Point Uses px"
#define kParamusePxHint "Center Point Uses px instead of "

using namespace OFX;

//base class of RadialBlurProcessor plugin
class FastRadialBlurProcessorBase
{
protected:
	OFX::ImageEffect &_effect;      /**< @brief effect to render with */
	OFX::Image       *_dstImg;        /**< @brief image to process into */
	const OFX::Image       *_srcImg;        /**< @brief image to process into */
public:
	/** @brief ctor */
	FastRadialBlurProcessorBase(OFX::ImageEffect &effect)
		: _effect(effect)
		, _dstImg(0)
		, _srcImg(0)
	{
	}

	/** @brief set the destination image */
	void setDstImg(OFX::Image *v) { _dstImg = v; }
	void setSrcImg(const OFX::Image *v) { _srcImg = v; }

};

// RadialBlurProcessor plugin processor
template <class PIX, int nComponents, int maxValue>
class FastRadialBlurProcessor : public FastRadialBlurProcessorBase
{
public:
	FastRadialBlurProcessor(OFX::ImageEffect &instance)
		: FastRadialBlurProcessorBase(instance)
	{
	}


};

//plugin itself
////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class FastRadialBlurPlugin : public OFX::ImageEffect
{
private:
	OFX::BooleanParam* _CenterUsePx;
	OFX::Double2DParam* _centerPoint;
public:
	/** @brief ctor */
	FastRadialBlurPlugin(OfxImageEffectHandle handle) : ImageEffect(handle)
		, _dstClip(0)
		, _srcClip(0)
	{
		_dstClip = fetchClip(kOfxImageEffectOutputClipName);
		assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB || _dstClip->getPixelComponents() == ePixelComponentRGBA));
		_srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
		assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB || _srcClip->getPixelComponents() == ePixelComponentRGBA));

		// NON-GENERIC
		_CenterUsePx = fetchBooleanParam(kParamUsePx);
		_centerPoint = fetchDouble2DParam(kParamCenterPoint);
	}
private:
	/* Override the render */
	virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

	virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime) OVERRIDE FINAL{ return true; }

	virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

	/* set up and run a processor */
	void setupAndProcess(FastRadialBlurProcessorBase & processor, const OFX::RenderArguments &args);

private:
	// do not need to delete these, the ImageEffect is managing them for us
	OFX::Clip *_dstClip;
	OFX::Clip *_srcClip;
};


void
FastRadialBlurPlugin::setupAndProcess(FastRadialBlurProcessorBase & processor, const OFX::RenderArguments &args){
	std::auto_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
	if (!dst.get()) {
		OFX::throwSuiteStatusException(kOfxStatFailed);
	}
	OFX::BitDepthEnum         dstBitDepth = dst->getPixelDepth();
	OFX::PixelComponentEnum   dstComponents = dst->getPixelComponents();
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
		_srcClip->fetchImage(args.time) : 0);
	if (src.get()) {
		if (
			(src->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && src->getField() != args.fieldToRender)) {
			setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
			OFX::throwSuiteStatusException(kOfxStatFailed);
		}
		OFX::BitDepthEnum    srcBitDepth = src->getPixelDepth();
		OFX::PixelComponentEnum srcComponents = src->getPixelComponents();
		if (srcBitDepth != dstBitDepth || srcComponents != dstComponents) {
			OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
		}
	}

	processor.setDstImg(dst.get());
	processor.setSrcImg(src.get());


	//processor.process();
}

void
FastRadialBlurPlugin::changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName)
{

}

// the overridden render function
void
FastRadialBlurPlugin::render(const OFX::RenderArguments &args)
{
	// instantiate the render code based on the pixel depth of the dst clip
	OFX::BitDepthEnum       dstBitDepth = _dstClip->getPixelDepth();
	OFX::PixelComponentEnum dstComponents = _dstClip->getPixelComponents();

	assert(kSupportsMultipleClipPARs || !_srcClip || _srcClip->getPixelAspectRatio() == _dstClip->getPixelAspectRatio());
	assert(kSupportsMultipleClipDepths || !_srcClip || _srcClip->getPixelDepth() == _dstClip->getPixelDepth());
	assert(dstComponents == OFX::ePixelComponentAlpha || dstComponents == OFX::ePixelComponentRGB || dstComponents == OFX::ePixelComponentRGBA);

	if (dstComponents == OFX::ePixelComponentAlpha) {
		switch (dstBitDepth) {
		case OFX::eBitDepthUByte: {
									  FastRadialBlurProcessor<unsigned char, 1, 255> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		case OFX::eBitDepthUShort: {
									   FastRadialBlurProcessor<unsigned short, 1, 65535> fred(*this);
									   setupAndProcess(fred, args);
									   break;
		}
		case OFX::eBitDepthFloat: {
									  FastRadialBlurProcessor<float, 1, 1> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		default:
			OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
		}
	}
	else if (dstComponents == OFX::ePixelComponentRGBA) {
		switch (dstBitDepth) {
		case OFX::eBitDepthUByte: {
									  FastRadialBlurProcessor<unsigned char, 4, 255> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		case OFX::eBitDepthUShort: {
									   FastRadialBlurProcessor<unsigned short, 4, 65535> fred(*this);
									   setupAndProcess(fred, args);
									   break;
		}
		case OFX::eBitDepthFloat: {
									  FastRadialBlurProcessor<float, 4, 1> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		default:
			OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
		}
	}
	else {
		assert(dstComponents == OFX::ePixelComponentRGB);
		switch (dstBitDepth) {
		case OFX::eBitDepthUByte: {
									  FastRadialBlurProcessor<unsigned char, 3, 255> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		case OFX::eBitDepthUShort: {
									   FastRadialBlurProcessor<unsigned short, 3, 65535> fred(*this);
									   setupAndProcess(fred, args);
									   break;
		}
		case OFX::eBitDepthFloat: {
									  FastRadialBlurProcessor<float, 3, 1> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		default:
			OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
		}
	}
}

//factory
mDeclarePluginFactory(FastRadialBlurFactory, {}, {});

void FastRadialBlurFactory::describe(OFX::ImageEffectDescriptor &desc)
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
	desc.setTemporalClipAccess(false);
	desc.setRenderTwiceAlways(false);
	desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
	desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
	desc.setRenderThreadSafety(kRenderThreadSafety);

}

void FastRadialBlurFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
{
	// Source clip only in the filter context
	// create the mandated source clip
	ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
	srcClip->addSupportedComponent(ePixelComponentRGBA);
	srcClip->addSupportedComponent(ePixelComponentRGB);
	srcClip->setTemporalClipAccess(false);
	srcClip->setSupportsTiles(kSupportsTiles);
	srcClip->setIsMask(false);

	// create the mandated output clip
	ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
	dstClip->addSupportedComponent(ePixelComponentRGBA);
	dstClip->addSupportedComponent(ePixelComponentRGB);
	dstClip->setSupportsTiles(kSupportsTiles);

	// make some pages and to things in
	PageParamDescriptor *page = desc.definePageParam("Controls");

	// center point
	{
		Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamCenterPoint);
		param->setLabel(kParamCenterPointLabel);
		param->setDoubleType(eDoubleTypeXYAbsolute);
		param->setDefault(0, 0);
		param->setIncrement(0.1);
		param->setUseHostNativeOverlayHandle(true);
		param->setUseHostOverlayHandle(true);
		if (page) {
			page->addChild(*param);
		}
	}

	// use px
	{
		BooleanParamDescriptor* param = desc.defineBooleanParam(kParamUsePx);
		param->setLabel(kParamUsePxLabel);
		param->setHint(kParamusePxHint);
		// don't check it by default: it is easy to obtain Uniform scaling using the slider or the interact
		param->setDefault(false);
		param->setAnimates(true);
		if (page) {
			page->addChild(*param);
		}
	}


}

OFX::ImageEffect* FastRadialBlurFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum /*context*/)
{
	return new FastRadialBlurPlugin(handle);
}

//register plugin
void getFastRadialBlurID(OFX::PluginFactoryArray &ids)
{
	static FastRadialBlurFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
	ids.push_back(&p);
}
