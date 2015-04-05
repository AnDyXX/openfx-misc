/*
AX Radial blur plugin.

Copyright (C) 2015 AnDyX


*/

#include "RadialBlur.h"

#include <cmath>
#include <cstring>
#ifdef _WINDOWS
#include <windows.h>
#endif

#include "ofxsProcessing.H"
#include "ofxsMaskMix.h"
#include "ofxsMacros.h"

#define kPluginName "RadialBlurAX"
#define kPluginGrouping "Blur"
#define kPluginDescription "Radial blur."
#define kPluginIdentifier "org.andyx.RadialBlurPlugin"
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

#define kParamSize "size"
#define kParamSizeLabel "Size"
#define kParamSizeHint "Size of effect"

#define kParamSteps "Steps"

#define kParamLuminanceLower "luminanceLower"
#define kParamLuminanceLowerLabel "Luminance Lower"

#define kParamLuminanceUpper "luminanceUpper"
#define kParamLuminanceUpperLabel "Luminance Upper"

using namespace OFX;

//base class of RadialBlurProcessor plugin
class RadialBlurProcessorBase : public OFX::ImageProcessor
{
protected:
	const OFX::Image *_srcImg;

	int centerX;
	int centerY;

	double size;

	int maxsteps;
public:
	RadialBlurProcessorBase(OFX::ImageEffect &instance)
		: OFX::ImageProcessor(instance)
		, _srcImg(0)
	{
		}

	void setSrcImg(const OFX::Image *v) { _srcImg = v; }

	void setValues(int _centerX, int _centerY, double _size, int _maxsteps){
		centerX = _centerX;
		centerY = _centerY;
		size = _size;
		maxsteps = _maxsteps;
	}
};

// RadialBlurProcessor plugin processor
template <class PIX, int nComponents, int maxValue>
class RadialBlurProcessor : public RadialBlurProcessorBase
{
public:
	RadialBlurProcessor(OFX::ImageEffect &instance)
		: RadialBlurProcessorBase(instance)
	{
	}

private:
	void calculateBlur(int x, int y, float * unpSrcPix, float * unpSum, int steps, int sizeX, int sizeY){
		if (steps == 0){
			unpSum[0] = unpSrcPix[0];
			unpSum[1] = unpSrcPix[1];
			unpSum[2] = unpSrcPix[2];
			unpSum[3] = unpSrcPix[3];
			return;
		}

		float divideBy = 0.0;
		int unpCount = 0;

		//go over line and get values from pixels
		for (int i = 0; i <= steps; i++){
			int regX = int(size * (sizeX * i) / (float)steps);
			int regY = int(size * (sizeY * i) / (float)steps);

			//get src pixel
			PIX *srcPix = (PIX *)(_srcImg ? _srcImg->getPixelAddress(regX + x, regY + y) : 0);

			//raw values from pixel in 0-1
			float unpSrcPix[4];

			//normalise values to 0-1
			ofxsUnPremult<PIX, nComponents, maxValue>(srcPix, unpSrcPix, true, 0);

			unpSum[0] += unpSrcPix[0];
			unpSum[1] += unpSrcPix[1];
			unpSum[2] += unpSrcPix[2];
			unpSum[3] += unpSrcPix[3];

			unpCount++;
		}

		//if (divideBy > 0.0){
		//	unpSum[0] /= divideBy;
		//	unpSum[1] /= divideBy;
		//	unpSum[2] /= divideBy;
		//}
		//else{
		//	unpSum[0] = 0.0;
		//	unpSum[1] = 0.0;
		//	unpSum[2] = 0.0;
		//}

		unpSum[0] /= (float)unpCount;
		unpSum[1] /= (float)unpCount;
		unpSum[2] /= (float)unpCount;
		unpSum[3] /= (float)unpCount;
	}

	void multiThreadProcessImages(OfxRectI procWindow)
	{
		assert(nComponents == 1 || nComponents == 3 || nComponents == 4);
		assert(_dstImg);


		for (int y = procWindow.y1; y < procWindow.y2; y++) {
			if (_effect.abort()) {
				break;
			}

			PIX *dstPix = (PIX *)_dstImg->getPixelAddress(procWindow.x1, y);

			//raw values from pixel in 0-1
			float unpSrcPix[4];

			//sum of values
			float unpSum[4];

			for (int x = procWindow.x1; x < procWindow.x2; x++) {
				//reset values
				unpSum[0] = 0.0;
				unpSum[1] = 0.0;
				unpSum[2] = 0.0;
				unpSum[3] = 0.0;

				//calculate differences
				int sizeX = centerX - x;
				int sizeY = centerY - y;

				int steps = int(std::ceil(size* std::max(std::abs(sizeX), std::abs(sizeY))));

				if (maxsteps > 0 && maxsteps < steps){
					steps = maxsteps;
				}

				//get src pixel
				PIX *srcPix = (PIX *)(_srcImg ? _srcImg->getPixelAddress(x, y) : 0);

				//normalise values to 0-1
				ofxsUnPremult<PIX, nComponents, maxValue>(srcPix, unpSrcPix, true, 0);

				//calculate blur
				calculateBlur(x, y, unpSrcPix, unpSum, steps, sizeX, sizeY);

				//normalise to 0 - max value
				ofxsPremultMaskMixPix<PIX, nComponents, maxValue, false>(unpSum, false, 0, x, y, srcPix, false, 0, 1.0f, false, dstPix);

				// increment the dst pixel
				dstPix += nComponents;
			}
		}
	}
};


////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class RadialBlurPlugin : public OFX::ImageEffect
{
private:
	OFX::BooleanParam* _CenterUsePx;
	OFX::Double2DParam* _centerPoint;
	OFX::DoubleParam  *_size;
	OFX::IntParam  *_steps;
public:
	/** @brief ctor */
	RadialBlurPlugin(OfxImageEffectHandle handle)
		: ImageEffect(handle)
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
		_size = fetchDoubleParam(kParamSize);
		_steps = fetchIntParam(kParamSteps);
	}
private:
	/* Override the render */
	virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

	virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime) OVERRIDE FINAL;

	virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

	/* set up and run a processor */
	void setupAndProcess(RadialBlurProcessorBase & processor, const OFX::RenderArguments &args);

private:
	// do not need to delete these, the ImageEffect is managing them for us
	OFX::Clip *_dstClip;
	OFX::Clip *_srcClip;
};

void
RadialBlurPlugin::setupAndProcess(RadialBlurProcessorBase & processor, const OFX::RenderArguments &args){
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
	processor.setRenderWindow(args.renderWindow);

	bool centerUsePx;
	_CenterUsePx->getValueAtTime(args.time, centerUsePx);

	OfxPointD center;
	_centerPoint->getValueAtTime(args.time, center.x, center.y);

	OfxRectI bounds = dst->getRegionOfDefinition();

	int centerX = int((centerUsePx ? dst->getRenderScale().x : (double)bounds.x2) * center.x);
	int centerY = int((centerUsePx ? dst->getRenderScale().y : (double)bounds.y2) * center.y);

	double size = _size->getValueAtTime(args.time);

	int steps = _steps->getValueAtTime(args.time);

	processor.setValues(centerX, centerY, size, steps);

	processor.process();
}

void
RadialBlurPlugin::changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName)
{

}

// the overridden render function
void
RadialBlurPlugin::render(const OFX::RenderArguments &args)
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
									  RadialBlurProcessor<unsigned char, 1, 255> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		case OFX::eBitDepthUShort: {
									   RadialBlurProcessor<unsigned short, 1, 65535> fred(*this);
									   setupAndProcess(fred, args);
									   break;
		}
		case OFX::eBitDepthFloat: {
									  RadialBlurProcessor<float, 1, 1> fred(*this);
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
									  RadialBlurProcessor<unsigned char, 4, 255> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		case OFX::eBitDepthUShort: {
									   RadialBlurProcessor<unsigned short, 4, 65535> fred(*this);
									   setupAndProcess(fred, args);
									   break;
		}
		case OFX::eBitDepthFloat: {
									  RadialBlurProcessor<float, 4, 1> fred(*this);
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
									  RadialBlurProcessor<unsigned char, 3, 255> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		case OFX::eBitDepthUShort: {
									   RadialBlurProcessor<unsigned short, 3, 65535> fred(*this);
									   setupAndProcess(fred, args);
									   break;
		}
		case OFX::eBitDepthFloat: {
									  RadialBlurProcessor<float, 3, 1> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		default:
			OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
		}
	}
}

bool
RadialBlurPlugin::isIdentity(const IsIdentityArguments &/*args*/, Clip * &identityClip, double & identityTime)
{
	return false;
}

//factory
mDeclarePluginFactory(RadialBlurFactory, {}, {});

void RadialBlurFactory::describe(OFX::ImageEffectDescriptor &desc)
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

void RadialBlurFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
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

	// Size
	{
		DoubleParamDescriptor *param = desc.defineDoubleParam(kParamSize);
		param->setLabel(kParamSizeLabel);
		param->setHint(kParamSizeHint);
		param->setDefault(0.1);
		param->setRange(0.0, 1.0);
		param->setIncrement(0.01);
		param->setDisplayRange(0.0, 1.0);
		param->setAnimates(true); // can animate
		param->setDoubleType(eDoubleTypeScale);
		if (page) {
			page->addChild(*param);
		}
	}

	// steps
	{
		IntParamDescriptor *param = desc.defineIntParam(kParamSteps);
		param->setLabel(kParamSteps);
		param->setDefault(10);
		param->setAnimates(true); // can animate
		if (page) {
			page->addChild(*param);
		}
	}


	// luminance lower
	{
		DoubleParamDescriptor* param = desc.defineDoubleParam(kParamLuminanceLower);
		param->setLabel(kParamLuminanceLowerLabel);
		param->setRange(0., 1.);
		param->setDisplayRange(0., 1.);
		param->setDigits(5);
		param->setDefault(0);
		param->setAnimates(true);
		if (page) {
			page->addChild(*param);
		}
	}

	// luminance (upper)
	{
		DoubleParamDescriptor* param = desc.defineDoubleParam(kParamLuminanceUpper);
		param->setLabel(kParamLuminanceUpperLabel);
		param->setRange(0., 1.);
		param->setDisplayRange(0., 1.);
		param->setDigits(5);
		param->setDefault(1.0);
		param->setAnimates(true);
		if (page) {
			page->addChild(*param);
		}
	}

	//mix
	// GENERIC (MASKED)
	//
	{
		OFX::DoubleParamDescriptor* param = desc.defineDoubleParam(kParamMix);
		param->setLabel(kParamMixLabel);
		param->setHint(kParamMixHint);
		param->setDefault(1.);
		param->setRange(0., 1.);
		param->setIncrement(0.01);
		param->setDisplayRange(0., 1.);
		if (page) {
			page->addChild(*param);
		}
	}
}

OFX::ImageEffect* RadialBlurFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum /*context*/)
{
	return new RadialBlurPlugin(handle);
}

//register plugin
void getRadialBlurID(OFX::PluginFactoryArray &ids)
{
	static RadialBlurFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
	ids.push_back(&p);
}
