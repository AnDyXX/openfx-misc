/*
AX Spherise plugin.

Copyright (C) 2015 AnDyX


*/

#include "Spherise.h"

#include <cmath>
#include <cstring>
#ifdef _WINDOWS
#include <windows.h>
#endif

#include "ofxsProcessing.H"
#include "ofxsMaskMix.h"
#include "ofxsMacros.h"

#define kPluginName "SpheriseAX"
#define kPluginGrouping "Transform"
#define kPluginDescription "Make spherise/unspherise around choosen point."
#define kPluginIdentifier "org.andyx.SpherisePlugin"
#define kPluginVersionMajor 0 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 1 // Increment this when you have fixed a bug or made it faster.


#define kSupportsTiles 1
#define kSupportsMultiResolution 1
#define kSupportsRenderScale 1
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths false
#define kRenderThreadSafety eRenderFullySafe

#define kParamCenterPoint "centerPoint"
#define kParamCenterPointLabel "Center Point"


using namespace OFX;

//base class of Spherise plugin
class SpheriseProcessorBase : public OFX::ImageProcessor
{
protected:
	const OFX::Image *_srcImg;
public:
	SpheriseProcessorBase(OFX::ImageEffect &instance)
		: OFX::ImageProcessor(instance)
		, _srcImg(0)
	{
		}

	void setSrcImg(const OFX::Image *v) { _srcImg = v; }
};


// Spherise plugin processor

template <class PIX, int nComponents, int maxValue>
class SpheriseProcessor : public SpheriseProcessorBase
{
public:
	SpheriseProcessor(OFX::ImageEffect &instance)
		: SpheriseProcessorBase(instance)
	{
	}

private:

	void multiThreadProcessImages(OfxRectI procWindow)
	{
		if (nComponents == 1) {
			return process<false, false, false, true >(procWindow);
		}
		else if (nComponents == 3) {
			return process<true, true, true, false>(procWindow);
		}
		else if (nComponents == 4) {
			return process<true, true, true, true >(procWindow);
		}
	}

	template<bool processR, bool processG, bool processB, bool processA>
	void process(const OfxRectI& procWindow)
	{
		assert(nComponents == 1 || nComponents == 3 || nComponents == 4);
		assert(_dstImg);

		for (int y = procWindow.y1; y < procWindow.y2; y++) {
			if (_effect.abort()) {
				break;
			}

			PIX *dstPix = (PIX *)_dstImg->getPixelAddress(procWindow.x1, y);

			//raw values from pixel in 0-1
			float unpPix[4];

			for (int x = procWindow.x1; x < procWindow.x2; x++) {
				//get src pixel
				const PIX *srcPix = (const PIX *)(_srcImg ? _srcImg->getPixelAddress(x, y) : 0);
				//normalise values to 0-1
				ofxsUnPremult<PIX, nComponents, maxValue>(srcPix, unpPix, true, 0);

				//unpPix[0] = 0.0f;
				//unpPix[2] = 0.1;

				ofxsPremultMaskMixPix<PIX, nComponents, maxValue, false>(unpPix, false, 0, x, y, srcPix, false, 0, 1.0f, false, dstPix);
				//ofxsJustPremult2<PIX, nComponents, maxValue, false>(unpPix, dstPix);
				//ofxsJustPremult<PIX, nComponents, maxValue, false>(unpPix, dstPix);
				// increment the dst pixel
				dstPix += nComponents;
			}
		}
	}
};

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class SpherisePlugin : public OFX::ImageEffect
{
public:
	/** @brief ctor */
	SpherisePlugin(OfxImageEffectHandle handle)
		: ImageEffect(handle)
		, _dstClip(0)
		, _srcClip(0)
		, _centerPoint(0)
	{
		_dstClip = fetchClip(kOfxImageEffectOutputClipName);
		assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentAlpha || _dstClip->getPixelComponents() == ePixelComponentRGB || _dstClip->getPixelComponents() == ePixelComponentRGBA));
		_srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
		assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentAlpha || _srcClip->getPixelComponents() == ePixelComponentRGB || _srcClip->getPixelComponents() == ePixelComponentRGBA));

		// NON-GENERIC
		_centerPoint = fetchDouble2DParam(kParamCenterPoint);
	}
private:
	/* Override the render */
	virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

	virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime) OVERRIDE FINAL;

	virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

	/* set up and run a processor */
	void setupAndProcess(SpheriseProcessorBase & processor, const OFX::RenderArguments &args);

private:
	// do not need to delete these, the ImageEffect is managing them for us
	OFX::Clip *_dstClip;
	OFX::Clip *_srcClip;

	// NON-GENERIC
	OFX::Double2DParam* _centerPoint;
};


void
SpherisePlugin::setupAndProcess(SpheriseProcessorBase & processor, const OFX::RenderArguments &args){
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
		if (src->getRenderScale().x != args.renderScale.x ||
			src->getRenderScale().y != args.renderScale.y ||
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
	processor.setRenderWindow(args.renderWindow);

	processor.process();

}

// the overridden render function
void
SpherisePlugin::render(const OFX::RenderArguments &args)
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
									  SpheriseProcessor<unsigned char, 1, 255> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		case OFX::eBitDepthUShort: {
									   SpheriseProcessor<unsigned short, 1, 65535> fred(*this);
									   setupAndProcess(fred, args);
									   break;
		}
		case OFX::eBitDepthFloat: {
									  SpheriseProcessor<float, 1, 1> fred(*this);
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
									  SpheriseProcessor<unsigned char, 4, 255> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		case OFX::eBitDepthUShort: {
									   SpheriseProcessor<unsigned short, 4, 65535> fred(*this);
									   setupAndProcess(fred, args);
									   break;
		}
		case OFX::eBitDepthFloat: {
									  SpheriseProcessor<float, 4, 1> fred(*this);
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
									  SpheriseProcessor<unsigned char, 3, 255> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		case OFX::eBitDepthUShort: {
									   SpheriseProcessor<unsigned short, 3, 65535> fred(*this);
									   setupAndProcess(fred, args);
									   break;
		}
		case OFX::eBitDepthFloat: {
									  SpheriseProcessor<float, 3, 1> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		default:
			OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
		}
	}
}

bool
SpherisePlugin::isIdentity(const IsIdentityArguments &/*args*/, Clip * &identityClip, double & identityTime)
{
	// NON-GENERIC

	OfxPointD centerPoint;

	_centerPoint->getValueAtTime(identityTime, centerPoint.x, centerPoint.y);

	if (centerPoint.x == 0. && centerPoint.y == 0.) {
		return true;
	}

	return false;
}

void
SpherisePlugin::changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName)
{

}

//factory
mDeclarePluginFactory(SpheriseFactory, {}, {});

void SpheriseFactory::describe(OFX::ImageEffectDescriptor &desc)
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

void SpheriseFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
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
	// translate
	{
		Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamCenterPoint);
		param->setLabel(kParamCenterPointLabel);
		param->setDoubleType(eDoubleTypeXYAbsolute);
		param->setDefault(0, 0);
		param->setIncrement(0.1);
		if (page) {
			page->addChild(*param);
		}
	}
}

OFX::ImageEffect* SpheriseFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum /*context*/)
{
	return new SpherisePlugin(handle);
}

//register plugin
void getSpherisePluginID(OFX::PluginFactoryArray &ids)
{
	static SpheriseFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
	ids.push_back(&p);
}
