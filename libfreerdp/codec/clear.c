/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ClearCodec Bitmap Compression
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/bitstream.h>
#include <winpr/stream.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/clear.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("codec.clear")

#define CLEARCODEC_FLAG_GLYPH_INDEX 0x01
#define CLEARCODEC_FLAG_GLYPH_HIT 0x02
#define CLEARCODEC_FLAG_CACHE_RESET 0x04

#define CLEARCODEC_VBAR_SIZE 32768
#define CLEARCODEC_VBAR_SHORT_SIZE 16384

typedef struct
{
	UINT32 size;
	UINT32 count;
	UINT32 width;
	UINT32 height;
	UINT32* pixels;
} CLEAR_GLYPH_ENTRY;

typedef struct
{
	UINT32 size;
	UINT32 count;
	BYTE* pixels;
} CLEAR_VBAR_ENTRY;

struct S_CLEAR_CONTEXT
{
	BOOL Compressor;
	NSC_CONTEXT* nsc;
	UINT32 seqNumber;
	BYTE* TempBuffer;
	size_t TempSize;
	UINT32 format;
	BOOL formatSet;
	UINT32 nscTolerance;
	UINT32 GlyphCacheCursor;
	CLEAR_GLYPH_ENTRY GlyphCache[4000];
	UINT32 VBarStorageCursor;
	UINT32 VBarStorageCount;
	CLEAR_VBAR_ENTRY VBarStorage[CLEARCODEC_VBAR_SIZE];
	UINT32 ShortVBarStorageCursor;
	UINT32 ShortVBarStorageCount;
	CLEAR_VBAR_ENTRY ShortVBarStorage[CLEARCODEC_VBAR_SHORT_SIZE];
	BOOL VBarResetPending;
	wLog* log;
};

typedef struct
{
	BYTE r;
	BYTE g;
	BYTE b;
} CLEAR_BGR;

static const UINT32 CLEAR_LOG2_FLOOR[256] = {
	0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

static const BYTE CLEAR_8BIT_MASKS[9] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF };

static void clear_reset_vbar_storage(CLEAR_CONTEXT* WINPR_RESTRICT clear, BOOL zero)
{
	if (zero)
	{
		for (size_t i = 0; i < ARRAYSIZE(clear->VBarStorage); i++)
			winpr_aligned_free(clear->VBarStorage[i].pixels);

		ZeroMemory(clear->VBarStorage, sizeof(clear->VBarStorage));
	}

	clear->VBarStorageCursor = 0;
	clear->VBarStorageCount = 0;

	if (zero)
	{
		for (size_t i = 0; i < ARRAYSIZE(clear->ShortVBarStorage); i++)
			winpr_aligned_free(clear->ShortVBarStorage[i].pixels);

		ZeroMemory(clear->ShortVBarStorage, sizeof(clear->ShortVBarStorage));
	}

	clear->ShortVBarStorageCursor = 0;
	clear->ShortVBarStorageCount = 0;
}

static void clear_reset_glyph_cache(CLEAR_CONTEXT* WINPR_RESTRICT clear)
{
	for (size_t i = 0; i < ARRAYSIZE(clear->GlyphCache); i++)
		winpr_aligned_free(clear->GlyphCache[i].pixels);

	ZeroMemory(clear->GlyphCache, sizeof(clear->GlyphCache));
}

static BOOL convert_color(BYTE* WINPR_RESTRICT dst, UINT32 nDstStep, UINT32 DstFormat, UINT32 nXDst,
                          UINT32 nYDst, UINT32 nWidth, UINT32 nHeight,
                          const BYTE* WINPR_RESTRICT src, UINT32 nSrcStep, UINT32 SrcFormat,
                          UINT32 nDstWidth, UINT32 nDstHeight,
                          const gdiPalette* WINPR_RESTRICT palette)
{
	if (nWidth + nXDst > nDstWidth)
		nWidth = nDstWidth - nXDst;

	if (nHeight + nYDst > nDstHeight)
		nHeight = nDstHeight - nYDst;

	return freerdp_image_copy_no_overlap(dst, DstFormat, nDstStep, nXDst, nYDst, nWidth, nHeight,
	                                     src, SrcFormat, nSrcStep, 0, 0, palette,
	                                     FREERDP_KEEP_DST_ALPHA);
}

static BOOL clear_decompress_nscodec(wLog* log, NSC_CONTEXT* WINPR_RESTRICT nsc, UINT32 width,
                                     UINT32 height, wStream* WINPR_RESTRICT s,
                                     UINT32 bitmapDataByteCount, BYTE* WINPR_RESTRICT pDstData,
                                     UINT32 DstFormat, UINT32 nDstStep, UINT32 nXDstRel,
                                     UINT32 nYDstRel, UINT32 nDstWidth, UINT32 nDstHeight)
{
	BOOL rc = 0;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, bitmapDataByteCount))
		return FALSE;

	rc = nsc_process_message(nsc, 32, width, height, Stream_Pointer(s), bitmapDataByteCount,
	                         pDstData, DstFormat, nDstStep, nXDstRel, nYDstRel, nDstWidth,
	                         nDstHeight, FREERDP_FLIP_NONE);
	Stream_Seek(s, bitmapDataByteCount);
	return rc;
}

static BOOL clear_decompress_subcode_rlex(wLog* log, wStream* WINPR_RESTRICT s,
                                          UINT32 bitmapDataByteCount, UINT32 width, UINT32 height,
                                          BYTE* WINPR_RESTRICT pDstData, UINT32 DstFormat,
                                          UINT32 nDstStep, UINT32 nXDstRel, UINT32 nYDstRel,
                                          UINT32 nDstWidth, UINT32 nDstHeight)
{
	UINT32 x = 0;
	UINT32 y = 0;
	UINT32 pixelCount = 0;
	UINT32 bitmapDataOffset = 0;
	size_t pixelIndex = 0;
	UINT32 numBits = 0;
	BYTE startIndex = 0;
	BYTE stopIndex = 0;
	BYTE suiteIndex = 0;
	BYTE suiteDepth = 0;
	BYTE paletteCount = 0;
	UINT32 palette[128] = WINPR_C_ARRAY_INIT;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, bitmapDataByteCount))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 1))
		return FALSE;
	Stream_Read_UINT8(s, paletteCount);
	bitmapDataOffset = 1 + (paletteCount * 3);

	if ((paletteCount > 127) || (paletteCount < 1))
	{
		WLog_Print(log, WLOG_ERROR, "paletteCount %" PRIu8 "", paletteCount);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(log, s, paletteCount, 3ull))
		return FALSE;

	for (UINT32 i = 0; i < paletteCount; i++)
	{
		BYTE r = 0;
		BYTE g = 0;
		BYTE b = 0;
		Stream_Read_UINT8(s, b);
		Stream_Read_UINT8(s, g);
		Stream_Read_UINT8(s, r);
		palette[i] = FreeRDPGetColor(DstFormat, r, g, b, 0xFF);
	}

	pixelIndex = 0;
	pixelCount = width * height;
	numBits = CLEAR_LOG2_FLOOR[paletteCount - 1] + 1;

	while (bitmapDataOffset < bitmapDataByteCount)
	{
		UINT32 tmp = 0;
		UINT32 color = 0;
		UINT32 runLengthFactor = 0;

		if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 2))
			return FALSE;

		Stream_Read_UINT8(s, tmp);
		Stream_Read_UINT8(s, runLengthFactor);
		bitmapDataOffset += 2;
		suiteDepth = (tmp >> numBits) & CLEAR_8BIT_MASKS[(8 - numBits)];
		stopIndex = tmp & CLEAR_8BIT_MASKS[numBits];
		startIndex = stopIndex - suiteDepth;

		if (runLengthFactor >= 0xFF)
		{
			if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 2))
				return FALSE;

			Stream_Read_UINT16(s, runLengthFactor);
			bitmapDataOffset += 2;

			if (runLengthFactor >= 0xFFFF)
			{
				if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
					return FALSE;

				Stream_Read_UINT32(s, runLengthFactor);
				bitmapDataOffset += 4;
			}
		}

		if (startIndex >= paletteCount)
		{
			WLog_Print(log, WLOG_ERROR, "startIndex %" PRIu8 " > paletteCount %" PRIu8 "]",
			           startIndex, paletteCount);
			return FALSE;
		}

		if (stopIndex >= paletteCount)
		{
			WLog_Print(log, WLOG_ERROR, "stopIndex %" PRIu8 " > paletteCount %" PRIu8 "]",
			           stopIndex, paletteCount);
			return FALSE;
		}

		suiteIndex = startIndex;

		if (suiteIndex > 127)
		{
			WLog_Print(log, WLOG_ERROR, "suiteIndex %" PRIu8 " > 127]", suiteIndex);
			return FALSE;
		}

		color = palette[suiteIndex];

		if ((pixelIndex + runLengthFactor) > pixelCount)
		{
			WLog_Print(log, WLOG_ERROR,
			           "pixelIndex %" PRIuz " + runLengthFactor %" PRIu32 " > pixelCount %" PRIu32
			           "",
			           pixelIndex, runLengthFactor, pixelCount);
			return FALSE;
		}

		for (UINT32 i = 0; i < runLengthFactor; i++)
		{
			BYTE* pTmpData = &pDstData[(nXDstRel + x) * FreeRDPGetBytesPerPixel(DstFormat) +
			                           (nYDstRel + y) * nDstStep];

			if ((nXDstRel + x < nDstWidth) && (nYDstRel + y < nDstHeight))
				FreeRDPWriteColor(pTmpData, DstFormat, color);

			if (++x >= width)
			{
				y++;
				x = 0;
			}
		}

		pixelIndex += runLengthFactor;

		if ((pixelIndex + (suiteDepth + 1)) > pixelCount)
		{
			WLog_Print(log, WLOG_ERROR,
			           "pixelIndex %" PRIuz " + suiteDepth %" PRIu8 " + 1 > pixelCount %" PRIu32 "",
			           pixelIndex, suiteDepth, pixelCount);
			return FALSE;
		}

		for (UINT32 i = 0; i <= suiteDepth; i++)
		{
			BYTE* pTmpData = &pDstData[(nXDstRel + x) * FreeRDPGetBytesPerPixel(DstFormat) +
			                           (nYDstRel + y) * nDstStep];
			UINT32 ccolor = palette[suiteIndex];

			if (suiteIndex > 127)
			{
				WLog_Print(log, WLOG_ERROR, "suiteIndex %" PRIu8 " > 127", suiteIndex);
				return FALSE;
			}

			suiteIndex++;

			if ((nXDstRel + x < nDstWidth) && (nYDstRel + y < nDstHeight))
				FreeRDPWriteColor(pTmpData, DstFormat, ccolor);

			if (++x >= width)
			{
				y++;
				x = 0;
			}
		}

		pixelIndex += (suiteDepth + 1);
	}

	if (pixelIndex != pixelCount)
	{
		WLog_Print(log, WLOG_ERROR, "pixelIndex %" PRIuz " != pixelCount %" PRIu32 "", pixelIndex,
		           pixelCount);
		return FALSE;
	}

	return TRUE;
}

static BOOL clear_resize_buffer(CLEAR_CONTEXT* WINPR_RESTRICT clear, UINT32 width, UINT32 height)
{
	if (!clear)
		return FALSE;

	const UINT64 size = 1ull * (width + 16ull) * (height + 16ull);
	const size_t bpp = FreeRDPGetBytesPerPixel(clear->format);
	if (size > UINT32_MAX / bpp)
		return FALSE;

	if (size > clear->TempSize / bpp)
	{
		BYTE* tmp = (BYTE*)winpr_aligned_recalloc(clear->TempBuffer,
		                                          WINPR_ASSERTING_INT_CAST(size_t, size), bpp, 32);

		if (!tmp)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "clear->TempBuffer winpr_aligned_recalloc failed for %" PRIu64 " bytes",
			           size);
			return FALSE;
		}

		clear->TempSize = WINPR_ASSERTING_INT_CAST(size_t, size* bpp);
		clear->TempBuffer = tmp;
	}

	return TRUE;
}

static BOOL clear_decompress_residual_data(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                           wStream* WINPR_RESTRICT s, UINT32 residualByteCount,
                                           UINT32 nWidth, UINT32 nHeight,
                                           BYTE* WINPR_RESTRICT pDstData, UINT32 DstFormat,
                                           UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                           UINT32 nDstWidth, UINT32 nDstHeight,
                                           const gdiPalette* WINPR_RESTRICT palette)
{
	UINT32 nSrcStep = 0;
	UINT32 suboffset = 0;
	BYTE* dstBuffer = nullptr;
	UINT32 pixelIndex = 0;
	UINT32 pixelCount = 0;

	if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, residualByteCount))
		return FALSE;

	suboffset = 0;
	pixelIndex = 0;
	pixelCount = nWidth * nHeight;

	if (!clear_resize_buffer(clear, nWidth, nHeight))
		return FALSE;

	dstBuffer = clear->TempBuffer;

	while (suboffset < residualByteCount)
	{
		BYTE r = 0;
		BYTE g = 0;
		BYTE b = 0;
		UINT32 runLengthFactor = 0;
		UINT32 color = 0;

		if ((residualByteCount - suboffset) < 4)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "residual run header exceeds residualByteCount %" PRIu32 " at offset %" PRIu32
			           "",
			           residualByteCount, suboffset);
			return FALSE;
		}

		if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 4))
			return FALSE;

		Stream_Read_UINT8(s, b);
		Stream_Read_UINT8(s, g);
		Stream_Read_UINT8(s, r);
		Stream_Read_UINT8(s, runLengthFactor);
		suboffset += 4;
		color = FreeRDPGetColor(clear->format, r, g, b, 0xFF);

		if (runLengthFactor >= 0xFF)
		{
			if ((residualByteCount - suboffset) < 2)
			{
				WLog_Print(clear->log, WLOG_ERROR,
				           "residual extended run header exceeds residualByteCount %" PRIu32
				           " at offset %" PRIu32 "",
				           residualByteCount, suboffset);
				return FALSE;
			}

			if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 2))
				return FALSE;

			Stream_Read_UINT16(s, runLengthFactor);
			suboffset += 2;

			if (runLengthFactor >= 0xFFFF)
			{
				if ((residualByteCount - suboffset) < 4)
				{
					WLog_Print(clear->log, WLOG_ERROR,
					           "residual long run header exceeds residualByteCount %" PRIu32
					           " at offset %" PRIu32 "",
					           residualByteCount, suboffset);
					return FALSE;
				}

				if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 4))
					return FALSE;

				Stream_Read_UINT32(s, runLengthFactor);
				suboffset += 4;
			}
		}

		if ((pixelIndex >= pixelCount) || (runLengthFactor > (pixelCount - pixelIndex)))
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "pixelIndex %" PRIu32 " + runLengthFactor %" PRIu32 " > pixelCount %" PRIu32
			           "",
			           pixelIndex, runLengthFactor, pixelCount);
			return FALSE;
		}

		for (UINT32 i = 0; i < runLengthFactor; i++)
		{
			FreeRDPWriteColor(dstBuffer, clear->format, color);
			dstBuffer += FreeRDPGetBytesPerPixel(clear->format);
		}

		pixelIndex += runLengthFactor;
	}

	nSrcStep = nWidth * FreeRDPGetBytesPerPixel(clear->format);

	if (pixelIndex != pixelCount)
	{
		WLog_Print(clear->log, WLOG_ERROR, "pixelIndex %" PRIu32 " != pixelCount %" PRIu32 "",
		           pixelIndex, pixelCount);
		return FALSE;
	}

	return convert_color(pDstData, nDstStep, DstFormat, nXDst, nYDst, nWidth, nHeight,
	                     clear->TempBuffer, nSrcStep, clear->format, nDstWidth, nDstHeight,
	                     palette);
}

static BOOL clear_decompress_subcodecs_data(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                            wStream* WINPR_RESTRICT s, UINT32 subcodecByteCount,
                                            UINT32 nWidth, UINT32 nHeight,
                                            BYTE* WINPR_RESTRICT pDstData, UINT32 DstFormat,
                                            UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                            UINT32 nDstWidth, UINT32 nDstHeight,
                                            const gdiPalette* WINPR_RESTRICT palette)
{
	UINT32 suboffset = 0;

	if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, subcodecByteCount))
		return FALSE;

	while (suboffset < subcodecByteCount)
	{
		if ((subcodecByteCount - suboffset) < 13)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "subcodec header exceeds subcodecByteCount %" PRIu32 " at offset %" PRIu32
			           "",
			           subcodecByteCount, suboffset);
			return FALSE;
		}

		if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 13))
			return FALSE;

		const UINT16 xStart = Stream_Get_UINT16(s);
		const UINT16 yStart = Stream_Get_UINT16(s);
		const UINT16 width = Stream_Get_UINT16(s);
		const UINT16 height = Stream_Get_UINT16(s);
		const UINT32 bitmapDataByteCount = Stream_Get_UINT32(s);
		const UINT8 subcodecId = Stream_Get_UINT8(s);
		suboffset += 13;

		if ((subcodecByteCount - suboffset) < bitmapDataByteCount)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "bitmapDataByteCount %" PRIu32 " exceeds subcodecByteCount %" PRIu32
			           " at offset %" PRIu32 "",
			           bitmapDataByteCount, subcodecByteCount, suboffset);
			return FALSE;
		}

		if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, bitmapDataByteCount))
			return FALSE;

		if ((width == 0) || (height == 0))
		{
			WLog_Print(clear->log, WLOG_ERROR, "invalid raw subcodec dimensions %" PRIu16 "x%" PRIu16,
			           width, height);
			return FALSE;
		}

		const UINT32 nXDstRel = nXDst + xStart;
		const UINT32 nYDstRel = nYDst + yStart;
		if (1ull * nXDstRel + width > nDstWidth)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "nXDstRel %" PRIu32 " + width %" PRIu16 " > nDstWidth %" PRIu32 "", nXDstRel,
			           width, nDstWidth);
			return FALSE;
		}
		if (1ull * nYDstRel + height > nDstHeight)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "nYDstRel %" PRIu32 " + height %" PRIu16 " > nDstHeight %" PRIu32 "",
			           nYDstRel, height, nDstHeight);
			return FALSE;
		}

		if (1ull * xStart + width > nWidth)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "xStart %" PRIu16 " + width %" PRIu16 " > nWidth %" PRIu32 "", xStart, width,
			           nWidth);
			return FALSE;
		}
		if (1ull * yStart + height > nHeight)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "yStart %" PRIu16 " + height %" PRIu16 " > nHeight %" PRIu32 "", yStart,
			           height, nHeight);
			return FALSE;
		}

		if (!clear_resize_buffer(clear, width, height))
			return FALSE;

		switch (subcodecId)
		{
			case 0: /* Uncompressed */
			{
				const UINT32 nSrcStep = width * FreeRDPGetBytesPerPixel(PIXEL_FORMAT_BGR24);
				const size_t nSrcSize = 1ull * nSrcStep * height;

				if (bitmapDataByteCount != nSrcSize)
				{
					WLog_Print(clear->log, WLOG_ERROR,
					           "bitmapDataByteCount %" PRIu32 " != nSrcSize %" PRIuz "",
					           bitmapDataByteCount, nSrcSize);
					return FALSE;
				}

				if (!convert_color(pDstData, nDstStep, DstFormat, nXDstRel, nYDstRel, width, height,
				                   Stream_Pointer(s), nSrcStep, PIXEL_FORMAT_BGR24, nDstWidth,
				                   nDstHeight, palette))
					return FALSE;

				Stream_Seek(s, bitmapDataByteCount);
			}
			break;

			case 1: /* NSCodec */
				if (!clear_decompress_nscodec(clear->log, clear->nsc, width, height, s,
				                              bitmapDataByteCount, pDstData, DstFormat, nDstStep,
				                              nXDstRel, nYDstRel, nDstWidth, nDstHeight))
					return FALSE;

				break;

			case 2: /* CLEARCODEC_SUBCODEC_RLEX */
				if (!clear_decompress_subcode_rlex(clear->log, s, bitmapDataByteCount, width,
				                                   height, pDstData, DstFormat, nDstStep, nXDstRel,
				                                   nYDstRel, nDstWidth, nDstHeight))
					return FALSE;

				break;

			default:
				WLog_Print(clear->log, WLOG_ERROR, "Unknown subcodec ID %" PRIu8 "", subcodecId);
				return FALSE;
		}

		suboffset += bitmapDataByteCount;
	}

	return TRUE;
}

static BOOL resize_vbar_entry(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                              CLEAR_VBAR_ENTRY* WINPR_RESTRICT vBarEntry)
{
	if (vBarEntry->count > vBarEntry->size)
	{
		const UINT32 bpp = FreeRDPGetBytesPerPixel(clear->format);
		const UINT32 oldPos = vBarEntry->size * bpp;
		const UINT32 diffSize = (vBarEntry->count - vBarEntry->size) * bpp;

		BYTE* tmp =
		    (BYTE*)winpr_aligned_recalloc(vBarEntry->pixels, vBarEntry->count, 1ull * bpp, 32);

		if (!tmp)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "vBarEntry->pixels winpr_aligned_recalloc %" PRIu32 " failed",
			           vBarEntry->count * bpp);
			return FALSE;
		}

		memset(&tmp[oldPos], 0, diffSize);
		vBarEntry->pixels = tmp;
		vBarEntry->size = vBarEntry->count;
	}

	if (!vBarEntry->pixels && vBarEntry->size)
	{
		WLog_Print(clear->log, WLOG_ERROR,
		           "vBarEntry->pixels is nullptr but vBarEntry->size is %" PRIu32 "",
		           vBarEntry->size);
		return FALSE;
	}

	return TRUE;
}

static BOOL clear_decompress_bands_data(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                        wStream* WINPR_RESTRICT s, UINT32 bandsByteCount,
                                        UINT32 nWidth, UINT32 nHeight,
                                        BYTE* WINPR_RESTRICT pDstData, UINT32 DstFormat,
                                        UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                        UINT32 nDstWidth, UINT32 nDstHeight)
{
	UINT32 suboffset = 0;

	if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, bandsByteCount))
		return FALSE;

	while (suboffset < bandsByteCount)
	{
		BYTE cr = 0;
		BYTE cg = 0;
		BYTE cb = 0;
		UINT16 xStart = 0;
		UINT16 xEnd = 0;
		UINT16 yStart = 0;
		UINT16 yEnd = 0;
		UINT32 colorBkg = 0;
		UINT16 vBarHeader = 0;
		UINT16 vBarYOn = 0;
		UINT16 vBarYOff = 0;
		UINT32 vBarCount = 0;
		UINT32 vBarPixelCount = 0;
		UINT32 vBarShortPixelCount = 0;

		if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 11))
			return FALSE;

		Stream_Read_UINT16(s, xStart);
		Stream_Read_UINT16(s, xEnd);
		Stream_Read_UINT16(s, yStart);
		Stream_Read_UINT16(s, yEnd);
		Stream_Read_UINT8(s, cb);
		Stream_Read_UINT8(s, cg);
		Stream_Read_UINT8(s, cr);
		suboffset += 11;
		colorBkg = FreeRDPGetColor(clear->format, cr, cg, cb, 0xFF);

		if (xEnd < xStart)
		{
			WLog_Print(clear->log, WLOG_ERROR, "xEnd %" PRIu16 " < xStart %" PRIu16 "", xEnd,
			           xStart);
			return FALSE;
		}

		if (yEnd < yStart)
		{
			WLog_Print(clear->log, WLOG_ERROR, "yEnd %" PRIu16 " < yStart %" PRIu16 "", yEnd,
			           yStart);
			return FALSE;
		}

		vBarCount = (xEnd - xStart) + 1;

		for (UINT32 i = 0; i < vBarCount; i++)
		{
			UINT32 vBarHeight = 0;
			CLEAR_VBAR_ENTRY* vBarEntry = nullptr;
			CLEAR_VBAR_ENTRY* vBarShortEntry = nullptr;
			BOOL vBarUpdate = FALSE;
			const BYTE* cpSrcPixel = nullptr;

			if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 2))
				return FALSE;

			Stream_Read_UINT16(s, vBarHeader);
			suboffset += 2;
			vBarHeight = (yEnd - yStart + 1);

			if (vBarHeight > 52)
			{
				WLog_Print(clear->log, WLOG_ERROR, "vBarHeight (%" PRIu32 ") > 52", vBarHeight);
				return FALSE;
			}

			if ((vBarHeader & 0xC000) == 0x4000) /* SHORT_VBAR_CACHE_HIT */
			{
				const UINT16 vBarIndex = (vBarHeader & 0x3FFF);
				vBarShortEntry = &(clear->ShortVBarStorage[vBarIndex]);

				if (!vBarShortEntry)
				{
					WLog_Print(clear->log, WLOG_ERROR, "missing vBarShortEntry %" PRIu16 "",
					           vBarIndex);
					return FALSE;
				}

				if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 1))
					return FALSE;

				Stream_Read_UINT8(s, vBarYOn);
				suboffset += 1;
				vBarShortPixelCount = vBarShortEntry->count;
				vBarUpdate = TRUE;
			}
			else if ((vBarHeader & 0xC000) == 0x0000) /* SHORT_VBAR_CACHE_MISS */
			{
				vBarYOn = (vBarHeader & 0xFF);
				vBarYOff = ((vBarHeader >> 8) & 0x3F);

				if (vBarYOff < vBarYOn)
				{
					WLog_Print(clear->log, WLOG_ERROR, "vBarYOff %" PRIu16 " < vBarYOn %" PRIu16 "",
					           vBarYOff, vBarYOn);
					return FALSE;
				}

				vBarShortPixelCount = (vBarYOff - vBarYOn);

				if (vBarShortPixelCount > 52)
				{
					WLog_Print(clear->log, WLOG_ERROR, "vBarShortPixelCount %" PRIu32 " > 52",
					           vBarShortPixelCount);
					return FALSE;
				}

				if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(clear->log, s, vBarShortPixelCount,
				                                                3ull))
					return FALSE;

				if (clear->ShortVBarStorageCursor >= CLEARCODEC_VBAR_SHORT_SIZE)
				{
					WLog_Print(clear->log, WLOG_ERROR,
					           "clear->ShortVBarStorageCursor %" PRIu32
					           " >= CLEARCODEC_VBAR_SHORT_SIZE (%" PRId32 ")",
					           clear->ShortVBarStorageCursor, CLEARCODEC_VBAR_SHORT_SIZE);
					return FALSE;
				}

				vBarShortEntry = &(clear->ShortVBarStorage[clear->ShortVBarStorageCursor]);
				vBarShortEntry->count = vBarShortPixelCount;

				if (!resize_vbar_entry(clear, vBarShortEntry))
					return FALSE;

				for (size_t y = 0; y < vBarShortPixelCount; y++)
				{
					BYTE r = 0;
					BYTE g = 0;
					BYTE b = 0;
					BYTE* dstBuffer =
					    &vBarShortEntry->pixels[y * FreeRDPGetBytesPerPixel(clear->format)];
					UINT32 color = 0;
					Stream_Read_UINT8(s, b);
					Stream_Read_UINT8(s, g);
					Stream_Read_UINT8(s, r);
					color = FreeRDPGetColor(clear->format, r, g, b, 0xFF);

					if (!FreeRDPWriteColor(dstBuffer, clear->format, color))
						return FALSE;
				}

				suboffset += (vBarShortPixelCount * 3);
				clear->ShortVBarStorageCursor =
				    (clear->ShortVBarStorageCursor + 1) % CLEARCODEC_VBAR_SHORT_SIZE;
				vBarUpdate = TRUE;
			}
			else if ((vBarHeader & 0x8000) == 0x8000) /* VBAR_CACHE_HIT */
			{
				const UINT16 vBarIndex = (vBarHeader & 0x7FFF);
				vBarEntry = &(clear->VBarStorage[vBarIndex]);

				/* If the cache was reset we need to fill in some dummy data. */
				if (vBarEntry->size == 0)
				{
					WLog_Print(clear->log, WLOG_WARN,
					           "Empty cache index %" PRIu16 ", filling dummy data", vBarIndex);
					vBarEntry->count = vBarHeight;

					if (!resize_vbar_entry(clear, vBarEntry))
						return FALSE;
				}
			}
			else
			{
				WLog_Print(clear->log, WLOG_ERROR, "invalid vBarHeader 0x%04" PRIX16 "",
				           vBarHeader);
				return FALSE; /* invalid vBarHeader */
			}

			if (vBarUpdate)
			{
				BYTE* pSrcPixel = nullptr;
				BYTE* dstBuffer = nullptr;

				if (clear->VBarStorageCursor >= CLEARCODEC_VBAR_SIZE)
				{
					WLog_Print(clear->log, WLOG_ERROR,
					           "clear->VBarStorageCursor %" PRIu32
					           " >= CLEARCODEC_VBAR_SIZE %" PRId32 "",
					           clear->VBarStorageCursor, CLEARCODEC_VBAR_SIZE);
					return FALSE;
				}

				vBarEntry = &(clear->VBarStorage[clear->VBarStorageCursor]);
				vBarPixelCount = vBarHeight;
				vBarEntry->count = vBarPixelCount;

				if (!resize_vbar_entry(clear, vBarEntry))
					return FALSE;

				dstBuffer = vBarEntry->pixels;
				/* if (y < vBarYOn), use colorBkg */
				UINT32 y = 0;
				UINT32 count = vBarYOn;

				if ((y + count) > vBarPixelCount)
					count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

				if (count > 0)
				{
					while (count--)
					{
						FreeRDPWriteColor(dstBuffer, clear->format, colorBkg);
						dstBuffer += FreeRDPGetBytesPerPixel(clear->format);
					}
				}

				/*
				 * if ((y >= vBarYOn) && (y < (vBarYOn + vBarShortPixelCount))),
				 * use vBarShortPixels at index (y - shortVBarYOn)
				 */
				y = vBarYOn;
				count = vBarShortPixelCount;

				if ((y + count) > vBarPixelCount)
					count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

				if (count > 0)
				{
					const size_t offset =
					    (1ull * y - vBarYOn) * FreeRDPGetBytesPerPixel(clear->format);
					pSrcPixel = &vBarShortEntry->pixels[offset];
					if (offset + count > vBarShortEntry->count)
					{
						WLog_Print(clear->log, WLOG_ERROR,
						           "offset + count > vBarShortEntry->count");
						return FALSE;
					}
				}
				for (size_t x = 0; x < count; x++)
				{
					UINT32 color = 0;
					color = FreeRDPReadColor(&pSrcPixel[x * FreeRDPGetBytesPerPixel(clear->format)],
					                         clear->format);

					if (!FreeRDPWriteColor(dstBuffer, clear->format, color))
						return FALSE;

					dstBuffer += FreeRDPGetBytesPerPixel(clear->format);
				}

				/* if (y >= (vBarYOn + vBarShortPixelCount)), use colorBkg */
				y = vBarYOn + vBarShortPixelCount;
				count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

				if (count > 0)
				{
					while (count--)
					{
						if (!FreeRDPWriteColor(dstBuffer, clear->format, colorBkg))
							return FALSE;

						dstBuffer += FreeRDPGetBytesPerPixel(clear->format);
					}
				}

				vBarEntry->count = vBarPixelCount;
				clear->VBarStorageCursor = (clear->VBarStorageCursor + 1) % CLEARCODEC_VBAR_SIZE;
			}

			if (vBarEntry->count != vBarHeight)
			{
				WLog_Print(clear->log, WLOG_ERROR,
				           "vBarEntry->count %" PRIu32 " != vBarHeight %" PRIu32 "",
				           vBarEntry->count, vBarHeight);
				vBarEntry->count = vBarHeight;

				if (!resize_vbar_entry(clear, vBarEntry))
					return FALSE;
			}

			const UINT32 nXDstRel = nXDst + xStart;
			const UINT32 nYDstRel = nYDst + yStart;
			cpSrcPixel = vBarEntry->pixels;

			if (i < nWidth)
			{
				UINT32 count = vBarEntry->count;

				if (count > nHeight)
					count = nHeight;

				if (nXDstRel + i >= nDstWidth)
					return FALSE;

				for (UINT32 y = 0; y < count; y++)
				{
					if (nYDstRel + y >= nDstHeight)
						return FALSE;

					BYTE* pDstPixel8 =
					    &pDstData[((nYDstRel + y) * nDstStep) +
					              ((nXDstRel + i) * FreeRDPGetBytesPerPixel(DstFormat))];
					UINT32 color = FreeRDPReadColor(cpSrcPixel, clear->format);
					color = FreeRDPConvertColor(color, clear->format, DstFormat, nullptr);

					if (!FreeRDPWriteColor(pDstPixel8, DstFormat, color))
						return FALSE;

					cpSrcPixel += FreeRDPGetBytesPerPixel(clear->format);
				}
			}
		}
	}

	return TRUE;
}

static BOOL clear_decompress_glyph_data(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                        wStream* WINPR_RESTRICT s, UINT32 glyphFlags, UINT32 nWidth,
                                        UINT32 nHeight, BYTE* WINPR_RESTRICT pDstData,
                                        UINT32 DstFormat, UINT32 nDstStep, UINT32 nXDst,
                                        UINT32 nYDst, UINT32 nDstWidth, UINT32 nDstHeight,
                                        const gdiPalette* WINPR_RESTRICT palette,
                                        BYTE** WINPR_RESTRICT ppGlyphData)
{
	UINT16 glyphIndex = 0;

	if (ppGlyphData)
		*ppGlyphData = nullptr;

	if ((glyphFlags & CLEARCODEC_FLAG_GLYPH_HIT) && !(glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX))
	{
		WLog_Print(clear->log, WLOG_ERROR, "Invalid glyph flags %08" PRIX32 "", glyphFlags);
		return FALSE;
	}

	if ((glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX) == 0)
		return TRUE;

	if ((nWidth * nHeight) > (1024 * 1024))
	{
		WLog_Print(clear->log, WLOG_ERROR, "glyph too large: %" PRIu32 "x%" PRIu32 "", nWidth,
		           nHeight);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, glyphIndex);

	if (glyphIndex >= 4000)
	{
		WLog_Print(clear->log, WLOG_ERROR, "Invalid glyphIndex %" PRIu16 "", glyphIndex);
		return FALSE;
	}

	if (glyphFlags & CLEARCODEC_FLAG_GLYPH_HIT)
	{
		UINT32 nSrcStep = 0;
		CLEAR_GLYPH_ENTRY* glyphEntry = &(clear->GlyphCache[glyphIndex]);
		BYTE* glyphData = nullptr;

		if (!glyphEntry)
		{
			WLog_Print(clear->log, WLOG_ERROR, "clear->GlyphCache[%" PRIu16 "]=nullptr",
			           glyphIndex);
			return FALSE;
		}

		glyphData = (BYTE*)glyphEntry->pixels;

		if (!glyphData)
		{
			WLog_Print(clear->log, WLOG_ERROR, "clear->GlyphCache[%" PRIu16 "]->pixels=nullptr",
			           glyphIndex);
			return FALSE;
		}

		if ((nWidth * nHeight) > glyphEntry->count)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "(nWidth %" PRIu32 " * nHeight %" PRIu32 ") > glyphEntry->count %" PRIu32 "",
			           nWidth, nHeight, glyphEntry->count);
			return FALSE;
		}

		nSrcStep = nWidth * FreeRDPGetBytesPerPixel(clear->format);
		return convert_color(pDstData, nDstStep, DstFormat, nXDst, nYDst, nWidth, nHeight,
		                     glyphData, nSrcStep, clear->format, nDstWidth, nDstHeight, palette);
	}

	if (glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX)
	{
		const UINT32 bpp = FreeRDPGetBytesPerPixel(clear->format);
		CLEAR_GLYPH_ENTRY* glyphEntry = &(clear->GlyphCache[glyphIndex]);
		const size_t count = 1ull * nWidth * nHeight;
		const size_t hlimit = SIZE_MAX / ((nWidth > 0) ? nWidth : 1);
		if ((nWidth == 0) || (nHeight == 0) || (hlimit < nHeight))
		{
			const char* exceeded = (hlimit < nHeight) ? "within" : "outside";
			WLog_Print(clear->log, WLOG_ERROR,
			           "CLEARCODEC_FLAG_GLYPH_INDEX: nWidth=%" PRIu32 ", nHeight=%" PRIu32
			           ", nWidth * nHeight is %s allowed range",
			           nWidth, nHeight, exceeded);
			return FALSE;
		}

		if (count > glyphEntry->size)
		{
			BYTE* tmp = winpr_aligned_recalloc(glyphEntry->pixels, count, 1ull * bpp, 32);

			if (!tmp)
			{
				WLog_Print(clear->log, WLOG_ERROR,
				           "glyphEntry->pixels winpr_aligned_recalloc %" PRIuz " failed!",
				           count * bpp);
				return FALSE;
			}

			glyphEntry->count = WINPR_ASSERTING_INT_CAST(UINT32, count);
			glyphEntry->size = glyphEntry->count;
			glyphEntry->pixels = (UINT32*)tmp;
		}

		if (!glyphEntry->pixels)
		{
			WLog_Print(clear->log, WLOG_ERROR, "glyphEntry->pixels=nullptr");
			return FALSE;
		}

		if (ppGlyphData)
			*ppGlyphData = (BYTE*)glyphEntry->pixels;

		return TRUE;
	}

	return TRUE;
}

static inline BOOL updateContextFormat(CLEAR_CONTEXT* WINPR_RESTRICT clear, UINT32 DstFormat,
                                       BOOL fromNew)
{
	if (!clear || !clear->nsc)
		return FALSE;

	if (!fromNew)
	{
		if (clear->formatSet)
		{
			if (!FreeRDPAreColorFormatsEqualNoAlpha(clear->format, DstFormat))
			{
				WLog_Print(
				    clear->log, WLOG_ERROR,
				    "Color format changed from %s to %s during decompression calls, usage error!",
				    FreeRDPGetColorFormatName(clear->format), FreeRDPGetColorFormatName(DstFormat));
				return FALSE;
			}
		}
		else
			clear->formatSet = TRUE;
	}
	clear->format = DstFormat;
	return nsc_context_set_parameters(clear->nsc, NSC_COLOR_FORMAT, DstFormat);
}

INT32 clear_decompress(CLEAR_CONTEXT* WINPR_RESTRICT clear, const BYTE* WINPR_RESTRICT pSrcData,
                       UINT32 SrcSize, UINT32 nWidth, UINT32 nHeight, BYTE* WINPR_RESTRICT pDstData,
                       UINT32 DstFormat, UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                       UINT32 nDstWidth, UINT32 nDstHeight,
                       const gdiPalette* WINPR_RESTRICT palette)
{
	INT32 rc = -1;
	BYTE seqNumber = 0;
	BYTE glyphFlags = 0;
	UINT32 residualByteCount = 0;
	UINT32 bandsByteCount = 0;
	UINT32 subcodecByteCount = 0;
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	BYTE* glyphData = nullptr;

	if (!pDstData)
		return -1002;

	if ((nDstWidth == 0) || (nDstHeight == 0))
		return -1022;

	if ((nWidth > 0xFFFF) || (nHeight > 0xFFFF))
		return -1004;

	if (nXDst > nDstWidth)
	{
		WLog_Print(clear->log, WLOG_WARN, "nXDst %" PRIu32 " > nDstWidth %" PRIu32, nXDst,
		           nDstWidth);
		return -1005;
	}

	if (nYDst > nDstHeight)
	{
		WLog_Print(clear->log, WLOG_WARN, "nYDst %" PRIu32 " > nDstHeight %" PRIu32, nYDst,
		           nDstHeight);
		return -1006;
	}

	wStream* s = Stream_StaticConstInit(&sbuffer, pSrcData, SrcSize);

	if (!s)
		return -2005;

	if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 2))
		goto fail;

	if (!updateContextFormat(clear, DstFormat, FALSE))
		goto fail;

	Stream_Read_UINT8(s, glyphFlags);
	Stream_Read_UINT8(s, seqNumber);

	if (!clear->seqNumber && seqNumber)
		clear->seqNumber = seqNumber;

	if (seqNumber != clear->seqNumber)
	{
		WLog_Print(clear->log, WLOG_ERROR, "Sequence number unexpected %" PRIu8 " - %" PRIu32 "",
		           seqNumber, clear->seqNumber);
		WLog_Print(clear->log, WLOG_ERROR, "seqNumber %" PRIu8 " != clear->seqNumber %" PRIu32 "",
		           seqNumber, clear->seqNumber);
		goto fail;
	}

	clear->seqNumber = (seqNumber + 1) % 256;

	if (glyphFlags & CLEARCODEC_FLAG_CACHE_RESET)
	{
		clear_reset_vbar_storage(clear, FALSE);
	}

	if (!clear_decompress_glyph_data(clear, s, glyphFlags, nWidth, nHeight, pDstData, DstFormat,
	                                 nDstStep, nXDst, nYDst, nDstWidth, nDstHeight, palette,
	                                 &glyphData))
	{
		WLog_Print(clear->log, WLOG_ERROR, "clear_decompress_glyph_data failed!");
		goto fail;
	}

	/* Read composition payload header parameters */
	if (Stream_GetRemainingLength(s) < 12)
	{
		const UINT32 mask = (CLEARCODEC_FLAG_GLYPH_HIT | CLEARCODEC_FLAG_GLYPH_INDEX);

		if ((glyphFlags & mask) == mask)
			goto finish;

		WLog_Print(clear->log, WLOG_ERROR,
		           "invalid glyphFlags, missing flags: 0x%02" PRIx8 " & 0x%02" PRIx32
		           " == 0x%02" PRIx32,
		           glyphFlags, mask, glyphFlags & mask);
		goto fail;
	}

	Stream_Read_UINT32(s, residualByteCount);
	Stream_Read_UINT32(s, bandsByteCount);
	Stream_Read_UINT32(s, subcodecByteCount);

	if ((residualByteCount > 0) && (subcodecByteCount > 0))
	{
		WLog_Print(clear->log, WLOG_ERROR,
		           "residualByteCount %" PRIu32 " overlaps subcodecByteCount %" PRIu32 "",
		           residualByteCount, subcodecByteCount);
		goto fail;
	}

	if (residualByteCount > 0)
	{
		if (!clear_decompress_residual_data(clear, s, residualByteCount, nWidth, nHeight, pDstData,
		                                    DstFormat, nDstStep, nXDst, nYDst, nDstWidth,
		                                    nDstHeight, palette))
		{
			WLog_Print(clear->log, WLOG_ERROR, "clear_decompress_residual_data failed!");
			goto fail;
		}
	}

	if (bandsByteCount > 0)
	{
		if (!clear_decompress_bands_data(clear, s, bandsByteCount, nWidth, nHeight, pDstData,
		                                 DstFormat, nDstStep, nXDst, nYDst, nDstWidth, nDstHeight))
		{
			WLog_Print(clear->log, WLOG_ERROR, "clear_decompress_bands_data failed!");
			goto fail;
		}
	}

	if (subcodecByteCount > 0)
	{
		if (!clear_decompress_subcodecs_data(clear, s, subcodecByteCount, nWidth, nHeight, pDstData,
		                                     DstFormat, nDstStep, nXDst, nYDst, nDstWidth,
		                                     nDstHeight, palette))
		{
			WLog_Print(clear->log, WLOG_ERROR, "clear_decompress_subcodecs_data failed!");
			goto fail;
		}
	}

	if (glyphData)
	{
		uint32_t w = MIN(nWidth, nDstWidth);
		if (nXDst > nDstWidth)
		{
			WLog_Print(clear->log, WLOG_WARN,
			           "glyphData copy area x exceeds destination: x=%" PRIu32 " > %" PRIu32, nXDst,
			           nDstWidth);
			w = 0;
		}
		else if (nXDst + w > nDstWidth)
		{
			WLog_Print(clear->log, WLOG_WARN,
			           "glyphData copy area x + width exceeds destination: x=%" PRIu32 " + %" PRIu32
			           " > %" PRIu32,
			           nXDst, w, nDstWidth);
			w = nDstWidth - nXDst;
		}

		if (w != nWidth)
		{
			WLog_Print(clear->log, WLOG_WARN,
			           "glyphData copy area width truncated: requested=%" PRIu32
			           ", truncated to %" PRIu32,
			           nWidth, w);
		}

		uint32_t h = MIN(nHeight, nDstHeight);
		if (nYDst > nDstHeight)
		{
			WLog_Print(clear->log, WLOG_WARN,
			           "glyphData copy area y exceeds destination: y=%" PRIu32 " > %" PRIu32, nYDst,
			           nDstHeight);
			h = 0;
		}
		else if (nYDst + h > nDstHeight)
		{
			WLog_Print(clear->log, WLOG_WARN,
			           "glyphData copy area y + height exceeds destination: x=%" PRIu32
			           " + %" PRIu32 " > %" PRIu32,
			           nYDst, h, nDstHeight);
			h = nDstHeight - nYDst;
		}

		if (h != nHeight)
		{
			WLog_Print(clear->log, WLOG_WARN,
			           "glyphData copy area height truncated: requested=%" PRIu32
			           ", truncated to %" PRIu32,
			           nHeight, h);
		}

		if (!freerdp_image_copy_no_overlap(glyphData, clear->format, 0, 0, 0, w, h, pDstData,
		                                   DstFormat, nDstStep, nXDst, nYDst, palette,
		                                   FREERDP_KEEP_DST_ALPHA))
			goto fail;
	}

finish:
	rc = 0;
fail:
	return rc;
}

#if !defined(WITHOUT_FREERDP_3x_DEPRECATED)
int clear_compress(WINPR_ATTR_UNUSED CLEAR_CONTEXT* WINPR_RESTRICT clear,
                   WINPR_ATTR_UNUSED const BYTE* WINPR_RESTRICT pSrcData,
                   WINPR_ATTR_UNUSED UINT32 SrcSize,
                   WINPR_ATTR_UNUSED BYTE** WINPR_RESTRICT ppDstData,
                   WINPR_ATTR_UNUSED UINT32* WINPR_RESTRICT pDstSize)
{
	WLog_Print(clear->log, WLOG_ERROR, "TODO: not implemented!");
	return 1;
}
#endif

static BOOL clear_encode_write_run(BYTE* WINPR_RESTRICT dst, size_t capacity,
                                   size_t* WINPR_RESTRICT offset, BYTE r, BYTE g, BYTE b,
                                   UINT32 runLength)
{
	if (!dst || !offset || (runLength == 0))
		return FALSE;

	const size_t required =
	    (runLength < 0xFF) ? 4 : ((runLength < 0xFFFF) ? 6 : 10);
	if (*offset > capacity || required > capacity - *offset)
		return FALSE;

	dst[(*offset)++] = b;
	dst[(*offset)++] = g;
	dst[(*offset)++] = r;

	if (runLength < 0xFF)
	{
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, runLength);
	}
	else if (runLength < 0xFFFF)
	{
		dst[(*offset)++] = 0xFF;
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, runLength & 0xFF);
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, (runLength >> 8) & 0xFF);
	}
	else
	{
		dst[(*offset)++] = 0xFF;
		dst[(*offset)++] = 0xFF;
		dst[(*offset)++] = 0xFF;
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, runLength & 0xFF);
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, (runLength >> 8) & 0xFF);
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, (runLength >> 16) & 0xFF);
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, (runLength >> 24) & 0xFF);
	}

	return TRUE;
}

static BOOL clear_encode_write_run_factor(BYTE* WINPR_RESTRICT dst, size_t capacity,
                                          size_t* WINPR_RESTRICT offset, UINT32 runLength)
{
	if (!dst || !offset)
		return FALSE;

	const size_t required = (runLength < 0xFF) ? 1 : ((runLength < 0xFFFF) ? 3 : 7);
	if ((*offset > capacity) || (required > capacity - *offset))
		return FALSE;

	if (runLength < 0xFF)
	{
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, runLength);
	}
	else if (runLength < 0xFFFF)
	{
		dst[(*offset)++] = 0xFF;
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, runLength & 0xFF);
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, (runLength >> 8) & 0xFF);
	}
	else
	{
		dst[(*offset)++] = 0xFF;
		dst[(*offset)++] = 0xFF;
		dst[(*offset)++] = 0xFF;
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, runLength & 0xFF);
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, (runLength >> 8) & 0xFF);
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, (runLength >> 16) & 0xFF);
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, (runLength >> 24) & 0xFF);
	}

	return TRUE;
}

static void clear_encode_read_bgr(const BYTE* WINPR_RESTRICT src, UINT32 SrcFormat,
                                  const gdiPalette* WINPR_RESTRICT palette,
                                  BYTE* WINPR_RESTRICT r, BYTE* WINPR_RESTRICT g,
                                  BYTE* WINPR_RESTRICT b)
{
	BYTE a = 0;
	const UINT32 color = FreeRDPReadColor(src, SrcFormat);

	FreeRDPSplitColor(color, SrcFormat, r, g, b, &a, palette);
}

static BOOL clear_encode_residual(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                  const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                  UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth,
                                  UINT32 nHeight, BYTE* WINPR_RESTRICT dst, size_t capacity,
                                  size_t* WINPR_RESTRICT offset,
                                  const gdiPalette* WINPR_RESTRICT palette)
{
	WINPR_ASSERT(clear);
	WINPR_ASSERT(pSrcData);
	WINPR_ASSERT(dst);
	WINPR_ASSERT(offset);

	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);
	BYTE runR = 0;
	BYTE runG = 0;
	BYTE runB = 0;
	UINT32 runLength = 0;

	for (UINT32 y = 0; y < nHeight; y++)
	{
		const BYTE* line = &pSrcData[(1ULL * (SrcY + y) * SrcStep) + (1ULL * SrcX * bpp)];

		for (UINT32 x = 0; x < nWidth; x++)
		{
			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;

			clear_encode_read_bgr(&line[1ULL * x * bpp], SrcFormat, palette, &r, &g, &b);

			if ((runLength > 0) && (r == runR) && (g == runG) && (b == runB) &&
			    (runLength < UINT32_MAX))
			{
				runLength++;
				continue;
			}

			if (runLength > 0)
			{
				if (!clear_encode_write_run(dst, capacity, offset, runR, runG, runB, runLength))
					return FALSE;
			}

			runR = r;
			runG = g;
			runB = b;
			runLength = 1;
		}
	}

	return clear_encode_write_run(dst, capacity, offset, runR, runG, runB, runLength);
}

static BOOL clear_encode_write_uint16(BYTE* WINPR_RESTRICT dst, size_t capacity,
                                      size_t* WINPR_RESTRICT offset, UINT16 value)
{
	if (!dst || !offset || (*offset > capacity) || (capacity - *offset < 2))
		return FALSE;

	dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, value & 0xFF);
	dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, (value >> 8) & 0xFF);
	return TRUE;
}

static BOOL clear_encode_write_uint32(BYTE* WINPR_RESTRICT dst, size_t capacity,
                                      size_t* WINPR_RESTRICT offset, UINT32 value)
{
	if (!dst || !offset || (*offset > capacity) || (capacity - *offset < 4))
		return FALSE;

	dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, value & 0xFF);
	dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, (value >> 8) & 0xFF);
	dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, (value >> 16) & 0xFF);
	dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, (value >> 24) & 0xFF);
	return TRUE;
}

static BOOL clear_encode_raw_subcodec(const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                       UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth,
                                       UINT32 nHeight, BYTE* WINPR_RESTRICT dst, size_t capacity,
                                       size_t* WINPR_RESTRICT offset,
                                       const gdiPalette* WINPR_RESTRICT palette)
{
	WINPR_ASSERT(pSrcData);
	WINPR_ASSERT(dst);
	WINPR_ASSERT(offset);

	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);
	const UINT32 bitmapDataByteCount = nWidth * nHeight * 3;

	if (!clear_encode_write_uint16(dst, capacity, offset, 0))
		return FALSE;
	if (!clear_encode_write_uint16(dst, capacity, offset, 0))
		return FALSE;
	if (!clear_encode_write_uint16(dst, capacity, offset, WINPR_ASSERTING_INT_CAST(UINT16, nWidth)))
		return FALSE;
	if (!clear_encode_write_uint16(dst, capacity, offset, WINPR_ASSERTING_INT_CAST(UINT16, nHeight)))
		return FALSE;
	if (!clear_encode_write_uint32(dst, capacity, offset, bitmapDataByteCount))
		return FALSE;

	if ((*offset > capacity) || (capacity - *offset < 1))
		return FALSE;
	dst[(*offset)++] = 0; /* Uncompressed raw BGR24 subcodec. */

	if ((*offset > capacity) || (capacity - *offset < bitmapDataByteCount))
		return FALSE;

	for (UINT32 y = 0; y < nHeight; y++)
	{
		const BYTE* line = &pSrcData[(1ULL * (SrcY + y) * SrcStep) + (1ULL * SrcX * bpp)];

		for (UINT32 x = 0; x < nWidth; x++)
		{
			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;

			clear_encode_read_bgr(&line[1ULL * x * bpp], SrcFormat, palette, &r, &g, &b);
			dst[(*offset)++] = b;
			dst[(*offset)++] = g;
			dst[(*offset)++] = r;
		}
	}

	return TRUE;
}

static UINT32 clear_encode_normalized_color(const BYTE* WINPR_RESTRICT src, UINT32 SrcFormat,
                                            const gdiPalette* WINPR_RESTRICT palette);

static BOOL clear_encode_vbar_matches(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                       const CLEAR_VBAR_ENTRY* WINPR_RESTRICT entry,
                                       const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                       UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 x,
                                       UINT32 nHeight, const gdiPalette* WINPR_RESTRICT palette)
{
	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);

	if (!entry || !entry->pixels || (entry->count != nHeight))
		return FALSE;

	for (UINT32 y = 0; y < nHeight; y++)
	{
		const BYTE* srcPixel =
		    &pSrcData[(1ULL * (SrcY + y) * SrcStep) + (1ULL * (SrcX + x) * bpp)];
		const UINT32 srcColor = clear_encode_normalized_color(srcPixel, SrcFormat, palette);
		const UINT32 cachedColor =
		    FreeRDPReadColor(&entry->pixels[1ULL * y * FreeRDPGetBytesPerPixel(clear->format)],
		                     clear->format);

		if (srcColor != cachedColor)
			return FALSE;
	}

	return TRUE;
}

static BOOL clear_encode_find_vbar(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                   const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                   UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 x,
                                   UINT32 nHeight, const gdiPalette* WINPR_RESTRICT palette,
                                   UINT16* WINPR_RESTRICT vBarIndex)
{
	const UINT32 count = clear->VBarStorageCount;
	if (count > 4096)
		return FALSE;

	for (UINT32 index = 0; index < count; index++)
	{
		if (clear_encode_vbar_matches(clear, &clear->VBarStorage[index], pSrcData, SrcFormat,
		                              SrcStep, SrcX, SrcY, x, nHeight, palette))
		{
			*vBarIndex = WINPR_ASSERTING_INT_CAST(UINT16, index);
			return TRUE;
		}
	}

	return FALSE;
}

static BOOL clear_encode_store_vbar(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                    const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                    UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 x,
                                    UINT32 nHeight, const gdiPalette* WINPR_RESTRICT palette,
                                    UINT16* WINPR_RESTRICT vBarIndex)
{
	WINPR_ASSERT(clear->VBarStorageCursor < CLEARCODEC_VBAR_SIZE);

	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);
	CLEAR_VBAR_ENTRY* entry = &clear->VBarStorage[clear->VBarStorageCursor];
	entry->count = nHeight;

	if (!resize_vbar_entry(clear, entry))
		return FALSE;

	for (UINT32 y = 0; y < nHeight; y++)
	{
		BYTE* dstPixel = &entry->pixels[1ULL * y * FreeRDPGetBytesPerPixel(clear->format)];
		const BYTE* srcPixel =
		    &pSrcData[(1ULL * (SrcY + y) * SrcStep) + (1ULL * (SrcX + x) * bpp)];
		const UINT32 color = clear_encode_normalized_color(srcPixel, SrcFormat, palette);

		if (!FreeRDPWriteColor(dstPixel, clear->format, color))
			return FALSE;
	}

	*vBarIndex = WINPR_ASSERTING_INT_CAST(UINT16, clear->VBarStorageCursor);
	clear->VBarStorageCursor = (clear->VBarStorageCursor + 1) % CLEARCODEC_VBAR_SIZE;
	if (clear->VBarStorageCount < CLEARCODEC_VBAR_SIZE)
		clear->VBarStorageCount++;
	return TRUE;
}

static BOOL clear_encode_store_short_vbar(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                          const CLEAR_VBAR_ENTRY* WINPR_RESTRICT vBarEntry,
                                          UINT32 nHeight)
{
	WINPR_ASSERT(clear->ShortVBarStorageCursor < CLEARCODEC_VBAR_SHORT_SIZE);

	CLEAR_VBAR_ENTRY* shortEntry = &clear->ShortVBarStorage[clear->ShortVBarStorageCursor];
	shortEntry->count = nHeight;

	if (!resize_vbar_entry(clear, shortEntry))
		return FALSE;

	CopyMemory(shortEntry->pixels, vBarEntry->pixels,
	           1ULL * nHeight * FreeRDPGetBytesPerPixel(clear->format));
	clear->ShortVBarStorageCursor =
	    (clear->ShortVBarStorageCursor + 1) % CLEARCODEC_VBAR_SHORT_SIZE;
	if (clear->ShortVBarStorageCount < CLEARCODEC_VBAR_SHORT_SIZE)
		clear->ShortVBarStorageCount++;
	return TRUE;
}

static BOOL clear_encode_bands(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                               const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                               UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth,
                               UINT32 nHeight, BYTE* WINPR_RESTRICT dst, size_t capacity,
                               size_t* WINPR_RESTRICT offset,
                               const gdiPalette* WINPR_RESTRICT palette)
{
	WINPR_ASSERT(clear);
	WINPR_ASSERT(pSrcData);
	WINPR_ASSERT(dst);
	WINPR_ASSERT(offset);

	if (nHeight > 52)
		return FALSE;

	if (!clear_encode_write_uint16(dst, capacity, offset, 0))
		return FALSE;
	if (!clear_encode_write_uint16(dst, capacity, offset,
	                               WINPR_ASSERTING_INT_CAST(UINT16, nWidth - 1)))
		return FALSE;
	if (!clear_encode_write_uint16(dst, capacity, offset, 0))
		return FALSE;
	if (!clear_encode_write_uint16(dst, capacity, offset,
	                               WINPR_ASSERTING_INT_CAST(UINT16, nHeight - 1)))
		return FALSE;

	if ((*offset > capacity) || (capacity - *offset < 3))
		return FALSE;
	dst[(*offset)++] = 0;
	dst[(*offset)++] = 0;
	dst[(*offset)++] = 0;

	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);
	const UINT32 previousBase =
	    (clear->VBarStorageCursor + CLEARCODEC_VBAR_SIZE - nWidth) % CLEARCODEC_VBAR_SIZE;
	for (UINT32 x = 0; x < nWidth; x++)
	{
		const UINT16 previousIndex =
		    WINPR_ASSERTING_INT_CAST(UINT16, (previousBase + x) % CLEARCODEC_VBAR_SIZE);
		if (!clear->VBarResetPending &&
		    clear_encode_vbar_matches(clear, &clear->VBarStorage[previousIndex], pSrcData,
		                              SrcFormat, SrcStep, SrcX, SrcY, x, nHeight, palette))
		{
			if (!clear_encode_write_uint16(dst, capacity, offset, 0x8000 | previousIndex))
				return FALSE;
			continue;
		}

		if (!clear_encode_write_uint16(dst, capacity, offset,
		                               WINPR_ASSERTING_INT_CAST(UINT16, nHeight << 8)))
			return FALSE;
		if ((*offset > capacity) || (capacity - *offset < 1ULL * nHeight * 3ULL))
			return FALSE;

		for (UINT32 y = 0; y < nHeight; y++)
		{
			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;
			const BYTE* srcPixel =
			    &pSrcData[(1ULL * (SrcY + y) * SrcStep) + (1ULL * (SrcX + x) * bpp)];

			clear_encode_read_bgr(srcPixel, SrcFormat, palette, &r, &g, &b);
			dst[(*offset)++] = b;
			dst[(*offset)++] = g;
			dst[(*offset)++] = r;
		}

		UINT16 vBarIndex = 0;
		if (!clear_encode_store_vbar(clear, pSrcData, SrcFormat, SrcStep, SrcX, SrcY, x, nHeight,
		                             palette, &vBarIndex))
			return FALSE;
		if (!clear_encode_store_short_vbar(clear, &clear->VBarStorage[vBarIndex], nHeight))
			return FALSE;
	}

	return TRUE;
}

static BOOL clear_encode_bands_size(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                    const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                    UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth,
                                    UINT32 nHeight, const gdiPalette* WINPR_RESTRICT palette,
                                    size_t* WINPR_RESTRICT size)
{
	WINPR_ASSERT(clear);
	WINPR_ASSERT(size);

	if ((nHeight > 52) || (nWidth < 32))
		return FALSE;

	size_t candidate = 11;
	const UINT32 previousBase =
	    (clear->VBarStorageCursor + CLEARCODEC_VBAR_SIZE - nWidth) % CLEARCODEC_VBAR_SIZE;
	for (UINT32 x = 0; x < nWidth; x++)
	{
		const UINT16 previousIndex =
		    WINPR_ASSERTING_INT_CAST(UINT16, (previousBase + x) % CLEARCODEC_VBAR_SIZE);
		if (!clear->VBarResetPending &&
		    clear_encode_vbar_matches(clear, &clear->VBarStorage[previousIndex], pSrcData,
		                              SrcFormat, SrcStep, SrcX, SrcY, x, nHeight, palette))
			candidate += 2;
		else
			candidate += 2ULL + 3ULL * nHeight;
	}

	*size = candidate;
	return TRUE;
}

static BOOL clear_encode_find_palette_index(const CLEAR_BGR* WINPR_RESTRICT palette,
                                            UINT32 paletteCount, BYTE r, BYTE g, BYTE b,
                                            BYTE* WINPR_RESTRICT index)
{
	for (UINT32 x = 0; x < paletteCount; x++)
	{
		if ((palette[x].r == r) && (palette[x].g == g) && (palette[x].b == b))
		{
			*index = WINPR_ASSERTING_INT_CAST(BYTE, x);
			return TRUE;
		}
	}

	return FALSE;
}

static BOOL clear_encode_rlex_subcodec(const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                       UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth,
                                       UINT32 nHeight, BYTE* WINPR_RESTRICT dst, size_t capacity,
                                       size_t* WINPR_RESTRICT offset,
                                       const gdiPalette* WINPR_RESTRICT palette)
{
	WINPR_ASSERT(pSrcData);
	WINPR_ASSERT(dst);
	WINPR_ASSERT(offset);

	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);
	const UINT32 pixelCount = nWidth * nHeight;
	CLEAR_BGR colors[127] = { 0 };
	UINT32 colorCount = 0;
	BYTE* indexes = calloc(pixelCount, sizeof(BYTE));
	BOOL rc = FALSE;

	if (!indexes)
		return FALSE;

	for (UINT32 y = 0; y < nHeight; y++)
	{
		const BYTE* line = &pSrcData[(1ULL * (SrcY + y) * SrcStep) + (1ULL * SrcX * bpp)];

		for (UINT32 x = 0; x < nWidth; x++)
		{
			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;
			BYTE index = 0;
			const UINT32 pixelIndex = y * nWidth + x;

			clear_encode_read_bgr(&line[1ULL * x * bpp], SrcFormat, palette, &r, &g, &b);

			if (!clear_encode_find_palette_index(colors, colorCount, r, g, b, &index))
			{
				if (colorCount >= ARRAYSIZE(colors))
					goto fail;

				index = WINPR_ASSERTING_INT_CAST(BYTE, colorCount);
				colors[colorCount].r = r;
				colors[colorCount].g = g;
				colors[colorCount].b = b;
				colorCount++;
			}

			indexes[pixelIndex] = index;
		}
	}

	if ((colorCount == 0) || (colorCount > 127))
		goto fail;

	if (!clear_encode_write_uint16(dst, capacity, offset, 0))
		goto fail;
	if (!clear_encode_write_uint16(dst, capacity, offset, 0))
		goto fail;
	if (!clear_encode_write_uint16(dst, capacity, offset, WINPR_ASSERTING_INT_CAST(UINT16, nWidth)))
		goto fail;
	if (!clear_encode_write_uint16(dst, capacity, offset, WINPR_ASSERTING_INT_CAST(UINT16, nHeight)))
		goto fail;

	const size_t bitmapCountOffset = *offset;
	if (!clear_encode_write_uint32(dst, capacity, offset, 0))
		goto fail;

	if ((*offset > capacity) || (capacity - *offset < 1))
		goto fail;
	dst[(*offset)++] = 2; /* CLEARCODEC_SUBCODEC_RLEX */

	const size_t rlexStart = *offset;
	if ((*offset > capacity) || (capacity - *offset < (1ULL + 3ULL * colorCount)))
		goto fail;

	dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, colorCount);
	for (UINT32 x = 0; x < colorCount; x++)
	{
		dst[(*offset)++] = colors[x].b;
		dst[(*offset)++] = colors[x].g;
		dst[(*offset)++] = colors[x].r;
	}

	const UINT32 numBits = CLEAR_LOG2_FLOOR[colorCount - 1] + 1;
	const UINT32 maxSuiteDepth = (1U << (8 - numBits)) - 1U;
	UINT32 pixelIndex = 0;

	while (pixelIndex < pixelCount)
	{
		const BYTE startIndex = indexes[pixelIndex];
		UINT32 runLength = 0;
		UINT32 suiteDepth = 0;
		UINT32 consume = 1;

		while ((pixelIndex + consume < pixelCount) &&
		       (indexes[pixelIndex + consume] == startIndex))
			consume++;

		if (consume > 1)
		{
			runLength = consume - 1;
			suiteDepth = 0;
		}
		else
		{
			while ((suiteDepth < maxSuiteDepth) && (pixelIndex + suiteDepth + 1 < pixelCount) &&
			       ((UINT32)startIndex + suiteDepth + 1 < colorCount) &&
			       (indexes[pixelIndex + suiteDepth + 1] ==
			        (BYTE)(startIndex + suiteDepth + 1)))
			{
				suiteDepth++;
			}
			consume = suiteDepth + 1;
		}

		const BYTE stopIndex = WINPR_ASSERTING_INT_CAST(BYTE, startIndex + suiteDepth);
		const BYTE tmp = WINPR_ASSERTING_INT_CAST(BYTE, (suiteDepth << numBits) | stopIndex);

		if ((*offset > capacity) || (capacity - *offset < 1))
			goto fail;
		dst[(*offset)++] = tmp;

		if (!clear_encode_write_run_factor(dst, capacity, offset, runLength))
			goto fail;

		pixelIndex += consume;
	}

	const size_t rlexSize = *offset - rlexStart;
	if (rlexSize > UINT32_MAX)
		goto fail;

	dst[bitmapCountOffset + 0] = WINPR_ASSERTING_INT_CAST(BYTE, rlexSize & 0xFF);
	dst[bitmapCountOffset + 1] = WINPR_ASSERTING_INT_CAST(BYTE, (rlexSize >> 8) & 0xFF);
	dst[bitmapCountOffset + 2] = WINPR_ASSERTING_INT_CAST(BYTE, (rlexSize >> 16) & 0xFF);
	dst[bitmapCountOffset + 3] = WINPR_ASSERTING_INT_CAST(BYTE, (rlexSize >> 24) & 0xFF);

	rc = TRUE;

fail:
	free(indexes);
	return rc;
}

static BOOL clear_encode_nsc_subcodec(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                      const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                      UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth,
                                      UINT32 nHeight, BYTE* WINPR_RESTRICT dst, size_t capacity,
                                      size_t* WINPR_RESTRICT offset)
{
	WINPR_ASSERT(clear);
	WINPR_ASSERT(pSrcData);
	WINPR_ASSERT(dst);
	WINPR_ASSERT(offset);

	if (!clear->nsc)
		return FALSE;

	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);
	const UINT64 pixelCount = 1ULL * nWidth * nHeight;
	const UINT64 streamCapacity = 4096ULL + pixelCount * 16ULL;
	const UINT32 verifyStep = nWidth * bpp;

	if (streamCapacity > SIZE_MAX)
		return FALSE;

	BYTE* input = calloc(nHeight, verifyStep);
	if (!input)
		return FALSE;

	for (UINT32 y = 0; y < nHeight; y++)
	{
		const BYTE* srcLine = &pSrcData[(1ULL * (SrcY + y) * SrcStep) + (1ULL * SrcX * bpp)];
		BYTE* dstLine = &input[1ULL * (nHeight - 1 - y) * verifyStep];
		CopyMemory(dstLine, srcLine, verifyStep);
	}

	wStream* s = Stream_New(nullptr, WINPR_ASSERTING_INT_CAST(size_t, streamCapacity));
	if (!s)
	{
		free(input);
		return FALSE;
	}

	BYTE* decoded = calloc(nHeight, verifyStep);
	if (!decoded)
	{
		free(input);
		Stream_Free(s, TRUE);
		return FALSE;
	}

	BOOL rc = FALSE;
	if (!nsc_context_set_parameters(clear->nsc, NSC_COLOR_FORMAT, SrcFormat))
		goto fail;
	if (!nsc_context_set_parameters(clear->nsc, NSC_COLOR_LOSS_LEVEL, 1))
		goto fail;
	if (!nsc_context_set_parameters(clear->nsc, NSC_ALLOW_SUBSAMPLING, 0))
		goto fail;

	if (!nsc_compose_message(clear->nsc, s, input, nWidth, nHeight, verifyStep))
		goto fail;

	const size_t nscSize = Stream_GetPosition(s);
	if ((nscSize == 0) || (nscSize > UINT32_MAX))
		goto fail;

	if (!nsc_process_message(clear->nsc, 32, nWidth, nHeight, Stream_Buffer(s),
	                         WINPR_ASSERTING_INT_CAST(UINT32, nscSize), decoded, SrcFormat,
	                         verifyStep, 0, 0, nWidth, nHeight, FREERDP_FLIP_NONE))
		goto fail;

	for (UINT32 y = 0; y < nHeight; y++)
	{
		const BYTE* srcLine = &pSrcData[(1ULL * (SrcY + y) * SrcStep) + (1ULL * SrcX * bpp)];
		const BYTE* decodedLine = &decoded[1ULL * y * verifyStep];

		for (UINT32 x = 0; x < nWidth; x++)
		{
			const UINT32 srcColor = FreeRDPReadColor(&srcLine[1ULL * x * bpp], SrcFormat);
			const UINT32 decodedColor = FreeRDPReadColor(&decodedLine[1ULL * x * bpp], SrcFormat);
			BYTE srcR = 0;
			BYTE srcG = 0;
			BYTE srcB = 0;
			BYTE srcA = 0;
			BYTE decodedR = 0;
			BYTE decodedG = 0;
			BYTE decodedB = 0;
			BYTE decodedA = 0;

			FreeRDPSplitColor(srcColor, SrcFormat, &srcR, &srcG, &srcB, &srcA, nullptr);
			FreeRDPSplitColor(decodedColor, SrcFormat, &decodedR, &decodedG, &decodedB, &decodedA,
			                  nullptr);

			const UINT32 diffR = (srcR > decodedR) ? (srcR - decodedR) : (decodedR - srcR);
			const UINT32 diffG = (srcG > decodedG) ? (srcG - decodedG) : (decodedG - srcG);
			const UINT32 diffB = (srcB > decodedB) ? (srcB - decodedB) : (decodedB - srcB);
			if ((diffR > clear->nscTolerance) || (diffG > clear->nscTolerance) ||
			    (diffB > clear->nscTolerance))
				goto fail;
		}
	}

	if (!nsc_context_set_parameters(clear->nsc, NSC_COLOR_FORMAT, SrcFormat))
		goto fail;
	if (!nsc_context_set_parameters(clear->nsc, NSC_COLOR_LOSS_LEVEL, 1))
		goto fail;
	if (!nsc_context_set_parameters(clear->nsc, NSC_ALLOW_SUBSAMPLING, 0))
		goto fail;

	if (!clear_encode_write_uint16(dst, capacity, offset, 0))
		goto fail;
	if (!clear_encode_write_uint16(dst, capacity, offset, 0))
		goto fail;
	if (!clear_encode_write_uint16(dst, capacity, offset, WINPR_ASSERTING_INT_CAST(UINT16, nWidth)))
		goto fail;
	if (!clear_encode_write_uint16(dst, capacity, offset,
	                               WINPR_ASSERTING_INT_CAST(UINT16, nHeight)))
		goto fail;
	if (!clear_encode_write_uint32(dst, capacity, offset,
	                               WINPR_ASSERTING_INT_CAST(UINT32, nscSize)))
		goto fail;

	if ((*offset > capacity) || (capacity - *offset < (1ULL + nscSize)))
		goto fail;

	dst[(*offset)++] = 1; /* CLEARCODEC_SUBCODEC_NS_CODEC */
	CopyMemory(&dst[*offset], Stream_Buffer(s), nscSize);
	*offset += nscSize;
	rc = TRUE;

fail:
	free(decoded);
	free(input);
	Stream_Free(s, TRUE);
	return rc;
}

static UINT32 clear_encode_normalized_color(const BYTE* WINPR_RESTRICT src, UINT32 SrcFormat,
                                            const gdiPalette* WINPR_RESTRICT palette)
{
	BYTE r = 0;
	BYTE g = 0;
	BYTE b = 0;

	clear_encode_read_bgr(src, SrcFormat, palette, &r, &g, &b);
	return FreeRDPGetColor(PIXEL_FORMAT_BGRX32, r, g, b, 0xFF);
}

static BOOL clear_encode_glyph_matches(const CLEAR_GLYPH_ENTRY* WINPR_RESTRICT entry,
                                       const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                       UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth,
                                       UINT32 nHeight,
                                       const gdiPalette* WINPR_RESTRICT palette)
{
	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);

	if (!entry || !entry->pixels || (entry->width != nWidth) || (entry->height != nHeight) ||
	    (entry->count != (nWidth * nHeight)))
		return FALSE;

	for (UINT32 y = 0; y < nHeight; y++)
	{
		const BYTE* line = &pSrcData[1ULL * (SrcY + y) * SrcStep + 1ULL * SrcX * bpp];

		for (UINT32 x = 0; x < nWidth; x++)
		{
			const UINT32 color =
			    clear_encode_normalized_color(&line[1ULL * x * bpp], SrcFormat, palette);
			if (entry->pixels[1ULL * y * nWidth + x] != color)
				return FALSE;
		}
	}

	return TRUE;
}

static BOOL clear_encode_find_glyph(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                    const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                    UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth,
                                    UINT32 nHeight, const gdiPalette* WINPR_RESTRICT palette,
                                    UINT16* WINPR_RESTRICT glyphIndex)
{
	for (size_t index = 0; index < ARRAYSIZE(clear->GlyphCache); index++)
	{
		if (clear_encode_glyph_matches(&clear->GlyphCache[index], pSrcData, SrcFormat, SrcStep,
		                               SrcX, SrcY, nWidth, nHeight, palette))
		{
			*glyphIndex = WINPR_ASSERTING_INT_CAST(UINT16, index);
			return TRUE;
		}
	}

	return FALSE;
}

static BOOL clear_encode_store_glyph(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                     const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                     UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth,
                                     UINT32 nHeight, const gdiPalette* WINPR_RESTRICT palette,
                                     UINT16 glyphIndex)
{
	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);
	const UINT32 count = nWidth * nHeight;
	CLEAR_GLYPH_ENTRY* entry = &clear->GlyphCache[glyphIndex];

	if (count > entry->size)
	{
		UINT32* tmp = winpr_aligned_recalloc(entry->pixels, count, sizeof(UINT32), 32);
		if (!tmp)
			return FALSE;

		entry->pixels = tmp;
		entry->size = count;
	}

	for (UINT32 y = 0; y < nHeight; y++)
	{
		const BYTE* line = &pSrcData[1ULL * (SrcY + y) * SrcStep + 1ULL * SrcX * bpp];

		for (UINT32 x = 0; x < nWidth; x++)
		{
			entry->pixels[1ULL * y * nWidth + x] =
			    clear_encode_normalized_color(&line[1ULL * x * bpp], SrcFormat, palette);
		}
	}

	entry->count = count;
	entry->width = nWidth;
	entry->height = nHeight;
	return TRUE;
}

typedef enum
{
	CLEAR_ENCODE_MODE_RESIDUAL = 0,
	CLEAR_ENCODE_MODE_RAW = 1,
	CLEAR_ENCODE_MODE_RLEX = 2,
	CLEAR_ENCODE_MODE_NSC = 3,
	CLEAR_ENCODE_MODE_BANDS = 4
} CLEAR_ENCODE_MODE;

typedef struct
{
	UINT64 pixelCount;
	UINT64 rawSubcodecSize;
	UINT64 maxSize;
	BOOL glyphEligible;
} CLEAR_ENCODE_LIMITS;

static BOOL clear_encode_patch_uint32(BYTE* WINPR_RESTRICT dst, size_t offset, size_t value)
{
	if (!dst || (value > UINT32_MAX))
		return FALSE;

	dst[offset + 0] = WINPR_ASSERTING_INT_CAST(BYTE, value & 0xFF);
	dst[offset + 1] = WINPR_ASSERTING_INT_CAST(BYTE, (value >> 8) & 0xFF);
	dst[offset + 2] = WINPR_ASSERTING_INT_CAST(BYTE, (value >> 16) & 0xFF);
	dst[offset + 3] = WINPR_ASSERTING_INT_CAST(BYTE, (value >> 24) & 0xFF);
	return TRUE;
}

static CLEAR_ENCODE_MODE clear_encode_select_mode(
    CLEAR_CONTEXT* WINPR_RESTRICT clear, const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
    UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth, UINT32 nHeight,
    BYTE* WINPR_RESTRICT dst, size_t capacity, size_t residualOffset, size_t residualSize,
    UINT64 rawSubcodecSize, const gdiPalette* WINPR_RESTRICT palette)
{
	size_t bestSize = residualSize;
	CLEAR_ENCODE_MODE mode = CLEAR_ENCODE_MODE_RESIDUAL;
	BOOL rlexAvailable = FALSE;
	size_t offset = residualOffset;

	if (clear_encode_rlex_subcodec(pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth, nHeight, dst,
	                               capacity, &offset, palette))
	{
		rlexAvailable = TRUE;
		const size_t rlexSubcodecSize = offset - residualOffset;
		if (rlexSubcodecSize < bestSize)
		{
			bestSize = rlexSubcodecSize;
			mode = CLEAR_ENCODE_MODE_RLEX;
		}
	}

	offset = residualOffset;
	if (!rlexAvailable &&
	    clear_encode_nsc_subcodec(clear, pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth, nHeight,
	                              dst, capacity, &offset))
	{
		const size_t nscSubcodecSize = offset - residualOffset;
		if (nscSubcodecSize < bestSize)
		{
			bestSize = nscSubcodecSize;
			mode = CLEAR_ENCODE_MODE_NSC;
		}
	}

	if (rawSubcodecSize < bestSize)
	{
		bestSize = WINPR_ASSERTING_INT_CAST(size_t, rawSubcodecSize);
		mode = CLEAR_ENCODE_MODE_RAW;
	}

	size_t bandsSize = 0;
	if ((mode != CLEAR_ENCODE_MODE_RLEX) &&
	    clear_encode_bands_size(clear, pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth, nHeight,
	                            palette, &bandsSize) &&
	    ((bandsSize < bestSize) || (nHeight == 52) || ((nWidth == 32) && (nHeight == 1))))
	{
		mode = CLEAR_ENCODE_MODE_BANDS;
	}

	return mode;
}

static BOOL clear_encode_emit_payload(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                      const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                      UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth,
                                      UINT32 nHeight, BYTE* WINPR_RESTRICT dst, size_t capacity,
                                      size_t* WINPR_RESTRICT offset,
                                      size_t residualCountOffset, size_t residualOffset,
                                      size_t residualSize, UINT64 rawSubcodecSize,
                                      CLEAR_ENCODE_MODE mode,
                                      const gdiPalette* WINPR_RESTRICT palette)
{
	WINPR_ASSERT(offset);

	switch (mode)
	{
		case CLEAR_ENCODE_MODE_RAW:
			if (!clear_encode_raw_subcodec(pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth, nHeight,
			                               dst, capacity, offset, palette))
				return FALSE;
			return clear_encode_patch_uint32(dst, residualCountOffset + 8,
			                                 WINPR_ASSERTING_INT_CAST(size_t, rawSubcodecSize));

		case CLEAR_ENCODE_MODE_RLEX:
			if (!clear_encode_rlex_subcodec(pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth,
			                                nHeight, dst, capacity, offset, palette))
				return FALSE;
			return clear_encode_patch_uint32(dst, residualCountOffset + 8, *offset - residualOffset);

		case CLEAR_ENCODE_MODE_NSC:
			if (!clear_encode_nsc_subcodec(clear, pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth,
			                               nHeight, dst, capacity, offset))
				return FALSE;
			return clear_encode_patch_uint32(dst, residualCountOffset + 8, *offset - residualOffset);

		case CLEAR_ENCODE_MODE_BANDS:
			if (!clear_encode_bands(clear, pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth, nHeight,
			                        dst, capacity, offset, palette))
				return FALSE;
			if (!clear_encode_patch_uint32(dst, residualCountOffset + 4, *offset - residualOffset))
				return FALSE;
			if (clear->VBarResetPending)
				dst[0] |= CLEARCODEC_FLAG_CACHE_RESET;
			clear->VBarResetPending = FALSE;
			return TRUE;

		case CLEAR_ENCODE_MODE_RESIDUAL:
		default:
			if (!clear_encode_residual(clear, pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth,
			                           nHeight, dst, capacity, offset, palette))
				return FALSE;
			return clear_encode_patch_uint32(dst, residualCountOffset, residualSize);
	}
}

static BOOL clear_encode_validate_input(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                        const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                        UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth,
                                        UINT32 nHeight)
{
	if (!clear || !clear->Compressor || !pSrcData || (nWidth == 0) || (nHeight == 0) ||
	    (nWidth > UINT16_MAX) || (nHeight > UINT16_MAX))
		return FALSE;

	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);
	if (bpp == 0)
		return FALSE;

	if ((SrcX > (UINT32_MAX / bpp)) || (nWidth > ((UINT32_MAX / bpp) - SrcX)))
		return FALSE;
	if (SrcY > (UINT32_MAX - nHeight))
		return FALSE;

	const UINT32 minStep = (SrcX + nWidth) * bpp;
	return SrcStep >= minStep;
}

static BOOL clear_encode_calculate_limits(UINT32 nWidth, UINT32 nHeight,
                                          CLEAR_ENCODE_LIMITS* WINPR_RESTRICT limits)
{
	WINPR_ASSERT(limits);

	limits->pixelCount = 1ULL * nWidth * nHeight;
	const UINT64 maxResidualSize = limits->pixelCount * 10ULL;
	limits->rawSubcodecSize = 13ULL + limits->pixelCount * 3ULL;
	const UINT64 maxBandsSize =
	    (nHeight <= 52) ? (11ULL + nWidth * (2ULL + nHeight * 3ULL)) : 0ULL;
	const UINT64 maxPayloadSize = MAX(MAX(maxResidualSize, limits->rawSubcodecSize), maxBandsSize);
	limits->glyphEligible = limits->pixelCount <= 1024ULL;
	const UINT64 glyphHeaderSize = limits->glyphEligible ? 2ULL : 0ULL;
	limits->maxSize = 2ULL + glyphHeaderSize + 12ULL + maxPayloadSize;

	return (limits->pixelCount <= UINT32_MAX) && (limits->rawSubcodecSize <= UINT32_MAX) &&
	       (limits->maxSize <= UINT32_MAX);
}

static BOOL clear_encode_emit_glyph_hit(CLEAR_CONTEXT* WINPR_RESTRICT clear, UINT16 glyphIndex,
                                        BYTE** WINPR_RESTRICT ppDstData,
                                        UINT32* WINPR_RESTRICT pDstSize)
{
	WINPR_ASSERT(clear);
	WINPR_ASSERT(ppDstData);
	WINPR_ASSERT(pDstSize);

	BYTE* dst = calloc(1, 4);
	if (!dst)
		return FALSE;

	dst[0] = CLEARCODEC_FLAG_GLYPH_INDEX | CLEARCODEC_FLAG_GLYPH_HIT;
	dst[1] = WINPR_ASSERTING_INT_CAST(BYTE, clear->seqNumber & 0xFF);
	dst[2] = WINPR_ASSERTING_INT_CAST(BYTE, glyphIndex & 0xFF);
	dst[3] = WINPR_ASSERTING_INT_CAST(BYTE, (glyphIndex >> 8) & 0xFF);

	*ppDstData = dst;
	*pDstSize = 4;
	clear->seqNumber = (clear->seqNumber + 1) & 0xFF;
	return TRUE;
}

static void clear_encode_init_stream_header(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                            BYTE* WINPR_RESTRICT dst, BOOL glyphEligible,
                                            UINT16 glyphIndex, size_t* WINPR_RESTRICT offset,
                                            size_t* WINPR_RESTRICT residualCountOffset)
{
	WINPR_ASSERT(clear);
	WINPR_ASSERT(dst);
	WINPR_ASSERT(offset);
	WINPR_ASSERT(residualCountOffset);

	dst[(*offset)++] = glyphEligible ? CLEARCODEC_FLAG_GLYPH_INDEX : 0; /* flags */
	dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, clear->seqNumber & 0xFF);
	if (glyphEligible)
	{
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, glyphIndex & 0xFF);
		dst[(*offset)++] = WINPR_ASSERTING_INT_CAST(BYTE, (glyphIndex >> 8) & 0xFF);
	}

	*residualCountOffset = *offset;
	*offset += 12;
}

static BOOL clear_encode_store_glyph_if_needed(
    CLEAR_CONTEXT* WINPR_RESTRICT clear, const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
    UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth, UINT32 nHeight,
    const gdiPalette* WINPR_RESTRICT palette, BOOL glyphEligible, UINT16 glyphIndex)
{
	if (!glyphEligible)
		return TRUE;

	if (!clear_encode_store_glyph(clear, pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth, nHeight,
	                              palette, glyphIndex))
		return FALSE;

	clear->GlyphCacheCursor =
	    (clear->GlyphCacheCursor + 1) % WINPR_ASSERTING_INT_CAST(UINT32, ARRAYSIZE(clear->GlyphCache));
	return TRUE;
}

INT32 clear_encode(CLEAR_CONTEXT* WINPR_RESTRICT clear, const BYTE* WINPR_RESTRICT pSrcData,
                   UINT32 SrcFormat, UINT32 SrcStep, UINT32 SrcX, UINT32 SrcY, UINT32 nWidth,
                   UINT32 nHeight, BYTE** WINPR_RESTRICT ppDstData,
                   UINT32* WINPR_RESTRICT pDstSize,
                   const gdiPalette* WINPR_RESTRICT palette)
{
	if (ppDstData)
		*ppDstData = nullptr;
	if (pDstSize)
		*pDstSize = 0;

	if (!ppDstData || !pDstSize ||
	    !clear_encode_validate_input(clear, pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth,
	                                 nHeight))
		return -1;

	CLEAR_ENCODE_LIMITS limits = { 0 };
	if (!clear_encode_calculate_limits(nWidth, nHeight, &limits))
		return -1;

	UINT16 glyphIndex = 0;

	if (limits.glyphEligible &&
	    clear_encode_find_glyph(clear, pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth, nHeight,
	                            palette, &glyphIndex))
	{
		return clear_encode_emit_glyph_hit(clear, glyphIndex, ppDstData, pDstSize) ? 0 : -1;
	}

	if (limits.glyphEligible)
	{
		glyphIndex =
		    WINPR_ASSERTING_INT_CAST(UINT16, clear->GlyphCacheCursor % ARRAYSIZE(clear->GlyphCache));
	}

	BYTE* dst = calloc(1, WINPR_ASSERTING_INT_CAST(size_t, limits.maxSize));
	if (!dst)
		return -1;

	size_t offset = 0;
	size_t residualCountOffset = 0;
	clear_encode_init_stream_header(clear, dst, limits.glyphEligible, glyphIndex, &offset,
	                                &residualCountOffset);

	const size_t residualOffset = offset;
	if (!clear_encode_residual(clear, pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth, nHeight,
	                           dst, WINPR_ASSERTING_INT_CAST(size_t, limits.maxSize), &offset,
	                           palette))
	{
		free(dst);
		return -1;
	}

	const size_t residualSize = offset - residualOffset;
	if (residualSize > UINT32_MAX)
	{
		free(dst);
		return -1;
	}

	const CLEAR_ENCODE_MODE mode = clear_encode_select_mode(
	    clear, pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth, nHeight, dst,
	    WINPR_ASSERTING_INT_CAST(size_t, limits.maxSize), residualOffset, residualSize,
	    limits.rawSubcodecSize, palette);

	ZeroMemory(&dst[residualCountOffset], 12);
	offset = residualOffset;

	if (!clear_encode_emit_payload(clear, pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth, nHeight,
	                               dst, WINPR_ASSERTING_INT_CAST(size_t, limits.maxSize), &offset,
	                               residualCountOffset, residualOffset, residualSize,
	                               limits.rawSubcodecSize, mode, palette))
	{
		free(dst);
		return -1;
	}

	BYTE* trimmed = realloc(dst, offset);
	if (trimmed)
		dst = trimmed;

	if (!clear_encode_store_glyph_if_needed(clear, pSrcData, SrcFormat, SrcStep, SrcX, SrcY, nWidth,
	                                        nHeight, palette, limits.glyphEligible, glyphIndex))
	{
		free(dst);
		return -1;
	}

	*ppDstData = dst;
	*pDstSize = WINPR_ASSERTING_INT_CAST(UINT32, offset);
	clear->seqNumber = (clear->seqNumber + 1) & 0xFF;
	return 0;
}

BOOL clear_context_reset(CLEAR_CONTEXT* WINPR_RESTRICT clear)
{
	if (!clear)
		return FALSE;

	/**
	 * The ClearCodec context is not bound to a particular surface,
	 * and its internal caches must NOT be reset on the ResetGraphics PDU.
	 */
	clear->seqNumber = 0;
	if (clear->Compressor)
	{
		clear_reset_vbar_storage(clear, TRUE);
		clear->VBarResetPending = TRUE;
	}
	return TRUE;
}

BOOL clear_context_set_encoder_nsc_tolerance(CLEAR_CONTEXT* WINPR_RESTRICT clear, UINT32 tolerance)
{
	if (!clear || !clear->Compressor)
		return FALSE;

	clear->nscTolerance = tolerance;
	return TRUE;
}

CLEAR_CONTEXT* clear_context_new(BOOL Compressor)
{
	CLEAR_CONTEXT* clear = (CLEAR_CONTEXT*)winpr_aligned_calloc(1, sizeof(CLEAR_CONTEXT), 32);

	if (!clear)
		return nullptr;
	clear->log = WLog_Get(TAG);
	clear->Compressor = Compressor;
	clear->nsc = nsc_context_new();

	if (!clear->nsc)
		goto error_nsc;

	if (!updateContextFormat(clear, PIXEL_FORMAT_BGRX32, TRUE))
		goto error_nsc;

	if (!clear_resize_buffer(clear, 512, 512))
		goto error_nsc;

	if (!clear->TempBuffer)
		goto error_nsc;

	if (!clear_context_reset(clear))
		goto error_nsc;
	clear->VBarResetPending = FALSE;

	return clear;
error_nsc:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	clear_context_free(clear);
	WINPR_PRAGMA_DIAG_POP
	return nullptr;
}

void clear_context_free(CLEAR_CONTEXT* WINPR_RESTRICT clear)
{
	if (!clear)
		return;

	nsc_context_free(clear->nsc);
	winpr_aligned_free(clear->TempBuffer);

	clear_reset_vbar_storage(clear, TRUE);
	clear_reset_glyph_cache(clear);

	winpr_aligned_free(clear);
}
