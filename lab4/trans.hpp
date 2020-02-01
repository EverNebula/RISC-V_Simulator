#ifndef TRANS_HEADER
#define TRANS_HEADER

#include <stdint.h>
#include <nmmintrin.h>
#include <immintrin.h>

typedef struct PixelRGB
{
	uint8_t padding, r, g, b;
	PixelRGB(uint8_t r, uint8_t g, uint8_t b):r(r), g(g), b(b){}
	PixelRGB(){}
} PixelRGB;

PixelRGB* YUV2RGB_Basic(const char *yuv, int nWidth, int nHeight);
PixelRGB* YUV2RGB_MMX(const char *yuv, int nWidth, int nHeight);
PixelRGB* YUV2RGB_SSE(const char *yuv, int nWidth, int nHeight);
PixelRGB* YUV2RGB_AVX(const char *yuv, int nWidth, int nHeight);

char* ARGB2YUV_Basic(const PixelRGB *rgb, int nWidth, int nHeight, int alpha);
char* ARGB2YUV_MMX(const PixelRGB *rgb, int nWidth, int nHeight, int alpha);
char* ARGB2YUV_SSE(const PixelRGB *rgb, int nWidth, int nHeight, int alpha);
char* ARGB2YUV_AVX(const PixelRGB *rgb, int nWidth, int nHeight, int alpha);

#endif