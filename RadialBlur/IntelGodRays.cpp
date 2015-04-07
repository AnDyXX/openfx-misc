/*
AX Radial blur plugin.

Copyright (C) 2015 AnDyX


*/

#include "IntelGodRays.h"

#include <cmath>
#include <cstring>
#ifdef _WINDOWS
#include <windows.h>
#endif

#include "ofxsProcessing.H"
#include "ofxsMaskMix.h"
#include "ofxsMacros.h"

#define kPluginName "IntelGodRaysAX"
#define kPluginGrouping "Blur"
#define kPluginDescription "IntelGodRays."
#define kPluginIdentifier "org.andyx.IntelGodRaysPlugin"
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

#define kParamBlend "BlendIm"
#define kParamBlendLabel "Blend"

#define kParamDecay "Decay"
#define kParamDecayLabel "Decay"

#define kParamWeight "Weight"
#define kParamWeightLabel "Weight"

#define kParamExposure "Exposure"
#define kParamExposureLabel "Exposure"

#define kParamThreshold "Threshold"
#define kParamThresholdLabel "Threshold"

#define kParamMergeOver "MergeOver"
#define kParamMergeOverLabel "Merge Over"

using namespace OFX;

//base class of RadialBlurProcessor plugin
class IntelGodRaysProcessorBase : public OFX::ImageProcessor
{
protected:
	const OFX::Image       *_srcImg;        /**< @brief image to process into */
	OfxPointI lightPosition;
	double blend;
	double decay;
	double weight;
	double exposure;
	double threshold;
	bool mergeOver;
public:
	/** @brief ctor */
	IntelGodRaysProcessorBase(OFX::ImageEffect &effect)
		: OFX::ImageProcessor(effect)
		, _srcImg(0)
	{
		}

	void setSrcImg(const OFX::Image *v) { _srcImg = v; }

	void setLightPosition(const OfxPointI _lightPosition){
		lightPosition = _lightPosition;
	}

	void setValues(double _blend, double _decay, double _weight, double _exposure, double _threshold, bool _mergeOver){
		blend = _blend;
		decay = _decay;
		weight = _weight;
		exposure = _exposure;
		threshold = _threshold;
		mergeOver = _mergeOver;
	}
};

// RadialBlurProcessor plugin processor
template <class PIX, int nComponents, int maxValue>
class IntelGodRaysProcessor : public IntelGodRaysProcessorBase
{
public:
	IntelGodRaysProcessor(OFX::ImageEffect &instance)
		: IntelGodRaysProcessorBase(instance)
	{
	}

	virtual void multiThreadProcessImages(OfxRectI window){
		//nothing to do here - we mutliThreadFunction
	}


	//main function that split calculations
	void multiThreadFunction(unsigned int threadId, unsigned int nThreads) OVERRIDE FINAL
	{
		//if (threadId > 0)
		//{
		//	return;
		//}

		//we competely ignore window size - we always need to render all 
		int h = _srcImg->getBounds().y2 - _srcImg->getBounds().y1;
		int w = _srcImg->getBounds().x2 - _srcImg->getBounds().x1;

		int raysCount = (h + w) * 2;

		OfxPointI imgSize;
		imgSize.x = w;
		imgSize.y = h;

		if ((std::abs(lightPosition.x) > 1000000) || (std::abs(lightPosition.y) > 1000000)){
			return;
		}

		for (int ray = 0; ray < raysCount; ray++){
			if ((ray % nThreads) == threadId){
				EvaluateRay(ray, imgSize, blend, lightPosition, decay, weight, exposure, threshold, mergeOver ? 1 : 0);
			}
		}
	}

	inline float convert_float(int in){
		return float(in);
	}

	inline void multVector(float* vector, float value){
		vector[0] *= value;
		vector[1] *= value;
		vector[2] *= value;
		vector[3] *= value;
	}



	inline void sumVector(float* vector, float * val){
		vector[0] += val[0];
		vector[1] += val[1];
		vector[2] += val[2];
		vector[3] += val[3];
	}

	inline void sumVector(float* vectorOut, float* vectorIn, float * val){
		vectorOut[0] = vectorIn[0] + val[0];
		vectorOut[1] = vectorIn[1] + val[1];
		vectorOut[2] = vectorIn[2] + val[2];
		vectorOut[3] = vectorIn[3] + val[3];
	}

	inline void copyVector(float* vector, float * val){
		vector[0] = val[0];
		vector[1] = val[1];
		vector[2] = val[2];
		vector[3] = val[3];
	}

	inline void multVector(float* vectorOut, float* vectorIn, float value){
		vectorOut[0] = vectorIn[0] * value;
		vectorOut[1] = vectorIn[1] * value;
		vectorOut[2] = vectorIn[2] * value;
		vectorOut[3] = vectorIn[3] * value;
	}

	inline void FuReadImagef(int x, int y, float * values){
		PIX * srcPix = (PIX *)(_srcImg ? _srcImg->getPixelAddress(x, y) : 0);
		//normalise values to 0-1
		ofxsUnPremult<PIX, nComponents, maxValue>(srcPix, values, true, 0);
	}

	inline void FuWriteImagef(int x, int y, float * value){
		PIX *srcPix = (PIX *)(_srcImg ? _srcImg->getPixelAddress(x, y) : 0);
		PIX *dstPix = (PIX *)_dstImg->getPixelAddress(x, y);

		ofxsPremultMaskMixPix<PIX, nComponents, maxValue, false>(value, false, 0, x, y, srcPix, false, 0, 1.0f, false, dstPix);
	}

	inline void mix(float* vectorOut, float* vectorIn1, float* vectorIn2, float inBlend){
		vectorOut[0] = inBlend * vectorIn2[0] + (1.f - inBlend) * vectorIn1[0];
		vectorOut[1] = inBlend * vectorIn2[1] + (1.f - inBlend) * vectorIn1[1];
		vectorOut[2] = inBlend * vectorIn2[2] + (1.f - inBlend) * vectorIn1[2];
		vectorOut[3] = inBlend * vectorIn2[3] + (1.f - inBlend) * vectorIn1[3];
	}

	// Copyright (c) 2009-2011 Intel Corporation
	// All rights reserved.
	//
	// WARRANTY DISCLAIMER
	//
	// THESE MATERIALS ARE PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR ITS
	// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
	// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
	// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY OR TORT (INCLUDING
	// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THESE
	// MATERIALS, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
	//
	// Intel Corporation is the author of the Materials, and requests that all
	// problem reports or change requests be submitted to it directly

	void EvaluateRay(int in_RayNum, OfxPointI imgSize, double blend,
		OfxPointI lightPosition, double Decay, double Weight, double InvExposure, double fThreshold, int iMergeOver)
	{
		int LightScreenPosX = lightPosition.x;
		int LightScreenPosY = lightPosition.y;

		//------------------------------------------------------------
		//-----------God Rays OpenCL HDR Post Processing here---------
		//------------------------------------------------------------
		//in_pData params
		double _InvExposure = InvExposure;

		// Width of the image
		int x_last = imgSize.x - 1;
		// Height of the image
		int y_last = imgSize.y - 1;
		// X coordinate of the God Rays light source
		int x = LightScreenPosX;
		// Y coordinate of the God Rays light source
		int y = LightScreenPosY;
		// X coordinate of the God Rays light source
		int x_s = LightScreenPosX;
		// Y coordinate of the God Rays light source
		int y_s = LightScreenPosY;

		/* ***********************************************
		Calculate the destination point of the rays
		and check to see if the rays intersect the image
		*********************************************** */

		// Initialize the destinations to zero representing the 
		// lower-left corner of the image, the (0,0) coordinate
		int x_dst = 0;
		int y_dst = 0;
		int x_dst_s = 0;
		int y_dst_s = 0;

		// Use the ray number as a destination pixel of the bottom border and check the range. 
		int Ray = in_RayNum;
		if (Ray < x_last)
		{
			// In this case, check to see if the ray is located outside of the image or
			// the ray from the left bottom corner pixel (also directed from the left). 
			if (y < 0 &&
				(x < 0 || Ray != 0))
			{
				// These rays do not paint any pixels, so you have no work to do
				// and you can exit the function.
				return;
			}
			// If you still have work to do, set up the destination points for the original
			// and shadow rays as a ray number. 
			x_dst = Ray;
			y_dst = 0;
			x_dst_s = Ray - 1;
			y_dst_s = 0;

			// If the original ray is a corner ray, fix the shadow ray
			// destination point, because it places at another border. 
			if (Ray == 0)
			{
				x_dst_s = 0;
				y_dst_s = 1;
			}
		}

		// If the ray number is greater than the bottom border range, check other borders. 
		else
		{
			// Fix the ray number by the bottom border length to exclude those
			// pixels from computation.
			Ray -= x_last;

			// Set the ray number as a destination pixel of the right border and check the range.
			if (Ray < y_last)
			{
				// To see what you need to do, check if the ray is located out from
				// the image and the ray from the right bottom corner pixel is also
				// directed from the right.
				if (x > x_last &&
					(y < 0 || Ray != 0))
				{
					return;
				}

				// Set up destination points for original and shadow rays as the
				// ray number if there is some work to do.
				x_dst = x_last;
				y_dst = Ray;
				x_dst_s = x_last;
				y_dst_s = Ray - 1;

				// Fix the shadow ray destination point, in case the original ray is a corner ray.
				if (Ray == 0)
				{
					x_dst_s = x_last - 1;
					y_dst_s = 0;
				}
			}

			// Repeat the steps for the top border if the ray number doesn't
			// correspond to the right border.
			else
			{
				Ray -= y_last;
				if (Ray < x_last)
				{
					if (y > y_last &&
						(x > x_last || Ray != 0))
					{
						return;
					}
					x_dst = x_last - Ray;
					y_dst = y_last;
					x_dst_s = x_last - Ray + 1;
					y_dst_s = y_last;
					if (Ray == 0)
					{
						x_dst_s = x_last;
						y_dst_s = y_last - 1;
					}
				}

				// Check left borders if the ray number doesn't correspond to the top one.
				else
				{
					Ray -= x_last;
					if (Ray < y_last)
					{
						if (x < 0 &&
							(y > y_last || Ray != 0))
						{
							return;
						}
						x_dst = 0;
						y_dst = y_last - Ray;
						x_dst_s = 0;
						y_dst_s = y_last - Ray + 1;
						if (Ray == 0)
						{
							x_dst_s = 1;
							y_dst_s = y_last;
						}
					}
					else
					{
						// If the ray number doesn't correspond to any border, you have no
						// more work to do, so exit from the function.
						return;
					}
				}
			}
		}
		// Calculate start and destination points sizes for each dimension
		// and select directions as the steps size. 
		int dx = x_dst - x;
		int dy = y_dst - y;
		int dx_s = x_dst_s - x;
		int dy_s = y_dst_s - y;
		int xstep = dx > 0 ? 1 : 0;
		int ystep = dy > 0 ? 1 : 0;
		int xstep_s = dx_s > 0 ? 1 : 0;
		int ystep_s = dy_s > 0 ? 1 : 0;
		if (dx < 0)
		{
			dx = -dx;
			xstep = -1;
		}
		if (dy < 0)
		{
			dy = -dy;
			ystep = -1;
		}
		if (dx_s < 0)
		{
			dx_s = -dx_s;
			xstep_s = -1;
		}
		if (dy_s < 0)
		{
			dy_s = -dy_s;
			ystep_s = -1;
		}
		// Calculate Bresenham parameters for the original ray and correct the
		// Decay value by the step length.
		double FixedDecay = 0.0f;
		int di = 0;
		int di_s = 0;
		int steps = 0;


		// Select the biggest dimension of the original ray.

		// If dx is greater than or equal to dy, use dx as the steps count.
		if (dx >= dy)
		{
			// Fix Decay by the step length computed as a ray length divided by steps count.
			FixedDecay = exp(Decay * sqrt(convert_float(dx) *
				convert_float(dx) + convert_float(dy) * convert_float(dy)) / convert_float(dx));

			// Set up the Bresenham delta error.
			di = 2 * dy - dx;

			// Crop the ray by the image borders to reduce the steps cont.

			// If the horizontal ray (dx >= dy) crosses over a left or right
			// border, you should crop it by the step count. In other words, you
			// know the first step in image borders and need to find the
			// corresponding y coordinate and the delta error value.
			if (x<0 || x>x_last)
			{
				// Select the count of steps that need to be omitted.
				int dx_crop = x < 0 ? -x : x - x_last;

				// Fix the delta error value by the omitted steps on X dimension.
				int new_di = di + (dx_crop - 1) * 2 * dy;

				// Calculate the appropriate steps count for Y dimension.
				int dy_crop = new_di / (2 * dx);

				// Check the final delta error value.
				new_di -= dy_crop * 2 * dx;

				// If a new delta error value is greater than 0, move by the Y
				// dimension when you cross the image range.
				if (new_di > 0)
				{
					// In that case, increase the steps count on Y dimension by 1.
					dy_crop++;
				}

				// Recalculate the current position and the delta error value.
				x += xstep*dx_crop;
				y += ystep*dy_crop;
				di += dx_crop * 2 * dy - dy_crop * 2 * dx;
			}

			// If the horizontal ray (dx >= dy) crosses either the bottom or top
			// border, you should crop it by the y coordinate. You know the y coordinate
			// and need to find the first corresponding step in the image borders and
			// the delta error value. */
			if (y<0 || y>y_last)
			{
				// Select the count of steps that you should omit on the Y coordinate.
				int dy_crop = y < 0 ? -y : y - y_last;

				// Fix the delta error value by omitting steps on the Y dimension.
				int new_di = di - (dy_crop - 1) * 2 * dx;

				// Calculate the appropriate steps count for the X dimension.
				int dx_crop = 1 - new_di / (2 * dy);

				// If a new delta error value is less than 0 and this value can
				// be divided by 2dy without a remainder, move by the Y dimension
				// when you cross the image range.
				if (new_di % (2 * dy) != 0 && new_di < 0)
				{
					// Increase steps count on the X dimension by 1.
					dx_crop++;
				}

				// Since you know the right count of omitted steps in all
				// dimensions, you can recalculate the current position and the
				// delta error value.
				x += xstep*dx_crop;
				y += ystep*dy_crop;
				di += dx_crop * 2 * dy - dy_crop * 2 * dx;
			}

			// At the end of cropping the horizontal ray, calculate the correct steps count.
			steps = abs(x_dst - x);
		}

		// If dy is greater than dx, use dy as the count of steps and perform
		// exactly the same steps but with different coordinates.
		else
		{

			// Fix Decay by the step length, which is computed as the ray length divided by the steps count.
			FixedDecay = exp(Decay * sqrt(convert_float(dx) *
				convert_float(dx) + convert_float(dy) * convert_float(dy)) / convert_float(dy));

			// Set up the Bresenham delta error.
			di = 2 * dx - dy;

			// Crop the ray by the image borders to reduce the steps cont.

			// If the vertical ray (dy > dx) crosses either a bottom or top
			// border, crop it by the step count.
			if (y<0 || y>y_last)
			{
				// Select the count of steps that should be omitted.
				int dy_crop = y < 0 ? -y : y - y_last;

				// Use the omitted steps to fix the delta error value on the y dimension.
				int new_di = di + (dy_crop - 1) * 2 * dx;

				// Calculate the appropriate steps count for the X dimension.
				int dx_crop = new_di / (2 * dy);

				// Check the final delta error value.
				new_di -= dx_crop * 2 * dy;

				// If a new delta error value is greater than zero,
				if (new_di > 0)
				{
					// increase the steps count on the X dimension by 1.
					dx_crop++;
				}

				// Recalculate the current position and the delta error value.
				x += xstep*dx_crop;
				y += ystep*dy_crop;
				di += dy_crop * 2 * dx - dx_crop * 2 * dy;
			}

			// If the vertical ray (dy > dx) crosses either a left or right
			// border, crop it on the X coordinate.
			if (x<0 || x>x_last)
			{
				// Select the count of steps that should be omitted on the X  coordinate.
				int dx_crop = x < 0 ? -x : x - x_last;

				// Use omitted steps to fix the delta error value on the X dimension.
				int new_di = di - (dx_crop - 1) * 2 * dy;

				// Calculate the appropriate steps count for the Y dimension.
				int dy_crop = 1 - new_di / (2 * dx);

				// If a new delta error value is less than 0 and this value can
				// be divided by 2dy without a remainder, then ...
				if (new_di % (2 * dx) != 0 && new_di < 0)
				{
					// increase the steps count on X dimension by 1.
					dy_crop++;
				}

				// Recalculate the current position and the delta error value.
				x += xstep*dx_crop;
				y += ystep*dy_crop;
				di += dy_crop * 2 * dx - dx_crop * 2 * dy;
			}

			// After cropping the vertical ray, calculate the correct steps count.
			steps = abs(y_dst - y);
		}

		// Omit some steps either at the beginning or the end of the shadow ray to
		// make the steps by pixels correspond to the original ray.
		int steps_begin = 0;
		int steps_lsat = 0;

		// Crop the shadow ray by exactly the same computation as the original one.
		if (dx_s >= dy_s)
		{
			di_s = 2 * dy_s - dx_s;
			if (x_s<0 || x_s>x_last)
			{
				int dx_crop_s = x_s<0 ? -x_s : x_s - x_last;
				int new_di_s = di_s + (dx_crop_s - 1) * 2 * dy_s;
				int dy_crop_s = new_di_s / (2 * dx_s);
				new_di_s -= dy_crop_s * 2 * dx_s;
				if (new_di_s>0)
				{
					dy_crop_s++;
				}
				x_s += xstep_s*dx_crop_s;
				y_s += ystep_s*dy_crop_s;
				di_s += dx_crop_s * 2 * dy_s - dy_crop_s * 2 * dx_s;
			}
			if (y_s<0 || y_s>y_last)
			{
				int dy_crop_s = y_s < 0 ? -y_s : y_s - y_last;
				int new_di_s = di_s - (dy_crop_s - 1) * 2 * dx_s;
				int dx_crop_s = 1 - new_di_s / (2 * dy_s);
				if (new_di_s % (2 * dy_s) != 0 && new_di_s < 0)
				{
					dx_crop_s++;
				}
				x_s += xstep_s*dx_crop_s;
				y_s += ystep_s*dy_crop_s;
				di_s += dx_crop_s * 2 * dy_s - dy_crop_s * 2 * dx_s;
			}

			// For the horizontal shadow ray, calculate the omitted steps as the
			// difference between the current position, (it can be different
			// after cropping), and the original position.
			steps_begin = xstep_s * (x - x_s);

			// If steps at the shadow ray should be omitted (the count of
			// omitted steps is greater than 0), omit them.
			if (steps_begin > 0)
			{
				// Calculate a new delta error value and move the start X coordinate 
				// to the same position that the original ray has.
				di_s += 2 * steps_begin * dy_s;
				x_s = x;

				// To move along the Y axes, do the following steps:
				if (di_s > 2 * dy_s)
				{
					// Correct the delta error value and ...
					di_s -= 2 * dx_s;

					// Move the current position by the Y dimension.
					y_s += ystep_s;

					// Move along the Y axes no more than one step. The distance
					// between rays must be no greater than 1; otherwise, it is
					// impossible to define the rays.
				}
			}

			// Define the count of steps that should be omitted at the end of
			// the shadow ray as the count of steps that should be compared,
			// (the count of shadow ray steps without steps omitted at the
			// beginning).
			steps_lsat = abs(x_dst_s - x_s) - steps_begin;
		}
		else // Vertical shadow rays
		{
			di_s = 2 * dx_s - dy_s;
			if (y_s<0 || y_s>y_last)
			{
				int dy_crop_s = y_s<0 ? -y_s : y_s - y_last;
				int new_di_s = di_s + (dy_crop_s - 1) * 2 * dx_s;
				int dx_crop_s = new_di_s / (2 * dy_s);
				new_di_s -= dx_crop_s * 2 * dy_s;
				if (new_di_s>0)
				{
					dx_crop_s++;
				}
				x_s += xstep_s*dx_crop_s;
				y_s += ystep_s*dy_crop_s;
				di_s += dy_crop_s * 2 * dx_s - dx_crop_s * 2 * dy_s;
			}
			if (x_s<0 || x_s>x_last)
			{
				int dx_crop_s = x_s < 0 ? -x_s : x_s - x_last;
				int new_di_s = di_s - (dx_crop_s - 1) * 2 * dy_s;
				int dy_crop_s = 1 - new_di_s / (2 * dx_s);
				if (new_di_s % (2 * dx_s) != 0 && new_di_s < 0)
				{
					dy_crop_s++;
				}
				x_s += xstep_s*dx_crop_s;
				y_s += ystep_s*dy_crop_s;
				di_s += dy_crop_s * 2 * dx_s - dx_crop_s * 2 * dy_s;
			}

			// For the vertical shadow ray, calculate the omitted steps by the
			// same computations but with different dimensions:

			// Setup the omitted steps at the beginning as the difference
			// between the current position and the original position.
			steps_begin = ystep_s * (y - y_s);

			// Omit the first steps at the shadow ray if required by checking
			// to see if the variable steps_begin is greater than zero.
			if (steps_begin > 0)
			{
				// Calculate a new delta error value
				di_s += 2 * steps_begin * dx_s;

				// Move the start X coordinate,
				y_s = y;
				if (di_s > 2 * dx_s)
				{
					// correct the delta error value, and move the current
					// position by the Y dimension.
					di_s -= 2 * dy_s;
					x_s += ystep_s;
				}
			}

			// Fix the count of omitted steps at the end.
			steps_lsat = abs(y_dst_s - y_s) - steps_begin;
		}

		// Load method parameters.

		double Decay_128 = FixedDecay;
		FixedDecay = 1.0f - FixedDecay;
		double nDecay_128 = FixedDecay;
		double NExposure_128 = _InvExposure;
		float summ_128[] = { 0.f, 0.f, 0.f, 0.f };

		//Load the first pixel of the ray.  

		float sample_128[] = { 0.f, 0.f, 0.f, 0.f };
		FuReadImagef(x, y, sample_128);

		// Apply the exposure to it to scale color values to appropriate ranges.
		//sample_128 = sample_128 * NExposure_128;
		multVector(sample_128, NExposure_128);


		// Check to see if this pixel can be used in a sum.
		if (sample_128[0] > fThreshold || sample_128[1] > fThreshold || sample_128[2] > fThreshold)
		{
			// Update the sum if required.
			//summ_128 = sample_128 * nDecay_128;
			multVector(summ_128, sample_128, nDecay_128);
		}

		// Check to see if the result can be written to the output pixel (the pixel is not
		// shaded by the shadow ray and it is not the first ray that should
		// paint the sun position).
		if (x != x_s || y != y_s || (x_dst == 0 && y_dst == 0 && x == LightScreenPosX && y == LightScreenPosY))
		{
			// Add the current sum value corrected by the Weight parameter to
			// the output buffer if possible.
			//float4 answer_128 = summ_128 * Weight;
			float answer_128[] = { 0.f, 0.f, 0.f, 0.f };
			multVector(answer_128, summ_128, Weight);

			//answer_128 = answer_128 + sample_128;
			sumVector(answer_128, sample_128);

			//Writing to output pixel
			FuWriteImagef(x, y, answer_128);
		}

		// In the main loop, go along the original ray.
		for (int i = 0; i < steps; i++)
		{
			// Make steps in the Bresenham loop for the original ray.
			if (dx >= dy)
			{
				x += xstep;
				if (di >= 0)
				{
					y += ystep;
					di -= 2 * dx;
				}
				di += 2 * dy;
			}
			else
			{
				y += ystep;
				if (di >= 0)
				{
					x += xstep;
					di -= 2 * dy;
				}
				di += 2 * dx;
			}

			// Make steps for the shadow rays if if they should not be omitted.
			if (steps_begin >= 0)
			{
				if (dx_s >= dy_s)
				{
					x_s += xstep_s;
					if (di_s >= 0)
					{
						y_s += ystep_s;
						di_s -= 2 * dx_s;
					}
					di_s += 2 * dy_s;
				}
				else
				{
					y_s += ystep_s;
					if (di_s >= 0)
					{
						x_s += xstep_s;
						di_s -= 2 * dy_s;
					}
					di_s += 2 * dx_s;
				}
			}
			else
			{
				steps_begin++;
			}
			// For each step, load the next pixel of the ray.
			//float* source_128 = FuReadImagef(x, y);
			float source_128[] = { 0.f, 0.f, 0.f, 0.f };
			FuReadImagef(x, y, source_128);

			//sample_128 = source_128;
			copyVector(sample_128, source_128);

			// Apply the exposure to it to scale color values to appropriate ranges.
			//summ_128 = summ_128 * Decay_128;
			multVector(summ_128, Decay_128);

			// Update the sum
			//sample_128 = sample_128 * NExposure_128;
			multVector(sample_128, NExposure_128);

			// check if this pixel can be used in a sum and
			if (sample_128[0] > fThreshold || sample_128[1] > fThreshold || sample_128[2] > fThreshold)
			{
				//sample_128 = sample_128*nDecay_128;
				multVector(sample_128, Decay_128);

				//summ_128 = summ_128 + sample_128;
				sumVector(summ_128, sample_128);
			}


			// Check if it is possible to write the result to the output pixel.
			if (x != x_s || y != y_s || i >= steps_lsat)
			{
				// Update the output buffer by the current sum value
				// corrected by the Weight parameter.	
				//float4 answer_128 = summ_128 * Weight;
				float answer_128[] = { 0.f, 0.f, 0.f, 0.f };
				multVector(answer_128, summ_128, Weight);

				// Hide Source
				if (iMergeOver)
				{
					float result_128[] = { 0.f, 0.f, 0.f, 0.f };
					sumVector(result_128, source_128, answer_128);
					mix(answer_128, source_128, result_128, blend);
				}
				else
				{
					float result_128[] = { 0.f, 0.f, 0.f, 0.f };
					mix(answer_128, result_128, answer_128, blend);
				}

				FuWriteImagef(x, y, answer_128);
			}
		}
	};
};

//plugin itself
////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class IntelGodRaysPlugin : public OFX::ImageEffect
{
private:
	OFX::BooleanParam * _CenterUsePx;
	OFX::Double2DParam * _centerPoint;
	OFX::DoubleParam  *_blend;
	OFX::DoubleParam  *_decay;
	OFX::DoubleParam  *_weight;
	OFX::DoubleParam  *_exposure;
	OFX::DoubleParam  *_threshold;
	OFX::BooleanParam * _mergeOver;
public:
	/** @brief ctor */
	IntelGodRaysPlugin(OfxImageEffectHandle handle) : ImageEffect(handle)
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
		_blend = fetchDoubleParam(kParamBlend);
		_decay = fetchDoubleParam(kParamDecay);
		_weight = fetchDoubleParam(kParamWeight);
		_exposure = fetchDoubleParam(kParamExposure);
		_threshold = fetchDoubleParam(kParamThreshold);
		_mergeOver = fetchBooleanParam(kParamMergeOver);
	}
private:
	/* Override the render */
	virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

	virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime) OVERRIDE FINAL{
		return false;
	};

	virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

	/* set up and run a processor */
	void setupAndProcess(IntelGodRaysProcessorBase & processor, const OFX::RenderArguments &args);

private:
	// do not need to delete these, the ImageEffect is managing them for us
	OFX::Clip *_dstClip;
	OFX::Clip *_srcClip;
};


void
IntelGodRaysPlugin::setupAndProcess(IntelGodRaysProcessorBase & processor, const OFX::RenderArguments &args){
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

	if (dst->getRenderScale().x != args.renderScale.x ||
		dst->getRenderScale().y != args.renderScale.y ||
		(dst->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && dst->getField() != args.fieldToRender)) {
		setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
		OFX::throwSuiteStatusException(kOfxStatFailed);
	}

	processor.setDstImg(dst.get());
	processor.setSrcImg(src.get());
	processor.setRenderWindow(args.renderWindow);

	OfxPointD center;
	_centerPoint->getValueAtTime(args.time, center.x, center.y);

	bool centerUsePx;
	_CenterUsePx->getValueAtTime(args.time, centerUsePx);

	OfxRectI bounds = dst->getRegionOfDefinition();

	OfxPointI lightCenter;

	lightCenter.x = int((centerUsePx ? dst->getRenderScale().x : (double)bounds.x2) * center.x);
	lightCenter.y = int((centerUsePx ? dst->getRenderScale().y : (double)bounds.y2) * center.y);

	processor.setLightPosition(lightCenter);

	double blend = _blend->getValueAtTime(args.time);
	double decay = -_decay->getValueAtTime(args.time);
	double weight = _weight->getValueAtTime(args.time);
	double exposure = _exposure->getValueAtTime(args.time);
	double threshold = _threshold->getValueAtTime(args.time);

	bool mergeOver;
	_mergeOver->getValueAtTime(args.time, mergeOver);

	processor.setValues(blend, decay, weight, exposure, threshold, mergeOver);

	processor.process();
}

void
IntelGodRaysPlugin::changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName)
{

}

// the overridden render function
void
IntelGodRaysPlugin::render(const OFX::RenderArguments &args)
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
									  IntelGodRaysProcessor<unsigned char, 1, 255> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		case OFX::eBitDepthUShort: {
									   IntelGodRaysProcessor<unsigned short, 1, 65535> fred(*this);
									   setupAndProcess(fred, args);
									   break;
		}
		case OFX::eBitDepthFloat: {
									  IntelGodRaysProcessor<float, 1, 1> fred(*this);
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
									  IntelGodRaysProcessor<unsigned char, 4, 255> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		case OFX::eBitDepthUShort: {
									   IntelGodRaysProcessor<unsigned short, 4, 65535> fred(*this);
									   setupAndProcess(fred, args);
									   break;
		}
		case OFX::eBitDepthFloat: {
									  IntelGodRaysProcessor<float, 4, 1> fred(*this);
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
									  IntelGodRaysProcessor<unsigned char, 3, 255> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		case OFX::eBitDepthUShort: {
									   IntelGodRaysProcessor<unsigned short, 3, 65535> fred(*this);
									   setupAndProcess(fred, args);
									   break;
		}
		case OFX::eBitDepthFloat: {
									  IntelGodRaysProcessor<float, 3, 1> fred(*this);
									  setupAndProcess(fred, args);
									  break;
		}
		default:
			OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
		}
	}
}

//factory
mDeclarePluginFactory(IntelGodRaysFactory, {}, {});

void IntelGodRaysFactory::describe(OFX::ImageEffectDescriptor &desc)
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

void IntelGodRaysFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
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

	//blend
	{
		DoubleParamDescriptor *param = desc.defineDoubleParam(kParamBlend);
		param->setLabel(kParamBlendLabel);
		param->setDefault(0.01);
		param->setRange(0, 1.);
		param->setIncrement(0.1);
		param->setDisplayRange(0, 0.2);
		param->setAnimates(true); // can animate
		param->setDoubleType(eDoubleTypePlain);
		if (page) {
			page->addChild(*param);
		}
	}

	//decay
	{
		DoubleParamDescriptor *param = desc.defineDoubleParam(kParamDecay);
		param->setLabel(kParamDecayLabel);
		param->setDefault(0.01);
		param->setRange(0, 0.1);
		param->setIncrement(0.01);
		param->setDisplayRange(0, 0.1);
		param->setAnimates(true); // can animate
		param->setDoubleType(eDoubleTypePlain);
		if (page) {
			page->addChild(*param);
		}
	}

	//Weight
	{
		DoubleParamDescriptor *param = desc.defineDoubleParam(kParamWeight);
		param->setLabel(kParamWeightLabel);
		param->setDefault(3.0);
		param->setRange(0, 10.);
		param->setIncrement(1.);
		param->setDisplayRange(0, 10.);
		param->setAnimates(true); // can animate
		param->setDoubleType(eDoubleTypePlain);
		if (page) {
			page->addChild(*param);
		}
	}

	//Exposure
	{
		DoubleParamDescriptor *param = desc.defineDoubleParam(kParamExposure);
		param->setLabel(kParamExposureLabel);
		param->setDefault(1.0);
		param->setRange(0, 5.);
		param->setIncrement(1.);
		param->setDisplayRange(0, 5.);
		param->setAnimates(true); // can animate
		param->setDoubleType(eDoubleTypePlain);
		if (page) {
			page->addChild(*param);
		}
	}

	//Threshold
	{
		DoubleParamDescriptor *param = desc.defineDoubleParam(kParamThreshold);
		param->setLabel(kParamThresholdLabel);
		param->setDefault(0.8);
		param->setRange(0, 1.);
		param->setIncrement(1.);
		param->setDisplayRange(0, 1.);
		param->setAnimates(true); // can animate
		param->setDoubleType(eDoubleTypePlain);
		if (page) {
			page->addChild(*param);
		}
	}

	// merge over
	{
		BooleanParamDescriptor* param = desc.defineBooleanParam(kParamMergeOver);
		param->setLabel(kParamMergeOverLabel);
		param->setDefault(true);
		param->setAnimates(true);
		if (page) {
			page->addChild(*param);
		}
	}
}

OFX::ImageEffect* IntelGodRaysFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum /*context*/)
{
	return new IntelGodRaysPlugin(handle);
}

//register plugin
void getIntelGodRaysID(OFX::PluginFactoryArray &ids)
{
	static IntelGodRaysFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
	ids.push_back(&p);
}
