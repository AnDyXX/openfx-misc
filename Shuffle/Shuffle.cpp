/*
OFX Shuffle plugin.

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

#include "Shuffle.h"

#include <cmath>
#ifdef _WINDOWS
#include <windows.h>
#endif
#include <set>
#include <algorithm>

#include "ofxsProcessing.H"
#include "ofxsPixelProcessor.h"
#include "ofxsMacros.h"

#define kPluginName "ShuffleOFX"
#define kPluginGrouping "Channel"
#define kPluginDescription "Rearrange channels from one or two inputs and/or convert to different bit depth or components. No colorspace conversion is done (mapping is linear, even for 8-bit and 16-bit types)."
#define kPluginIdentifier "net.sf.openfx.ShufflePlugin"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kSupportsTiles 1
#define kSupportsMultiResolution 1
#define kSupportsRenderScale 1
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths true // can convert depth
#define kRenderThreadSafety eRenderFullySafe

#define kEnableMultiPlanar true

#define kParamOutputComponents "outputComponents"
#define kParamOutputComponentsLabel "Output Components"
#define kParamOutputComponentsHint "Components in the output"
#define kParamOutputComponentsOptionRGBA "RGBA"
#define kParamOutputComponentsOptionRGB "RGB"
#define kParamOutputComponentsOptionAlpha "Alpha"
#ifdef OFX_EXTENSIONS_NATRON
#define kParamOutputComponentsOptionXY "XY"
#endif

#define kParamOutputChannels "outputChannels"
#define kParamOutputChannelsLabel "Channels"
#define kParamOutputChannelsHint "The channels that will be written to in output"


#define kParamOutputBitDepth "outputBitDepth"
#define kParamOutputBitDepthLabel "Output Bit Depth"
#define kParamOutputBitDepthHint "Bit depth of the output.\nWARNING: the conversion is linear, even for 8-bit or 16-bit depth. Use with care."
#define kParamOutputBitDepthOptionByte "Byte (8 bits)"
#define kParamOutputBitDepthOptionShort "Short (16 bits)"
#define kParamOutputBitDepthOptionFloat "Float (32 bits)"

#define kParamOutputR "outputR"
#define kParamOutputRLabel "R"
#define kParamOutputRHint "Input channel for the output red channel"

#define kParamOutputG "outputG"
#define kParamOutputGLabel "G"
#define kParamOutputGHint "Input channel for the output green channel"

#define kParamOutputB "outputB"
#define kParamOutputBLabel "B"
#define kParamOutputBHint "Input channel for the output blue channel"

#define kParamOutputA "outputA"
#define kParamOutputALabel "A"
#define kParamOutputAHint "Input channel for the output alpha channel"

#define kParamOutputOptionAR "A.r"
#define kParamOutputOptionARHint "R channel from input A"
#define kParamOutputOptionAG "A.g"
#define kParamOutputOptionAGHint "G channel from input A"
#define kParamOutputOptionAB "A.b"
#define kParamOutputOptionABHint "B channel from input A"
#define kParamOutputOptionAA "A.a"
#define kParamOutputOptionAAHint "A channel from input A"
#define kParamOutputOption0 "0"
#define kParamOutputOption0Hint "0 constant channel"
#define kParamOutputOption1 "1"
#define kParamOutputOption1Hint "1 constant channel"
#define kParamOutputOptionBR "B.r"
#define kParamOutputOptionBRHint "R channel from input B"
#define kParamOutputOptionBG "B.g"
#define kParamOutputOptionBGHint "G channel from input B"
#define kParamOutputOptionBB "B.b"
#define kParamOutputOptionBBHint "B channel from input B"
#define kParamOutputOptionBA "B.a"
#define kParamOutputOptionBAHint "A channel from input B"

#define kShuffleColorPlaneName "RGBA"
#define kShuffleMotionBackwardPlaneName "Backward"
#define kShuffleMotionForwardPlaneName "Forward"
#define kShuffleDisparityLeftPlaneName "DisparityLeft"
#define kShuffleDisparityRightPlaneName "DisparityRight"

#define kParamClipInfo "clipInfo"
#define kParamClipInfoLabel "Clip Info..."
#define kParamClipInfoHint "Display information about the inputs"

// TODO: sRGB/Rec.709 conversions for byte/short types

enum InputChannelEnum {
	eInputChannelAR = 0,
	eInputChannelAG,
	eInputChannelAB,
	eInputChannelAA,
	eInputChannel0,
	eInputChannel1,
	eInputChannelBR,
	eInputChannelBG,
	eInputChannelBB,
	eInputChannelBA,
};

#define kClipA "A"
#define kClipB "B"

static bool gSupportsBytes = false;
static bool gSupportsShorts = false;
static bool gSupportsFloats = false;
static bool gSupportsRGBA = false;
static bool gSupportsRGB = false;
static bool gSupportsAlpha = false;
#ifdef OFX_EXTENSIONS_NATRON
static bool gSupportsXY = false;
#endif
static bool gSupportsDynamicChoices = false;
static bool gIsMultiPlanar = false;

static OFX::PixelComponentEnum gOutputComponentsMap[5]; // 4 components + a sentinel at the end with ePixelComponentNone
static OFX::BitDepthEnum gOutputBitDepthMap[4]; // 3 possible bit depths + a sentinel

using namespace OFX;

class ShufflerBase : public OFX::ImageProcessor
{
protected:
	const OFX::Image *_srcImgA;
	const OFX::Image *_srcImgB;
	PixelComponentEnum _outputComponents;
	int _outputComponentCount;
	BitDepthEnum _outputBitDepth;
	std::vector<InputChannelEnum> _channelMap;

public:
	ShufflerBase(OFX::ImageEffect &instance)
		: OFX::ImageProcessor(instance)
		, _srcImgA(0)
		, _srcImgB(0)
		, _outputComponents(ePixelComponentNone)
		, _outputComponentCount(0)
		, _outputBitDepth(eBitDepthNone)
		, _channelMap()
	{
		}

	void setSrcImg(const OFX::Image *A, const OFX::Image *B) { _srcImgA = A; _srcImgB = B; }

	void setValues(PixelComponentEnum outputComponents,
		int outputComponentCount,
		BitDepthEnum outputBitDepth,
		const std::vector<InputChannelEnum> &channelMap)
	{
		_outputComponents = outputComponents,
			_outputComponentCount = outputComponentCount,
			_outputBitDepth = outputBitDepth;
		assert(_outputComponentCount == (int)channelMap.size());
		_channelMap = channelMap;
	}
};

//////////////////////////////
// PIXEL CONVERSION ROUTINES

/// maps 0-(numvals-1) to 0.-1.
template<int numvals>
static float intToFloat(int value)
{
	return value / (float)(numvals - 1);
}

/// maps °.-1. to 0-(numvals-1)
template<int numvals>
static int floatToInt(float value)
{
	if (value <= 0) {
		return 0;
	}
	else if (value >= 1.) {
		return numvals - 1;
	}
	return value * (numvals - 1) + 0.5f;
}

template <typename SRCPIX, typename DSTPIX>
static DSTPIX convertPixelDepth(SRCPIX pix);

///explicit template instantiations

template <> float convertPixelDepth(unsigned char pix)
{
	return intToFloat<65536>(pix);
}

template <> unsigned short convertPixelDepth(unsigned char pix)
{
	// 0x01 -> 0x0101, 0x02 -> 0x0202, ..., 0xff -> 0xffff
	return (unsigned short)((pix << 8) + pix);
}

template <> unsigned char convertPixelDepth(unsigned char pix)
{
	return pix;
}

template <> unsigned char convertPixelDepth(unsigned short pix)
{
	// the following is from ImageMagick's quantum.h
	return (unsigned char)(((pix + 128UL) - ((pix + 128UL) >> 8)) >> 8);
}

template <> float convertPixelDepth(unsigned short pix)
{
	return intToFloat<65536>(pix);
}

template <> unsigned short convertPixelDepth(unsigned short pix)
{
	return pix; 
}

template <> unsigned char convertPixelDepth(float pix)
{
	return (unsigned char)floatToInt<256>(pix);
}

template <> unsigned short convertPixelDepth(float pix)
{
	return (unsigned short)floatToInt<65536>(pix);
}

template <> float convertPixelDepth(float pix)
{
	return pix;
}


template <class PIXSRC, class PIXDST, int nComponentsDst>
class Shuffler : public ShufflerBase
{
public:
	Shuffler(OFX::ImageEffect &instance)
		: ShufflerBase(instance)
	{
	}

private:
	void multiThreadProcessImages(OfxRectI procWindow)
	{
		const OFX::Image* channelMapImg[nComponentsDst];
		int channelMapComp[nComponentsDst]; // channel component, or value if no image
		int srcMapComp[4]; // R,G,B,A components for src
		PixelComponentEnum srcComponents = ePixelComponentNone;
		if (_srcImgA) {
			srcComponents = _srcImgA->getPixelComponents();
		}
		else if (_srcImgB) {
			srcComponents = _srcImgB->getPixelComponents();
		}
		switch (srcComponents) {
		case OFX::ePixelComponentRGBA:
			srcMapComp[0] = 0;
			srcMapComp[1] = 1;
			srcMapComp[2] = 2;
			srcMapComp[3] = 3;
			break;
		case OFX::ePixelComponentRGB:
			srcMapComp[0] = 0;
			srcMapComp[1] = 1;
			srcMapComp[2] = 2;
			srcMapComp[3] = -1;
			break;
		case OFX::ePixelComponentAlpha:
			srcMapComp[0] = -1;
			srcMapComp[1] = -1;
			srcMapComp[2] = -1;
			srcMapComp[3] = 0;
			break;
#ifdef OFX_EXTENSIONS_NATRON
		case OFX::ePixelComponentXY:
			srcMapComp[0] = 0;
			srcMapComp[1] = 1;
			srcMapComp[2] = -1;
			srcMapComp[3] = -1;
			break;
#endif
		default:
			srcMapComp[0] = -1;
			srcMapComp[1] = -1;
			srcMapComp[2] = -1;
			srcMapComp[3] = -1;
			break;
		}
		for (int c = 0; c < nComponentsDst; ++c) {
			channelMapImg[c] = NULL;
			channelMapComp[c] = 0;
			switch (_channelMap[c]) {
			case eInputChannelAR:
				if (_srcImgA && srcMapComp[0] >= 0) {
					channelMapImg[c] = _srcImgA;
					channelMapComp[c] = srcMapComp[0]; // srcImg may not have R!!!
				}
				break;
			case eInputChannelAG:
				if (_srcImgA && srcMapComp[1] >= 0) {
					channelMapImg[c] = _srcImgA;
					channelMapComp[c] = srcMapComp[1];
				}
				break;
			case eInputChannelAB:
				if (_srcImgA && srcMapComp[2] >= 0) {
					channelMapImg[c] = _srcImgA;
					channelMapComp[c] = srcMapComp[2];
				}
				break;
			case eInputChannelAA:
				if (_srcImgA && srcMapComp[3] >= 0) {
					channelMapImg[c] = _srcImgA;
					channelMapComp[c] = srcMapComp[3];
				}
				break;
			case eInputChannel0:
				channelMapComp[c] = 0;
				break;
			case eInputChannel1:
				channelMapComp[c] = 1;
				break;
			case eInputChannelBR:
				if (_srcImgB && srcMapComp[0] >= 0) {
					channelMapImg[c] = _srcImgB;
					channelMapComp[c] = srcMapComp[0];
				}
				break;
			case eInputChannelBG:
				if (_srcImgB && srcMapComp[1] >= 0) {
					channelMapImg[c] = _srcImgB;
					channelMapComp[c] = srcMapComp[1];
				}
				break;
			case eInputChannelBB:
				if (_srcImgB && srcMapComp[2] >= 0) {
					channelMapImg[c] = _srcImgB;
					channelMapComp[c] = srcMapComp[2];
				}
				break;
			case eInputChannelBA:
				if (_srcImgB && srcMapComp[3] >= 0) {
					channelMapImg[c] = _srcImgB;
					channelMapComp[c] = srcMapComp[3];
				}
				break;
			}
		}
		// now compute the transformed image, component by component
		for (int c = 0; c < nComponentsDst; ++c) {
			const OFX::Image* srcImg = channelMapImg[c];
			int srcComp = channelMapComp[c];

			for (int y = procWindow.y1; y < procWindow.y2; y++) {
				if (_effect.abort()) {
					break;
				}

				PIXDST *dstPix = (PIXDST *)_dstImg->getPixelAddress(procWindow.x1, y);

				for (int x = procWindow.x1; x < procWindow.x2; x++) {
					PIXSRC *srcPix = (PIXSRC *)(srcImg ? srcImg->getPixelAddress(x, y) : 0);
					// if there is a srcImg but we are outside of its RoD, it should be considered black and transparent
					dstPix[c] = srcImg ? convertPixelDepth<PIXSRC, PIXDST>(srcPix ? srcPix[srcComp] : 0) : convertPixelDepth<float, PIXDST>(srcComp);
					dstPix += nComponentsDst;
				}
			}
		}
	}
};

struct InputPlaneChannel {
	OFX::Image* img;
	int channelIndex;
	bool fillZero;

	InputPlaneChannel() : img(0), channelIndex(-1), fillZero(true) {}
};

class MultiPlaneShufflerBase : public OFX::ImageProcessor
{
protected:

	int _outputComponentCount;
	BitDepthEnum _outputBitDepth;
	int _nComponentsDst;
	std::vector<InputPlaneChannel> _inputPlanes;

public:
	MultiPlaneShufflerBase(OFX::ImageEffect &instance)
		: OFX::ImageProcessor(instance)
		, _outputComponentCount(0)
		, _outputBitDepth(eBitDepthNone)
		, _nComponentsDst(0)
		, _inputPlanes(_nComponentsDst)
	{
		}

	void setValues(int outputComponentCount,
		BitDepthEnum outputBitDepth,
		const std::vector<InputPlaneChannel>& planes)
	{
		_outputComponentCount = outputComponentCount,
			_outputBitDepth = outputBitDepth;
		_inputPlanes = planes;

	}
};


template <class PIXSRC, class PIXDST, int nComponentsDst>
class MultiPlaneShuffler : public MultiPlaneShufflerBase
{
public:
	MultiPlaneShuffler(OFX::ImageEffect &instance)
		: MultiPlaneShufflerBase(instance)
	{
	}

private:
	void multiThreadProcessImages(OfxRectI procWindow)
	{
		assert(_inputPlanes.size() == nComponentsDst);
		// now compute the transformed image, component by component
		for (int c = 0; c < nComponentsDst; ++c) {

			const OFX::Image* srcImg = _inputPlanes[c].img;
			int srcComp = _inputPlanes[c].channelIndex;
			if (!srcImg) {
				srcComp = _inputPlanes[c].fillZero ? 0. : 1.;
			}

			for (int y = procWindow.y1; y < procWindow.y2; y++) {
				if (_effect.abort()) {
					break;
				}

				PIXDST *dstPix = (PIXDST *)_dstImg->getPixelAddress(procWindow.x1, y);

				for (int x = procWindow.x1; x < procWindow.x2; x++) {
					PIXSRC *srcPix = (PIXSRC *)(srcImg ? srcImg->getPixelAddress(x, y) : 0);
					// if there is a srcImg but we are outside of its RoD, it should be considered black and transparent
					dstPix[c] = srcImg ? convertPixelDepth<PIXSRC, PIXDST>(srcPix ? srcPix[srcComp] : 0) : convertPixelDepth<float, PIXDST>(srcComp);
					dstPix += nComponentsDst;
				}
			}
		}
	}
};


////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class ShufflePlugin : public OFX::ImageEffect
{
public:
	/** @brief ctor */
	ShufflePlugin(OfxImageEffectHandle handle, OFX::ContextEnum context)
		: ImageEffect(handle)
		, _dstClip(0)
		, _srcClipA(0)
		, _srcClipB(0)
		, _outputComponents(0)
		, _outputBitDepth(0)
		, _r(0)
		, _g(0)
		, _b(0)
		, _a(0)
	{
		_dstClip = fetchClip(kOfxImageEffectOutputClipName);
		assert(_dstClip && (1 <= _dstClip->getPixelComponentCount() && _dstClip->getPixelComponentCount() <= 4));
		_srcClipA = fetchClip(context == eContextGeneral ? kClipA : kOfxImageEffectSimpleSourceClipName);
		assert(_srcClipA && (1 <= _srcClipA->getPixelComponentCount() && _srcClipA->getPixelComponentCount() <= 4));
		if (context == eContextGeneral) {
			_srcClipB = fetchClip(kClipB);
			assert(_srcClipB && (1 <= _srcClipB->getPixelComponentCount() && _srcClipB->getPixelComponentCount() <= 4));
		}
		if (gIsMultiPlanar) {
			_outputComponents = fetchChoiceParam(kParamOutputChannels);
		}
		else {
			_outputComponents = fetchChoiceParam(kParamOutputComponents);
		}
		if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
			_outputBitDepth = fetchChoiceParam(kParamOutputBitDepth);
		}
		_r = fetchChoiceParam(kParamOutputR);
		_g = fetchChoiceParam(kParamOutputG);
		_b = fetchChoiceParam(kParamOutputB);
		_a = fetchChoiceParam(kParamOutputA);
		//enableComponents();
	}

private:

	void setChannelsFromRed();

	/* Override the render */
	virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

	/* override is identity */
	virtual bool isIdentity(const OFX::IsIdentityArguments &args, OFX::Clip * &identityClip, double &identityTime) OVERRIDE FINAL;

	virtual bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;

	virtual void getClipComponents(const OFX::ClipComponentsArguments& args, OFX::ClipComponentsSetter& clipComponents) OVERRIDE FINAL;

	/** @brief get the clip preferences */
	virtual void getClipPreferences(ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;

	/* override changedParam */
	virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

	/** @brief called when a clip has just been changed in some way (a rewire maybe) */
	virtual void changedClip(const InstanceChangedArgs &args, const std::string &clipName) OVERRIDE FINAL;

private:

	bool isIdentityInternal(OFX::Clip*& identityClip);

	bool getPlaneNeededForParam(const std::list<std::string>& aComponents,
		const std::list<std::string>& bComponents,
		OFX::ChoiceParam* param,
		OFX::Clip** clip,
		std::string* ofxPlane,
		std::string* ofxComponents,
		int* channelIndexInPlane) const;

	bool getPlaneNeededInOutput(const std::list<std::string>& components,
		OFX::ChoiceParam* param,
		std::string* ofxPlane,
		std::string* ofxComponents) const;

	void buildChannelMenus(const std::list<std::string> &outputComponents);

	void enableComponents();

	/* internal render function */
	template <class DSTPIX, int nComponentsDst>
	void renderInternalForDstBitDepth(const OFX::RenderArguments &args, OFX::BitDepthEnum srcBitDepth);

	template <int nComponentsDst>
	void renderInternal(const OFX::RenderArguments &args, OFX::BitDepthEnum srcBitDepth, OFX::BitDepthEnum dstBitDepth);

	/* set up and run a processor */
	void setupAndProcess(ShufflerBase &, const OFX::RenderArguments &args);
	void setupAndProcessMultiPlane(MultiPlaneShufflerBase &, const OFX::RenderArguments &args);

	// do not need to delete these, the ImageEffect is managing them for us
	OFX::Clip *_dstClip;
	OFX::Clip *_srcClipA;
	OFX::Clip *_srcClipB;

	OFX::ChoiceParam *_outputComponents;
	OFX::ChoiceParam *_outputBitDepth;
	OFX::ChoiceParam *_r;
	OFX::ChoiceParam *_g;
	OFX::ChoiceParam *_b;
	OFX::ChoiceParam *_a;
};




static void extractChannelsFromComponentString(const std::string& comp,
	std::string* layer,
	std::string* pairedLayer, //< if disparity or motion vectors
	std::vector<std::string>* channels)
{
	if (comp == kOfxImageComponentAlpha) {
		//*layer = kShuffleColorPlaneName;
		channels->push_back("A");
	}
	else if (comp == kOfxImageComponentRGB) {
		//*layer = kShuffleColorPlaneName;
		channels->push_back("R");
		channels->push_back("G");
		channels->push_back("B");
	}
	else if (comp == kOfxImageComponentRGBA) {
		//*layer = kShuffleColorPlaneName;
		channels->push_back("R");
		channels->push_back("G");
		channels->push_back("B");
		channels->push_back("A");
	}
	else if (comp == kFnOfxImageComponentMotionVectors) {
		*layer = kShuffleMotionBackwardPlaneName;
		*pairedLayer = kShuffleMotionForwardPlaneName;
		channels->push_back("U");
		channels->push_back("V");
	}
	else if (comp == kFnOfxImageComponentStereoDisparity) {
		*layer = kShuffleDisparityLeftPlaneName;
		*pairedLayer = kShuffleDisparityRightPlaneName;
		channels->push_back("X");
		channels->push_back("Y");
#ifdef OFX_EXTENSIONS_NATRON
	}
	else if (comp == kNatronOfxImageComponentXY) {
		channels->push_back("X");
		channels->push_back("Y");
	}
	else {
		std::vector<std::string> layerChannels = mapPixelComponentCustomToLayerChannels(comp);
		if (layerChannels.size() >= 1) {
			*layer = layerChannels[0];
			channels->assign(layerChannels.begin() + 1, layerChannels.end());
		}
#endif
	}
}

static void appendComponents(const std::string& clipName,
	const std::list<std::string>& components,
	OFX::ChoiceParam** params)
{

	std::list<std::string> usedComps;
	for (std::list<std::string>::const_iterator it = components.begin(); it != components.end(); ++it) {
		std::string layer, secondLayer;
		std::vector<std::string> channels;
		extractChannelsFromComponentString(*it, &layer, &secondLayer, &channels);
		if (channels.empty()) {
			continue;
		}
		if (layer.empty()) {
			//Ignore color plane
			continue;
		}
		for (std::size_t i = 0; i < channels.size(); ++i) {
			std::string opt = clipName + ".";
			if (!layer.empty()) {
				opt.append(layer);
				opt.push_back('.');
			}
			opt.append(channels[i]);

			if (std::find(usedComps.begin(), usedComps.end(), opt) == usedComps.end()) {
				usedComps.push_back(opt);
				for (int j = 0; j < 4; ++j) {
					params[j]->appendOption(opt, channels[i] + " channel from " + (layer.empty() ? std::string() : std::string("layer/view ") + layer + " of ") + "input " + clipName);
				}
			}

		}

		if (!secondLayer.empty()) {
			for (std::size_t i = 0; i < channels.size(); ++i) {
				std::string opt = clipName + ".";
				if (!secondLayer.empty()) {
					opt.append(secondLayer);
					opt.push_back('.');
				}
				opt.append(channels[i]);
				if (std::find(usedComps.begin(), usedComps.end(), opt) == usedComps.end()) {
					usedComps.push_back(opt);
					for (int j = 0; j < 4; ++j) {
						params[j]->appendOption(opt, channels[i] + " channel from layer " + secondLayer + " of input " + clipName);
					}
				}
			}
		}
	}
}

template<typename T>
static void
addInputChannelOptionsRGBA(T* outputR, OFX::ContextEnum context)
{
	assert(outputR->getNOptions() == eInputChannelAR);
	outputR->appendOption(kParamOutputOptionAR, kParamOutputOptionARHint);
	assert(outputR->getNOptions() == eInputChannelAG);
	outputR->appendOption(kParamOutputOptionAG, kParamOutputOptionAGHint);
	assert(outputR->getNOptions() == eInputChannelAB);
	outputR->appendOption(kParamOutputOptionAB, kParamOutputOptionABHint);
	assert(outputR->getNOptions() == eInputChannelAA);
	outputR->appendOption(kParamOutputOptionAA, kParamOutputOptionAAHint);
	assert(outputR->getNOptions() == eInputChannel0);
	outputR->appendOption(kParamOutputOption0, kParamOutputOption0Hint);
	assert(outputR->getNOptions() == eInputChannel1);
	outputR->appendOption(kParamOutputOption1, kParamOutputOption1Hint);
	if (context == eContextGeneral) {
		assert(outputR->getNOptions() == eInputChannelBR);
		outputR->appendOption(kParamOutputOptionBR, kParamOutputOptionBRHint);
		assert(outputR->getNOptions() == eInputChannelBG);
		outputR->appendOption(kParamOutputOptionBG, kParamOutputOptionBGHint);
		assert(outputR->getNOptions() == eInputChannelBB);
		outputR->appendOption(kParamOutputOptionBB, kParamOutputOptionBBHint);
		assert(outputR->getNOptions() == eInputChannelBA);
		outputR->appendOption(kParamOutputOptionBA, kParamOutputOptionBAHint);
	}
}

void
ShufflePlugin::buildChannelMenus(const std::list<std::string> &outputComponents)
{
	assert(gSupportsDynamicChoices);

	std::list<std::string> componentsA = _srcClipA->getComponentsPresent();
	std::list<std::string> componentsB = _srcClipB->getComponentsPresent();
	_r->resetOptions();
	_g->resetOptions();
	_b->resetOptions();
	_a->resetOptions();

	//Always add RGBA channels for color plane
	addInputChannelOptionsRGBA(_r, getContext());
	addInputChannelOptionsRGBA(_g, getContext());
	addInputChannelOptionsRGBA(_b, getContext());
	addInputChannelOptionsRGBA(_a, getContext());

	OFX::ChoiceParam* params[4] = { _r, _g, _b, _a };
	appendComponents(kClipA, componentsA, params);
	appendComponents(kClipB, componentsB, params);

	if (gIsMultiPlanar) {

		_outputComponents->resetOptions();
		_outputComponents->appendOption(kShuffleColorPlaneName);
		_outputComponents->appendOption(kShuffleMotionForwardPlaneName);
		_outputComponents->appendOption(kShuffleMotionBackwardPlaneName);
		_outputComponents->appendOption(kShuffleDisparityLeftPlaneName);
		_outputComponents->appendOption(kShuffleDisparityRightPlaneName);
#ifdef OFX_EXTENSIONS_NATRON
		for (std::list<std::string>::const_iterator it = outputComponents.begin(); it != outputComponents.end(); ++it) {
			std::string layer, secondLayer;
			std::vector<std::string> channels;
			extractChannelsFromComponentString(*it, &layer, &secondLayer, &channels);
			if (channels.empty()) {
				continue;
			}
			if (layer.empty()) {
				//Ignore color plane
				continue;
			}
			_outputComponents->appendOption(layer);
		}
#endif
	}
}

bool
ShufflePlugin::getPlaneNeededForParam(const std::list<std::string>& aComponents,
const std::list<std::string>& bComponents,
OFX::ChoiceParam* param,
OFX::Clip** clip,
std::string* ofxPlane,
std::string* ofxComponents,
int* channelIndexInPlane) const
{
	assert(clip);
	*clip = 0;

	int channelIndex;
	param->getValue(channelIndex);
	std::string channelEncoded;
	param->getOption(channelIndex, channelEncoded);
	if (channelEncoded.empty()) {
		return false;
	}

	if (channelEncoded == kParamOutputOption0) {
		*ofxComponents = kParamOutputOption0;
		return true;
	}

	if (channelEncoded == kParamOutputOption1) {
		*ofxComponents = kParamOutputOption1;
		return true;
	}

	std::string clipName = kClipA;

	// Must be at least something like "A."
	if (channelEncoded.size() < clipName.size() + 1) {
		return false;
	}

	if (channelEncoded.substr(0, clipName.size()) == clipName) {
		*clip = _srcClipA;
	}

	if (!*clip) {
		clipName = kClipB;
		if (channelEncoded.substr(0, clipName.size()) == clipName) {
			*clip = _srcClipB;
		}
	}

	if (!*clip) {
		return false;
	}

	std::size_t lastDotPos = channelEncoded.find_last_of('.');
	if (lastDotPos == std::string::npos || lastDotPos == channelEncoded.size() - 1) {
		*clip = 0;
		return false;
	}

	std::string chanName = channelEncoded.substr(lastDotPos + 1, std::string::npos);
	std::string layerName;
	for (std::size_t i = clipName.size() + 1; i < lastDotPos; ++i) {
		layerName.push_back(channelEncoded[i]);
	}

	if (layerName == kShuffleColorPlaneName || layerName.empty()) {
		std::string comp = (*clip)->getPixelComponentsProperty();
		if (chanName == "r" || chanName == "R" || chanName == "x" || chanName == "X") {
			*channelIndexInPlane = 0;
		}
		else if (chanName == "g" || chanName == "G" || chanName == "y" || chanName == "Y") {
			*channelIndexInPlane = 1;
		}
		else if (chanName == "b" || chanName == "B" || chanName == "z" || chanName == "Z") {
			*channelIndexInPlane = 2;
		}
		else if (chanName == "a" || chanName == "A" || chanName == "w" || chanName == "W") {
			if (comp == kOfxImageComponentAlpha) {
				*channelIndexInPlane = 0;
			}
			else if (comp == kOfxImageComponentRGBA) {
				*channelIndexInPlane = 3;
			}
			else {
				*ofxComponents = kParamOutputOption0;
				return true;
			}
		}
		else {
			assert(false);
		}
		*ofxComponents = comp;
		*ofxPlane = kFnOfxImagePlaneColour;
		return true;
	}
	else if (layerName == kShuffleDisparityLeftPlaneName) {
		if (chanName == "x" || chanName == "X") {
			*channelIndexInPlane = 0;
		}
		else if (chanName == "y" || chanName == "Y") {
			*channelIndexInPlane = 1;
		}
		else {
			assert(false);
		}
		*ofxComponents = kFnOfxImageComponentStereoDisparity;
		*ofxPlane = kFnOfxImagePlaneStereoDisparityLeft;
		return true;
	}
	else if (layerName == kShuffleDisparityRightPlaneName) {
		if (chanName == "x" || chanName == "X") {
			*channelIndexInPlane = 0;
		}
		else if (chanName == "y" || chanName == "Y") {
			*channelIndexInPlane = 1;
		}
		else {
			assert(false);
		}
		*ofxComponents = kFnOfxImageComponentStereoDisparity;
		*ofxPlane = kFnOfxImagePlaneStereoDisparityRight;
		return true;
	}
	else if (layerName == kShuffleMotionBackwardPlaneName) {
		if (chanName == "u" || chanName == "U") {
			*channelIndexInPlane = 0;
		}
		else if (chanName == "v" || chanName == "V") {
			*channelIndexInPlane = 1;
		}
		else {
			assert(false);
		}
		*ofxComponents = kFnOfxImageComponentMotionVectors;
		*ofxPlane = kFnOfxImagePlaneBackwardMotionVector;
		return true;
	}
	else if (layerName == kShuffleMotionForwardPlaneName) {
		if (chanName == "u" || chanName == "U") {
			*channelIndexInPlane = 0;
		}
		else if (chanName == "v" || chanName == "V") {
			*channelIndexInPlane = 1;
		}
		else {
			assert(false);
		}
		*ofxComponents = kFnOfxImageComponentMotionVectors;
		*ofxPlane = kFnOfxImagePlaneForwardMotionVector;
		return true;
#ifdef OFX_EXTENSIONS_NATRON
	}
	else {
		//Find in aComponents or bComponents a layer matching the name of the layer
		for (std::list<std::string>::const_iterator it = aComponents.begin(); it != aComponents.end(); ++it) {
			if (it->find(layerName) != std::string::npos) {
				//We found a matching layer
				std::string realLayerName;
				std::vector<std::string> channels;
				std::vector<std::string> layerChannels = mapPixelComponentCustomToLayerChannels(*it);
				if (layerChannels.empty()) {
					// ignore it
					continue;
				}
				channels.assign(layerChannels.begin() + 1, layerChannels.end());
				int foundChannel = -1;
				for (std::size_t i = 0; i < channels.size(); ++i) {
					if (channels[i] == chanName) {
						foundChannel = i;
						break;
					}
				}
				assert(foundChannel != -1);
				*ofxPlane = *it;
				*channelIndexInPlane = foundChannel;
				*ofxComponents = *it;
				return true;
			}
		}

		for (std::list<std::string>::const_iterator it = bComponents.begin(); it != bComponents.end(); ++it) {
			if (it->find(layerName) != std::string::npos) {
				//We found a matching layer
				std::string realLayerName;
				std::vector<std::string> channels;
				std::vector<std::string> layerChannels = mapPixelComponentCustomToLayerChannels(*it);
				if (layerChannels.empty()) {
					// ignore it
					continue;
				}
				channels.assign(layerChannels.begin() + 1, layerChannels.end());
				int foundChannel = -1;
				for (std::size_t i = 0; i < channels.size(); ++i) {
					if (channels[i] == chanName) {
						foundChannel = i;
						break;
					}
				}
				assert(foundChannel != -1);
				*ofxPlane = *it;
				*channelIndexInPlane = foundChannel;
				*ofxComponents = *it;
				return true;
			}
		}
#endif // OFX_EXTENSIONS_NATRON
	}
	return false;
}


bool
ShufflePlugin::getPlaneNeededInOutput(const std::list<std::string>& components,
OFX::ChoiceParam* param,
std::string* ofxPlane,
std::string* ofxComponents) const
{
	int layer_i;
	param->getValue(layer_i);
	std::string layerName;
	param->getOption(layer_i, layerName);

	if (layerName == kShuffleColorPlaneName || layerName.empty()) {
		std::string comp = _dstClip->getPixelComponentsProperty();
		*ofxComponents = comp;
		*ofxPlane = kFnOfxImagePlaneColour;
		return true;
	}
	else if (layerName == kShuffleDisparityLeftPlaneName) {
		*ofxComponents = kFnOfxImageComponentStereoDisparity;
		*ofxPlane = kFnOfxImagePlaneStereoDisparityLeft;
		return true;
	}
	else if (layerName == kShuffleDisparityRightPlaneName) {
		*ofxComponents = kFnOfxImageComponentStereoDisparity;
		*ofxPlane = kFnOfxImagePlaneStereoDisparityRight;
		return true;
	}
	else if (layerName == kShuffleMotionBackwardPlaneName) {
		*ofxComponents = kFnOfxImageComponentMotionVectors;
		*ofxPlane = kFnOfxImagePlaneBackwardMotionVector;
		return true;
	}
	else if (layerName == kShuffleMotionForwardPlaneName) {
		*ofxComponents = kFnOfxImageComponentMotionVectors;
		*ofxPlane = kFnOfxImagePlaneForwardMotionVector;
		return true;
#ifdef OFX_EXTENSIONS_NATRON
	}
	else {
		//Find in aComponents or bComponents a layer matching the name of the layer
		for (std::list<std::string>::const_iterator it = components.begin(); it != components.end(); ++it) {
			if (it->find(layerName) != std::string::npos) {
				//We found a matching layer
				std::string realLayerName;
				std::vector<std::string> layerChannels = mapPixelComponentCustomToLayerChannels(*it);
				if (layerChannels.empty()) {
					// ignore it
					continue;
				}
				*ofxPlane = *it;
				*ofxComponents = *it;
				return true;
			}
		}
#endif // OFX_EXTENSIONS_NATRON
	}
	return false;
}


void
ShufflePlugin::getClipComponents(const OFX::ClipComponentsArguments& /*args*/, OFX::ClipComponentsSetter& clipComponents)
{

	std::list<std::string> componentsA = _srcClipA->getComponentsPresent();
	std::list<std::string> componentsB = _srcClipB->getComponentsPresent();

	if (gIsMultiPlanar) {
		std::list<std::string> outputComponents = _dstClip->getComponentsPresent();
		std::string ofxPlane, ofxComp;
		getPlaneNeededInOutput(outputComponents, _outputComponents, &ofxPlane, &ofxComp);
		clipComponents.addClipComponents(*_dstClip, ofxComp);
	}
	else {
		int outputComponents_i;
		_outputComponents->getValue(outputComponents_i);
		PixelComponentEnum outputComponents = gOutputComponentsMap[outputComponents_i];
		clipComponents.addClipComponents(*_dstClip, outputComponents);
	}

	OFX::ChoiceParam* params[4] = { _r, _g, _b, _a };

	std::map<OFX::Clip*, std::set<std::string> > clipMap;

	for (int i = 0; i < 4; ++i) {
		std::string ofxComp, ofxPlane;
		int channelIndex;
		OFX::Clip* clip = 0;
		bool ok = getPlaneNeededForParam(componentsA, componentsB, params[i], &clip, &ofxPlane, &ofxComp, &channelIndex);
		assert(ok);
		if (ofxComp == kParamOutputOption0 || ofxComp == kParamOutputOption1) {
			continue;
		}
		assert(clip);

		std::map<OFX::Clip*, std::set<std::string> >::iterator foundClip = clipMap.find(clip);
		if (foundClip == clipMap.end()) {
			std::set<std::string> s;
			s.insert(ofxComp);
			clipMap.insert(std::make_pair(clip, s));
			clipComponents.addClipComponents(*clip, ofxComp);
		}
		else {
			std::pair<std::set<std::string>::iterator, bool> ret = foundClip->second.insert(ofxComp);
			if (ret.second) {
				clipComponents.addClipComponents(*clip, ofxComp);
			}
		}
	}


}

struct IdentityChoiceData
{
	OFX::Clip* clip;
	std::string components;
	int index;
};

bool
ShufflePlugin::isIdentityInternal(OFX::Clip*& identityClip)
{
	if (!gSupportsDynamicChoices || !gIsMultiPlanar) {
		int r_i;
		_r->getValue(r_i);
		InputChannelEnum r = InputChannelEnum(r_i);
		int g_i;
		_g->getValue(g_i);
		InputChannelEnum g = InputChannelEnum(g_i);
		int b_i;
		_b->getValue(b_i);
		InputChannelEnum b = InputChannelEnum(b_i);
		int a_i;
		_a->getValue(a_i);
		InputChannelEnum a = InputChannelEnum(a_i);

		if (r == eInputChannelAR && g == eInputChannelAG && b == eInputChannelAB && a == eInputChannelAA && _srcClipA) {
			identityClip = _srcClipA;

			return true;
		}
		if (r == eInputChannelBR && g == eInputChannelBG && b == eInputChannelBB && a == eInputChannelBA && _srcClipB) {
			identityClip = _srcClipB;

			return true;
		}
		return false;
	}
	else {
		std::list<std::string> componentsA = _srcClipA->getComponentsPresent();
		std::list<std::string> componentsB = _srcClipB->getComponentsPresent();

		OFX::ChoiceParam* params[4] = { _r, _g, _b, _a };
		IdentityChoiceData data[4];

		int expectedIndex = -1;
		for (int i = 0; i < 4; ++i) {
			std::string plane;
			bool ok = getPlaneNeededForParam(componentsA, componentsB, params[i], &data[i].clip, &plane, &data[i].components, &data[i].index);
			assert(ok);
			if (!plane.empty()) {
				//This is not the color plane, no identity
				return false;
			}
			if (i > 0) {
				if (data[i].index != expectedIndex || data[i].components != data[0].components ||
					data[i].clip != data[0].clip) {
					return false;
				}
			}
			expectedIndex = data[i].index + 1;
		}
		identityClip = data[0].clip;
		return true;
	}
}

bool
ShufflePlugin::isIdentity(const OFX::IsIdentityArguments &/*args*/, OFX::Clip * &identityClip, double &/*identityTime*/)
{
	return isIdentityInternal(identityClip);
}

bool
ShufflePlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
	OFX::Clip* identityClip = 0;
	if (isIdentityInternal(identityClip)) {
		rod = identityClip->getRegionOfDefinition(args.time);
		return true;
	}
	if (_srcClipA && _srcClipA->isConnected() && _srcClipB && _srcClipB->isConnected()) {
		OfxRectD rodA = _srcClipA->getRegionOfDefinition(args.time);
		OfxRectD rodB = _srcClipB->getRegionOfDefinition(args.time);
		rod.x1 = std::min(rodA.x1, rodB.x1);
		rod.y1 = std::min(rodA.y1, rodB.y1);
		rod.x2 = std::max(rodA.x2, rodB.x2);
		rod.y2 = std::max(rodA.y2, rodB.y2);

		return true;
	}
	return false;
}


////////////////////////////////////////////////////////////////////////////////
/** @brief render for the filter */




/* set up and run a processor */
void
ShufflePlugin::setupAndProcess(ShufflerBase &processor, const OFX::RenderArguments &args)
{
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


	InputChannelEnum r, g, b, a;
	// compute the components mapping tables
	std::vector<InputChannelEnum> channelMap;

	std::auto_ptr<const OFX::Image> srcA((_srcClipA && _srcClipA->isConnected()) ?
		_srcClipA->fetchImage(args.time) : 0);
	std::auto_ptr<const OFX::Image> srcB((_srcClipB && _srcClipB->isConnected()) ?
		_srcClipB->fetchImage(args.time) : 0);
	OFX::BitDepthEnum srcBitDepth = eBitDepthNone;
	OFX::PixelComponentEnum srcComponents = ePixelComponentNone;
	if (srcA.get()) {
		if (srcA->getRenderScale().x != args.renderScale.x ||
			srcA->getRenderScale().y != args.renderScale.y ||
			(srcA->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && srcA->getField() != args.fieldToRender)) {
			setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
			OFX::throwSuiteStatusException(kOfxStatFailed);
		}
		srcBitDepth = srcA->getPixelDepth();
		srcComponents = srcA->getPixelComponents();
		assert(_srcClipA->getPixelComponents() == srcComponents);
	}

	if (srcB.get()) {
		if (srcB->getRenderScale().x != args.renderScale.x ||
			srcB->getRenderScale().y != args.renderScale.y ||
			(srcB->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && srcB->getField() != args.fieldToRender)) {
			setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
			OFX::throwSuiteStatusException(kOfxStatFailed);
		}
		OFX::BitDepthEnum    srcBBitDepth = srcB->getPixelDepth();
		OFX::PixelComponentEnum srcBComponents = srcB->getPixelComponents();
		assert(_srcClipB->getPixelComponents() == srcBComponents);
		// both input must have the same bit depth and components
		if ((srcBitDepth != eBitDepthNone && srcBitDepth != srcBBitDepth) ||
			(srcComponents != ePixelComponentNone && srcComponents != srcBComponents)) {
			OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
		}
	}

	int r_i;
	_r->getValue(r_i);
	r = InputChannelEnum(r_i);
	int g_i;
	_g->getValue(g_i);
	g = InputChannelEnum(g_i);
	int b_i;
	_b->getValue(b_i);
	b = InputChannelEnum(b_i);
	int a_i;
	_a->getValue(a_i);
	a = InputChannelEnum(a_i);


	switch (dstComponents) {
	case OFX::ePixelComponentRGBA:
		channelMap.resize(4);
		channelMap[0] = r;
		channelMap[1] = g;
		channelMap[2] = b;
		channelMap[3] = a;
		break;
#ifdef OFX_EXTENSIONS_NATRON
	case OFX::ePixelComponentXY:
		channelMap.resize(2);
		channelMap[0] = r;
		channelMap[1] = g;
		break;
#endif
	case OFX::ePixelComponentRGB:
		channelMap.resize(3);
		channelMap[0] = r;
		channelMap[1] = g;
		channelMap[2] = b;
		break;
	case OFX::ePixelComponentAlpha:
		channelMap.resize(1);
		channelMap[0] = a;
		break;
	default:
		channelMap.resize(0);
		break;
	}
	processor.setSrcImg(srcA.get(), srcB.get());

	int outputComponents_i;
	_outputComponents->getValue(outputComponents_i);
	PixelComponentEnum outputComponents = gOutputComponentsMap[outputComponents_i];
	assert(dstComponents == outputComponents);
	BitDepthEnum outputBitDepth = srcBitDepth;
	if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
		int outputBitDepth_i;
		_outputBitDepth->getValue(outputBitDepth_i);
		outputBitDepth = gOutputBitDepthMap[outputBitDepth_i];
	}
	assert(outputBitDepth == dstBitDepth);
	int outputComponentCount = dst->getPixelComponentCount();

	processor.setValues(outputComponents, outputComponentCount, outputBitDepth, channelMap);

	processor.setDstImg(dst.get());
	processor.setRenderWindow(args.renderWindow);

	processor.process();
}

class InputImagesHolder_RAII
{
	std::vector<OFX::Image*> images;
public:

	InputImagesHolder_RAII()
		: images()
	{

	}

	void appendImage(OFX::Image* img)
	{
		images.push_back(img);
	}

	~InputImagesHolder_RAII()
	{
		for (std::size_t i = 0; i < images.size(); ++i) {
			delete images[i];
		}
	}
};

void
ShufflePlugin::setupAndProcessMultiPlane(MultiPlaneShufflerBase & processor, const OFX::RenderArguments &args)
{
	std::string dstOfxPlane, dstOfxComp;
	std::list<std::string> outputComponents = _dstClip->getComponentsPresent();
	getPlaneNeededInOutput(outputComponents, _outputComponents, &dstOfxPlane, &dstOfxComp);

	std::auto_ptr<OFX::Image> dst(_dstClip->fetchImagePlane(args.time, args.renderView, dstOfxPlane.c_str()));
	if (!dst.get()) {
		OFX::throwSuiteStatusException(kOfxStatFailed);
	}
	OFX::BitDepthEnum         dstBitDepth = dst->getPixelDepth();
	int nDstComponents = dst->getPixelComponentCount();
	if (dstBitDepth != _dstClip->getPixelDepth() ||
		nDstComponents != _dstClip->getPixelComponentCount()) {
		setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong depth or components");
		OFX::throwSuiteStatusException(kOfxStatFailed);
	}
	if (dst->getRenderScale().x != args.renderScale.x ||
		dst->getRenderScale().y != args.renderScale.y ||
		(dst->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && dst->getField() != args.fieldToRender)) {
		setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
		OFX::throwSuiteStatusException(kOfxStatFailed);
	}




	std::list<std::string> componentsA = _srcClipA->getComponentsPresent();
	std::list<std::string> componentsB = _srcClipB->getComponentsPresent();

	OFX::ChoiceParam* params[4] = { _r, _g, _b, _a };

	InputImagesHolder_RAII imagesHolder;
	OFX::BitDepthEnum srcBitDepth = eBitDepthNone;

	std::map<OFX::Clip*, std::map<std::string, OFX::Image*> > fetchedPlanes;

	std::vector<InputPlaneChannel> planes;
	for (int i = 0; i < nDstComponents; ++i) {

		InputPlaneChannel p;
		OFX::Clip* clip = 0;
		std::string plane, ofxComp;
		bool ok = getPlaneNeededForParam(componentsA, componentsB,
			nDstComponents == 1 ? params[3] : params[i],
			&clip, &plane, &ofxComp, &p.channelIndex);
		assert(ok);

		p.img = 0;
		if (ofxComp == kParamOutputOption0) {
			p.fillZero = true;
		}
		else if (ofxComp == kParamOutputOption1) {
			p.fillZero = false;
		}
		else {
			std::map<OFX::Clip*, std::map<std::string, OFX::Image*> >::iterator foundClip = fetchedPlanes.find(clip);
			if (foundClip == fetchedPlanes.end()) {
				p.img = clip->fetchImagePlane(args.time, args.renderView, plane.c_str());
				if (p.img) {
					std::map<std::string, OFX::Image*> planes;
					planes.insert(std::make_pair(plane, p.img));
					fetchedPlanes.insert(std::make_pair(clip, planes));
					imagesHolder.appendImage(p.img);
				}
			}
			else {
				std::map<std::string, OFX::Image*>::iterator foundPlane = foundClip->second.find(plane);
				if (foundPlane == foundClip->second.end()) {
					foundClip->second.insert(std::make_pair(plane, p.img));
				}
				else {
					p.img = foundPlane->second;
				}

			}
		}

		if (p.img) {

			if (p.img->getRenderScale().x != args.renderScale.x ||
				p.img->getRenderScale().y != args.renderScale.y ||
				(p.img->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && p.img->getField() != args.fieldToRender)) {
				setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
				OFX::throwSuiteStatusException(kOfxStatFailed);
			}
			if (srcBitDepth == eBitDepthNone) {
				srcBitDepth = p.img->getPixelDepth();
			}
			else {
				// both input must have the same bit depth and components
				if (srcBitDepth != eBitDepthNone && srcBitDepth != p.img->getPixelDepth()) {
					OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
				}
			}
		}
		planes.push_back(p);
	}

	BitDepthEnum outputBitDepth = srcBitDepth;
	if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
		int outputBitDepth_i;
		_outputBitDepth->getValue(outputBitDepth_i);
		outputBitDepth = gOutputBitDepthMap[outputBitDepth_i];
	}
	assert(outputBitDepth == dstBitDepth);

	processor.setValues(nDstComponents, outputBitDepth, planes);

	processor.setDstImg(dst.get());
	processor.setRenderWindow(args.renderWindow);

	processor.process();
}

template <class DSTPIX, int nComponentsDst>
void
ShufflePlugin::renderInternalForDstBitDepth(const OFX::RenderArguments &args, OFX::BitDepthEnum srcBitDepth)
{
	if (!gIsMultiPlanar || !gSupportsDynamicChoices) {
		switch (srcBitDepth) {
		case OFX::eBitDepthUByte: {
									  Shuffler<unsigned char, DSTPIX, nComponentsDst> fred(*this);
									  setupAndProcess(fred, args);
		}
			break;
		case OFX::eBitDepthUShort: {
									   Shuffler<unsigned short, DSTPIX, nComponentsDst> fred(*this);
									   setupAndProcess(fred, args);
		}
			break;
		case OFX::eBitDepthFloat: {
									  Shuffler<float, DSTPIX, nComponentsDst> fred(*this);
									  setupAndProcess(fred, args);
		}
			break;
		default:
			OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
		}
	}
	else {
		switch (srcBitDepth) {
		case OFX::eBitDepthUByte: {
									  MultiPlaneShuffler<unsigned char, DSTPIX, nComponentsDst> fred(*this);
									  setupAndProcessMultiPlane(fred, args);
		}
			break;
		case OFX::eBitDepthUShort: {
									   MultiPlaneShuffler<unsigned short, DSTPIX, nComponentsDst> fred(*this);
									   setupAndProcessMultiPlane(fred, args);
		}
			break;
		case OFX::eBitDepthFloat: {
									  MultiPlaneShuffler<float, DSTPIX, nComponentsDst> fred(*this);
									  setupAndProcessMultiPlane(fred, args);
		}
			break;
		default:
			OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
		}

	}
}

// the internal render function
template <int nComponentsDst>
void
ShufflePlugin::renderInternal(const OFX::RenderArguments &args, OFX::BitDepthEnum srcBitDepth, OFX::BitDepthEnum dstBitDepth)
{
	switch (dstBitDepth) {
	case OFX::eBitDepthUByte:
		renderInternalForDstBitDepth<unsigned char, nComponentsDst>(args, srcBitDepth);
		break;
	case OFX::eBitDepthUShort:
		renderInternalForDstBitDepth<unsigned short, nComponentsDst>(args, srcBitDepth);
		break;
	case OFX::eBitDepthFloat:
		renderInternalForDstBitDepth<float, nComponentsDst>(args, srcBitDepth);
		break;
	default:
		OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
	}
}

// the overridden render function
void
ShufflePlugin::render(const OFX::RenderArguments &args)
{
	assert(_srcClipA && _srcClipB && _dstClip);
	if (!_srcClipA || !_srcClipB || !_dstClip) {
		OFX::throwSuiteStatusException(kOfxStatFailed);
	}
	// instantiate the render code based on the pixel depth of the dst clip
	OFX::BitDepthEnum       dstBitDepth = _dstClip->getPixelDepth();
	OFX::PixelComponentEnum dstComponents = _dstClip->getPixelComponents();
#ifdef DEBUG
	// Follow the OpênFX spec:
	// check that dstComponents is consistent with the result of getClipPreferences
	// (@see getClipPreferences).
	if (gIsMultiPlanar && gSupportsDynamicChoices) {
		std::list<std::string> outputComponents = _dstClip->getComponentsPresent();
		buildChannelMenus(outputComponents);
		std::string ofxPlane, ofxComponents;
		getPlaneNeededInOutput(outputComponents, _outputComponents, &ofxPlane, &ofxComponents);

		OFX::PixelComponentEnum pixelComps = mapStrToPixelComponentEnum(ofxComponents);
		if (pixelComps != OFX::ePixelComponentCustom) {
			assert(dstComponents == pixelComps);
		}
		else {
			int nComps = std::max((int)mapPixelComponentCustomToLayerChannels(ofxComponents).size() - 1, 0);
			switch (nComps) {
			case 1:
				pixelComps = OFX::ePixelComponentAlpha;
				break;
			case 2:
				pixelComps = OFX::ePixelComponentXY;
				break;
			case 3:
				pixelComps = OFX::ePixelComponentRGB;
				break;
			case 4:
				pixelComps = OFX::ePixelComponentRGBA;
			default:
				break;
			}
			assert(dstComponents == pixelComps);
		}
	}
	else {
		// set the components of _dstClip
		int outputComponents_i;
		_outputComponents->getValue(outputComponents_i);
		PixelComponentEnum outputComponents = gOutputComponentsMap[outputComponents_i];
		assert(dstComponents == outputComponents);
	}

	if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
		// set the bitDepth of _dstClip
		int outputBitDepth_i;
		_outputBitDepth->getValue(outputBitDepth_i);
		BitDepthEnum outputBitDepth = gOutputBitDepthMap[outputBitDepth_i];
		assert(dstBitDepth == outputBitDepth);
	}
#endif
	int dstComponentCount = _dstClip->getPixelComponentCount();
	assert(1 <= dstComponentCount && dstComponentCount <= 4);

	assert(kSupportsMultipleClipPARs || _srcClipA->getPixelAspectRatio() == _dstClip->getPixelAspectRatio());
	assert(kSupportsMultipleClipDepths || _srcClipA->getPixelDepth() == _dstClip->getPixelDepth());
	assert(kSupportsMultipleClipPARs || _srcClipB->getPixelAspectRatio() == _dstClip->getPixelAspectRatio());
	assert(kSupportsMultipleClipDepths || _srcClipB->getPixelDepth() == _dstClip->getPixelDepth());
	// get the components of _dstClip

	if (!gIsMultiPlanar) {
		int outputComponents_i;
		_outputComponents->getValue(outputComponents_i);
		PixelComponentEnum outputComponents = gOutputComponentsMap[outputComponents_i];
		if (dstComponents != outputComponents) {
			setPersistentMessage(OFX::Message::eMessageError, "", "Shuffle: OFX Host did not take into account output components");
			OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
		}
	}

	if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
		// get the bitDepth of _dstClip
		int outputBitDepth_i;
		_outputBitDepth->getValue(outputBitDepth_i);
		BitDepthEnum outputBitDepth = gOutputBitDepthMap[outputBitDepth_i];
		if (dstBitDepth != outputBitDepth) {
			setPersistentMessage(OFX::Message::eMessageError, "", "Shuffle: OFX Host did not take into account output bit depth");
			OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
		}
	}

	OFX::BitDepthEnum srcBitDepth = _srcClipA->getPixelDepth();

	if (_srcClipA->isConnected() && _srcClipB->isConnected()) {
		OFX::BitDepthEnum srcBBitDepth = _srcClipB->getPixelDepth();
		// both input must have the same bit depth
		if (srcBitDepth != srcBBitDepth) {
			setPersistentMessage(OFX::Message::eMessageError, "", "Shuffle: both inputs must have the same bit depth");
			OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
		}
	}

	switch (dstComponentCount) {
	case 4:
		renderInternal<4>(args, srcBitDepth, dstBitDepth);
		break;
	case 3:
		renderInternal<3>(args, srcBitDepth, dstBitDepth);
		break;
	case 2:
		renderInternal<2>(args, srcBitDepth, dstBitDepth);
		break;
	case 1:
		renderInternal<1>(args, srcBitDepth, dstBitDepth);
		break;
	}
}


/* Override the clip preferences */
void
ShufflePlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
	if (gIsMultiPlanar && gSupportsDynamicChoices) {
		std::list<std::string> outputComponents = _dstClip->getComponentsPresent();
		buildChannelMenus(outputComponents);
		std::string ofxPlane, ofxComponents;
		getPlaneNeededInOutput(outputComponents, _outputComponents, &ofxPlane, &ofxComponents);

		OFX::PixelComponentEnum pixelComps = mapStrToPixelComponentEnum(ofxComponents);
		if (pixelComps != OFX::ePixelComponentCustom) {
			clipPreferences.setClipComponents(*_dstClip, pixelComps);
		}
		else {
			int nComps = std::max((int)mapPixelComponentCustomToLayerChannels(ofxComponents).size() - 1, 0);
			switch (nComps) {
			case 1:
				pixelComps = OFX::ePixelComponentAlpha;
				break;
#ifdef OFX_EXTENSIONS_NATRON
			case 2:
				pixelComps = OFX::ePixelComponentXY;
				break;
#endif
			case 3:
				pixelComps = OFX::ePixelComponentRGB;
				break;
			case 4:
				pixelComps = OFX::ePixelComponentRGBA;
			default:
				break;
			}
			clipPreferences.setClipComponents(*_dstClip, pixelComps);
		}
	}
	else {
		// set the components of _dstClip
		int outputComponents_i;
		_outputComponents->getValue(outputComponents_i);
		PixelComponentEnum outputComponents = gOutputComponentsMap[outputComponents_i];
		clipPreferences.setClipComponents(*_dstClip, outputComponents);
	}

	if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
		// set the bitDepth of _dstClip
		int outputBitDepth_i;
		_outputBitDepth->getValue(outputBitDepth_i);
		BitDepthEnum outputBitDepth = gOutputBitDepthMap[outputBitDepth_i];
		clipPreferences.setClipBitDepth(*_dstClip, outputBitDepth);
	}
}

static std::string
imageFormatString(PixelComponentEnum components, BitDepthEnum bitDepth)
{
	std::string s;
	switch (components) {
	case OFX::ePixelComponentRGBA:
		s += "RGBA";
		break;
	case OFX::ePixelComponentRGB:
		s += "RGB";
		break;
	case OFX::ePixelComponentAlpha:
		s += "Alpha";
		break;
#ifdef OFX_EXTENSIONS_NUKE
	case OFX::ePixelComponentMotionVectors:
		s += "MotionVectors";
		break;
	case OFX::ePixelComponentStereoDisparity:
		s += "StereoDisparity";
		break;
#endif
#ifdef OFX_EXTENSIONS_NATRON
	case OFX::ePixelComponentXY:
		s += "XY";
		break;
#endif
	case OFX::ePixelComponentCustom:
		s += "Custom";
		break;
	case OFX::ePixelComponentNone:
		s += "None";
		break;
	default:
		s += "[unknown components]";
		break;
	}
	switch (bitDepth) {
	case OFX::eBitDepthUByte:
		s += "8u";
		break;
	case OFX::eBitDepthUShort:
		s += "16u";
		break;
	case OFX::eBitDepthFloat:
		s += "32f";
		break;
	case OFX::eBitDepthCustom:
		s += "x";
		break;
	case OFX::eBitDepthNone:
		s += "0";
		break;
#ifdef OFX_EXTENSIONS_VEGAS
	case OFX::eBitDepthUByteBGRA:
		s += "8uBGRA";
		break;
	case OFX::eBitDepthUShortBGRA:
		s += "16uBGRA";
		break;
	case OFX::eBitDepthFloatBGRA:
		s += "32fBGRA";
		break;
#endif
	default:
		s += "[unknown bit depth]";
		break;
	}
	return s;
}

static bool endsWith(const std::string &str, const std::string &suffix)
{
	return ((str.size() >= suffix.size()) &&
		(str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0));
}

void
ShufflePlugin::setChannelsFromRed()
{
	int r_i;
	_r->getValue(r_i);
	std::string rChannel;
	_r->getOption(r_i, rChannel);

	if (endsWith(rChannel, ".R") || endsWith(rChannel, ".r")) {
		std::string base = rChannel.substr(0, rChannel.size() - 2);

		bool gSet = false;
		bool bSet = false;
		bool aSet = false;

		int nOpt = _g->getNOptions();

		int indexOf0 = -1;
		int indexOf1 = -1;

		for (int i = 0; i < nOpt; ++i) {
			std::string opt;
			_r->getOption(i, opt);

			if (opt == kParamOutputOption0) {
				indexOf0 = i;
			}
			else if (opt == kParamOutputOption1) {
				indexOf1 = i;
			}
			else if (opt.substr(0, base.size()) == base) {
				std::string chan = opt.substr(base.size());
				if (chan == ".G" || chan == ".g") {
					_g->setValue(i);
					gSet = true;
				}
				else if (chan == ".B" || chan == ".b") {
					_b->setValue(i);
					bSet = true;
				}
				else if (chan == ".A" || chan == ".a") {
					_a->setValue(i);
					aSet = true;
				}
			}
			if (gSet && bSet && aSet && indexOf0 != -1 && indexOf1 != -1) {
				// we're done
				break;
			}
		}
		assert(indexOf0 != -1 && indexOf1 != -1);
		if (!gSet) {
			_g->setValue(indexOf0);
		}
		if (!bSet) {
			_b->setValue(indexOf0);
		}
		if (!aSet) {
			_a->setValue(indexOf1);
		}
	}
}

void
ShufflePlugin::changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName)
{
	if (paramName == kParamOutputComponents || paramName == kParamOutputChannels) {
		enableComponents();
	}
	else if (paramName == kParamClipInfo && args.reason == eChangeUserEdit) {
		std::string msg;
		msg += "Input A: ";
		if (!_srcClipA) {
			msg += "N/A";
		}
		else {
			msg += imageFormatString(_srcClipA->getPixelComponents(), _srcClipA->getPixelDepth());
		}
		msg += "\n";
		if (getContext() == eContextGeneral) {
			msg += "Input B: ";
			if (!_srcClipB) {
				msg += "N/A";
			}
			else {
				msg += imageFormatString(_srcClipB->getPixelComponents(), _srcClipB->getPixelDepth());
			}
			msg += "\n";
		}
		msg += "Output: ";
		if (!_dstClip) {
			msg += "N/A";
		}
		else {
			msg += imageFormatString(_dstClip->getPixelComponents(), _dstClip->getPixelDepth());
		}
		msg += "\n";
		sendMessage(OFX::Message::eMessageMessage, "", msg);
	}
#ifdef OFX_EXTENSIONS_NATRON
	else if (paramName == kParamOutputR) {
		setChannelsFromRed();
	}
#endif
}

void
ShufflePlugin::changedClip(const InstanceChangedArgs &/*args*/, const std::string &clipName)
{
	if (getContext() == eContextGeneral &&
		(clipName == kClipA || clipName == kClipB)) {
		// check that A and B are compatible if they're both connected
		if (_srcClipA && _srcClipA->isConnected() && _srcClipB && _srcClipB->isConnected()) {
			OFX::BitDepthEnum srcABitDepth = _srcClipA->getPixelDepth();
			OFX::BitDepthEnum srcBBitDepth = _srcClipB->getPixelDepth();
			// both input must have the same bit depth
			if (srcABitDepth != srcBBitDepth) {
				setPersistentMessage(OFX::Message::eMessageError, "", "Shuffle: both inputs must have the same bit depth");
				OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
			}
		}
	}
}

void
ShufflePlugin::enableComponents()
{
	if (!gIsMultiPlanar) {
		int outputComponents_i;
		_outputComponents->getValue(outputComponents_i);
		switch (gOutputComponentsMap[outputComponents_i]) {
		case ePixelComponentRGBA:
			_r->setEnabled(true);
			_g->setEnabled(true);
			_b->setEnabled(true);
			_a->setEnabled(true);
			break;
		case ePixelComponentRGB:
			_r->setEnabled(true);
			_g->setEnabled(true);
			_b->setEnabled(true);
			_a->setEnabled(false);
			break;
		case ePixelComponentAlpha:
			_r->setEnabled(false);
			_g->setEnabled(false);
			_b->setEnabled(false);
			_a->setEnabled(true);
			break;
#ifdef OFX_EXTENSIONS_NUKE
		case ePixelComponentMotionVectors:
		case ePixelComponentStereoDisparity:
			_r->setEnabled(true);
			_g->setEnabled(true);
			_b->setEnabled(false);
			_a->setEnabled(false);
			break;
#endif
#ifdef OFX_EXTENSIONS_NATRON
		case ePixelComponentXY:
			_r->setEnabled(true);
			_g->setEnabled(true);
			_b->setEnabled(false);
			_a->setEnabled(false);
			break;
#endif
		default:
			assert(0);
			break;
		}
	}
	else {
		std::list<std::string> components = _dstClip->getComponentsPresent();
		std::string ofxPlane, ofxComp;
		getPlaneNeededInOutput(components, _outputComponents, &ofxPlane, &ofxComp);
		std::vector<std::string> compNames;
		if (ofxPlane == kFnOfxImagePlaneColour || ofxPlane.empty()) {
			std::string comp = _dstClip->getPixelComponentsProperty();
			if (comp == kOfxImageComponentRGB) {
				compNames.push_back("R");
				compNames.push_back("G");
				compNames.push_back("B");
			}
			else if (comp == kOfxImageComponentRGBA) {
				compNames.push_back("R");
				compNames.push_back("G");
				compNames.push_back("B");
				compNames.push_back("A");
			}
			else if (comp == kOfxImageComponentAlpha) {
				compNames.push_back("Alpha");
			}

		}
		else if (ofxComp == kFnOfxImageComponentStereoDisparity) {
			compNames.push_back("X");
			compNames.push_back("Y");
		}
		else if (ofxComp == kFnOfxImageComponentMotionVectors) {
			compNames.push_back("U");
			compNames.push_back("V");
#ifdef OFX_EXTENSIONS_NATRON
		}
		else {
			std::vector<std::string> layerChannels = mapPixelComponentCustomToLayerChannels(ofxComp);
			if (layerChannels.size() >= 1) {
				compNames.assign(layerChannels.begin() + 1, layerChannels.end());
			}

#endif
		}
		if (compNames.size() == 1) {
			_r->setEnabled(false);
			_r->setIsSecret(true);
			_g->setEnabled(false);
			_g->setIsSecret(true);
			_b->setEnabled(false);
			_b->setIsSecret(true);
			_a->setEnabled(true);
			_a->setIsSecret(false);
			_a->setLabel(compNames[0]);
		}
		else if (compNames.size() == 2) {
			_r->setEnabled(true);
			_r->setIsSecret(false);
			_r->setLabel(compNames[0]);
			_g->setEnabled(true);
			_g->setIsSecret(false);
			_g->setLabel(compNames[1]);
			_b->setEnabled(false);
			_b->setIsSecret(true);
			_a->setEnabled(false);
			_a->setIsSecret(true);
		}
		else if (compNames.size() == 3) {
			_r->setEnabled(true);
			_r->setIsSecret(false);
			_r->setLabel(compNames[0]);
			_g->setEnabled(true);
			_g->setLabel(compNames[1]);
			_g->setIsSecret(false);
			_b->setEnabled(true);
			_b->setIsSecret(false);
			_b->setLabel(compNames[2]);
			_a->setEnabled(false);
			_a->setIsSecret(true);

		}
		else if (compNames.size() == 4) {
			_r->setEnabled(true);
			_r->setIsSecret(false);
			_r->setLabel(compNames[0]);
			_g->setEnabled(true);
			_g->setLabel(compNames[1]);
			_g->setIsSecret(false);
			_b->setEnabled(true);
			_b->setIsSecret(false);
			_b->setLabel(compNames[2]);
			_a->setEnabled(true);
			_a->setIsSecret(false);
			_a->setLabel(compNames[3]);
		}
		else {
			//Unsupported
			throwSuiteStatusException(kOfxStatFailed);
		}
	}
}


mDeclarePluginFactory(ShufflePluginFactory, {}, {});

void ShufflePluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
	// basic labels
	desc.setLabel(kPluginName);
	desc.setPluginGrouping(kPluginGrouping);
	desc.setPluginDescription(kPluginDescription);

	desc.addSupportedContext(eContextFilter);
	desc.addSupportedContext(eContextGeneral);
	desc.addSupportedBitDepth(eBitDepthUByte);
	desc.addSupportedBitDepth(eBitDepthUShort);
	desc.addSupportedBitDepth(eBitDepthFloat);

	if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
		for (ImageEffectHostDescription::PixelDepthArray::const_iterator it = getImageEffectHostDescription()->_supportedPixelDepths.begin();
			it != getImageEffectHostDescription()->_supportedPixelDepths.end();
			++it) {
			switch (*it) {
			case eBitDepthUByte:
				gSupportsBytes = true;
				break;
			case eBitDepthUShort:
				gSupportsShorts = true;
				break;
			case eBitDepthFloat:
				gSupportsFloats = true;
				break;
			default:
				// other bitdepths are not supported by this plugin
				break;
			}
		}
	}
	{
		int i = 0;
		// Note: gOutputBitDepthMap must have size # of bit depths + 1 !
		if (gSupportsFloats) {
			gOutputBitDepthMap[i] = eBitDepthFloat;
			++i;
		}
		if (gSupportsShorts) {
			gOutputBitDepthMap[i] = eBitDepthUShort;
			++i;
		}
		if (gSupportsBytes) {
			gOutputBitDepthMap[i] = eBitDepthUByte;
			++i;
		}
		assert(sizeof(gOutputBitDepthMap) >= sizeof(gOutputBitDepthMap[0])*(i + 1));
		gOutputBitDepthMap[i] = eBitDepthNone;
	}
	for (ImageEffectHostDescription::PixelComponentArray::const_iterator it = getImageEffectHostDescription()->_supportedComponents.begin();
		it != getImageEffectHostDescription()->_supportedComponents.end();
		++it) {
		switch (*it) {
		case ePixelComponentRGBA:
			gSupportsRGBA = true;
			break;
		case ePixelComponentRGB:
			gSupportsRGB = true;
			break;
		case ePixelComponentAlpha:
			gSupportsAlpha = true;
			break;
#ifdef OFX_EXTENSIONS_NATRON
		case ePixelComponentXY:
			gSupportsXY = true;
			break;
#endif
		default:
			// other components are not supported by this plugin
			break;
		}
	}
	{
		int i = 0;
		// Note: gOutputComponentsMap must have size # of component types + 1 !
		if (gSupportsRGBA) {
			gOutputComponentsMap[i] = ePixelComponentRGBA;
			++i;
		}
		if (gSupportsRGB) {
			gOutputComponentsMap[i] = ePixelComponentRGB;
			++i;
		}
		if (gSupportsAlpha) {
			gOutputComponentsMap[i] = ePixelComponentAlpha;
			++i;
		}
#ifdef OFX_EXTENSIONS_NATRON
		if (gSupportsXY) {
			gOutputComponentsMap[i] = ePixelComponentXY;
			++i;
		}
#endif
		assert(sizeof(gOutputComponentsMap) >= sizeof(gOutputComponentsMap[0])*(i + 1));
		gOutputComponentsMap[i] = ePixelComponentNone;
	}

	// set a few flags
	desc.setSingleInstance(false);
	desc.setHostFrameThreading(false);
	desc.setSupportsMultiResolution(kSupportsMultiResolution);
	desc.setSupportsTiles(kSupportsTiles);
	desc.setTemporalClipAccess(false);
	desc.setRenderTwiceAlways(false);
	desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
	// say we can support multiple pixel depths on in and out
	desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
	desc.setRenderThreadSafety(kRenderThreadSafety);

#ifdef OFX_EXTENSIONS_NATRON
	gSupportsDynamicChoices = OFX::getImageEffectHostDescription()->supportsDynamicChoices;
#else
	gSupportsDynamicChoices = false;
#endif
#ifdef OFX_EXTENSIONS_NUKE
	gIsMultiPlanar = kEnableMultiPlanar && OFX::getImageEffectHostDescription()->isMultiPlanar;
	if (gIsMultiPlanar) {
		// This enables fetching different planes from the input.
		// Generally the user will read a multi-layered EXR file in the Reader node and then use the shuffle
		// to redirect the plane's channels into RGBA color plane.

		desc.setIsMultiPlanar(true);

		// We are pass-through in output, meaning another shuffle below could very well
		// access all planes again. Note that for multi-planar effects this is mandatory to be called
		// since default is false.
		desc.setPassThroughForNotProcessedPlanes(ePassThroughLevelPassThroughNonRenderedPlanes);
	}
#else 
	gIsMultiPlanar = false;
#endif
}

static void
addInputChannelOptionsRGBA(ChoiceParamDescriptor* outputR, InputChannelEnum def, OFX::ContextEnum context)
{
	addInputChannelOptionsRGBA(outputR, context);
	outputR->setDefault(def);
}

void ShufflePluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
{

#ifdef OFX_EXTENSIONS_NUKE
	if (gIsMultiPlanar && !OFX::fetchSuite(kFnOfxImageEffectPlaneSuite, 2)) {
		OFX::throwHostMissingSuiteException(kFnOfxImageEffectPlaneSuite);
	}
#endif

	if (context == eContextGeneral) {
		ClipDescriptor* srcClipB = desc.defineClip(kClipB);
		srcClipB->addSupportedComponent(ePixelComponentRGBA);
		srcClipB->addSupportedComponent(ePixelComponentRGB);
		srcClipB->addSupportedComponent(ePixelComponentAlpha);
#ifdef OFX_EXTENSIONS_NATRON
		srcClipB->addSupportedComponent(ePixelComponentXY);
#endif
		srcClipB->setTemporalClipAccess(false);
		srcClipB->setSupportsTiles(kSupportsTiles);
		srcClipB->setOptional(true);

		ClipDescriptor* srcClipA = desc.defineClip(kClipA);
		srcClipA->addSupportedComponent(ePixelComponentRGBA);
		srcClipA->addSupportedComponent(ePixelComponentRGB);
		srcClipA->addSupportedComponent(ePixelComponentAlpha);
#ifdef OFX_EXTENSIONS_NATRON
		srcClipA->addSupportedComponent(ePixelComponentXY);
#endif
		srcClipA->setTemporalClipAccess(false);
		srcClipA->setSupportsTiles(kSupportsTiles);
		srcClipA->setOptional(false);
	}
	else {
		ClipDescriptor* srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
		srcClip->addSupportedComponent(ePixelComponentRGBA);
		srcClip->addSupportedComponent(ePixelComponentRGB);
		srcClip->addSupportedComponent(ePixelComponentAlpha);
#ifdef OFX_EXTENSIONS_NATRON
		srcClip->addSupportedComponent(ePixelComponentXY);
#endif
		srcClip->setTemporalClipAccess(false);
		srcClip->setSupportsTiles(kSupportsTiles);
	}
	{
		// create the mandated output clip
		ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
		dstClip->addSupportedComponent(ePixelComponentRGBA);
		dstClip->addSupportedComponent(ePixelComponentRGB);
		dstClip->addSupportedComponent(ePixelComponentAlpha);
#ifdef OFX_EXTENSIONS_NATRON
		dstClip->addSupportedComponent(ePixelComponentXY);
#endif
		dstClip->setSupportsTiles(kSupportsTiles);
	}

	// make some pages and to things in
	PageParamDescriptor *page = desc.definePageParam("Controls");

	// outputComponents
	if (!gIsMultiPlanar) {
		ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputComponents);
		param->setLabel(kParamOutputComponentsLabel);
		param->setHint(kParamOutputComponentsHint);
		// the following must be in the same order as in describe(), so that the map works
		if (gSupportsRGBA) {
			assert(gOutputComponentsMap[param->getNOptions()] == ePixelComponentRGBA);
			param->appendOption(kParamOutputComponentsOptionRGBA);
		}
		if (gSupportsRGB) {
			assert(gOutputComponentsMap[param->getNOptions()] == ePixelComponentRGB);
			param->appendOption(kParamOutputComponentsOptionRGB);
		}
		if (gSupportsAlpha) {
			assert(gOutputComponentsMap[param->getNOptions()] == ePixelComponentAlpha);
			param->appendOption(kParamOutputComponentsOptionAlpha);
		}
#ifdef OFX_EXTENSIONS_NATRON
		if (gSupportsXY) {
			assert(gOutputComponentsMap[param->getNOptions()] == ePixelComponentXY);
			param->appendOption(kParamOutputComponentsOptionXY);
		}
#endif
		param->setDefault(0);
		param->setAnimates(false);
		desc.addClipPreferencesSlaveParam(*param);
		if (page) {
			page->addChild(*param);
		}
	}
	else {
		ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputChannels);
		param->setLabel(kParamOutputChannelsLabel);
		param->setHint(kParamOutputChannelsHint);
#ifdef OFX_EXTENSIONS_NATRON
		param->setUserCanAddNewChoice(true);
#endif
		param->appendOption(kShuffleColorPlaneName);
		param->appendOption(kShuffleMotionForwardPlaneName);
		param->appendOption(kShuffleMotionBackwardPlaneName);
		param->appendOption(kShuffleDisparityLeftPlaneName);
		param->appendOption(kShuffleDisparityRightPlaneName);
		desc.addClipPreferencesSlaveParam(*param);
		if (page) {
			page->addChild(*param);
		}
	}

	// ouputBitDepth
	if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
		ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputBitDepth);
		param->setLabel(kParamOutputBitDepthLabel);
		param->setHint(kParamOutputBitDepthHint);
		// the following must be in the same order as in describe(), so that the map works
		if (gSupportsFloats) {
			// coverity[check_return]
			assert(0 <= param->getNOptions() && param->getNOptions() < 4 && gOutputBitDepthMap[param->getNOptions()] == eBitDepthFloat);
			param->appendOption(kParamOutputBitDepthOptionFloat);
		}
		if (gSupportsShorts) {
			// coverity[check_return]
			assert(0 <= param->getNOptions() && param->getNOptions() < 4 && gOutputBitDepthMap[param->getNOptions()] == eBitDepthUShort);
			param->appendOption(kParamOutputBitDepthOptionShort);
		}
		if (gSupportsBytes) {
			// coverity[check_return]
			assert(0 <= param->getNOptions() && param->getNOptions() < 4 && gOutputBitDepthMap[param->getNOptions()] == eBitDepthUByte);
			param->appendOption(kParamOutputBitDepthOptionByte);
		}
		param->setDefault(0);
		param->setAnimates(false);
#ifndef DEBUG
		// Shuffle only does linear conversion, which is useless for 8-bits and 16-bits formats.
		// Disable it for now (in the future, there may be colorspace conversion options)
		param->setIsSecret(true); // always secret
#endif
		desc.addClipPreferencesSlaveParam(*param);
		if (page) {
			page->addChild(*param);
		}
	}

	if (gSupportsRGB || gSupportsRGBA) {
		// outputR
		{
			ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputR);
			param->setLabel(kParamOutputRLabel);
			param->setHint(kParamOutputRHint);
			addInputChannelOptionsRGBA(param, eInputChannelAR, context);
			if (page) {
				page->addChild(*param);
			}
		}

		// outputG
		{
		ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputG);
		param->setLabel(kParamOutputGLabel);
		param->setHint(kParamOutputGHint);
		addInputChannelOptionsRGBA(param, eInputChannelAG, context);
		if (page) {
			page->addChild(*param);
		}
	}

		// outputB
		{
			ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputB);
			param->setLabel(kParamOutputBLabel);
			param->setHint(kParamOutputBHint);
			addInputChannelOptionsRGBA(param, eInputChannelAB, context);
			if (page) {
				page->addChild(*param);
			}
		}
	}
	// ouputA
	if (gSupportsRGBA || gSupportsAlpha) {
		ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputA);
		param->setLabel(kParamOutputALabel);
		param->setHint(kParamOutputAHint);
		addInputChannelOptionsRGBA(param, eInputChannelAA, context);
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
}

OFX::ImageEffect* ShufflePluginFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context)
{
	return new ShufflePlugin(handle, context);
}

void getShufflePluginID(OFX::PluginFactoryArray &ids)
{
	static ShufflePluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
	ids.push_back(&p);
}
