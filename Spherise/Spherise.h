/*
AX Spherise plugin.

Copyright (C) 2015 AnDyX

*/

#ifndef Misc_Spherise_h
#define Misc_Spherise_h

#include "ofxsImageEffect.h"
// unpPix is normalized between [0,1]
template <class PIX, int nComponents, int maxValue, bool masked>
void
ofxsJustPremult2(const float unpPix[4], PIX *dstPix) //!< destination pixel
{
	ofxsPremultMaskMixPix<PIX, nComponents, maxValue, false>(unpPix, false, 0, 0, 0, dstPix, false, 0, 1.0f, false, dstPix);
}

// unpPix is normalized between [0,1]
template <class PIX, int nComponents, int maxValue, bool masked>
void
ofxsJustPremult(const float unpPix[4], PIX *dstPix) //!< destination pixel
{
	float tmpPix[nComponents];

	//ofxsPremult<PIX, nComponents, maxValue>(unpPix, tmpPix, false , 0);
	for (int c = 0; c < nComponents; ++c) {
		tmpPix[c] = unpPix[c] * maxValue;
	}
	
	// no mask, no mix
	for (int c = 0; c < nComponents; ++c) {
		dstPix[c] = PIX(ofxsClampIfInt<maxValue>(tmpPix[c], 0, maxValue));
	}
}


void getSpherisePluginID(OFX::PluginFactoryArray &ids);

#endif // Misc_Spherise_h
