#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/platform.h>

#include <freerdp/codec/clear.h>

WINPR_PRAGMA_DIAG_PUSH
WINPR_PRAGMA_DIAG_IGNORED_UNUSED_CONST_VAR
/* [MS-RDPEGFX] 4.1.1.1 Example 1 */
static const BYTE PREPARE_CLEAR_EXAMPLE_1[] = "\x03\xc3\x11\x00";
static const BYTE TEST_CLEAR_EXAMPLE_1[] = "\x03\xc3\x11\x00";
WINPR_PRAGMA_DIAG_POP

/* [MS-RDPEGFX] 4.1.1.1 Example 2 */
static const BYTE TEST_CLEAR_EXAMPLE_2[] =
    "\x00\x0d\x00\x00\x00\x00\x00\x00\x00\x00\x82\x00\x00\x00\x00\x00"
    "\x00\x00\x4e\x00\x11\x00\x75\x00\x00\x00\x02\x0e\xff\xff\xff\x00"
    "\x00\x00\xdb\xff\xff\x00\x3a\x90\xff\xb6\x66\x66\xb6\xff\xb6\x66"
    "\x00\x90\xdb\xff\x00\x00\x3a\xdb\x90\x3a\x3a\x90\xdb\x66\x00\x00"
    "\xff\xff\xb6\x64\x64\x64\x11\x04\x11\x4c\x11\x4c\x11\x4c\x11\x4c"
    "\x11\x4c\x00\x47\x13\x00\x01\x01\x04\x00\x01\x00\x00\x47\x16\x00"
    "\x11\x02\x00\x47\x29\x00\x11\x01\x00\x49\x0a\x00\x01\x00\x04\x00"
    "\x01\x00\x00\x4a\x0a\x00\x09\x00\x01\x00\x00\x47\x05\x00\x01\x01"
    "\x1c\x00\x01\x00\x11\x4c\x11\x4c\x11\x4c\x00\x47\x0d\x4d\x00\x4d";

/* [MS-RDPEGFX] 4.1.1.1 Example 3 */
static const BYTE TEST_CLEAR_EXAMPLE_3[] =
    "\x00\xdf\x0e\x00\x00\x00\x8b\x00\x00\x00\x00\x00\x00\x00\xfe\xfe"
    "\xfe\xff\x80\x05\xff\xff\xff\x40\xfe\xfe\xfe\x40\x00\x00\x3f\x00"
    "\x03\x00\x0b\x00\xfe\xfe\xfe\xc5\xd0\xc6\xd0\xc7\xd0\x68\xd4\x69"
    "\xd4\x6a\xd4\x6b\xd4\x6c\xd4\x6d\xd4\x1a\xd4\x1a\xd4\xa6\xd0\x6e"
    "\xd4\x6f\xd4\x70\xd4\x71\xd4\x72\xd4\x73\xd4\x74\xd4\x21\xd4\x22"
    "\xd4\x23\xd4\x24\xd4\x25\xd4\xd9\xd0\xda\xd0\xdb\xd0\xc5\xd0\xc5"
    "\xd0\xdc\xd0\xc2\xd0\x21\xd4\x22\xd4\x23\xd4\x24\xd4\x25\xd4\xc9"
    "\xd0\xca\xd0\x5a\xd4\x2b\xd1\x28\xd1\x2c\xd1\x75\xd4\x27\xd4\x28"
    "\xd4\x29\xd4\x2a\xd4\x1a\xd4\x1a\xd4\x1a\xd4\xb7\xd0\xb8\xd0\xb9"
    "\xd0\xba\xd0\xbb\xd0\xbc\xd0\xbd\xd0\xbe\xd0\xbf\xd0\xc0\xd0\xc1"
    "\xd0\xc2\xd0\xc3\xd0\xc4\xd0";

/* [MS-RDPEGFX] 4.1.1.1 Example 4 */
static const BYTE TEST_CLEAR_EXAMPLE_4[] =
    "\x01\x0b\x78\x00\x00\x00\x00\x00\x46\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x06\x00\x00\x00\x0e\x00\x00\x00\x00\x00\x0f\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xb6\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xb6\x66\xff\xff\xff\xff\xff\xff\xff\xb6\x66\xdb\x90\x3a\xff\xff"
    "\xb6\xff\xff\xff\xff\xff\xff\xff\xff\xff\x46\x91\x47\x91\x48\x91"
    "\x49\x91\x4a\x91\x1b\x91";

static BOOL test_ClearDecompressExample(UINT32 nr, UINT32 width, UINT32 height,
                                        const BYTE* pSrcData, const UINT32 SrcSize)
{
	BOOL rc = FALSE;
	int status = 0;
	BYTE* pDstData = calloc(4ULL * width, height);
	CLEAR_CONTEXT* clear = clear_context_new(FALSE);

	if (!clear || !pDstData)
		goto fail;

	status = clear_decompress(clear, pSrcData, SrcSize, width, height, pDstData,
	                          PIXEL_FORMAT_XRGB32, 0, 0, 0, width, height, nullptr);
	(void)printf("clear_decompress example %" PRIu32 " status: %d\n", nr, status);
	(void)fflush(stdout);
	rc = (status == 0);
fail:
	clear_context_free(clear);
	free(pDstData);
	return rc;
}

static void clear_test_write_color(BYTE* dst, UINT32 format, BYTE r, BYTE g, BYTE b)
{
	const UINT32 color = FreeRDPGetColor(format, r, g, b, 0xFF);
	WINPR_ASSERT(FreeRDPWriteColor(dst, format, color));
}

static BOOL clear_test_compare_images(const BYTE* expected, const BYTE* actual, UINT32 format,
                                      UINT32 width, UINT32 height, UINT32 step)
{
	const UINT32 bpp = FreeRDPGetBytesPerPixel(format);

	for (UINT32 y = 0; y < height; y++)
	{
		const BYTE* expectedLine = &expected[1ULL * y * step];
		const BYTE* actualLine = &actual[1ULL * y * step];

		for (UINT32 x = 0; x < width; x++)
		{
			const UINT32 expectedColor = FreeRDPReadColor(&expectedLine[1ULL * x * bpp], format);
			const UINT32 actualColor = FreeRDPReadColor(&actualLine[1ULL * x * bpp], format);

			if (expectedColor != actualColor)
			{
				(void)printf("pixel mismatch at %" PRIu32 ",%" PRIu32 ": expected 0x%08" PRIx32
				             ", actual 0x%08" PRIx32 "\n",
				             x, y, expectedColor, actualColor);
				return FALSE;
			}
		}
	}

	return TRUE;
}

static BOOL clear_test_compare_images_tolerance(const BYTE* expected, const BYTE* actual,
                                                UINT32 format, UINT32 width, UINT32 height,
                                                UINT32 step, UINT32 tolerance)
{
	const UINT32 bpp = FreeRDPGetBytesPerPixel(format);

	for (UINT32 y = 0; y < height; y++)
	{
		const BYTE* expectedLine = &expected[1ULL * y * step];
		const BYTE* actualLine = &actual[1ULL * y * step];

		for (UINT32 x = 0; x < width; x++)
		{
			BYTE expectedR = 0;
			BYTE expectedG = 0;
			BYTE expectedB = 0;
			BYTE expectedA = 0;
			BYTE actualR = 0;
			BYTE actualG = 0;
			BYTE actualB = 0;
			BYTE actualA = 0;
			const UINT32 expectedColor =
			    FreeRDPReadColor(&expectedLine[1ULL * x * bpp], format);
			const UINT32 actualColor = FreeRDPReadColor(&actualLine[1ULL * x * bpp], format);

			FreeRDPSplitColor(expectedColor, format, &expectedR, &expectedG, &expectedB,
			                  &expectedA, nullptr);
			FreeRDPSplitColor(actualColor, format, &actualR, &actualG, &actualB, &actualA,
			                  nullptr);

			const UINT32 diffR =
			    (expectedR > actualR) ? (expectedR - actualR) : (actualR - expectedR);
			const UINT32 diffG =
			    (expectedG > actualG) ? (expectedG - actualG) : (actualG - expectedG);
			const UINT32 diffB =
			    (expectedB > actualB) ? (expectedB - actualB) : (actualB - expectedB);

			if ((diffR > tolerance) || (diffG > tolerance) || (diffB > tolerance))
			{
				(void)printf("pixel tolerance mismatch at %" PRIu32 ",%" PRIu32
				             ": expected 0x%08" PRIx32 ", actual 0x%08" PRIx32 "\n",
				             x, y, expectedColor, actualColor);
				return FALSE;
			}
		}
	}

	return TRUE;
}

static UINT32 clear_test_read_uint32_le(const BYTE* src)
{
	return ((UINT32)src[0]) | (((UINT32)src[1]) << 8) | (((UINT32)src[2]) << 16) |
	       (((UINT32)src[3]) << 24);
}

static UINT16 clear_test_read_uint16_le(const BYTE* src)
{
	return (UINT16)(((UINT16)src[0]) | (((UINT16)src[1]) << 8));
}

static void clear_test_write_uint32_le(BYTE* dst, UINT32 value)
{
	dst[0] = (BYTE)(value & 0xFF);
	dst[1] = (BYTE)((value >> 8) & 0xFF);
	dst[2] = (BYTE)((value >> 16) & 0xFF);
	dst[3] = (BYTE)((value >> 24) & 0xFF);
}

static void clear_test_write_uint16_le(BYTE* dst, UINT16 value)
{
	dst[0] = (BYTE)(value & 0xFF);
	dst[1] = (BYTE)((value >> 8) & 0xFF);
}

static UINT32 clear_test_residual_byte_count(const BYTE* encoded)
{
	const UINT32 offset = (encoded[0] & 0x01) ? 4 : 2;
	return clear_test_read_uint32_le(&encoded[offset]);
}

static UINT32 clear_test_bands_byte_count(const BYTE* encoded)
{
	const UINT32 offset = (encoded[0] & 0x01) ? 8 : 6;
	return clear_test_read_uint32_le(&encoded[offset]);
}

static UINT32 clear_test_subcodec_byte_count(const BYTE* encoded)
{
	const UINT32 offset = (encoded[0] & 0x01) ? 12 : 10;
	return clear_test_read_uint32_le(&encoded[offset]);
}

static UINT32 clear_test_payload_offset(const BYTE* encoded)
{
	return (encoded[0] & 0x01) ? 16 : 14;
}

static UINT32 clear_test_glyph_residual_byte_count(const BYTE* encoded)
{
	return clear_test_read_uint32_le(&encoded[4]);
}

static UINT32 clear_test_glyph_bands_byte_count(const BYTE* encoded)
{
	return clear_test_read_uint32_le(&encoded[8]);
}

static UINT32 clear_test_glyph_subcodec_byte_count(const BYTE* encoded)
{
	return clear_test_read_uint32_le(&encoded[12]);
}

static UINT32 clear_test_composite_offset(const BYTE* encoded)
{
	return (encoded[0] & 0x01) ? 4 : 2;
}

static BOOL clear_test_decode_residual(UINT32 width, UINT32 height, const BYTE* residualData,
                                       UINT32 actualResidualByteCount,
                                       UINT32 declaredResidualByteCount)
{
	BYTE stream[64] = { 0 };
	BYTE dst[16] = { 0 };
	CLEAR_CONTEXT* clear = clear_context_new(FALSE);
	const UINT32 dstStep = width * FreeRDPGetBytesPerPixel(PIXEL_FORMAT_BGRX32);
	BOOL rc = FALSE;

	if (!clear || (actualResidualByteCount > (sizeof(stream) - 14)) || (dstStep > sizeof(dst)))
		goto fail;

	stream[0] = 0; /* flags */
	stream[1] = 0; /* sequence */
	clear_test_write_uint32_le(&stream[2], declaredResidualByteCount);
	/* bandsByteCount and subcodecByteCount remain zero. */
	CopyMemory(&stream[14], residualData, actualResidualByteCount);

	rc = clear_decompress(clear, stream, 14 + actualResidualByteCount, width, height, dst,
	                      PIXEL_FORMAT_BGRX32, dstStep, 0, 0, width, height, nullptr) < 0;

fail:
	clear_context_free(clear);
	return rc;
}

static BOOL clear_test_decode_rejects_residual(UINT32 width, UINT32 height,
                                               const BYTE* residualData,
                                               UINT32 residualByteCount)
{
	return clear_test_decode_residual(width, height, residualData, residualByteCount,
	                                  residualByteCount);
}

static BOOL clear_test_decode_subcodec(UINT32 width, UINT32 height, const BYTE* subcodecData,
                                       UINT32 actualSubcodecByteCount,
                                       UINT32 declaredSubcodecByteCount)
{
	BYTE stream[128] = { 0 };
	BYTE dst[64] = { 0 };
	CLEAR_CONTEXT* clear = clear_context_new(FALSE);
	const UINT32 dstStep = width * FreeRDPGetBytesPerPixel(PIXEL_FORMAT_BGRX32);
	BOOL rc = FALSE;

	if (!clear || (actualSubcodecByteCount > (sizeof(stream) - 14)) ||
	    (1ULL * dstStep * height > sizeof(dst)))
		goto fail;

	stream[0] = 0; /* flags */
	stream[1] = 0; /* sequence */
	clear_test_write_uint32_le(&stream[10], declaredSubcodecByteCount);
	CopyMemory(&stream[14], subcodecData, actualSubcodecByteCount);

	rc = clear_decompress(clear, stream, 14 + actualSubcodecByteCount, width, height, dst,
	                      PIXEL_FORMAT_BGRX32, dstStep, 0, 0, width, height, nullptr) < 0;

fail:
	clear_context_free(clear);
	return rc;
}

static BOOL clear_test_decode_stream_rejects(UINT32 width, UINT32 height, const BYTE* stream,
                                             UINT32 streamByteCount)
{
	BYTE dst[64] = { 0 };
	CLEAR_CONTEXT* clear = clear_context_new(FALSE);
	const UINT32 dstStep = width * FreeRDPGetBytesPerPixel(PIXEL_FORMAT_BGRX32);
	BOOL rc = FALSE;

	if (!clear || (1ULL * dstStep * height > sizeof(dst)))
		goto fail;

	rc = clear_decompress(clear, stream, streamByteCount, width, height, dst, PIXEL_FORMAT_BGRX32,
	                      dstStep, 0, 0, width, height, nullptr) < 0;

fail:
	clear_context_free(clear);
	return rc;
}

static BOOL clear_test_decode_stream_accepts(UINT32 width, UINT32 height, const BYTE* stream,
                                             UINT32 streamByteCount, const BYTE* expected)
{
	BYTE dst[64] = { 0 };
	CLEAR_CONTEXT* clear = clear_context_new(FALSE);
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 dstStep = width * FreeRDPGetBytesPerPixel(format);
	BOOL rc = FALSE;

	if (!clear || (1ULL * dstStep * height > sizeof(dst)))
		goto fail;

	if (clear_decompress(clear, stream, streamByteCount, width, height, dst, format, dstStep, 0, 0,
	                     width, height, nullptr) != 0)
		goto fail;

	rc = clear_test_compare_images(expected, dst, format, width, height, dstStep);

fail:
	clear_context_free(clear);
	return rc;
}

static void clear_test_write_raw_subcodec_header(BYTE* dst, UINT16 xStart, UINT16 yStart,
                                                 UINT16 width, UINT16 height,
                                                 UINT32 bitmapDataByteCount, BYTE subcodecId)
{
	clear_test_write_uint16_le(&dst[0], xStart);
	clear_test_write_uint16_le(&dst[2], yStart);
	clear_test_write_uint16_le(&dst[4], width);
	clear_test_write_uint16_le(&dst[6], height);
	clear_test_write_uint32_le(&dst[8], bitmapDataByteCount);
	dst[12] = subcodecId;
}

static BOOL clear_test_roundtrip_encoded(const BYTE* src, UINT32 width, UINT32 height,
                                         UINT32 format, UINT32 step, const BYTE* encoded,
                                         UINT32 encodedSize)
{
	BYTE* dst = calloc(1ULL * step, height);
	CLEAR_CONTEXT* decoder = clear_context_new(FALSE);
	BOOL rc = FALSE;

	if (!dst || !decoder)
		goto fail;

	if (clear_decompress(decoder, encoded, encodedSize, width, height, dst, format, step, 0, 0,
	                     width, height, nullptr) != 0)
		goto fail;

	rc = clear_test_compare_images(src, dst, format, width, height, step);

fail:
	clear_context_free(decoder);
	free(dst);
	return rc;
}

static BOOL clear_test_roundtrip_encoded_tolerance(const BYTE* src, UINT32 width, UINT32 height,
                                                   UINT32 format, UINT32 step,
                                                   const BYTE* encoded, UINT32 encodedSize,
                                                   UINT32 tolerance)
{
	BYTE* dst = calloc(1ULL * step, height);
	CLEAR_CONTEXT* decoder = clear_context_new(FALSE);
	BOOL rc = FALSE;

	if (!dst || !decoder)
		goto fail;

	if (clear_decompress(decoder, encoded, encodedSize, width, height, dst, format, step, 0, 0,
	                     width, height, nullptr) != 0)
		goto fail;

	rc = clear_test_compare_images_tolerance(src, dst, format, width, height, step, tolerance);

fail:
	clear_context_free(decoder);
	free(dst);
	return rc;
}

static BOOL clear_test_encode_image(const BYTE* src, UINT32 width, UINT32 height, BYTE** encoded,
                                    UINT32* encodedSize)
{
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	const INT32 status = clear_encode(encoder, src, format, step, 0, 0, width, height, encoded,
	                                  encodedSize, nullptr);

	clear_context_free(encoder);
	return status == 0;
}

static BOOL clear_test_encode_image_format(const BYTE* src, UINT32 format, UINT32 width,
                                           UINT32 height, BYTE** encoded, UINT32* encodedSize)
{
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	const INT32 status = clear_encode(encoder, src, format, step, 0, 0, width, height, encoded,
	                                  encodedSize, nullptr);

	clear_context_free(encoder);
	return status == 0;
}

static BOOL clear_test_encode_image_nsc_tolerance(const BYTE* src, UINT32 width, UINT32 height,
                                                  UINT32 tolerance, BYTE** encoded,
                                                  UINT32* encodedSize)
{
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	INT32 status = -1;

	if (!clear_context_set_encoder_nsc_tolerance(encoder, tolerance))
		goto fail;

	status = clear_encode(encoder, src, format, step, 0, 0, width, height, encoded, encodedSize,
	                      nullptr);

fail:
	clear_context_free(encoder);
	return status == 0;
}

static BOOL clear_test_encoded_subcodec_id(const BYTE* encoded, UINT32 encodedSize, BYTE* id)
{
	const UINT32 offset = (encoded && (encoded[0] & 0x01)) ? 28 : 26;

	if (!encoded || !id || (encodedSize <= offset) || (clear_test_subcodec_byte_count(encoded) < 13))
		return FALSE;

	*id = encoded[offset];
	return TRUE;
}

static void clear_test_fill_unique_image(BYTE* src, UINT32 width, UINT32 height)
{
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);

	for (UINT32 y = 0; y < height; y++)
	{
		BYTE* line = &src[1ULL * y * step];

		for (UINT32 x = 0; x < width; x++)
		{
			const BYTE seed = (BYTE)((x * 37 + y * 19 + 3) & 0xFF);
			clear_test_write_color(&line[1ULL * x * 4], format, seed, (BYTE)(seed + 17),
			                       (BYTE)(seed + 29));
		}
	}
}

static BOOL TestClearEncodeRejectsDecoderContext(void)
{
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	const BYTE src[4] = { 0 };
	CLEAR_CONTEXT* clear = clear_context_new(FALSE);
	const INT32 status = clear_encode(clear, src, PIXEL_FORMAT_BGRX32, 4, 0, 0, 1, 1, &encoded,
	                                  &encodedSize, nullptr);

	clear_context_free(clear);
	free(encoded);
	return status < 0;
}

#if !defined(WITHOUT_FREERDP_3x_DEPRECATED)
static BOOL TestClearEncodeRejectsBrokenDeprecatedContract(void)
{
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	const BYTE src[4] = { 0 };
	CLEAR_CONTEXT* clear = clear_context_new(TRUE);
	const int status = clear_compress(clear, src, sizeof(src), &encoded, &encodedSize);
	const BOOL rc = (status == 1) && (encoded == nullptr) && (encodedSize == 0);

	clear_context_free(clear);
	free(encoded);
	return rc;
}
#endif

static BOOL TestClearEncodeRejectsInvalidArguments(void)
{
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	const BYTE src[4] = { 0 };
	CLEAR_CONTEXT* clear = clear_context_new(TRUE);
	BOOL rc = TRUE;

	rc = rc && (clear_encode(nullptr, src, PIXEL_FORMAT_BGRX32, 4, 0, 0, 1, 1, &encoded,
	                         &encodedSize, nullptr) < 0);
	rc = rc && (clear_encode(clear, nullptr, PIXEL_FORMAT_BGRX32, 4, 0, 0, 1, 1, &encoded,
	                         &encodedSize, nullptr) < 0);
	rc = rc && (clear_encode(clear, src, PIXEL_FORMAT_BGRX32, 4, 0, 0, 0, 1, &encoded,
	                         &encodedSize, nullptr) < 0);
	rc = rc && (clear_encode(clear, src, PIXEL_FORMAT_BGRX32, 3, 0, 0, 1, 1, &encoded,
	                         &encodedSize, nullptr) < 0);
	rc = rc && (clear_encode(clear, src, PIXEL_FORMAT_BGRX32, 4, 0, 0, 1, 1, nullptr,
	                         &encodedSize, nullptr) < 0);
	rc = rc && (clear_encode(clear, src, PIXEL_FORMAT_BGRX32, 4, 0, 0, 1, 1, &encoded, nullptr,
	                         nullptr) < 0);

	clear_context_free(clear);
	free(encoded);
	return rc;
}

static BOOL TestClearEncodeRejectsBoundaryArguments(void)
{
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	const BYTE src[16] = { 0 };
	CLEAR_CONTEXT* clear = clear_context_new(TRUE);
	BOOL rc = TRUE;

	rc = rc && (clear_encode(clear, src, PIXEL_FORMAT_BGRX32, 4, 0, 0, 1, 0, &encoded,
	                         &encodedSize, nullptr) < 0);
	rc = rc && (clear_encode(clear, src, PIXEL_FORMAT_BGRX32, 4, 0, 0, 65536, 1, &encoded,
	                         &encodedSize, nullptr) < 0);
	rc = rc && (clear_encode(clear, src, PIXEL_FORMAT_BGRX32, 4, 0, 0, 1, 65536, &encoded,
	                         &encodedSize, nullptr) < 0);
	rc = rc && (clear_encode(clear, src, PIXEL_FORMAT_BGRX32, 8, 1, 0, 2, 1, &encoded,
	                         &encodedSize, nullptr) < 0);
	rc = rc && (clear_encode(clear, src, PIXEL_FORMAT_BGRX32, 4, UINT32_MAX, 0, 1, 1,
	                         &encoded, &encodedSize, nullptr) < 0);
	rc = rc && (clear_encode(clear, src, PIXEL_FORMAT_BGRX32, 4, 0, UINT32_MAX, 1, 1,
	                         &encoded, &encodedSize, nullptr) < 0);

	clear_context_free(clear);
	free(encoded);
	return rc;
}

static BOOL TestClearEncodeRandomGeneratedRoundtrips(void)
{
	static const UINT32 seeds[] = { 0x12345678, 0xA5A5A5A5, 0xC001D00D, 0x5EEDFACE };
	static const UINT32 widths[] = { 1, 7, 16, 53 };
	static const UINT32 heights[] = { 1, 5, 17, 9 };
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	BOOL rc = FALSE;

	if (!encoder)
		goto fail;

	for (size_t index = 0; index < ARRAYSIZE(seeds); index++)
	{
		const UINT32 width = widths[index];
		const UINT32 height = heights[index];
		const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
		BYTE* src = calloc(1ULL * step, height);
		BYTE* encoded = nullptr;
		UINT32 encodedSize = 0;
		UINT32 state = seeds[index];

		if (!src)
			goto fail;

		for (UINT32 pos = 0; pos < step * height; pos++)
		{
			state = state * 1664525u + 1013904223u;
			src[pos] = (BYTE)(state >> 24);
		}
		for (UINT32 pos = 3; pos < step * height; pos += 4)
			src[pos] = 0xFF;

		if (clear_encode(encoder, src, format, step, 0, 0, width, height, &encoded,
		                 &encodedSize, nullptr) != 0)
		{
			free(src);
			free(encoded);
			goto fail;
		}

		if (!clear_test_roundtrip_encoded(src, width, height, format, step, encoded, encodedSize))
		{
			free(src);
			free(encoded);
			goto fail;
		}

		free(src);
		free(encoded);
	}

	rc = TRUE;
fail:
	clear_context_free(encoder);
	return rc;
}

static BOOL TestClearEncodeResidualRunLengthWireBoundaries(void)
{
	typedef struct
	{
		UINT32 runLength;
		UINT32 width;
		UINT32 height;
		UINT32 residualSize;
		BYTE firstFactor;
		UINT16 extended16;
		UINT32 extended32;
	} RUN_CASE;

	static const RUN_CASE cases[] = {
		{ 1, 1, 1, 4, 1, 0, 0 },
		{ 254, 254, 1, 4, 254, 0, 0 },
		{ 255, 255, 1, 6, 0xFF, 255, 0 },
		{ 65534, 32767, 2, 6, 0xFF, 65534, 0 },
		{ 65535, 255, 257, 10, 0xFF, 0xFFFF, 65535 },
		{ 65536, 256, 256, 10, 0xFF, 0xFFFF, 65536 }
	};

	for (size_t index = 0; index < ARRAYSIZE(cases); index++)
	{
		const RUN_CASE* current = &cases[index];
		const UINT32 width = current->width;
		const UINT32 height = current->height;
		const UINT32 format = PIXEL_FORMAT_BGRX32;
		const UINT32 bpp = FreeRDPGetBytesPerPixel(format);
		const UINT32 step = width * bpp;
		const BOOL glyphEligible = (1ULL * width * height) <= 1024ULL;
		const UINT32 headerSize = glyphEligible ? 16 : 14;
		const UINT32 payloadOffset = headerSize;
		const BYTE expectedFlags = glyphEligible ? 0x01 : 0;
		BYTE* src = calloc(1ULL * width * height, bpp);
		BYTE* encoded = nullptr;
		UINT32 encodedSize = 0;
		CLEAR_CONTEXT* clear = clear_context_new(TRUE);
		BOOL rc = FALSE;

		if (!src || !clear)
			goto fail_case;

		for (UINT32 y = 0; y < height; y++)
		{
			BYTE* line = &src[1ULL * y * step];
			for (UINT32 x = 0; x < width; x++)
				clear_test_write_color(&line[1ULL * x * bpp], format, 0x11, 0x22, 0x33);
		}

		if (clear_encode(clear, src, format, step, 0, 0, width, height, &encoded, &encodedSize,
		                 nullptr) != 0)
			goto fail_case;

		if (!encoded || (encodedSize != (headerSize + current->residualSize)) ||
		    (encoded[0] != expectedFlags) || (encoded[1] != 0) ||
		    (clear_test_residual_byte_count(encoded) != current->residualSize) ||
		    (clear_test_bands_byte_count(encoded) != 0) ||
		    (clear_test_subcodec_byte_count(encoded) != 0))
			goto fail_case;

		if (glyphEligible && (clear_test_read_uint16_le(&encoded[2]) != 0))
			goto fail_case;

		if ((encoded[payloadOffset] != 0x33) || (encoded[payloadOffset + 1] != 0x22) ||
		    (encoded[payloadOffset + 2] != 0x11) ||
		    (encoded[payloadOffset + 3] != current->firstFactor))
			goto fail_case;

		if (current->residualSize >= 6)
		{
			if (clear_test_read_uint16_le(&encoded[payloadOffset + 4]) != current->extended16)
				goto fail_case;
		}

		if (current->residualSize >= 10)
		{
			if (clear_test_read_uint32_le(&encoded[payloadOffset + 6]) != current->extended32)
				goto fail_case;
		}

		rc = TRUE;

	fail_case:
		clear_context_free(clear);
		free(encoded);
		free(src);

		if (!rc)
		{
			(void)printf("residual run boundary failed for length %" PRIu32 "\n",
			             current->runLength);
			if (encoded)
			{
				(void)printf("encodedSize=%" PRIu32 ", flags=0x%02" PRIx8
				             ", residual=%" PRIu32 ", bands=%" PRIu32 ", subcodec=%" PRIu32
				             ", payloadOffset=%" PRIu32 "\n",
				             encodedSize, encoded[0], clear_test_residual_byte_count(encoded),
				             clear_test_bands_byte_count(encoded),
				             clear_test_subcodec_byte_count(encoded), payloadOffset);
			}
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL TestClearEncodeResidualSequenceWrapAndReset(void)
{
	const UINT32 width = 1;
	const UINT32 height = 1;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE src[4] = { 0 };
	CLEAR_CONTEXT* clear = clear_context_new(TRUE);
	BOOL rc = FALSE;

	clear_test_write_color(src, format, 0x44, 0x55, 0x66);

	if (!clear)
		goto fail;

	for (UINT32 index = 0; index < 257; index++)
	{
		BYTE* encoded = nullptr;
		UINT32 encodedSize = 0;
		const BYTE expectedSequence = (BYTE)(index & 0xFF);

		if (clear_encode(clear, src, format, step, 0, 0, width, height, &encoded, &encodedSize,
		                 nullptr) != 0)
		{
			free(encoded);
			goto fail;
		}

		if (!encoded || (encodedSize < 2) || (encoded[1] != expectedSequence))
		{
			free(encoded);
			goto fail;
		}

		free(encoded);
	}

	if (!clear_context_reset(clear))
		goto fail;

	{
		BYTE* encoded = nullptr;
		UINT32 encodedSize = 0;

		if (clear_encode(clear, src, format, step, 0, 0, width, height, &encoded, &encodedSize,
		                 nullptr) != 0)
		{
			free(encoded);
			goto fail;
		}

		rc = encoded && (encodedSize >= 2) && (encoded[1] == 0);
		free(encoded);
	}

fail:
	clear_context_free(clear);
	return rc;
}

static BOOL TestClearDecodeRejectsMalformedResidualStreams(void)
{
	const BYTE underfill[] = { 0x30, 0x20, 0x10, 0x01 };
	const BYTE overfill[] = { 0x30, 0x20, 0x10, 0x02 };
	const BYTE zeroFill[] = { 0x30, 0x20, 0x10, 0x00 };
	const BYTE truncatedRun[] = { 0x30, 0x20, 0x10 };
	const BYTE truncatedExtendedRun[] = { 0x30, 0x20, 0x10, 0xFF };
	const BYTE nestedByteCountOverflow[] = { 0x30, 0x20, 0x10, 0xFF, 0x01, 0x00 };
	BOOL rc = TRUE;

	rc = rc && clear_test_decode_rejects_residual(2, 1, underfill, sizeof(underfill));
	rc = rc && clear_test_decode_rejects_residual(1, 1, overfill, sizeof(overfill));
	rc = rc && clear_test_decode_rejects_residual(1, 1, zeroFill, sizeof(zeroFill));
	rc = rc && clear_test_decode_rejects_residual(1, 1, truncatedRun, sizeof(truncatedRun));
	rc = rc && clear_test_decode_rejects_residual(1, 1, truncatedExtendedRun,
	                                              sizeof(truncatedExtendedRun));
	rc = rc && clear_test_decode_residual(1, 1, nestedByteCountOverflow,
	                                      sizeof(nestedByteCountOverflow), 4);
	return rc;
}

static BOOL TestClearDecodeRejectsMalformedTopLevelStreams(void)
{
	const BYTE truncatedHeader[] = { 0x00, 0x00, 0x01 };
	const BYTE truncatedCompositeCounts[] = { 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };
	const BYTE hugeResidualByteCount[] = { 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
		                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		                                   0x00, 0x00 };
	const BYTE hugeBandsByteCount[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		                                0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
		                                0x00, 0x00 };
	const BYTE hugeSubcodecByteCount[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		                                   0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
		                                   0xFF, 0xFF };
	BOOL rc = TRUE;

	rc = rc && clear_test_decode_stream_rejects(1, 1, truncatedHeader, sizeof(truncatedHeader));
	rc = rc && clear_test_decode_stream_rejects(1, 1, truncatedCompositeCounts,
	                                            sizeof(truncatedCompositeCounts));
	rc = rc && clear_test_decode_stream_rejects(1, 1, hugeResidualByteCount,
	                                            sizeof(hugeResidualByteCount));
	rc = rc && clear_test_decode_stream_rejects(1, 1, hugeBandsByteCount,
	                                            sizeof(hugeBandsByteCount));
	rc = rc && clear_test_decode_stream_rejects(1, 1, hugeSubcodecByteCount,
	                                            sizeof(hugeSubcodecByteCount));
	return rc;
}

static BOOL TestClearDecodeRejectsMalformedGlyphStreams(void)
{
	const BYTE hitWithoutIndex[] = { 0x02, 0x00 };
	const BYTE truncatedGlyphIndex[] = { 0x01, 0x00 };
	const BYTE invalidGlyphIndex[] = { 0x01, 0x00, 0xA0, 0x0F };
	const BYTE emptyCacheHit[] = { 0x03, 0x00, 0x00, 0x00 };
	BOOL rc = TRUE;

	rc = rc && clear_test_decode_stream_rejects(1, 1, hitWithoutIndex, sizeof(hitWithoutIndex));
	rc = rc &&
	     clear_test_decode_stream_rejects(1, 1, truncatedGlyphIndex, sizeof(truncatedGlyphIndex));
	rc = rc && clear_test_decode_stream_rejects(1, 1, invalidGlyphIndex,
	                                            sizeof(invalidGlyphIndex));
	rc = rc && clear_test_decode_stream_rejects(1, 1, emptyCacheHit, sizeof(emptyCacheHit));
	return rc;
}

static BOOL clear_test_decode_rejects_bands(UINT32 width, UINT32 height, const BYTE* bandsData,
                                            UINT32 bandsByteCount)
{
	BYTE stream[64] = { 0 };

	if (bandsByteCount > (sizeof(stream) - 14))
		return FALSE;

	stream[0] = 0;
	stream[1] = 0;
	clear_test_write_uint32_le(&stream[6], bandsByteCount);
	CopyMemory(&stream[14], bandsData, bandsByteCount);
	return clear_test_decode_stream_rejects(width, height, stream, 14 + bandsByteCount);
}

static BOOL TestClearDecodeRejectsMalformedVBarStreams(void)
{
	BYTE band[16] = { 0 };
	BOOL rc = TRUE;

	rc = rc && clear_test_decode_rejects_bands(1, 1, band, 10);

	clear_test_write_uint16_le(&band[0], 0);
	clear_test_write_uint16_le(&band[2], 0);
	clear_test_write_uint16_le(&band[4], 0);
	clear_test_write_uint16_le(&band[6], 52);
	clear_test_write_uint16_le(&band[11], 0);
	rc = rc && clear_test_decode_rejects_bands(1, 1, band, 13);

	ZeroMemory(band, sizeof(band));
	clear_test_write_uint16_le(&band[0], 1);
	clear_test_write_uint16_le(&band[2], 0);
	clear_test_write_uint16_le(&band[4], 0);
	clear_test_write_uint16_le(&band[6], 0);
	rc = rc && clear_test_decode_rejects_bands(1, 1, band, 11);

	ZeroMemory(band, sizeof(band));
	clear_test_write_uint16_le(&band[0], 0);
	clear_test_write_uint16_le(&band[2], 0);
	clear_test_write_uint16_le(&band[4], 0);
	clear_test_write_uint16_le(&band[6], 0);
	clear_test_write_uint16_le(&band[11], 0x0102);
	rc = rc && clear_test_decode_rejects_bands(1, 1, band, 13);

	return rc;
}

static BOOL TestClearEncodeSequenceStartsAtZero(void)
{
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	const BYTE src[4] = { 0 };
	CLEAR_CONTEXT* clear = clear_context_new(TRUE);
	const INT32 status = clear_encode(clear, src, PIXEL_FORMAT_BGRX32, 4, 0, 0, 1, 1, &encoded,
	                                  &encodedSize, nullptr);
	const BOOL rc = (status == 0) && encoded && (encodedSize >= 4) && (encoded[0] == 0x01) &&
	                (encoded[1] == 0) && (clear_test_read_uint16_le(&encoded[2]) == 0);

	clear_context_free(clear);
	free(encoded);
	return rc;
}

static BOOL TestClearEncodeResidualRoundtripSmallBgrx(void)
{
	const UINT32 width = 2;
	const UINT32 height = 2;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE src[16] = { 0 };
	BYTE dst[16] = { 0 };
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	CLEAR_CONTEXT* decoder = clear_context_new(FALSE);
	BOOL rc = FALSE;

	clear_test_write_color(&src[0], format, 0x10, 0x20, 0x30);
	clear_test_write_color(&src[4], format, 0x10, 0x20, 0x30);
	clear_test_write_color(&src[8], format, 0x40, 0x50, 0x60);
	clear_test_write_color(&src[12], format, 0x70, 0x80, 0x90);

	if (!encoder || !decoder)
		goto fail;

	if (clear_encode(encoder, src, format, step, 0, 0, width, height, &encoded, &encodedSize,
	                 nullptr) != 0)
		goto fail;

	if (!encoded || (encodedSize < 14))
		goto fail;

	if (clear_decompress(decoder, encoded, encodedSize, width, height, dst, format, step, 0, 0,
	                     width, height, nullptr) != 0)
		goto fail;

	rc = clear_test_compare_images(src, dst, format, width, height, step);

fail:
	if (!rc && encoded)
	{
		(void)printf("raw subcodec failed: size=%" PRIu32 ", flags=0x%02" PRIx8
		             ", residual=%" PRIu32 ", bands=%" PRIu32 ", subcodec=%" PRIu32 "\n",
		             encodedSize, encoded[0], clear_test_residual_byte_count(encoded),
		             clear_test_bands_byte_count(encoded), clear_test_subcodec_byte_count(encoded));
	}
	clear_context_free(decoder);
	clear_context_free(encoder);
	free(encoded);
	return rc;
}

static BOOL TestClearEncodeRawSubcodecRoundtrip(void)
{
	const UINT32 width = 4;
	const UINT32 height = 4;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE src[64] = { 0 };
	BYTE dst[64] = { 0 };
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	CLEAR_CONTEXT* decoder = clear_context_new(FALSE);
	BOOL rc = FALSE;

	for (UINT32 y = 0; y < height; y++)
	{
		BYTE* line = &src[1ULL * y * step];
		for (UINT32 x = 0; x < width; x++)
		{
			const BYTE seed = (BYTE)(1 + x + y * width);
			clear_test_write_color(&line[1ULL * x * 4], format, seed, (BYTE)(0x40 + seed),
			                       (BYTE)(0x80 + seed));
		}
	}

	if (!encoder || !decoder)
		goto fail;

	if (clear_encode(encoder, src, format, step, 0, 0, width, height, &encoded, &encodedSize,
	                 nullptr) != 0)
		goto fail;

	const UINT32 subcodecOffset = (encoded[0] & 0x01) ? 16 : 14;
	const UINT32 headerSize = subcodecOffset;
	if (!encoded || (encodedSize != (headerSize + 13 + width * height * 3)))
		goto fail;

	if ((clear_test_residual_byte_count(encoded) != 0) ||
	    (clear_test_bands_byte_count(encoded) != 0) ||
	    (clear_test_subcodec_byte_count(encoded) != (13 + width * height * 3)))
		goto fail;

	if ((clear_test_read_uint16_le(&encoded[subcodecOffset]) != 0) ||
	    (clear_test_read_uint16_le(&encoded[subcodecOffset + 2]) != 0) ||
	    (clear_test_read_uint16_le(&encoded[subcodecOffset + 4]) != width) ||
	    (clear_test_read_uint16_le(&encoded[subcodecOffset + 6]) != height) ||
	    (clear_test_read_uint32_le(&encoded[subcodecOffset + 8]) != (width * height * 3)) ||
	    (encoded[subcodecOffset + 12] != 0))
		goto fail;

	if (clear_decompress(decoder, encoded, encodedSize, width, height, dst, format, step, 0, 0,
	                     width, height, nullptr) != 0)
		goto fail;

	rc = clear_test_compare_images(src, dst, format, width, height, step);

fail:
	clear_context_free(decoder);
	clear_context_free(encoder);
	free(encoded);
	return rc;
}

static BOOL TestClearEncodeStrategyNotLargerThanRaw(void)
{
	const UINT32 width = 5;
	const UINT32 height = 5;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE src[100] = { 0 };
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	BOOL rc = FALSE;

	for (UINT32 y = 0; y < height; y++)
	{
		BYTE* line = &src[1ULL * y * step];
		for (UINT32 x = 0; x < width; x++)
		{
			const BYTE seed = (BYTE)(0x20 + x * 13 + y * 17);
			clear_test_write_color(&line[1ULL * x * 4], format, seed, (BYTE)(seed + 3),
			                       (BYTE)(seed + 7));
		}
	}

	if (!encoder)
		goto fail;

	if (clear_encode(encoder, src, format, step, 0, 0, width, height, &encoded, &encodedSize,
	                 nullptr) != 0)
		goto fail;

	const UINT32 headerSize = (encoded && (encoded[0] & 0x01)) ? 16 : 14;
	rc = encoded && (encodedSize <= (headerSize + 13 + width * height * 3));

fail:
	clear_context_free(encoder);
	free(encoded);
	return rc;
}

static BOOL TestClearDecodeRejectsMalformedRawSubcodecStreams(void)
{
	BYTE record[32] = { 0 };
	BOOL rc = TRUE;

	clear_test_write_raw_subcodec_header(record, 0, 0, 1, 1, 2, 0);
	record[13] = 0x30;
	record[14] = 0x20;
	rc = rc && clear_test_decode_subcodec(1, 1, record, 15, 15);

	ZeroMemory(record, sizeof(record));
	clear_test_write_raw_subcodec_header(record, 0, 0, 1, 1, 4, 0);
	record[13] = 0x30;
	record[14] = 0x20;
	record[15] = 0x10;
	record[16] = 0x00;
	rc = rc && clear_test_decode_subcodec(1, 1, record, 17, 17);

	ZeroMemory(record, sizeof(record));
	clear_test_write_raw_subcodec_header(record, 1, 0, 2, 1, 6, 0);
	rc = rc && clear_test_decode_subcodec(2, 1, record, 19, 19);

	ZeroMemory(record, sizeof(record));
	clear_test_write_raw_subcodec_header(record, 0, 1, 1, 2, 6, 0);
	rc = rc && clear_test_decode_subcodec(1, 2, record, 19, 19);

	ZeroMemory(record, sizeof(record));
	clear_test_write_raw_subcodec_header(record, 0, 0, 0, 1, 0, 0);
	rc = rc && clear_test_decode_subcodec(1, 1, record, 13, 13);

	ZeroMemory(record, sizeof(record));
	clear_test_write_raw_subcodec_header(record, 0, 0, 1, 0, 0, 0);
	rc = rc && clear_test_decode_subcodec(1, 1, record, 13, 13);

	ZeroMemory(record, sizeof(record));
	clear_test_write_raw_subcodec_header(record, 0, 0, 1, 1, 3, 0x7F);
	record[13] = 0x30;
	record[14] = 0x20;
	record[15] = 0x10;
	rc = rc && clear_test_decode_subcodec(1, 1, record, 16, 16);

	ZeroMemory(record, sizeof(record));
	clear_test_write_raw_subcodec_header(record, 0, 0, 1, 1, 3, 0);
	record[13] = 0x30;
	record[14] = 0x20;
	record[15] = 0x10;
	rc = rc && clear_test_decode_subcodec(1, 1, record, 16, 12);

	ZeroMemory(record, sizeof(record));
	clear_test_write_raw_subcodec_header(record, 0, 0, 1, 1, 3, 0);
	record[13] = 0x30;
	record[14] = 0x20;
	record[15] = 0x10;
	rc = rc && clear_test_decode_subcodec(1, 1, record, 16, 15);

	ZeroMemory(record, sizeof(record));
	clear_test_write_raw_subcodec_header(record, 0, 0, 1, 1, 3, 0);
	record[13] = 0x30;
	record[14] = 0x20;
	record[15] = 0x10;
	rc = rc && clear_test_decode_subcodec(1, 1, record, 16, 17);

	return rc;
}

static BOOL TestClearDecodeRawSubcodecNonOverlappingRectangles(void)
{
	BYTE stream[14 + 16 + 16] = { 0 };
	BYTE expected[8] = { 0 };
	UINT32 offset = 14;
	const UINT32 format = PIXEL_FORMAT_BGRX32;

	stream[0] = 0;
	stream[1] = 0;
	clear_test_write_uint32_le(&stream[10], 32);

	clear_test_write_raw_subcodec_header(&stream[offset], 0, 0, 1, 1, 3, 0);
	stream[offset + 13] = 0x30;
	stream[offset + 14] = 0x20;
	stream[offset + 15] = 0x10;
	offset += 16;

	clear_test_write_raw_subcodec_header(&stream[offset], 1, 0, 1, 1, 3, 0);
	stream[offset + 13] = 0x60;
	stream[offset + 14] = 0x50;
	stream[offset + 15] = 0x40;

	clear_test_write_color(&expected[0], format, 0x10, 0x20, 0x30);
	clear_test_write_color(&expected[4], format, 0x40, 0x50, 0x60);
	return clear_test_decode_stream_accepts(2, 1, stream, sizeof(stream), expected);
}

static BOOL TestClearDecodeRejectsResidualAndRawSubcodecOverlap(void)
{
	BYTE stream[14 + 4 + 16] = { 0 };
	UINT32 offset = 14;

	stream[0] = 0;
	stream[1] = 0;
	clear_test_write_uint32_le(&stream[2], 4);
	clear_test_write_uint32_le(&stream[10], 16);
	stream[offset++] = 0x30;
	stream[offset++] = 0x20;
	stream[offset++] = 0x10;
	stream[offset++] = 0x01;

	clear_test_write_raw_subcodec_header(&stream[offset], 0, 0, 1, 1, 3, 0);
	stream[offset + 13] = 0x60;
	stream[offset + 14] = 0x50;
	stream[offset + 15] = 0x40;

	return clear_test_decode_stream_rejects(1, 1, stream, sizeof(stream));
}

static BOOL TestClearDecodeRejectsMalformedRlexSubcodecStreams(void)
{
	BYTE record[32] = { 0 };
	BOOL rc = TRUE;

	clear_test_write_raw_subcodec_header(record, 0, 0, 1, 1, 0, 2);
	rc = rc && clear_test_decode_subcodec(1, 1, record, 13, 13);

	ZeroMemory(record, sizeof(record));
	clear_test_write_raw_subcodec_header(record, 0, 0, 1, 1, 1, 2);
	record[13] = 0;
	rc = rc && clear_test_decode_subcodec(1, 1, record, 14, 14);

	ZeroMemory(record, sizeof(record));
	clear_test_write_raw_subcodec_header(record, 0, 0, 1, 1, 4, 2);
	record[13] = 2;
	record[14] = 0x30;
	record[15] = 0x20;
	record[16] = 0x10;
	rc = rc && clear_test_decode_subcodec(1, 1, record, 17, 17);

	ZeroMemory(record, sizeof(record));
	clear_test_write_raw_subcodec_header(record, 0, 0, 1, 1, 6, 2);
	record[13] = 1;
	record[14] = 0x30;
	record[15] = 0x20;
	record[16] = 0x10;
	record[17] = 0x01;
	record[18] = 0x00;
	rc = rc && clear_test_decode_subcodec(1, 1, record, 19, 19);

	return rc;
}

static BOOL TestClearEncodeRlexPaletteBoundaries(void)
{
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	BOOL rc = TRUE;

	{
		const UINT32 width = 16;
		const UINT32 height = 1;
		const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
		BYTE src[64] = { 0 };
		BYTE* encoded = nullptr;
		UINT32 encodedSize = 0;

		for (UINT32 x = 0; x < width; x++)
			clear_test_write_color(&src[1ULL * x * 4], format, 0x10, 0x20, 0x30);

		rc = rc && clear_test_encode_image(src, width, height, &encoded, &encodedSize);
		rc = rc && encoded &&
		     clear_test_roundtrip_encoded(src, width, height, format, step, encoded, encodedSize);
		free(encoded);
	}

	{
		const UINT32 width = 1270;
		const UINT32 height = 1;
		const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
		BYTE src[5080] = { 0 };
		BYTE* encoded = nullptr;
		UINT32 encodedSize = 0;

		for (UINT32 x = 0; x < width; x++)
		{
			const BYTE color = WINPR_ASSERTING_INT_CAST(BYTE, x / 10);
			clear_test_write_color(&src[1ULL * x * 4], format, (BYTE)(color + 2),
			                       (BYTE)(color + 1), color);
		}

		rc = rc && clear_test_encode_image(src, width, height, &encoded, &encodedSize);
		rc = rc &&
		     clear_test_roundtrip_encoded(src, width, height, format, step, encoded, encodedSize);
		free(encoded);
	}

	{
		const UINT32 width = 40;
		const UINT32 height = 127;
		const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
		BYTE* src = calloc(1ULL * step, height);
		BYTE* encoded = nullptr;
		UINT32 encodedSize = 0;
		BYTE subcodecId = 0;

		if (!src)
			return FALSE;

		for (UINT32 y = 0; y < height; y++)
		{
			BYTE* line = &src[1ULL * y * step];
			const BYTE a = (BYTE)(1 + y);
			const BYTE b = (BYTE)(1 + ((y + 1) % 127));

			for (UINT32 x = 0; x < width; x++)
			{
				const BYTE seed = (x < 10 || (x >= 20 && x < 30)) ? a : b;
				clear_test_write_color(&line[1ULL * x * 4], format, seed, (BYTE)(seed + 1),
				                       (BYTE)(seed + 2));
			}
		}

		rc = rc && clear_test_encode_image(src, width, height, &encoded, &encodedSize);
		rc = rc && clear_test_encoded_subcodec_id(encoded, encodedSize, &subcodecId) &&
		     (subcodecId == 2);
		rc = rc &&
		     clear_test_roundtrip_encoded(src, width, height, format, step, encoded, encodedSize);
		free(encoded);
		free(src);
	}

	{
		const UINT32 width = 10;
		const UINT32 height = 128;
		const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
		BYTE* src = calloc(1ULL * step, height);
		BYTE* encoded = nullptr;
		UINT32 encodedSize = 0;
		BYTE subcodecId = 0;

		if (!src)
			return FALSE;

		for (UINT32 y = 0; y < height; y++)
		{
			BYTE* line = &src[1ULL * y * step];
			for (UINT32 x = 0; x < width; x++)
			{
				const BYTE seed = (BYTE)(1 + y);
				clear_test_write_color(&line[1ULL * x * 4], format, seed, (BYTE)(seed + 1),
				                       (BYTE)(seed + 2));
			}
		}

		rc = rc && clear_test_encode_image(src, width, height, &encoded, &encodedSize);
		if (clear_test_encoded_subcodec_id(encoded, encodedSize, &subcodecId))
			rc = rc && (subcodecId != 2);
		rc = rc &&
		     clear_test_roundtrip_encoded(src, width, height, format, step, encoded, encodedSize);
		free(encoded);
		free(src);
	}

	return rc;
}

static BOOL TestClearEncodeRlexWireShape(void)
{
	const UINT32 width = 64;
	const UINT32 height = 1;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE src[256] = { 0 };
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	BOOL rc = FALSE;

	for (UINT32 x = 0; x < width; x++)
	{
		if ((x % 2) == 0)
			clear_test_write_color(&src[1ULL * x * 4], format, 0x10, 0x20, 0x30);
		else
			clear_test_write_color(&src[1ULL * x * 4], format, 0x40, 0x50, 0x60);
	}

	if (!clear_test_encode_image(src, width, height, &encoded, &encodedSize))
		goto fail;

	const UINT32 subcodecOffset = (encoded && (encoded[0] & 0x01)) ? 16 : 14;
	if (!encoded || (encodedSize < 36) || (clear_test_residual_byte_count(encoded) != 0) ||
	    (clear_test_bands_byte_count(encoded) != 0) || (clear_test_subcodec_byte_count(encoded) == 0))
		goto fail;

	if ((clear_test_read_uint16_le(&encoded[subcodecOffset]) != 0) ||
	    (clear_test_read_uint16_le(&encoded[subcodecOffset + 2]) != 0) ||
	    (clear_test_read_uint16_le(&encoded[subcodecOffset + 4]) != width) ||
	    (clear_test_read_uint16_le(&encoded[subcodecOffset + 6]) != height) ||
	    (encoded[subcodecOffset + 12] != 2))
		goto fail;

	const UINT32 rlexByteCount = clear_test_read_uint32_le(&encoded[subcodecOffset + 8]);
	if ((rlexByteCount != (clear_test_subcodec_byte_count(encoded) - 13)) ||
	    (encoded[subcodecOffset + 13] != 2))
		goto fail;

	if ((encoded[subcodecOffset + 14] != 0x30) || (encoded[subcodecOffset + 15] != 0x20) ||
	    (encoded[subcodecOffset + 16] != 0x10) || (encoded[subcodecOffset + 17] != 0x60) ||
	    (encoded[subcodecOffset + 18] != 0x50) || (encoded[subcodecOffset + 19] != 0x40))
		goto fail;

	for (UINT32 offset = subcodecOffset + 20; offset < encodedSize; offset += 2)
	{
		if ((encoded[offset] != 0x03) || (encoded[offset + 1] != 0x00))
			goto fail;
	}

	rc = clear_test_roundtrip_encoded(src, width, height, format, step, encoded, encodedSize);

fail:
	free(encoded);
	return rc;
}

static BOOL TestClearEncodeRlexCompressionWin(void)
{
	const UINT32 width = 64;
	const UINT32 height = 1;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE src[256] = { 0 };
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	BYTE subcodecId = 0;
	BOOL rc = FALSE;

	for (UINT32 x = 0; x < width; x++)
	{
		if ((x % 2) == 0)
			clear_test_write_color(&src[1ULL * x * 4], format, 0x10, 0x20, 0x30);
		else
			clear_test_write_color(&src[1ULL * x * 4], format, 0x40, 0x50, 0x60);
	}

	if (!clear_test_encode_image(src, width, height, &encoded, &encodedSize))
		goto fail;

	rc = clear_test_encoded_subcodec_id(encoded, encodedSize, &subcodecId) && (subcodecId == 2) &&
	     (encodedSize < (14 + 13 + width * height * 3)) &&
	     clear_test_roundtrip_encoded(src, width, height, format, step, encoded, encodedSize);

fail:
	free(encoded);
	return rc;
}

static BOOL TestClearEncodeNscSubcodecRoundtrip(void)
{
	const UINT32 width = 256;
	const UINT32 height = 128;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE* src = calloc(1ULL * step, height);
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	BYTE subcodecId = 0;
	BOOL rc = FALSE;

	if (!src)
		goto fail;

	for (UINT32 y = 0; y < height; y++)
	{
		BYTE* line = &src[1ULL * y * step];
		for (UINT32 x = 0; x < width; x++)
		{
			const BYTE value = (BYTE)x;
			clear_test_write_color(&line[1ULL * x * 4], format, 0, 0, value);
		}
	}

	if (!clear_test_encode_image_nsc_tolerance(src, width, height, 1, &encoded, &encodedSize))
		goto fail;

	rc = clear_test_encoded_subcodec_id(encoded, encodedSize, &subcodecId) && (subcodecId == 1) &&
	     (encodedSize < (14 + 13 + width * height * 3)) &&
	     clear_test_roundtrip_encoded_tolerance(src, width, height, format, step, encoded,
	                                           encodedSize, 1);

fail:
	free(encoded);
	free(src);
	return rc;
}

static BOOL TestClearEncodeNscFallbackWhenLarger(void)
{
	const UINT32 width = 10;
	const UINT32 height = 128;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE* src = calloc(1ULL * step, height);
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	BYTE subcodecId = 0;
	BOOL rc = FALSE;

	if (!src)
		goto fail;

	for (UINT32 y = 0; y < height; y++)
	{
		BYTE* line = &src[1ULL * y * step];
		for (UINT32 x = 0; x < width; x++)
		{
			const BYTE seed = (BYTE)(1 + y);
			clear_test_write_color(&line[1ULL * x * 4], format, seed, (BYTE)(seed + 1),
			                       (BYTE)(seed + 2));
		}
	}

	if (!clear_test_encode_image(src, width, height, &encoded, &encodedSize))
		goto fail;

	rc = clear_test_roundtrip_encoded(src, width, height, format, step, encoded, encodedSize);
	if (clear_test_encoded_subcodec_id(encoded, encodedSize, &subcodecId))
		rc = rc && (subcodecId != 1);

fail:
	free(encoded);
	free(src);
	return rc;
}

static BOOL TestClearEncodeNscRepeatedLoop(void)
{
	const UINT32 width = 256;
	const UINT32 height = 128;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE* src = calloc(1ULL * step, height);
	BOOL rc = TRUE;

	if (!src)
		return FALSE;

	for (UINT32 y = 0; y < height; y++)
	{
		BYTE* line = &src[1ULL * y * step];
		for (UINT32 x = 0; x < width; x++)
		{
			const BYTE value = (BYTE)x;
			clear_test_write_color(&line[1ULL * x * 4], format, 0, 0, value);
		}
	}

	for (UINT32 index = 0; index < 16; index++)
	{
		BYTE* encoded = nullptr;
		UINT32 encodedSize = 0;
		BYTE subcodecId = 0;

		rc = rc &&
		     clear_test_encode_image_nsc_tolerance(src, width, height, 1, &encoded, &encodedSize);
		rc = rc && clear_test_encoded_subcodec_id(encoded, encodedSize, &subcodecId) &&
		     (subcodecId == 1);
		rc = rc && clear_test_roundtrip_encoded_tolerance(src, width, height, format, step,
		                                                  encoded, encodedSize, 1);
		free(encoded);

		if (!rc)
			break;
	}

	free(src);
	return rc;
}

static BOOL TestClearEncodeGlyphHitAfterMiss(void)
{
	const UINT32 width = 16;
	const UINT32 height = 16;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE src[16 * 16 * 4] = { 0 };
	BYTE dst[16 * 16 * 4] = { 0 };
	BYTE* miss = nullptr;
	BYTE* hit = nullptr;
	UINT32 missSize = 0;
	UINT32 hitSize = 0;
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	CLEAR_CONTEXT* decoder = clear_context_new(FALSE);
	BOOL rc = FALSE;

	clear_test_fill_unique_image(src, width, height);

	if (!encoder || !decoder)
		goto fail;

	if (clear_encode(encoder, src, format, step, 0, 0, width, height, &miss, &missSize,
	                 nullptr) != 0)
		goto fail;
	if (!miss || (missSize <= 16) || (miss[0] != 0x01) || (miss[1] != 0) ||
	    (clear_test_read_uint16_le(&miss[2]) != 0))
		goto fail;
	if ((clear_test_residual_byte_count(&miss[2]) == 0) &&
	    (clear_test_subcodec_byte_count(&miss[2]) == 0))
		goto fail;
	if (clear_decompress(decoder, miss, missSize, width, height, dst, format, step, 0, 0, width,
	                     height, nullptr) != 0)
		goto fail;
	if (!clear_test_compare_images(src, dst, format, width, height, step))
		goto fail;

	ZeroMemory(dst, sizeof(dst));
	if (clear_encode(encoder, src, format, step, 0, 0, width, height, &hit, &hitSize, nullptr) !=
	    0)
		goto fail;
	if (!hit || (hitSize != 4) || (hit[0] != 0x03) || (hit[1] != 1) ||
	    (clear_test_read_uint16_le(&hit[2]) != 0))
		goto fail;
	if (clear_decompress(decoder, hit, hitSize, width, height, dst, format, step, 0, 0, width,
	                     height, nullptr) != 0)
		goto fail;

	rc = clear_test_compare_images(src, dst, format, width, height, step);

fail:
	clear_context_free(decoder);
	clear_context_free(encoder);
	free(hit);
	free(miss);
	return rc;
}

static BOOL TestClearEncodeGlyphOversizedBoundary(void)
{
	const UINT32 width = 33;
	const UINT32 height = 32;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE* src = calloc(1ULL * step, height);
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	BOOL rc = FALSE;

	if (!src)
		goto fail;

	clear_test_fill_unique_image(src, width, height);

	if (!clear_test_encode_image(src, width, height, &encoded, &encodedSize))
		goto fail;
	if (!encoded || (encodedSize < 14) || ((encoded[0] & 0x03) != 0))
		goto fail;

	rc = clear_test_roundtrip_encoded(src, width, height, format, step, encoded, encodedSize);

fail:
	free(encoded);
	free(src);
	return rc;
}

static BOOL TestClearEncodeGlyphSurvivesContextReset(void)
{
	const UINT32 width = 16;
	const UINT32 height = 16;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE src[16 * 16 * 4] = { 0 };
	BYTE dst[16 * 16 * 4] = { 0 };
	BYTE* miss = nullptr;
	BYTE* hit = nullptr;
	UINT32 missSize = 0;
	UINT32 hitSize = 0;
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	CLEAR_CONTEXT* decoder = clear_context_new(FALSE);
	BOOL rc = FALSE;

	clear_test_fill_unique_image(src, width, height);

	if (!encoder || !decoder)
		goto fail;
	if (clear_encode(encoder, src, format, step, 0, 0, width, height, &miss, &missSize,
	                 nullptr) != 0)
		goto fail;
	if (clear_decompress(decoder, miss, missSize, width, height, dst, format, step, 0, 0, width,
	                     height, nullptr) != 0)
		goto fail;
	if (!clear_context_reset(encoder) || !clear_context_reset(decoder))
		goto fail;
	ZeroMemory(dst, sizeof(dst));

	if (clear_encode(encoder, src, format, step, 0, 0, width, height, &hit, &hitSize, nullptr) !=
	    0)
		goto fail;
	if (!hit || (hitSize != 4) || (hit[0] != 0x03) || (hit[1] != 0) ||
	    (clear_test_read_uint16_le(&hit[2]) != 0))
		goto fail;
	if (clear_decompress(decoder, hit, hitSize, width, height, dst, format, step, 0, 0, width,
	                     height, nullptr) != 0)
		goto fail;

	rc = clear_test_compare_images(src, dst, format, width, height, step);

fail:
	clear_context_free(decoder);
	clear_context_free(encoder);
	free(hit);
	free(miss);
	return rc;
}

static BOOL TestClearEncodeGlyphEvictionReusesIndex(void)
{
	const UINT32 width = 2;
	const UINT32 height = 2;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE original[2 * 2 * 4] = { 0 };
	BYTE current[2 * 2 * 4] = { 0 };
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	BOOL rc = FALSE;

	clear_test_fill_unique_image(original, width, height);

	if (!encoder)
		goto fail;
	if (clear_encode(encoder, original, format, step, 0, 0, width, height, &encoded,
	                 &encodedSize, nullptr) != 0)
		goto fail;
	if (!encoded || (encoded[0] != 0x01) || (clear_test_read_uint16_le(&encoded[2]) != 0))
		goto fail;
	free(encoded);
	encoded = nullptr;

	for (UINT32 index = 1; index <= 4000; index++)
	{
		for (UINT32 pixel = 0; pixel < (width * height); pixel++)
		{
			const BYTE seed = (BYTE)((index + pixel * 53) & 0xFF);
			clear_test_write_color(&current[1ULL * pixel * 4], format, seed,
			                       (BYTE)(seed ^ 0x5A), (BYTE)(seed + index / 17));
		}

		if (clear_encode(encoder, current, format, step, 0, 0, width, height, &encoded,
		                 &encodedSize, nullptr) != 0)
			goto fail;
		free(encoded);
		encoded = nullptr;
	}

	if (clear_encode(encoder, original, format, step, 0, 0, width, height, &encoded,
	                 &encodedSize, nullptr) != 0)
		goto fail;
	if (!encoded || (encoded[0] != 0x01) || (encodedSize <= 16) ||
	    (clear_test_read_uint16_le(&encoded[2]) != 1))
		goto fail;
	free(encoded);
	encoded = nullptr;

	if (clear_encode(encoder, original, format, step, 0, 0, width, height, &encoded,
	                 &encodedSize, nullptr) != 0)
		goto fail;
	rc = encoded && (encodedSize == 4) && (encoded[0] == 0x03) &&
	     (clear_test_read_uint16_le(&encoded[2]) == 1);

fail:
	clear_context_free(encoder);
	free(encoded);
	return rc;
}

static void clear_test_fill_vbar_image(BYTE* src, UINT32 width, UINT32 height)
{
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);

	for (UINT32 y = 0; y < height; y++)
	{
		BYTE* line = &src[1ULL * y * step];
		for (UINT32 x = 0; x < width; x++)
		{
			const BYTE seed = (BYTE)((x * 11 + y * 7 + 5) & 0xFF);
			clear_test_write_color(&line[1ULL * x * 4], format, seed, (BYTE)(seed + 31),
			                       (BYTE)(seed + 63));
		}
	}
}

static BOOL TestClearEncodeVBarHeightBoundary(void)
{
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	static const UINT32 heights[] = { 1, 52, 53 };
	BOOL rc = TRUE;

	for (size_t index = 0; index < ARRAYSIZE(heights); index++)
	{
		const UINT32 height = heights[index];
		const UINT32 width = 32;
		const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
		BYTE* src = calloc(1ULL * step, height);
		BYTE* encoded = nullptr;
		UINT32 encodedSize = 0;
		BOOL current = FALSE;

		if (!src)
			return FALSE;

		clear_test_fill_vbar_image(src, width, height);

		if (!clear_test_encode_image(src, width, height, &encoded, &encodedSize))
			goto case_done;

		if (height <= 52)
		{
			const UINT32 compositeOffset = clear_test_composite_offset(encoded);
			const UINT32 bandOffset = compositeOffset + 12;
			const UINT32 bandsSize = 11 + width * (2 + height * 3);

			if (!encoded || (encodedSize != (bandOffset + bandsSize)) ||
			    (clear_test_residual_byte_count(encoded) != 0) ||
			    (clear_test_bands_byte_count(encoded) != bandsSize) ||
			    (clear_test_subcodec_byte_count(encoded) != 0))
				goto case_done;
			if ((clear_test_read_uint16_le(&encoded[bandOffset]) != 0) ||
			    (clear_test_read_uint16_le(&encoded[bandOffset + 2]) != (width - 1)) ||
			    (clear_test_read_uint16_le(&encoded[bandOffset + 4]) != 0) ||
			    (clear_test_read_uint16_le(&encoded[bandOffset + 6]) != (height - 1)))
				goto case_done;
			if (clear_test_read_uint16_le(&encoded[bandOffset + 11]) != (height << 8))
				goto case_done;
		}
		else if (encoded && (clear_test_bands_byte_count(encoded) != 0))
			goto case_done;

		current = clear_test_roundtrip_encoded(src, width, height, format, step, encoded,
		                                       encodedSize);

	case_done:
		if (!current)
			(void)printf("vbar height boundary failed for height %" PRIu32 "\n", height);
		rc = rc && current;
		free(encoded);
		free(src);
	}

	return rc;
}

static BOOL TestClearEncodeVBarCacheHitAfterMiss(void)
{
	const UINT32 width = 32;
	const UINT32 height = 52;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE* src = calloc(1ULL * step, height);
	BYTE* dst = calloc(1ULL * step, height);
	BYTE* miss = nullptr;
	BYTE* hit = nullptr;
	UINT32 missSize = 0;
	UINT32 hitSize = 0;
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	CLEAR_CONTEXT* decoder = clear_context_new(FALSE);
	BOOL rc = FALSE;

	if (!src || !dst || !encoder || !decoder)
		goto fail;
	clear_test_fill_vbar_image(src, width, height);

	if (clear_encode(encoder, src, format, step, 0, 0, width, height, &miss, &missSize,
	                 nullptr) != 0)
		goto fail;
	if (clear_decompress(decoder, miss, missSize, width, height, dst, format, step, 0, 0, width,
	                     height, nullptr) != 0)
		goto fail;
	if (!clear_test_compare_images(src, dst, format, width, height, step))
		goto fail;

	ZeroMemory(dst, 1ULL * step * height);
	if (clear_encode(encoder, src, format, step, 0, 0, width, height, &hit, &hitSize, nullptr) !=
	    0)
		goto fail;

	const UINT32 compositeOffset = clear_test_composite_offset(hit);
	const UINT32 bandOffset = compositeOffset + 12;
	const UINT32 hitBandsSize = 11 + width * 2;
	if (!hit || (clear_test_bands_byte_count(hit) != hitBandsSize) ||
	    (hitSize != (bandOffset + hitBandsSize)) ||
	    ((clear_test_read_uint16_le(&hit[bandOffset + 11]) & 0x8000) != 0x8000))
		goto fail;
	if (clear_decompress(decoder, hit, hitSize, width, height, dst, format, step, 0, 0, width,
	                     height, nullptr) != 0)
		goto fail;

	rc = clear_test_compare_images(src, dst, format, width, height, step);

fail:
	clear_context_free(decoder);
	clear_context_free(encoder);
	free(hit);
	free(miss);
	free(dst);
	free(src);
	return rc;
}

static BOOL TestClearEncodeVBarCacheReset(void)
{
	const UINT32 width = 32;
	const UINT32 height = 52;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE* src = calloc(1ULL * step, height);
	BYTE* dst = calloc(1ULL * step, height);
	BYTE* miss = nullptr;
	BYTE* resetMiss = nullptr;
	UINT32 missSize = 0;
	UINT32 resetMissSize = 0;
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	CLEAR_CONTEXT* decoder = clear_context_new(FALSE);
	BOOL rc = FALSE;

	if (!src || !dst || !encoder || !decoder)
		goto fail;
	clear_test_fill_vbar_image(src, width, height);

	if (clear_encode(encoder, src, format, step, 0, 0, width, height, &miss, &missSize,
	                 nullptr) != 0)
		goto fail;
	if (clear_decompress(decoder, miss, missSize, width, height, dst, format, step, 0, 0, width,
	                     height, nullptr) != 0)
		goto fail;
	if (!clear_context_reset(encoder) || !clear_context_reset(decoder))
		goto fail;
	ZeroMemory(dst, 1ULL * step * height);

	if (clear_encode(encoder, src, format, step, 0, 0, width, height, &resetMiss,
	                 &resetMissSize, nullptr) != 0)
		goto fail;
	if (!resetMiss || ((resetMiss[0] & 0x04) != 0x04) ||
	    (clear_test_bands_byte_count(resetMiss) == 0))
		goto fail;
	if (clear_decompress(decoder, resetMiss, resetMissSize, width, height, dst, format, step, 0,
	                     0, width, height, nullptr) != 0)
		goto fail;

	rc = clear_test_compare_images(src, dst, format, width, height, step);

fail:
	clear_context_free(decoder);
	clear_context_free(encoder);
	free(resetMiss);
	free(miss);
	free(dst);
	free(src);
	return rc;
}

static BOOL TestClearEncodeVBarCursorWrap(void)
{
	const UINT32 width = 64;
	const UINT32 height = 52;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	const UINT32 step = width * FreeRDPGetBytesPerPixel(format);
	BYTE* src = calloc(1ULL * step, height);
	BYTE* encoded = nullptr;
	UINT32 encodedSize = 0;
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	BOOL rc = FALSE;

	if (!src || !encoder)
		goto fail;

	for (UINT32 frame = 0; frame < 513; frame++)
	{
		clear_test_fill_vbar_image(src, width, height);
		for (UINT32 pixel = 0; pixel < width; pixel++)
		{
			src[pixel * 4] = (BYTE)(src[pixel * 4] + frame);
			src[pixel * 4 + 1] = (BYTE)(src[pixel * 4 + 1] + (frame >> 8));
		}

		if (clear_encode(encoder, src, format, step, 0, 0, width, height, &encoded,
		                 &encodedSize, nullptr) != 0)
			goto fail;
		if (!encoded || (clear_test_bands_byte_count(encoded) == 0))
			goto fail;
		free(encoded);
		encoded = nullptr;
	}

	clear_test_fill_vbar_image(src, width, height);
	if (clear_encode(encoder, src, format, step, 0, 0, width, height, &encoded, &encodedSize,
	                 nullptr) != 0)
		goto fail;

	const UINT32 compositeOffset = clear_test_composite_offset(encoded);
	const UINT32 bandOffset = compositeOffset + 12;
	rc = encoded && (clear_test_bands_byte_count(encoded) != 0) &&
	     (clear_test_read_uint16_le(&encoded[bandOffset + 11]) == (height << 8));

fail:
	clear_context_free(encoder);
	free(encoded);
	free(src);
	return rc;
}

int TestFreeRDPCodecClear(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* Example 1 needs a filled glyph cache
	if (!test_ClearDecompressExample(1, 8, 9, TEST_CLEAR_EXAMPLE_1,
	                                 sizeof(TEST_CLEAR_EXAMPLE_1)))
	    return -1;
	*/
	if (!test_ClearDecompressExample(2, 78, 17, TEST_CLEAR_EXAMPLE_2, sizeof(TEST_CLEAR_EXAMPLE_2)))
		return -1;

	if (!test_ClearDecompressExample(3, 64, 24, TEST_CLEAR_EXAMPLE_3, sizeof(TEST_CLEAR_EXAMPLE_3)))
		return -1;

	if (!test_ClearDecompressExample(4, 7, 15, TEST_CLEAR_EXAMPLE_4, sizeof(TEST_CLEAR_EXAMPLE_4)))
		return -1;

#if !defined(WITHOUT_FREERDP_3x_DEPRECATED)
	if (!TestClearEncodeRejectsBrokenDeprecatedContract())
		return -1;
#endif

	if (!TestClearEncodeRejectsDecoderContext())
		return -1;

	if (!TestClearEncodeRejectsInvalidArguments())
		return -1;

	if (!TestClearEncodeRejectsBoundaryArguments())
		return -1;
	(void)printf("TestClearEncodeRejectsBoundaryArguments passed\n");

	if (!TestClearEncodeRandomGeneratedRoundtrips())
		return -1;
	(void)printf("TestClearEncodeRandomGeneratedRoundtrips passed\n");

	if (!TestClearEncodeSequenceStartsAtZero())
		return -1;

	if (!TestClearEncodeResidualRoundtripSmallBgrx())
		return -1;
	(void)printf("TestClearEncodeResidualRoundtripSmallBgrx passed\n");

	if (!TestClearEncodeResidualRunLengthWireBoundaries())
		return -1;
	(void)printf("TestClearEncodeResidualRunLengthWireBoundaries passed\n");

	if (!TestClearEncodeResidualSequenceWrapAndReset())
		return -1;
	(void)printf("TestClearEncodeResidualSequenceWrapAndReset passed\n");

	if (!TestClearDecodeRejectsMalformedResidualStreams())
		return -1;
	(void)printf("TestClearDecodeRejectsMalformedResidualStreams passed\n");

	if (!TestClearDecodeRejectsMalformedTopLevelStreams())
		return -1;
	(void)printf("TestClearDecodeRejectsMalformedTopLevelStreams passed\n");

	if (!TestClearDecodeRejectsMalformedGlyphStreams())
		return -1;
	(void)printf("TestClearDecodeRejectsMalformedGlyphStreams passed\n");

	if (!TestClearDecodeRejectsMalformedVBarStreams())
		return -1;
	(void)printf("TestClearDecodeRejectsMalformedVBarStreams passed\n");

	if (!TestClearEncodeRawSubcodecRoundtrip())
		return -1;
	(void)printf("TestClearEncodeRawSubcodecRoundtrip passed\n");

	if (!TestClearEncodeStrategyNotLargerThanRaw())
		return -1;
	(void)printf("TestClearEncodeStrategyNotLargerThanRaw passed\n");

	if (!TestClearDecodeRejectsMalformedRawSubcodecStreams())
		return -1;
	(void)printf("TestClearDecodeRejectsMalformedRawSubcodecStreams passed\n");

	if (!TestClearDecodeRawSubcodecNonOverlappingRectangles())
		return -1;
	(void)printf("TestClearDecodeRawSubcodecNonOverlappingRectangles passed\n");

	if (!TestClearDecodeRejectsResidualAndRawSubcodecOverlap())
		return -1;
	(void)printf("TestClearDecodeRejectsResidualAndRawSubcodecOverlap passed\n");

	if (!TestClearDecodeRejectsMalformedRlexSubcodecStreams())
		return -1;
	(void)printf("TestClearDecodeRejectsMalformedRlexSubcodecStreams passed\n");

	if (!TestClearEncodeRlexPaletteBoundaries())
		return -1;
	(void)printf("TestClearEncodeRlexPaletteBoundaries passed\n");

	if (!TestClearEncodeRlexWireShape())
		return -1;
	(void)printf("TestClearEncodeRlexWireShape passed\n");

	if (!TestClearEncodeRlexCompressionWin())
		return -1;
	(void)printf("TestClearEncodeRlexCompressionWin passed\n");

	if (!TestClearEncodeNscSubcodecRoundtrip())
		return -1;
	(void)printf("TestClearEncodeNscSubcodecRoundtrip passed\n");

	if (!TestClearEncodeNscFallbackWhenLarger())
		return -1;
	(void)printf("TestClearEncodeNscFallbackWhenLarger passed\n");

	if (!TestClearEncodeNscRepeatedLoop())
		return -1;
	(void)printf("TestClearEncodeNscRepeatedLoop passed\n");

	if (!TestClearEncodeGlyphHitAfterMiss())
		return -1;
	(void)printf("TestClearEncodeGlyphHitAfterMiss passed\n");

	if (!TestClearEncodeGlyphOversizedBoundary())
		return -1;
	(void)printf("TestClearEncodeGlyphOversizedBoundary passed\n");

	if (!TestClearEncodeGlyphSurvivesContextReset())
		return -1;
	(void)printf("TestClearEncodeGlyphSurvivesContextReset passed\n");

	if (!TestClearEncodeGlyphEvictionReusesIndex())
		return -1;
	(void)printf("TestClearEncodeGlyphEvictionReusesIndex passed\n");

	if (!TestClearEncodeVBarHeightBoundary())
		return -1;
	(void)printf("TestClearEncodeVBarHeightBoundary passed\n");

	if (!TestClearEncodeVBarCacheHitAfterMiss())
		return -1;
	(void)printf("TestClearEncodeVBarCacheHitAfterMiss passed\n");

	if (!TestClearEncodeVBarCacheReset())
		return -1;
	(void)printf("TestClearEncodeVBarCacheReset passed\n");

	if (!TestClearEncodeVBarCursorWrap())
		return -1;
	(void)printf("TestClearEncodeVBarCursorWrap passed\n");

	return 0;
}
