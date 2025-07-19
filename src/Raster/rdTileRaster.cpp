#include "fixed.h"

#include <math.h>
#include <bitset>

#include "rdRaster.h"

//typedef numeric::fixed<24, 8> fixed_t;
typedef float fixed_t;

//static inline __m128i mm_min_epu16_sse2(__m128i a, __m128i b)
//{
//	__m128i mask = _mm_cmplt_epi16(_mm_xor_si128(a, _mm_set1_epi16(0x8000)),
//								   _mm_xor_si128(b, _mm_set1_epi16(0x8000)));
//	return _mm_or_si128(_mm_and_si128(mask, a), _mm_andnot_si128(mask, b));
//}

typedef struct rdTriSetup
{
	fixed_t A0, A1, A2;
	fixed_t B0, B1, B2;
	fixed_t C0, C1, C2;
	fixed_t area;
} rdTriSetup;

typedef struct rdPoint2Fixed
{
	fixed_t x, y;
} rdPoint2Fixed;

typedef struct rdTri8
{
	uint8_t i1, i2, i3;
} rdTri8;

//flex_t stdMath_Rcp(fixed_t x)
//{
//	return 1.0f / (flex_t)x;
//}

flex_t stdMath_Rcp(flex_t x)
{
	//flex_t rcp = _mm_cvtss_f32(_mm_rcp_ss(_mm_set_ss(x)));
	//rcp = rcp * (2.0f - x * rcp);
	//return rcp;

	__m128 val = _mm_set_ss(x);               // Load scalar
	__m128 rcp = _mm_rcp_ss(val);             // Approximate (12-bit)
	rcp = _mm_mul_ss(rcp, _mm_sub_ss(_mm_set_ss(2.0f),
									 _mm_mul_ss(rcp, val)));    // Refine to ~23-bit precision
	return _mm_cvtss_f32(rcp);
//		union { float f; uint32_t i; } conv;
//		conv.f = x;
//
//		// Magic constant for reciprocal guess:
//		// Found empirically or through research (similar to fast inverse sqrt)
//		conv.i = 0x7EF311C3 - conv.i;
//
//		float y = conv.f;
//		y = y * (2.0f - x * y);
//		y = y * (2.0f - x * y);
//
//		return y;
	
	//unsigned i = *(unsigned*)&f;
	//i = 0x7effffffU - i;
	//return *(float*)&i;
}

#ifdef __cplusplus
extern "C" {
#endif

#include "Engine/rdroid.h"
#include "Engine/rdActive.h"
#include "Platform/std3D.h"
#include "Engine/rdColormap.h"
#include "General/stdMath.h"
#include "Win95/stdDisplay.h"

#undef min
#undef max

#ifdef TILE_SW_RASTER

	// per-thread tile framebuffer cache
	static thread_local uint8_t rdRaster_TileColor[RDCACHE_FINE_TILE_SIZE * RDCACHE_FINE_TILE_SIZE];
	static thread_local uint16_t rdRaster_TileDepth[RDCACHE_FINE_TILE_SIZE * RDCACHE_FINE_TILE_SIZE];
	//static thread_local uint16_t rdRaster_TileHiZ = 0xFFFF;

	// pre-apply extra light and ambient for vertices
	void rdRaster_BlendVertexIntensities(rdProcEntry* face)
	{
		float ambient = (rdroid_curRenderOptions & 2) ? face->ambientLight : 0.0f;
		for (uint32_t i = 0; i < face->numVertices; ++i)
		{
			float sum = stdMath_Saturate(face->extralight + face->vertexIntensities[i]);
			float lit = stdMath_Saturate(ambient < sum ? sum : ambient);
			face->vertexIntensities[i] = lit * 63.0f;
		}
	}

	void rdRaster_ClearBins()
	{
		rdCanvas* pCanvas = rdCamera_pCurCamera->canvas;
		memset(rdCamera_pCurCamera->canvas->coarseTileBits, 0, sizeof(uint64_t) * pCanvas->coarseTileCount * RDCACHE_TILE_BINNING_STRIDE);
		memset(rdCamera_pCurCamera->canvas->tileBits, 0, sizeof(uint64_t) * pCanvas->tileCount * RDCACHE_TILE_BINNING_STRIDE);
	}
	
	// Coarse tile binning
	// TILETODO: consider binning by geomode/texmode/lightmode up front so we can flush in order
	void rdRaster_BinFaceCoarse(rdProcEntry* face)
	{
		rdCanvas* pCanvas = rdCamera_pCurCamera->canvas;
		uint64_t* coarseTileBits = pCanvas->coarseTileBits;
		int coarseTileWidth = pCanvas->coarseTileWidth;

		if (!face || face->geometryMode == 0 || !face->material)
			return;

		//int geometryMode = rdroid_curGeometryMode < face->geometryMode ? rdroid_curGeometryMode : face->geometryMode;
		int lightingMode = rdroid_curLightingMode < face->lightingMode ? rdroid_curLightingMode : face->lightingMode;
		//int textureMode = rdroid_curTextureMode < face->textureMode ? rdroid_curTextureMode : face->textureMode;

		//rdMaterial* mat = face->material;
		//int cel = (face->wallCel == 0xFFFFFFFF) ? face->material->celIdx : (int)face->wallCel;
		//rdTexinfo* texinfo = face->material->texinfos[stdMath_ClampInt(cel, 0, face->material->num_texinfo - 1)];
		//
		//if (geometryMode == 4 && !(texinfo->header.texture_type & 8))
		//	geometryMode = 3;
		
		int coarseMinX = stdMath_ClampInt((int32_t)(face->x_min / RDCACHE_COARSE_TILE_SIZE), 0, pCanvas->coarseTileWidth - 1);
		int coarseMaxX = stdMath_ClampInt((int32_t)(face->x_max / RDCACHE_COARSE_TILE_SIZE), 0, pCanvas->coarseTileWidth - 1);
		int coarseMinY = stdMath_ClampInt((int32_t)(face->y_min / RDCACHE_COARSE_TILE_SIZE), 0, pCanvas->coarseTileHeight - 1);
		int coarseMaxY = stdMath_ClampInt((int32_t)(face->y_max / RDCACHE_COARSE_TILE_SIZE), 0, pCanvas->coarseTileHeight - 1);

		if (lightingMode == RD_LIGHTMODE_GOURAUD)
			rdRaster_BlendVertexIntensities(face);

		//int idx = (int)(entry - rdCache_aTileEntries);
		int idx = (int)(face - rdCache_aProcFaces);
		for (int cy = coarseMinY; cy <= coarseMaxY; cy++)
		{
			for (int cx = coarseMinX; cx <= coarseMaxX; cx++)
			{
				const int linear_tile = cy * coarseTileWidth + cx;
				const int binned_bitmask_offset = (RDCACHE_TILE_BINNING_STRIDE * linear_tile);

				const uint32_t bin_index = idx / 64;
				const uint32_t bin_place = idx % 64;

				coarseTileBits[binned_bitmask_offset + bin_index] |= (1ull << bin_place);
			}
		}
	}

	// Fine tile binning
	void rdRaster_BinFaces()
	{
		rdCanvas* pCanvas = rdCamera_pCurCamera->canvas;
		for (int coarseTileIdx = 0; coarseTileIdx < pCanvas->coarseTileCount; ++coarseTileIdx)
		{
			uint64_t* coarseMask = &pCanvas->coarseTileBits[coarseTileIdx * RDCACHE_TILE_BINNING_STRIDE];
			int coarseTileX = coarseTileIdx % pCanvas->coarseTileWidth;
			int coarseTileY = coarseTileIdx / pCanvas->coarseTileWidth;

			int coarseX0 = coarseTileX * RDCACHE_COARSE_TILE_SIZE;
			int coarseY0 = coarseTileY * RDCACHE_COARSE_TILE_SIZE;
			int coarseX1 = coarseX0 + RDCACHE_COARSE_TILE_SIZE - 1;
			int coarseY1 = coarseY0 + RDCACHE_COARSE_TILE_SIZE - 1;

			for (int triWord = 0; triWord < RDCACHE_TILE_BINNING_STRIDE; triWord++)
			{
				uint64_t maskWord = coarseMask[triWord];
				while (maskWord != 0)
				{
					int bitPos = stdMath_FindLSB64(maskWord);
					int triIndex = triWord * 64 + bitPos;
					maskWord ^= (1ULL << bitPos);

					rdProcEntry* tri = &rdCache_aProcFaces[triIndex];

					// Clip triangle bounds to coarse tile bounds
					int triMinX = tri->x_min > coarseX0 ? tri->x_min : coarseX0;
					int triMaxX = tri->x_max < coarseX1 ? tri->x_max : coarseX1;
					int triMinY = tri->y_min > coarseY0 ? tri->y_min : coarseY0;
					int triMaxY = tri->y_max < coarseY1 ? tri->y_max : coarseY1;

					// Convert to fine tile local coordinates
					int fineMinX = (triMinX - coarseX0) / RDCACHE_FINE_TILE_SIZE;
					int fineMaxX = (triMaxX - coarseX0) / RDCACHE_FINE_TILE_SIZE;
					int fineMinY = (triMinY - coarseY0) / RDCACHE_FINE_TILE_SIZE;
					int fineMaxY = (triMaxY - coarseY0) / RDCACHE_FINE_TILE_SIZE;

					// Bin triangle into fine tiles
					for (int fy = fineMinY; fy <= fineMaxY; fy++)
					{
						for (int fx = fineMinX; fx <= fineMaxX; fx++)
						{
							int fineTileX = coarseTileX * RDCACHE_FINE_PER_COARSE + fx;
							int fineTileY = coarseTileY * RDCACHE_FINE_PER_COARSE + fy;
							fineTileX = stdMath_ClampInt(fineTileX, 0, pCanvas->tileWidth - 1);
							fineTileY = stdMath_ClampInt(fineTileY, 0, pCanvas->tileHeight - 1);
							int fineTileIdx = fineTileY * pCanvas->tileWidth + fineTileX;
							int binned_bitmask_offset = (RDCACHE_TILE_BINNING_STRIDE * fineTileIdx);

							const uint32_t bin_index = triIndex / 64;
							const uint32_t bin_place = triIndex % 64;

							pCanvas->tileBits[binned_bitmask_offset + bin_index] |= (1ull << bin_place);

						}
					}
				}
			}
		}
	}

#ifdef TARGET_SSE
	// Copy framebuffer data to thread-local cache (improves cache access)
	void rdRaster_CopyToTileMem(int tileX, int tileY)
	{
		rdCanvas* pCanvas = rdCamera_pCurCamera->canvas;
		int width = pCanvas->vbuffer->format.width;
		int height = pCanvas->vbuffer->format.height;

		uint8_t* colorBuffer = (uint8_t*)pCanvas->vbuffer->surface_lock_alloc;
		uint16_t* zBuffer = (uint16_t*)pCanvas->d3d_vbuf->surface_lock_alloc;

		const int tileMinX = tileX * RDCACHE_FINE_TILE_SIZE;
		const int tileMinY = tileY * RDCACHE_FINE_TILE_SIZE;

		if (tileMinX >= width || tileMinY >= height)
			return;

		const int maxW = RDCACHE_FINE_TILE_SIZE;
		const int maxH = RDCACHE_FINE_TILE_SIZE;

		const int tileWidth = (tileMinX + maxW > width) ? (width - tileMinX) : maxW;
		const int tileHeight = (tileMinY + maxH > height) ? (height - tileMinY) : maxH;

		//rdRaster_TileHiZ = 0xFFFF;

		// Copy color buffer row-wise
		for (int row = 0; row < tileHeight; ++row)
		{
			const uint8_t* src = colorBuffer + (tileMinY + row) * width + tileMinX;
			uint8_t* dst = rdRaster_TileColor + row * maxW;

			// Copy 16 pixels at a time (8 bpp)
			for (int i = 0; i < tileWidth; i += 16)
			{
				__m128i v = _mm_loadu_si128((__m128i*)(src + i));
				_mm_storeu_si128((__m128i*)(dst + i), v);
			}

			// Clear remaining columns if needed
			if (tileWidth < maxW)
				memset(dst + tileWidth, 0, maxW - tileWidth);
		}

		// Clear remaining rows if needed
		for (int row = tileHeight; row < maxH; ++row)
			memset(rdRaster_TileColor + row * maxW, 0, maxW);
		
		// If the clear flag is set, just clear the tile depth outright
		if (pCanvas->bIdk & 4)
		{
			memset(rdRaster_TileDepth, 0xFFFF, sizeof(rdRaster_TileDepth));
		}
		else // Otherwise copy the depth buffer to the tile
		{
			// Copy depth buffer row-wise
			for (int row = 0; row < tileHeight; ++row)
			{
				const uint16_t* src = zBuffer + (tileMinY + row) * width + tileMinX;
				uint16_t* dst = (uint16_t*)rdRaster_TileDepth + row * maxW;

				//__m128i minZ = _mm_set1_epi16(rdRaster_TileHiZ);

				// Copy 8 pixels at a time (16 bpp)
				for (int i = 0; i < tileWidth; i += 8)
				{
					__m128i zval = _mm_loadu_si128((__m128i*)(src + i));
					_mm_storeu_si128((__m128i*)(dst + i), zval);

					// Update HiZ
					//minZ = mm_min_epu16_sse2(minZ, zval);
				}

				// Horizontal min
				//uint16_t zVals[8];
				//_mm_storeu_si128((__m128i*)zVals, minZ);
				//for (int i = 0; i < 8; ++i)
				//	if (zVals[i] < rdRaster_TileHiZ)
				//		rdRaster_TileHiZ = zVals[i];

				// Clear remaining columns if needed
				if (tileWidth < maxW)
					memset(dst + tileWidth, 0, (maxW - tileWidth) * sizeof(uint16_t));
			}

			// Clear remaining rows if needed
			for (int row = tileHeight; row < maxH; ++row)
				memset(rdRaster_TileDepth + row * maxW, 0, maxW * sizeof(uint16_t));
		}
	}

	// Copies the tile cache back to the framebuffer
	void rdRaster_CopyFromTileMem(int tileX, int tileY)
	{
		rdCanvas* pCanvas = rdCamera_pCurCamera->canvas;
		int width = pCanvas->vbuffer->format.width;
		int height = pCanvas->vbuffer->format.height;

		uint8_t* colorBuffer = (uint8_t*)pCanvas->vbuffer->surface_lock_alloc;
		uint16_t* zBuffer = (uint16_t*)pCanvas->d3d_vbuf->surface_lock_alloc;

		const int tileMinX = tileX * RDCACHE_FINE_TILE_SIZE;
		const int tileMinY = tileY * RDCACHE_FINE_TILE_SIZE;

		if (tileMinX >= width || tileMinY >= height)
			return;

		const int maxW = RDCACHE_FINE_TILE_SIZE;
		const int maxH = RDCACHE_FINE_TILE_SIZE;

		const int tileWidth = (tileMinX + maxW > width) ? (width - tileMinX) : maxW;
		const int tileHeight = (tileMinY + maxH > height) ? (height - tileMinY) : maxH;

		// Copy row by row for color buffer
		for (int row = 0; row < tileHeight; ++row)
		{
			uint8_t* dst = colorBuffer + (tileMinY + row) * width + tileMinX;
			const uint8_t* src = rdRaster_TileColor + row * maxW;

			// Copy 16 pixels at a time (8 bpp)
			for (int i = 0; i < tileWidth; i += 16)
			{
				__m128i v = _mm_loadu_si128((__m128i*)(src + i));
				_mm_storeu_si128((__m128i*)(dst + i), v);
			}
		}

		// Copy row by row for zbuffer
		for (int row = 0; row < tileHeight; ++row)
		{
			uint16_t* dst = zBuffer + (tileMinY + row) * width + tileMinX;
			const uint16_t* src = (uint16_t*)rdRaster_TileDepth + row * maxW;

			// Copy 8 pixels at a time (16 bpp)
			for (int i = 0; i < tileWidth; i += 8)
			{
				__m128i v = _mm_loadu_si128((__m128i*)(src + i));
				_mm_storeu_si128((__m128i*)(dst + i), v);
			}
		}
	}
#else

	void rdRaster_CopyToTileMem(int tileX, int tileY)
	{
		rdCanvas* pCanvas = rdCamera_pCurCamera->canvas;
		int width_in_pixels = pCanvas->vbuffer->format.width;
		int height_in_pixels = pCanvas->vbuffer->format.height;
		uint8_t* colorBuffer = (uint8_t*)pCanvas->vbuffer->surface_lock_alloc;
		uint16_t* zBuffer = (uint16_t*)(pCanvas->d3d_vbuf->surface_lock_alloc);

		int tileMinX = tileX * RDCACHE_FINE_TILE_SIZE;
		int tileMinY = tileY * RDCACHE_FINE_TILE_SIZE;

		// Check if tile start is outside framebuffer bounds
		if (tileMinX >= width_in_pixels || tileMinY >= height_in_pixels)
			return;

		int tileWidth = RDCACHE_FINE_TILE_SIZE;
		int tileHeight = RDCACHE_FINE_TILE_SIZE;

		// Clamp tile width and height to avoid reading outside
		tileWidth = width_in_pixels - tileMinX;
		if (tileWidth > RDCACHE_FINE_TILE_SIZE)
			tileWidth = RDCACHE_FINE_TILE_SIZE;

		tileHeight = height_in_pixels - tileMinY;
		if (tileHeight > RDCACHE_FINE_TILE_SIZE)
			tileHeight = RDCACHE_FINE_TILE_SIZE;

		if (tileWidth <= 0 || tileHeight <= 0)
			return;

		//rdRaster_TileHiZ = 0xFFFF;

		// Copy color buffer row-wise
		for (int row = 0; row < tileHeight; ++row)
		{
			uint8_t* src = colorBuffer + (tileMinY + row) * width_in_pixels + tileMinX;
			uint8_t* dst = rdRaster_TileColor + row * RDCACHE_FINE_TILE_SIZE;
			memcpy(dst, src, tileWidth);

			if (tileWidth < RDCACHE_FINE_TILE_SIZE)
				memset(dst + tileWidth, 0, RDCACHE_FINE_TILE_SIZE - tileWidth);
		}

		// Clear remaining rows if needed
		for (int row = tileHeight; row < RDCACHE_FINE_TILE_SIZE; ++row)
			memset(rdRaster_TileColor + row * RDCACHE_FINE_TILE_SIZE, 0, RDCACHE_FINE_TILE_SIZE);

		// If the clear flag is set, just clear the tile depth outright
		if (pCanvas->bIdk & 4)
		{
			memset(rdRaster_TileDepth, 0xFFFF, sizeof(rdRaster_TileDepth));
		}
		else
		{
			// Copy depth buffer row-wise
			for (int row = 0; row < tileHeight; ++row)
			{
				uint16_t* src = zBuffer + (tileMinY + row) * width_in_pixels + tileMinX;
				uint16_t* dst = (uint16_t*)(rdRaster_TileDepth)+row * RDCACHE_FINE_TILE_SIZE;
				memcpy(dst, src, tileWidth * sizeof(uint16_t));

				//for (int x = 0; x < tileWidth; ++x)
					//rdRaster_TileHiZ = src[x] < rdRaster_TileHiZ ? src[x] : rdRaster_TileHiZ;

				if (tileWidth < RDCACHE_FINE_TILE_SIZE)
					memset(dst + tileWidth, 0, (RDCACHE_FINE_TILE_SIZE - tileWidth) * sizeof(uint16_t));
			}


			// Clear remaining rows if needed
			for (int row = tileHeight; row < RDCACHE_FINE_TILE_SIZE; ++row)
				memset(rdRaster_TileDepth + row * RDCACHE_FINE_TILE_SIZE, 0, RDCACHE_FINE_TILE_SIZE * sizeof(uint16_t));
		}
	}

	void rdRaster_CopyFromTileMem(int tileX, int tileY)
	{
		rdCanvas* pCanvas = rdCamera_pCurCamera->canvas;
		int width_in_pixels = pCanvas->vbuffer->format.width;
		int height_in_pixels = pCanvas->vbuffer->format.height;
		
		uint8_t* colorBuffer = (uint8_t*)pCanvas->vbuffer->surface_lock_alloc;
		uint16_t* zBuffer = (uint16_t*)(pCanvas->d3d_vbuf->surface_lock_alloc);

		int tileMinX = tileX * RDCACHE_FINE_TILE_SIZE;
		int tileMinY = tileY * RDCACHE_FINE_TILE_SIZE;

		// Return early if tile start is outside the framebuffer
		if (tileMinX >= width_in_pixels || tileMinY >= height_in_pixels)
			return;

		int tileWidth = RDCACHE_FINE_TILE_SIZE;
		int tileHeight = RDCACHE_FINE_TILE_SIZE;

		tileWidth = width_in_pixels - tileMinX;
		if (tileWidth > RDCACHE_FINE_TILE_SIZE)
			tileWidth = RDCACHE_FINE_TILE_SIZE;
		else if (tileWidth < 0)
			tileWidth = 0;

		tileHeight = height_in_pixels - tileMinY;
		if (tileHeight > RDCACHE_FINE_TILE_SIZE)
			tileHeight = RDCACHE_FINE_TILE_SIZE;
		else if (tileHeight < 0)
			tileHeight = 0;

		// Copy row by row for color buffer
		for (int row = 0; row < tileHeight; ++row)
		{
			uint8_t* dst = colorBuffer + (tileMinY + row) * width_in_pixels + tileMinX;
			uint8_t* src = rdRaster_TileColor + row * RDCACHE_FINE_TILE_SIZE;
			memcpy(dst, src, tileWidth);
		}

		// Copy row by row for zbuffer
		for (int row = 0; row < tileHeight; ++row)
		{
			uint16_t* dst = zBuffer + (tileMinY + row) * width_in_pixels + tileMinX;
			uint16_t* src = (uint16_t*)rdRaster_TileDepth + row * RDCACHE_FINE_TILE_SIZE;
			memcpy(dst, src, tileWidth * sizeof(uint16_t));
		}
	}
#endif

#ifdef JOB_SYSTEM
	#include "Modules/std/stdJob.h"
	
	// Processes a tile
	void rdRaster_FlushBinJob(uint32_t jobIndex, uint32_t groupIndex)
	{
		rdCanvas* pCanvas = rdCamera_pCurCamera->canvas;

		if(jobIndex >= pCanvas->tileCount)
			return;

		int fineX = jobIndex % pCanvas->tileWidth;
		int fineY = jobIndex / pCanvas->tileWidth;

		rdRaster_CopyToTileMem(fineX, fineY);

		//rdCache_DrawTileOpaque(fineX, fineY);

		uint64_t* fineMask = &pCanvas->tileBits[jobIndex * RDCACHE_TILE_BINNING_STRIDE];
		for (int word = 0; word < RDCACHE_TILE_BINNING_STRIDE; word++)
		{
			uint64_t bits = fineMask[word];
			while (bits != 0)
			{
				int bit = stdMath_FindLSB64(bits);
				int triIndex = word * 64 + bit;
				bits ^= (1ull << bit);

				rdCache_DrawFaceTiled(&rdCache_aProcFaces[triIndex], fineX, fineY);
			}
		}

		rdRaster_CopyFromTileMem(fineX, fineY);
}
#endif

void rdRaster_FlushBins()
{
	rdCanvas* pCanvas = rdCamera_pCurCamera->canvas;
	stdDisplay_VBufferLock(pCanvas->vbuffer);
	stdDisplay_VBufferLock(pCanvas->d3d_vbuf);

	int width_in_pixels = pCanvas->vbuffer->format.width;
	int height_in_pixels = pCanvas->vbuffer->format.height;
	uint8_t* colorBuffer = (uint8_t*)pCanvas->vbuffer->surface_lock_alloc;
	uint16_t* zBuffer = (uint16_t*)(pCanvas->d3d_vbuf->surface_lock_alloc);

#ifdef JOB_SYSTEM
	stdJob_Dispatch(pCanvas->tileCount, 8, rdRaster_FlushBinJob);
	stdJob_Wait();
#else
	// traversing coarse bins helps reduce the number of tiles we need to process
	for (int coarseIdx = 0; coarseIdx < pCanvas->coarseTileCount; coarseIdx++)
	{
		uint64_t* coarseMask = &pCanvas->coarseTileBits[coarseIdx * RDCACHE_TILE_BINNING_STRIDE];

		int coarseX = coarseIdx % pCanvas->coarseTileWidth;
		int coarseY = coarseIdx / pCanvas->coarseTileWidth;

		for (int fy = 0; fy < RDCACHE_FINE_PER_COARSE; fy++)
		{
			for (int fx = 0; fx < RDCACHE_FINE_PER_COARSE; fx++)
			{
				int fineX = coarseX * RDCACHE_FINE_PER_COARSE + fx;
				int fineY = coarseY * RDCACHE_FINE_PER_COARSE + fy;
				int fineIdx = fineY * pCanvas->tileWidth + fineX;

				rdRaster_CopyToTileMem(fineX, fineY);

				uint64_t* fineMask = &pCanvas->tileBits[fineIdx * RDCACHE_TILE_BINNING_STRIDE];
				for (int word = 0; word < RDCACHE_TILE_BINNING_STRIDE; word++)
				{
					uint64_t bits = fineMask[word];
					while (bits != 0)
					{
						int bit = stdMath_FindLSB64(bits);
						int triIndex = word * 64 + bit;
						bits ^= (1ull << bit);

						rdCache_DrawFaceTiled(&rdCache_aProcFaces[triIndex], fineX, fineY);
					}
				}

				rdRaster_CopyFromTileMem(fineX, fineY);
			}
		}
	}
#endif
	pCanvas->bIdk &= ~4;

	stdDisplay_VBufferUnlock(pCanvas->vbuffer);
	stdDisplay_VBufferUnlock(pCanvas->d3d_vbuf);
}

// TILEDTODO: use me to add fill rule
int rdRaster_IsTopLeft(flex_t x0, flex_t y0, flex_t x1, flex_t y1)
{
	return (y0 == y1 && x0 > x1) || (y0 < y1);
}

bool rdRaster_SetupTriangle(rdTriSetup* out, const rdPoint2Fixed* v0, const rdPoint2Fixed* v1, const rdPoint2Fixed* v2)
{
	out->A0 = v2->y - v1->y;
	out->B0 = v1->x - v2->x;
	out->C0 = v1->y * v2->x - v1->x * v2->y;

	out->A1 = v0->y - v2->y;
	out->B1 = v2->x - v0->x;
	out->C1 = v2->y * v0->x - v2->x * v0->y;

	out->A2 = v1->y - v0->y;
	out->B2 = v0->x - v1->x;
	out->C2 = v0->y * v1->x - v0->x * v1->y;

	out->area = out->A2 * v2->x + out->B2 * v2->y + out->C2;
	//out->area = out->B1 * out->A2 - out->B2 * out->A1;
	//out->area = (v1->x - v0->x) * (v2->y - v0->y) - (v1->y - v0->y) * (v2->x - v0->x);

	if(out->area <= 1e-6 && out->area >= 1e-6)//fabs(out->area) <= 0.0f)
		return false;

	// If winding is opposite of area sign, flip edge equations
	if (out->area < 0.0f)
	{
		out->A0 = -out->A0; out->B0 = -out->B0; out->C0 = -out->C0;
		out->A1 = -out->A1; out->B1 = -out->B1; out->C1 = -out->C1;
		out->A2 = -out->A2; out->B2 = -out->B2; out->C2 = -out->C2;
		out->area = -out->area;
	}

	return true;
}

int rdRaster_GetMipmapLevel(rdTexinfo* texinfo, flex_t z_min)
{
	int mipmap_level = 1;
	if (texinfo->texture_ptr->num_mipmaps == 2)
	{
		if (z_min <= (flex_d_t)rdroid_aMipDistances.y)
			mipmap_level = 0;
	}
	else if (texinfo->texture_ptr->num_mipmaps == 3)
	{
		if (z_min <= (flex_d_t)rdroid_aMipDistances.x)
			mipmap_level = 0;
		else if (z_min > (flex_d_t)rdroid_aMipDistances.y)
			mipmap_level = 2;
	}
	else if (texinfo->texture_ptr->num_mipmaps == 4)
	{
		if (z_min <= (flex_d_t)rdroid_aMipDistances.x)
		{
			mipmap_level = 0;
		}
		else if (z_min > (flex_d_t)rdroid_aMipDistances.y)
		{
			if (z_min > (flex_d_t)rdroid_aMipDistances.z)
				mipmap_level = 3;
			else
				mipmap_level = 2;
		}
	}
	else if (texinfo->texture_ptr->num_mipmaps == 1)
	{
		mipmap_level = 0;
	}
	return mipmap_level;
}

#ifdef __cplusplus
}
#endif

// Conditional types
struct empty{};
template <bool Enabled, typename T> using maybe = std::conditional_t<Enabled, T, empty>;

template <bool Enabled> using uint8_maybe = maybe<Enabled, uint8_t>;
template <bool Enabled> using flex_maybe = maybe<Enabled, flex_t>;
template <bool Enabled> using int_maybe = maybe<Enabled, int32_t>;
template <bool Enabled> using uint_maybe = maybe<Enabled, uint32_t>;

// Draws a face to a tile by triangulating it and using edge functions
// Templated using constexpr branches to avoid having to duplicate a crap ton of code
template <int8_t ZMethod, int8_t GeoMode, int8_t TextureMode, int8_t LightMode, bool UseDiscard, bool UseAlpha>
void rdRaster_DrawToTile(rdProcEntry* entry, rdTexinfo* texinfo, int tileX, int tileY)
{
	// Setup a few compile time options
	static constexpr bool ReadZ = ZMethod == RD_ZBUFFER_READ_NOWRITE || ZMethod == RD_ZBUFFER_READ_WRITE;
	static constexpr bool WriteZ = ZMethod == RD_ZBUFFER_NOREAD_WRITE || ZMethod == RD_ZBUFFER_READ_WRITE;
	static constexpr bool UseFlatLight = (LightMode == RD_LIGHTMODE_NOTLIT) || (LightMode == RD_LIGHTMODE_DIFFUSE);
	static constexpr bool UseGouraud = LightMode == RD_LIGHTMODE_GOURAUD;
	static constexpr bool UseSolidColor = GeoMode == RD_GEOMODE_SOLIDCOLOR;
	static constexpr bool UseTexture = GeoMode == RD_GEOMODE_TEXTURED && TextureMode >= 0;
	static constexpr bool Perspective = TextureMode == RD_TEXTUREMODE_PERSPECTIVE;

	//uint16_t iz_min = (uint16_t)(int)(entry->z_min * 65536.0f + 0.5f);
	//if(iz_min >= rdRaster_TileHiZ)
	//	return;

	int tileMinX = tileX * RDCACHE_FINE_TILE_SIZE;
	int tileMinY = tileY * RDCACHE_FINE_TILE_SIZE;
	int tileMaxX = tileMinX + RDCACHE_FINE_TILE_SIZE - 1;
	int tileMaxY = tileMinY + RDCACHE_FINE_TILE_SIZE - 1;

	// todo: I assume this gets compiled out for permutations that don't use them?
	uint8_t* transparency = ((uint8_t*)entry->colormap->transparency);
	uint8_t* lightLevels = ((uint8_t*)entry->colormap->lightlevel);
	
	flex_t invFar = stdMath_Rcp(rdCamera_pCurCamera->pClipFrustum->field_0.z);

	// Texture/color setup
	uint8_maybe<UseSolidColor> solidColor;
	if constexpr (UseSolidColor)
		solidColor = (uint8_t)(texinfo->header.field_4 & 0xFF);

	maybe<UseTexture, stdVBuffer*> tex;
	maybe<UseTexture, uint8_t*> pixels;
	flex_maybe<UseTexture> mip_scale;
	int_maybe<UseTexture> mipmap_level, texRowShift, uWrap, vWrap;
	if constexpr (UseTexture)
	{
		flex_t z_min = entry->z_min * rdCamera_GetMipmapScalar(); // MOTS added
		mipmap_level = rdRaster_GetMipmapLevel(texinfo, z_min);
		tex = texinfo->texture_ptr->texture_struct[mipmap_level];
		//stdDisplay_VBufferLock(tex);

		mip_scale = stdMath_Rcp((flex_t)(1 << mipmap_level));
		pixels = (uint8_t*)tex->surface_lock_alloc;

		texRowShift = texinfo->texture_ptr->width_bitcnt - mipmap_level;
		uWrap = (texinfo->texture_ptr->width_minus_1 >> (mipmap_level & 31)) << 16;
		vWrap = (texinfo->texture_ptr->height_minus_1 >> (mipmap_level & 31)) << (texRowShift & 31);
	}

	// Lighting setup
	// Gouraud setup is done before binning
	uint_maybe<UseFlatLight> flatLight;

	if constexpr (LightMode == RD_LIGHTMODE_NOTLIT)
	{
		flex_t ambientLight = 0.0f;
		if ((rdroid_curRenderOptions & RD_USE_AMBIENT_LIGHT) != 0)
			ambientLight = entry->ambientLight;

		flex_t light = stdMath_Saturate(entry->extralight);
		light = stdMath_Clamp(light, ambientLight, 1.0);
		flatLight = (uint32_t)(light * 63.0f) * 256;
	}
	else if constexpr (LightMode == RD_LIGHTMODE_DIFFUSE)
	{
		flex_t ambientLight = 0.0f;
		if ((rdroid_curRenderOptions & RD_USE_AMBIENT_LIGHT) != 0)
			ambientLight = entry->ambientLight;

		flex_t light = entry->extralight + entry->light_level_static;
		light = stdMath_Clamp(light, ambientLight, 1.0);
		flatLight = (uint32_t)(light * 63.0f) * 256;
	}

	// Vertex cache
	rdPoint2Fixed v[24];
	flex_t invZ[24];
	flex_maybe<UseGouraud> light[24];
	flex_maybe<UseTexture> uZ[24];
	flex_maybe<UseTexture> vZ[24];

	for (int i = 0; i < entry->numVertices; ++i)
	{
		v[i].x = entry->vertices[i].x;
		v[i].y = entry->vertices[i].y;

		flex_t z = entry->vertices[i].z * invFar;
		invZ[i] = stdMath_Rcp(z);
		
		if constexpr (UseGouraud)
			light[i] = entry->vertexIntensities[i];
	
		if constexpr (UseTexture)
		{
			flex_t scale = rdRaster_fixedScale * mip_scale;
			if constexpr (Perspective)
				scale *= invZ[i];
			uZ[i] = entry->vertexUVs[i].x * scale;
			vZ[i] = entry->vertexUVs[i].y * scale;
		}		
	}

	// Run through the face as triangles
	int verts = entry->numVertices - 2;
	
	rdTri8 tri;
	tri.i1 = 0;
	tri.i2 = 1;
	tri.i3 = (uint8_t)entry->numVertices - 1;
	for (int i = 0; i < verts; ++i)
	{
		const rdPoint2Fixed* p0 = &v[tri.i1];
		const rdPoint2Fixed* p1 = &v[tri.i2];
		const rdPoint2Fixed* p2 = &v[tri.i3];

		// Triangle bounding box (integer pixel coords)
		flex_t fminX = std::min(std::min(p0->x, p1->x), p2->x);
		flex_t fmaxX = std::max(std::max(p0->x, p1->x), p2->x);
		flex_t fminY = std::min(std::min(p0->y, p1->y), p2->y);
		flex_t fmaxY = std::max(std::max(p0->y, p1->y), p2->y);

		int minX = (int)std::min(std::max(std::floor(fminX), (flex_t )tileMinX), (flex_t)tileMaxX);
		int maxX = (int)std::min(std::max(std::ceil(fmaxX), (flex_t )tileMinX), (flex_t)tileMaxX);
		int minY = (int)std::min(std::max(std::floor(fminY), (flex_t )tileMinY), (flex_t)tileMaxY);
		int maxY = (int)std::min(std::max(std::ceil(fmaxY), (flex_t )tileMinY), (flex_t )tileMaxY);
		
		// Tile offset along X (for tile cache access)
		int tileOffsetX = (minX - tileMinX);

		// Setup the edge functions
		rdTriSetup setup;
		if (!rdRaster_SetupTriangle(&setup, p0, p1, p2))
		{
			if ((i & 1) != 0)
				tri.i1 = tri.i3--;
			else
				tri.i1 = tri.i2++;
			continue; // Degenerate triangle
		}

		flex_t invArea = stdMath_Rcp(setup.area);

		// Z interpolators
		flex_t z0 = invZ[tri.i1];
		flex_t z1 = (invZ[tri.i2] - z0) * invArea;
		flex_t z2 = (invZ[tri.i3] - z0) * invArea;

		// UV interpolators
		flex_maybe<UseTexture> u0, u1, u2, v0, v1, v2;
		if constexpr (UseTexture)
		{
			u0 = uZ[tri.i1];
			u1 = (uZ[tri.i2] - uZ[tri.i1]) * invArea;
			u2 = (uZ[tri.i3] - uZ[tri.i1]) * invArea;

			v0 = vZ[tri.i1];
			v1 = (vZ[tri.i2] - vZ[tri.i1]) * invArea;
			v2 = (vZ[tri.i3] - vZ[tri.i1]) * invArea;
		}

		// Light interpolators
		flex_maybe<UseGouraud> light0, light1, light2;
		if constexpr (UseGouraud)
		{
			light0 = light[tri.i1];
			light1 = (light[tri.i2] - light0) * invArea;
			light2 = (light[tri.i3] - light0) * invArea;
		}

		// Pixel center at top-left of the bounding box
		fixed_t px0 = (flex_t)(minX + 0.5f);
		fixed_t py0 = (flex_t)(minY + 0.5f);

		// Initial edge values at top-left pixel
		fixed_t w0_row = setup.A0 * px0 + (setup.B0 * py0 + setup.C0);
		fixed_t w1_row = setup.A1 * px0 + (setup.B1 * py0 + setup.C1);
		fixed_t w2_row = setup.A2 * px0 + (setup.B2 * py0 + setup.C2);

		// Per-pixel X step (delta across scanline)
		fixed_t w0_dx = setup.A0;
		fixed_t w1_dx = setup.A1;
		fixed_t w2_dx = setup.A2;

		// Per-row Y step (delta down scanlines)
		fixed_t w0_dy = setup.B0;
		fixed_t w1_dy = setup.B1;
		fixed_t w2_dy = setup.B2;

		// Z step per pixel
		flex_t z_dx = fma(z1, w1_dx, z2 * w2_dx);

		// UV step per pixel
		flex_maybe<UseTexture> u_dx, v_dx;
		if constexpr (UseTexture)
		{
			u_dx = fma(u1, w1_dx, u2 * w2_dx);
			v_dx = fma(v1, w1_dx, v2 * w2_dx);
		}

		// Light step per pixel
		flex_maybe<UseGouraud> l_dx;
		if constexpr (UseGouraud)
			l_dx = fma(light1, w1_dx, light2 * w2_dx);

		for (int y = minY; y <= maxY; y++)
		{
			// Current barycentric weights
			fixed_t w0 = w0_row;
			fixed_t w1 = w1_row;
			fixed_t w2 = w2_row;

			// Z at row start
			flex_t z_row = fma(w1_row, z1, fma(w2_row, z2, z0));
			
			// UV at row start
			flex_maybe<UseTexture> u_row, v_row;
			if constexpr (UseTexture)
			{
				u_row = fma(w1_row, u1, fma(w2_row, u2, u0));
				v_row = fma(w1_row, v1, fma(w2_row, v2, v0));
			}

			// Light at row start
			flex_maybe<UseGouraud> l_row;
			if constexpr (UseGouraud)
				l_row = fma(w1_row, light1, fma(w2_row, light2, light0));

			// Tile coordinate at row start
			int offset = (y - tileMinY) * RDCACHE_FINE_TILE_SIZE + tileOffsetX;
			for (int x = minX; x <= maxX; x++)
			{
				// Barycentric test (could be faster..)
				if (w0 > 0 && w1 > 0 && w2 > 0)
				{
					flex_t z = stdMath_Rcp(z_row);

					// Integer depth for depth testing
					uint16_t zi = (uint16_t)(int)(fma(z, 65535.0f, 0.5f));
					uint16_t depthMask = 0xFFFF;

					// Early Z
					if constexpr (!UseDiscard && ReadZ)
						depthMask = (zi <= rdRaster_TileDepth[offset]) ? 0xFFFF : 0;

					// Texture/Color
					uint8_t index = 0;
					if constexpr (UseSolidColor)
						index = solidColor;
					
					if constexpr (UseTexture)
					{
						flex_t u = u_row;
						flex_t v = v_row;
						if constexpr (Perspective)
						{
							u *= z;
							v *= z;
						}

						// Moved to fixed point centered at 0x8000 for wrapping
						// todo: would be nice to use fixed point up front but it doesn't work atm...
						int32_t x_fixed = (int32_t)u + 0x8000;
						int32_t y_fixed = (int32_t)v + 0x8000;

						// Coordinate wrapping
						int32_t x_wrapped = (x_fixed & uWrap) >> 16;
						int32_t y_wrapped = (y_fixed >> (16 - texRowShift)) & vWrap;

						// Read the texture
						index = depthMask ? pixels[x_wrapped + y_wrapped] : 0;
					}
					
					// Alpha test
					uint16_t discardMask = 0;
					if constexpr (UseDiscard)
						discardMask = (index == 0) ? 0xFFFF : 0;
					
					// Late Z
					if constexpr (UseDiscard && ReadZ)
						depthMask = (zi <= rdRaster_TileDepth[offset]) ? 0xFFFF : 0;
						
					const uint16_t write_mask = depthMask & ~discardMask;
					const uint16_t read_mask = ~write_mask;

					if constexpr (WriteZ)
					{
						uint16_t oldZ = rdRaster_TileDepth[offset];
						rdRaster_TileDepth[offset] = (write_mask & zi) | (read_mask & oldZ);
						//rdRaster_TileHiZ = zi < rdRaster_TileHiZ ? zi : rdRaster_TileHiZ;
					}

					// Lighting
					if constexpr (UseGouraud)
						index = write_mask ? lightLevels[(uint32_t)stdMath_Clamp(l_row, 0.0f, 63.0f) * 256 + index] : 0;

					if constexpr (UseFlatLight)
						index = write_mask ? lightLevels[flatLight + index] : 0;

					// Blending
					uint8_t oldColor oldIndex = rdRaster_TileColor[offset];
					if constexpr (UseAlpha)
						index = write_mask ? transparency[oldIndex * 256 + index] : 0;
							
					rdRaster_TileColor[offset] = (write_mask & index) | (read_mask & oldIndex);
				}

				// column step
				w0 += w0_dx;
				w1 += w1_dx;
				w2 += w2_dx;

				z_row += z_dx;

				if constexpr (UseTexture)
				{
					u_row += u_dx;
					v_row += v_dx;
				}

				if constexpr (UseGouraud)
					l_row += l_dx;

				++offset;
			}

			// row step
			w0_row += w0_dy;
			w1_row += w1_dy;
			w2_row += w2_dy;
		}

		// Move to next triangle
		if ((i & 1) != 0)
			tri.i1 = tri.i3--;
		else
			tri.i1 = tri.i2++;
	}
}

// Nested dispatches to choose a compile time permutation

template <int8_t ZMethod, int8_t GeoMode, int8_t TextureMode, int8_t LightMode, bool UseDiscard>
void rdRaster_DrawToTileByAlpha(rdProcEntry* face, rdTexinfo* texinfo, int tileX, int tileY)
{
	bool alpha = (face->type & 2) != 0;
	if (alpha)
		rdRaster_DrawToTile<ZMethod, GeoMode, TextureMode, LightMode, UseDiscard, true>(face, texinfo, tileX, tileY);
	else
		rdRaster_DrawToTile<ZMethod, GeoMode, TextureMode, LightMode, UseDiscard, false>(face, texinfo, tileX, tileY);
}

template <int8_t ZMethod, int8_t GeoMode, int8_t TextureMode, int8_t LightMode>
void rdRaster_DrawToTileByDiscard(rdProcEntry* face, rdTexinfo* texinfo, int tileX, int tileY)
{
	if (TextureMode < 0)
	{
		rdRaster_DrawToTileByAlpha<ZMethod, GeoMode, TextureMode, LightMode, false>(face, texinfo, tileX, tileY);
	}
	else
	{
		bool discard = (texinfo->texture_ptr->alpha_en & 1) != 0;
		if (discard)
			rdRaster_DrawToTileByAlpha<ZMethod, GeoMode, TextureMode, LightMode, true>(face, texinfo, tileX, tileY);
		else
			rdRaster_DrawToTileByAlpha<ZMethod, GeoMode, TextureMode, LightMode, false>(face, texinfo, tileX, tileY);
	}
}

template <int8_t ZMethod, int8_t GeoMode, int8_t TextureMode>
void rdRaster_DrawToTileByLightMode(rdProcEntry* face, rdTexinfo* texinfo, int tileX, int tileY)
{
	int lightingMode = rdroid_curLightingMode < face->lightingMode ? rdroid_curLightingMode : face->lightingMode;
	switch (lightingMode)
	{
	case RD_LIGHTMODE_FULLYLIT:
		rdRaster_DrawToTileByDiscard<ZMethod, GeoMode, TextureMode, RD_LIGHTMODE_FULLYLIT>(face, texinfo, tileX, tileY);
		break;
	case RD_LIGHTMODE_NOTLIT:
		rdRaster_DrawToTileByDiscard<ZMethod, GeoMode, TextureMode, RD_LIGHTMODE_NOTLIT>(face, texinfo, tileX, tileY);
		break;
	case RD_LIGHTMODE_DIFFUSE:
		rdRaster_DrawToTileByDiscard<ZMethod, GeoMode, TextureMode, RD_LIGHTMODE_DIFFUSE>(face, texinfo, tileX, tileY);
		break;
	case RD_LIGHTMODE_GOURAUD:
		rdRaster_DrawToTileByDiscard<ZMethod, GeoMode, TextureMode, RD_LIGHTMODE_GOURAUD>(face, texinfo, tileX, tileY);
		break;
	default:
		break;
	}
}

template <int8_t ZMethod, int8_t GeoMode>
void rdRaster_DrawToTileByTextureMode(rdProcEntry* face, rdTexinfo* texinfo, int tileX, int tileY)
{
	int textureMode = rdroid_curTextureMode < face->textureMode ? rdroid_curTextureMode : face->textureMode;
	switch (textureMode)
	{
	case RD_TEXTUREMODE_AFFINE:
		rdRaster_DrawToTileByLightMode<ZMethod, GeoMode, RD_TEXTUREMODE_AFFINE>(face, texinfo, tileX, tileY);
		break;
	case RD_TEXTUREMODE_PERSPECTIVE:
		rdRaster_DrawToTileByLightMode<ZMethod, GeoMode, RD_TEXTUREMODE_PERSPECTIVE>(face, texinfo, tileX, tileY);
		break;
	default:
		break;
	}
}

template <int8_t ZMethod>
void rdRaster_DrawToTileByGeoMode(rdProcEntry* face, rdTexinfo* texinfo, int tileX, int tileY)
{
	int geometryMode = rdroid_curGeometryMode < face->geometryMode ? rdroid_curGeometryMode : face->geometryMode;

	if (geometryMode == 4 && !(texinfo->header.texture_type & 8))
		geometryMode = 3;

	switch (geometryMode)
	{
	case RD_GEOMODE_SOLIDCOLOR:
		rdRaster_DrawToTileByLightMode<ZMethod, RD_GEOMODE_SOLIDCOLOR, -1>(face, texinfo, tileX, tileY);
		break;
	case RD_GEOMODE_TEXTURED:
		rdRaster_DrawToTileByTextureMode<ZMethod, RD_GEOMODE_TEXTURED>(face, texinfo, tileX, tileY);
		break;
	default:
		break;
	}
}

#ifdef __cplusplus
extern "C" {
#endif

void rdCache_DrawFaceTiled(rdProcEntry* face, int tileX, int tileY)
{
	if (!face || face->geometryMode == 0 || !face->material)
		return;

	int geometryMode = rdroid_curGeometryMode < face->geometryMode ? rdroid_curGeometryMode : face->geometryMode;
	int lightingMode = rdroid_curLightingMode < face->lightingMode ? rdroid_curLightingMode : face->lightingMode;
	int textureMode = rdroid_curTextureMode < face->textureMode ? rdroid_curTextureMode : face->textureMode;

	rdMaterial* mat = face->material;
	rdTexinfo* texinfo = NULL;

	if (mat)
	{
		int cel = (face->wallCel == 0xFFFFFFFF) ? mat->celIdx : (int)face->wallCel;
		if (cel < 0) cel = 0;
		else if ((uint32_t)cel >= mat->num_texinfo) cel = mat->num_texinfo - 1;
		texinfo = mat->texinfos[cel];
	}

	if (rdroid_curOcclusionMethod) // jk edge table method, just do full z
	{
		rdRaster_DrawToTileByGeoMode<RD_ZBUFFER_READ_WRITE>(face, texinfo, tileX, tileY);
	}
	else
	{	
		switch (rdroid_curZBufferMethod)
		{
		case RD_ZBUFFER_NOREAD_NOWRITE:
			rdRaster_DrawToTileByGeoMode<RD_ZBUFFER_NOREAD_NOWRITE>(face, texinfo, tileX, tileY);
			break;
		case RD_ZBUFFER_NOREAD_WRITE:
			rdRaster_DrawToTileByGeoMode<RD_ZBUFFER_NOREAD_WRITE>(face, texinfo, tileX, tileY);
			break;
		case RD_ZBUFFER_READ_WRITE:
			rdRaster_DrawToTileByGeoMode<RD_ZBUFFER_READ_WRITE>(face, texinfo, tileX, tileY);
			break;
		case RD_ZBUFFER_READ_NOWRITE:
			rdRaster_DrawToTileByGeoMode<RD_ZBUFFER_READ_NOWRITE>(face, texinfo, tileX, tileY);
			break;
		default:
			break;
		}
	}
}

#endif

#ifdef __cplusplus
}
#endif