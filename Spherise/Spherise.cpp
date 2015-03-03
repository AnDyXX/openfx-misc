/*
OFX Spherise plugin.

Copyright (C) 2015 AnDyX


*/

#include "Spherise.h"

#include <cmath>
#include <cstring>
#ifdef _WINDOWS
#include <windows.h>
#endif

#include "ofxsTransformInteract.h"

#include "ofxsProcessing.H"
#include "ofxsMaskMix.h"
#include "ofxsMacros.h"

#define kPluginName "SpheriseOFX"
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



#define kParamTransformTranslate "translate"
#define kParamTransformTranslateLabel "Translate"





using namespace OFX;


////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class AddSpherisePlugin : public OFX::ImageEffect
{
public:
	/** @brief ctor */
	AddSpherisePlugin(OfxImageEffectHandle handle)
		: ImageEffect(handle)
		, _dstClip(0)
		, _srcClip(0)
	{
		_dstClip = fetchClip(kOfxImageEffectOutputClipName);
		assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentAlpha || _dstClip->getPixelComponents() == ePixelComponentRGB || _dstClip->getPixelComponents() == ePixelComponentRGBA));
		_srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
		assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentAlpha || _srcClip->getPixelComponents() == ePixelComponentRGB || _srcClip->getPixelComponents() == ePixelComponentRGBA));
	}
private:
	/* Override the render */
	virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

	virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime) OVERRIDE FINAL;

	virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

private:
	// do not need to delete these, the ImageEffect is managing them for us
	OFX::Clip *_dstClip;
	OFX::Clip *_srcClip;
};


// the overridden render function
void
AddSpherisePlugin::render(const OFX::RenderArguments &args)
{
}

bool
AddSpherisePlugin::isIdentity(const IsIdentityArguments &/*args*/, Clip * &identityClip, double &/*identityTime*/)
{
	identityClip = _srcClip;
	return true;
}

void
AddSpherisePlugin::changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName)
{

}

//factory
mDeclarePluginFactory(AddSpheriseFactory, {}, {});

void AddSpheriseFactory::describe(OFX::ImageEffectDescriptor &desc)
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

void AddSpheriseFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
{
	// Source clip only in the filter context
	// create the mandated source clip
	ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
	srcClip->addSupportedComponent(ePixelComponentNone);
	srcClip->addSupportedComponent(ePixelComponentRGBA);
	srcClip->addSupportedComponent(ePixelComponentRGB);
	srcClip->addSupportedComponent(ePixelComponentAlpha);
	srcClip->addSupportedComponent(ePixelComponentCustom);
	srcClip->setTemporalClipAccess(false);
	srcClip->setSupportsTiles(kSupportsTiles);
	srcClip->setIsMask(false);

	// create the mandated output clip
	ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
	dstClip->addSupportedComponent(ePixelComponentNone);
	dstClip->addSupportedComponent(ePixelComponentRGBA);
	dstClip->addSupportedComponent(ePixelComponentRGB);
	dstClip->addSupportedComponent(ePixelComponentAlpha);
	dstClip->addSupportedComponent(ePixelComponentCustom);
	dstClip->setSupportsTiles(kSupportsTiles);

	// make some pages and to things in
	PageParamDescriptor *page = desc.definePageParam("Controls");
	// translate
	{
		Double2DParamDescriptor* param = desc.defineDouble2DParam(kParamTransformTranslate);
		param->setLabel(kParamTransformTranslateLabel);
		param->setDoubleType(eDoubleTypeXYAbsolute);
		param->setDefault(0, 0);
		param->setIncrement(10.);
		if (page) {
			page->addChild(*param);
		}
	}
}

OFX::ImageEffect* AddSpheriseFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum /*context*/)
{
	return new AddSpherisePlugin(handle);
}

//register plugin
void getSpherisePluginID(OFX::PluginFactoryArray &ids)
{
	static AddSpheriseFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
	ids.push_back(&p);
}
