#include "trans.hpp"
#include <stdio.h>

inline int clip(int val)
{
	if (val < 0)
		return 0;
	else if (val > 255)
		return 255;
	return val;
}

PixelRGB* YUV2RGB_Basic(const char *yuv, int nWidth, int nHeight)
{
	int nPixel, nLength;
	nPixel  = nWidth * nHeight;
	nLength = nWidth * nHeight * 3 / 2;

	PixelRGB *result = new PixelRGB[nPixel];

	for (int h = 0; h < nHeight; ++h)
		for (int w = 0; w < nWidth; ++w)
		{
			int y = (uint8_t)yuv[h*nWidth + w];
			int u = (uint8_t)yuv[nPixel + (h/2)*(nWidth/2) + (w/2)];
			int v = (uint8_t)yuv[nPixel + nPixel/4 + (h/2)*(nWidth/2) + (w/2)];

			int r = 1.164383 * (y-16) + 1.596027 * (v-128);
			int g = 1.164383 * (y-16) - 0.391762 * (u-128) - 0.812968 * (v-128);
			int b = 1.164383 * (y-16) + 2.017232 * (u-128);

			result[h*nWidth + w] = PixelRGB(clip(r), clip(g), clip(b));
		}

	return result;
}

PixelRGB* YUV2RGB_MMX(const char *yuv, int nWidth, int nHeight)
{
	int nPixel, nLength;
	nPixel  = nWidth * nHeight;
	nLength = nWidth * nHeight * 3 / 2;

	PixelRGB *result = new PixelRGB[nPixel];

	for (int h = 0; h < nHeight; ++h)
		for (int w = 0; w < nWidth; ++w)
		{
			int y = (uint8_t)yuv[h*nWidth + w];
			int u = (uint8_t)yuv[nPixel + (h/2)*(nWidth/2) + (w/2)];
			int v = (uint8_t)yuv[nPixel + nPixel/4 + (h/2)*(nWidth/2) + (w/2)];

			int r = 1.164383 * (y-16) + 1.596027 * (v-128);
			int g = 1.164383 * (y-16) - 0.391762 * (u-128) - 0.812968 * (v-128);
			int b = 1.164383 * (y-16) + 2.017232 * (u-128);

			result[h*nWidth + w] = PixelRGB(clip(r), clip(g), clip(b));
		}

	return result;
}

PixelRGB* YUV2RGB_SSE(const char *yuv, int nWidth, int nHeight)
{
	int nPixel, nLength;
	nPixel  = nWidth * nHeight;
	nLength = nWidth * nHeight * 3 / 2;

	PixelRGB *result = new PixelRGB[nPixel];

	__m128 r_arg = _mm_set_ps(0.0, 1.164383, 0.0, 1.596027);
	__m128 g_arg = _mm_set_ps(0.0, 1.164383, -0.391762, -0.812968);
	__m128 b_arg = _mm_set_ps(0.0, 1.164383, 2.017232, 0.0);
	__m128 c_zero = _mm_set_ps(0.0, 0.0, 0.0, 0.0);
	__m128 c_max = _mm_set_ps(255.0, 255.0, 255.0, 255.0);

	for (int h = 0; h < nHeight; ++h)
		for (int w = 0; w < nWidth; ++w)
		{
			int y = (uint8_t)yuv[h*nWidth + w];
			int u = (uint8_t)yuv[nPixel + (h/2)*(nWidth/2) + (w/2)];
			int v = (uint8_t)yuv[nPixel + nPixel/4 + (h/2)*(nWidth/2) + (w/2)];

			__m128 v_yuv = _mm_set_ps(1.0, y-16, u-128, v-128);

			int r, g, b;

			__m128 v_res;
			v_res = _mm_dp_ps(v_yuv, r_arg, 0xf1);
			v_res = _mm_add_ps(v_res, _mm_dp_ps(v_yuv, g_arg, 0xf2));
			v_res = _mm_add_ps(v_res, _mm_dp_ps(v_yuv, b_arg, 0xf4));
			v_res = _mm_max_ps(c_zero, _mm_min_ps(c_max, v_res));

			r = _mm_extract_ps(v_res, 0x0);
			r = *(float*)(&r);
			g = _mm_extract_ps(v_res, 0x1);
			g = *(float*)(&g);
			b = _mm_extract_ps(v_res, 0x2);
			b = *(float*)(&b);

			result[h*nWidth + w] = PixelRGB(r, g, b);
		}

	return result;
}

PixelRGB* YUV2RGB_AVX(const char *yuv, int nWidth, int nHeight)
{
	int nPixel, nLength;
	nPixel  = nWidth * nHeight;
	nLength = nWidth * nHeight * 3 / 2;

	PixelRGB *result = new PixelRGB[nPixel];

	__m256 r_arg = _mm256_set_ps(0.0, 1.164383, 0.0, 1.596027,
								0.0, 1.164383, 0.0, 1.596027);
	__m256 g_arg = _mm256_set_ps(0.0, 1.164383, -0.391762, -0.812968,
								0.0, 1.164383, -0.391762, -0.812968);
	__m256 b_arg = _mm256_set_ps(0.0, 1.164383, 2.017232, 0.0,
								0.0, 1.164383, 2.017232, 0.0);
	__m256 c_zero = _mm256_set_ps(0.0, 0.0, 0.0, 0.0,
								0.0, 0.0, 0.0, 0.0);
	__m256 c_max = _mm256_set_ps(255.0, 255.0, 255.0, 255.0,
								255.0, 255.0, 255.0, 255.0);

	for (int h = 0; h < nHeight; ++h)
		for (int w = 0; w < nWidth; w += 2)
		{
			int off = h*nWidth + w;

			int y1 = (uint8_t)yuv[off];
			int y2 = (uint8_t)yuv[off+1];
			int u = (uint8_t)yuv[nPixel + (h/2)*(nWidth/2) + (w/2)];
			int v = (uint8_t)yuv[nPixel + nPixel/4 + (h/2)*(nWidth/2) + (w/2)];

			__m256 v_yuv = _mm256_set_ps(1.0, y1-16, u-128, v-128,
										1.0, y2-16, u-128, v-128);

			int r1, g1, b1;
			int r2, g2, b2;

			__m256 v_res;
			v_res = _mm256_dp_ps(v_yuv, r_arg, 0xf1);
			v_res = _mm256_add_ps(v_res, _mm256_dp_ps(v_yuv, g_arg, 0xf2));
			v_res = _mm256_add_ps(v_res, _mm256_dp_ps(v_yuv, b_arg, 0xf4));
			v_res = _mm256_max_ps(c_zero, _mm256_min_ps(c_max, v_res));

			r1 = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x1), 0x0);
			r1 = *(float*)(&r1);
			r2 = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x0), 0x0);
			r2 = *(float*)(&r2);
			g1 = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x1), 0x1);
			g1 = *(float*)(&g1);
			g2 = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x0), 0x1);
			g2 = *(float*)(&g2);
			b1 = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x1), 0x2);
			b1 = *(float*)(&b1);
			b2 = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x0), 0x2);
			b2 = *(float*)(&b2);

			result[off] = PixelRGB(r1, g1, b1);
			result[off+1] = PixelRGB(r2, g2, b2);
		}

	return result;
}

char* ARGB2YUV_Basic(const PixelRGB *rgb, int nWidth, int nHeight, int alpha)
{
	int nPixel, nLength;
	nPixel  = nWidth * nHeight;
	nLength = nWidth * nHeight * 3 / 2;

	char *result = new char[nLength];

	for (int h = 0; h < nHeight; ++h)
		for (int w = 0; w < nWidth; ++w)
		{
			int off = h*nWidth + w;

			int r = alpha * rgb[off].r / 256;
			int g = alpha * rgb[off].g / 256;
			int b = alpha * rgb[off].b / 256;

			int y = 0.256788 * r + 0.504129 * g + 0.097906 * b + 16;
			int u = -0.148223 * r - 0.290993 * g + 0.439216 * b + 128;
			int v = 0.439216 * r - 0.367788 * g - 0.071427 * b + 128;

			result[off] = y;
			result[nPixel + (h/2)*(nWidth/2) + (w/2)] = u;
			result[nPixel + nPixel/4 + (h/2)*(nWidth/2) + (w/2)] = v;
		}

	return result;
}

char* ARGB2YUV_MMX(const PixelRGB *rgb, int nWidth, int nHeight, int alpha)
{
	int nPixel, nLength;
	nPixel  = nWidth * nHeight;
	nLength = nWidth * nHeight * 3 / 2;

	char *result = new char[nLength];

	__m64 c_alpha = _mm_set_pi16(1, alpha, alpha, alpha);
	__m64 c_srl = _mm_set_pi16(0, 0, 0, 8);

	for (int h = 0; h < nHeight; ++h)
		for (int w = 0; w < nWidth; ++w)
		{
			int off = h*nWidth + w;

			__m64 v_rgb = _mm_set_pi16(1, rgb[off].r, rgb[off].g, rgb[off].b);
			v_rgb = _mm_srl_pi16(_m_pmullw(v_rgb, c_alpha), c_srl);

			int64_t tmp;
			int y, u, v;
			__m64 v_res;

			v_res = _m_pmaddwd(v_rgb, _mm_set_pi16(0, 8414, 16519, 3208));
			tmp = _m_to_int64(v_res);
			y = (((int)tmp + (int)(tmp >> 32)) >> 15) + 16;

			v_res = _m_pmaddwd(v_rgb, _mm_set_pi16(0, -4856, -9535, 14392));
			tmp = _m_to_int64(v_res);
			u = (((int)tmp + (int)(tmp >> 32)) >> 15) + 128;

			v_res = _m_pmaddwd(v_rgb, _mm_set_pi16(0, 14392, -12051, -2340));
			tmp = _m_to_int64(v_res);
			v = (((int)tmp + (int)(tmp >> 32)) >> 15) + 128;

			result[off] = y;
			result[nPixel + (h/2)*(nWidth/2) + (w/2)] = u;
			result[nPixel + nPixel/4 + (h/2)*(nWidth/2) + (w/2)] = v;
		}

	return result;
}

char* ARGB2YUV_SSE(const PixelRGB *rgb, int nWidth, int nHeight, int alpha)
{
	int nPixel, nLength;
	nPixel  = nWidth * nHeight;
	nLength = nWidth * nHeight * 3 / 2;

	char *result = new char[nLength];

	__m128 y_arg = _mm_set_ps(16.0, 0.256788, 0.504129, 0.097906);
	__m128 u_arg = _mm_set_ps(128.0, -0.148223, -0.290993, 0.439216);
	__m128 v_arg = _mm_set_ps(128.0, 0.439216, -0.367788, -0.071427);

	for (int h = 0; h < nHeight; ++h)
		for (int w = 0; w < nWidth; ++w)
		{
			int off = h*nWidth + w;

			float t_alpha = (float)alpha / 256;
			__m128 v_rgb = _mm_mul_ps(_mm_set_ps(1.0, rgb[off].r, rgb[off].g, rgb[off].b),
									_mm_set_ps(1.0, t_alpha, t_alpha, t_alpha));
			
			int y, u, v;

			__m128 v_res;
			v_res = _mm_dp_ps(v_rgb, y_arg, 0xff);
			y = _mm_extract_ps(v_res, 0x0);
			y = *(float*)(&y);

			v_res = _mm_dp_ps(v_rgb, u_arg, 0xff);
			u = _mm_extract_ps(v_res, 0x0);
			u = *(float*)(&u);

			v_res = _mm_dp_ps(v_rgb, v_arg, 0xff);
			v = _mm_extract_ps(v_res, 0x0);
			v = *(float*)(&v);

			result[off] = y;
			result[nPixel + (h/2)*(nWidth/2) + (w/2)] = u;
			result[nPixel + nPixel/4 + (h/2)*(nWidth/2) + (w/2)] = v;
		}

	return result;
}

char* ARGB2YUV_AVX(const PixelRGB *rgb, int nWidth, int nHeight, int alpha)
{
	int nPixel, nLength;
	nPixel  = nWidth * nHeight;
	nLength = nWidth * nHeight * 3 / 2;

	char *result = new char[nLength];

	__m256 y_arg = _mm256_set_ps(16.0, 0.256788, 0.504129, 0.097906,
							16.0, 0.256788, 0.504129, 0.097906);
	__m256 u_arg = _mm256_set_ps(128.0, -0.148223, -0.290993, 0.439216,
							128.0, -0.148223, -0.290993, 0.439216);
	__m256 v_arg = _mm256_set_ps(128.0, 0.439216, -0.367788, -0.071427,
							128.0, 0.439216, -0.367788, -0.071427);

	for (int h = 0; h < nHeight; ++h)
		for (int w = 0; w < nWidth; w += 2)
		{
			int off = h*nWidth + w;

			float t_alpha = (float)alpha / 256;
			__m256 v_rgb = _mm256_mul_ps(_mm256_set_ps(1.0, rgb[off].r, rgb[off].g, rgb[off].b,
												1.0, rgb[off+1].r, rgb[off+1].g, rgb[off+1].b),
									_mm256_set_ps(1.0, t_alpha, t_alpha, t_alpha,
											1.0, t_alpha, t_alpha, t_alpha));
			
			int y1, y2, u, v;

			__m256 v_res;
			v_res = _mm256_dp_ps(v_rgb, y_arg, 0xff);
			y1 = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x1), 0x0);
			y1 = *(float*)(&y1);
			y2 = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x0), 0x0);
			y2 = *(float*)(&y2);

			v_res = _mm256_dp_ps(v_rgb, u_arg, 0xff);
			u = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x0), 0x0);
			u = *(float*)(&u);

			v_res = _mm256_dp_ps(v_rgb, v_arg, 0xff);
			v = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x0), 0x0);
			v = *(float*)(&v);

			result[off] = y1;
			result[off+1] = y2;
			result[nPixel + (h/2)*(nWidth/2) + (w/2)] = u;
			result[nPixel + nPixel/4 + (h/2)*(nWidth/2) + (w/2)] = v;
		}

	return result;
}
