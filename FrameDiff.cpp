// Code extracted from avisynth+ conditional_functions.cpp
// and then other places for the types and macros that function depends on, down the rabbit hole.
//
// Most of the code in the file is: Copyright 2002 Ben Rudiak-Gould et al.
// Under GNU GPL, same license as that of this program.
//
// Q: Wy not use Invoke(YDifferenceFromPrevious)?
// A: Because there is no way to set current_frame per thread. Only the global current_frame var
//    makes the runtime filter pick up on a custom frame, but in an MT runtime, using the global
//    var is a disaster that strikes immediately upon running with more than one thread.
//
//    The root of the problem is known, but unfortunately the following discussion has not yet
//    reached a conclusion:
//    https://forum.doom9.org/showthread.php?p=1777178#post1777178
//
//    There's a pull request on github, but it's not been merged. And.. even if it eventually will,
//    this filter won't be usable on any other version of avisynth in that case. So... that's why
//    this copy-paste anti-pattern for reuse.

#include <windows.h>
#include <immintrin.h>
#include <limits>
#include <algorithm>
#include "FrameDiff.h"
#undef max

// Boiler plate with the guts of supporting constructs to get the diff function (last fun in file) to work.

#define CS_YUY2 0x60000004  // From avisynth.h for avisynth+ 
#define CS_Shift_Sample_Bits 16
#define IS_POWER2(n) ((n) && !((n) & ((n) - 1)))
#define IS_PTR_ALIGNED(ptr, align) (((uintptr_t)ptr & ((uintptr_t)(align-1))) == 0)

template<typename T>
T clamp(T n, T min, T max)
{
	n = n > max ? max : n;
	return n < min ? min : n;
}

template<typename T>
static bool IsPtrAligned(T* ptr, size_t align)
{
	IS_POWER2(align);
	return (bool)IS_PTR_ALIGNED(ptr, align);
}

template<typename pixel_t>
static double get_sad_c(const BYTE* c_plane8, const BYTE* t_plane8, size_t height, size_t width, size_t c_pitch, size_t t_pitch) {
	const pixel_t *c_plane = reinterpret_cast<const pixel_t *>(c_plane8);
	const pixel_t *t_plane = reinterpret_cast<const pixel_t *>(t_plane8);
	c_pitch /= sizeof(pixel_t);
	t_pitch /= sizeof(pixel_t);
	typedef typename std::conditional < sizeof(pixel_t) == 4, double, __int64>::type sum_t;
	sum_t accum = 0; // int32 holds sum of maximum 16 Mpixels for 8 bit, and 65536 pixels for uint16_t pixels

	for (size_t y = 0; y < height; y++) {
		for (size_t x = 0; x < width; x++) {
			accum += std::abs(t_plane[x] - c_plane[x]);
		}
		c_plane += c_pitch;
		t_plane += t_pitch;
	}
	return (double)accum;

}

#ifndef _WIN64
static size_t get_sad_isse(const BYTE* src_ptr, const BYTE* other_ptr, size_t height, size_t width, size_t src_pitch, size_t other_pitch) {
	size_t mod8_width = width / 8 * 8;
	size_t result = 0;
	__m64 sum = _mm_setzero_si64();

	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < mod8_width; x += 8) {
			__m64 src = *reinterpret_cast<const __m64*>(src_ptr + x);
			__m64 other = *reinterpret_cast<const __m64*>(other_ptr + x);
			__m64 sad = _mm_sad_pu8(src, other);
			sum = _mm_add_pi32(sum, sad);
		}

		for (size_t x = mod8_width; x < width; ++x) {
			result += std::abs(src_ptr[x] - other_ptr[x]);
		}

		src_ptr += src_pitch;
		other_ptr += other_pitch;
	}
	result += _mm_cvtsi64_si32(sum);
	_mm_empty();
	return result;
}
#endif
// works for uint8_t, but there is a specific, bit faster function above
// also used from conditionalfunctions
// packed rgb template masks out alpha plane for RGB32/RGB64
template<typename pixel_t, bool packedRGB3264>
__int64 calculate_sad_8_or_16_sse2(const BYTE* cur_ptr, const BYTE* other_ptr, int cur_pitch, int other_pitch, size_t rowsize, size_t height)
{
	size_t mod16_width = rowsize / 16 * 16;

	__m128i zero = _mm_setzero_si128();
	__int64 totalsum = 0; // fullframe SAD exceeds int32 at 8+ bit

	__m128i rgb_mask;
	if (packedRGB3264) {
		if (sizeof(pixel_t) == 1)
			rgb_mask = _mm_set1_epi32(0x00FFFFFF);
		else
			rgb_mask = _mm_set_epi32(0x0000FFFF, 0xFFFFFFFF, 0x0000FFFF, 0xFFFFFFFF);
	}

	for (size_t y = 0; y < height; y++)
	{
		__m128i sum = _mm_setzero_si128(); // for one row int is enough
		for (size_t x = 0; x < mod16_width; x += 16)
		{
			__m128i src1, src2;
			src1 = _mm_load_si128((__m128i *) (cur_ptr + x));   // 16 bytes or 8 words
			src2 = _mm_load_si128((__m128i *) (other_ptr + x));
			if (packedRGB3264) {
				src1 = _mm_and_si128(src1, rgb_mask); // mask out A channel
				src2 = _mm_and_si128(src2, rgb_mask);
			}
			if (sizeof(pixel_t) == 1) {
				// this is uint_16 specific, but leave here for sample
				sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, src2)); // sum0_32, 0, sum1_32, 0
			}
			else if (sizeof(pixel_t) == 2) {
				__m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
				__m128i smaller_t = _mm_subs_epu16(src2, src1);
				__m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
				// 8 x uint16 absolute differences
				sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
				sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
				// sum0_32, sum1_32, sum2_32, sum3_32
			}
		}
		// summing up partial sums
		if (sizeof(pixel_t) == 2) {
			// at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
			__m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
			__m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
			sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
			/* SSSE3: told to be not too fast
			sum = _mm_hadd_epi32(sum, zero);  // A1+A2, B1+B2, 0+0, 0+0
			sum = _mm_hadd_epi32(sum, zero);  // A1+A2+B1+B2, 0+0+0+0, 0+0+0+0, 0+0+0+0
			*/
		}

		// sum here: two 32 bit partial result: sum1 0 sum2 0
		__m128i sum_hi = _mm_unpackhi_epi64(sum, zero);
		// or: __m128i sum_hi = _mm_castps_si128(_mm_movehl_ps(_mm_setzero_ps(), _mm_castsi128_ps(sum)));
		sum = _mm_add_epi32(sum, sum_hi);
		int rowsum = _mm_cvtsi128_si32(sum);

		// rest
		if (mod16_width != rowsize) {
			if (packedRGB3264)
				for (size_t x = mod16_width / sizeof(pixel_t) / 4; x < rowsize / sizeof(pixel_t) / 4; x += 4) {
					rowsum += std::abs(reinterpret_cast<const pixel_t *>(cur_ptr)[x * 4 + 0] - reinterpret_cast<const pixel_t *>(other_ptr)[x * 4 + 0]) +
						std::abs(reinterpret_cast<const pixel_t *>(cur_ptr)[x * 4 + 1] - reinterpret_cast<const pixel_t *>(other_ptr)[x * 4 + 1]) +
						std::abs(reinterpret_cast<const pixel_t *>(cur_ptr)[x * 4 + 2] - reinterpret_cast<const pixel_t *>(other_ptr)[x * 4 + 2]);
					// no alpha
				}
			else
				for (size_t x = mod16_width / sizeof(pixel_t); x < rowsize / sizeof(pixel_t); ++x) {
					rowsum += std::abs(reinterpret_cast<const pixel_t *>(cur_ptr)[x] - reinterpret_cast<const pixel_t *>(other_ptr)[x]);
				}
		}

		totalsum += rowsum;

		cur_ptr += cur_pitch;
		other_ptr += other_pitch;
	}
	return totalsum;
}
// #endif

int BitsPerComponent(VideoInfo& vi) {
	// unlike BitsPerPixel, this returns the real
	// component size as 8/10/12/14/16/32 bit
	if (vi.pixel_type == CS_YUY2)
		return 8;
	// planar YUV/RGB and packed RGB
	const int componentBitSizes[8] = { 8,16,32,0,0,10,12,14 };
	return componentBitSizes[(vi.pixel_type >> CS_Shift_Sample_Bits) & 7];
}

int ComponentSize(VideoInfo& vi) {
	if (vi.IsPlanar()) {
		const int componentSizes[8] = { 1,2,4,0,0,2,2,2 };
		return componentSizes[(vi.pixel_type >> CS_Shift_Sample_Bits) & 7];
	}
	return 1; // YUY2
}


// The actual function of interest
float YDiff(AVSValue clip, int n, int offset, IScriptEnvironment* env) {
	if (!clip.IsClip())
		env->ThrowError("SmoothSkip::YDiff: No clip supplied!");

	PClip child = clip.AsClip();
	VideoInfo vi = child->GetVideoInfo();
	if (!vi.IsPlanar()) {
		env->ThrowError("SmoothSkip::YDiff: Only planar YUV or planar RGB images images supported!");
	}

	n = clamp(n, 0, vi.num_frames - 1);
	int n2 = clamp(n + offset, 0, vi.num_frames - 1);
	int plane = PLANAR_Y;

	PVideoFrame src = child->GetFrame(n, env);
	PVideoFrame src2 = child->GetFrame(n2, env);

	int pixelsize = ComponentSize(vi);
	int bits_per_pixel = BitsPerComponent(vi);

	const BYTE* srcp = src->GetReadPtr(plane);
	const BYTE* srcp2 = src2->GetReadPtr(plane);
	int height = src->GetHeight(plane);
	int rowsize = src->GetRowSize(plane);
	int width = rowsize / pixelsize;
	int pitch = src->GetPitch(plane);
	int pitch2 = src2->GetPitch(plane);

	if (width == 0 || height == 0)
		env->ThrowError("SmoothSkip::YDiff: No chroma planes in greyscale clip!");

	int total_pixels = width * height;
	bool sum_in_32bits;
	if (pixelsize == 4)
		sum_in_32bits = false;
	else // worst case check
		sum_in_32bits = ((__int64)total_pixels * ((1 << bits_per_pixel) - 1)) <= std::numeric_limits<int>::max();

	double sad = 0;
	// for c: width, for sse: rowsize
	{
		if ((pixelsize == 2) && (env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(srcp2, 16) && rowsize >= 16) {
			sad = (double)calculate_sad_8_or_16_sse2<uint16_t, false>(srcp, srcp2, pitch, pitch2, rowsize, height); // in focus, no overflow
		}
		else if ((pixelsize == 1) && (env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(srcp2, 16) && rowsize >= 16) {
			sad = (double)calculate_sad_8_or_16_sse2<uint8_t, false>(srcp, srcp2, pitch, pitch2, rowsize, height); // in focus, no overflow
		}
		else
#ifndef _WIN64
			if ((pixelsize == 1) && sum_in_32bits && (env->GetCPUFlags() & CPUF_INTEGER_SSE) && width >= 8) {
				sad = get_sad_isse(srcp, srcp2, height, width, pitch, pitch2);
			}
			else
#endif
			{
				if (pixelsize == 1)
					sad = get_sad_c<uint8_t>(srcp, srcp2, height, width, pitch, pitch2);
				else if (pixelsize == 2)
					sad = get_sad_c<uint16_t>(srcp, srcp2, height, width, pitch, pitch2);
				else // pixelsize==4
					sad = get_sad_c<float>(srcp, srcp2, height, width, pitch, pitch2);
			}
	}

	return (float)(sad / ((double)height * width));
}
