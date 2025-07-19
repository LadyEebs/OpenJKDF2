#include "stdDisplay.h"

#include "std.h"
#include "stdPlatform.h"
#include "jk.h"
#include "Win95/Video.h"
#include "Win95/Window.h"
#include "General/stdColor.h"

#include "Platform/std3D.h"
#include "Modules/std/stdJob.h"
#include "Modules/std/stdProfiler.h"

void stdDisplay_SetGammaTable(int len, flex_d_t *table)
{
    stdDisplay_gammaTableLen = len;
    stdDisplay_paGammaTable = table;
}

uint8_t* stdDisplay_GetPalette()
{
    return (uint8_t*)stdDisplay_gammaPalette;
}

int stdDisplay_SortVideoModes(stdVideoMode* modeA, stdVideoMode* modeB)
{
	// Prioritize active modes
	if (modeA->field_0 == 0 && modeB->field_0 != 0)
	{
		return 1; // B comes before A
	}
	if (modeA->field_0 != 0 && modeB->field_0 == 0)
	{
		return -1; // A comes before B
	}

	// If both inactive or both active, compare by bit depth
	uint32_t bppA = modeA->format.format.bpp;
	uint32_t bppB = modeB->format.format.bpp;
	if (bppA != bppB)
	{
		return bppA - bppB;
	}

	// Then compare width
	int widthA = modeA->format.width;
	int widthB = modeB->format.width;
	if (widthA != widthB)
	{
		return widthA - widthB;
	}

	// Finally compare height
	return modeA->format.height - modeB->format.height;
}

#ifndef SDL2_RENDER

#else
#include "SDL2_helper.h"
#include <assert.h>

uint32_t Video_menuTexId = 0;
uint32_t Video_overlayTexId = 0;
rdColor24 stdDisplay_masterPalette[256];
int Video_bModeSet = 0;

int stdDisplay_Startup()
{
    stdDisplay_bStartup = 1;
    return 1;
}

int stdDisplay_FindClosestDevice(void* a)
{
    Video_dword_866D78 = 0;
    return 0;
}

int stdDisplay_Open(int a)
{
    stdDisplay_pCurDevice = &stdDisplay_aDevices[0];
    stdDisplay_bOpen = 1;
    return 1;
}

void stdDisplay_Close()
{
    stdDisplay_bOpen = 0;
}

int stdDisplay_FindClosestMode(render_pair *a1, struct stdVideoMode *render_surface, unsigned int max_modes)
{
    Video_curMode = 0;
    stdDisplay_bPaged = 1;
    stdDisplay_bModeSet = 1;
    return 0;
}

int stdDisplay_SetMode(unsigned int modeIdx, const void *palette, int paged)
{
    uint32_t newW = Window_xSize;
    uint32_t newH = Window_ySize;

    //if (jkGame_isDDraw)
    {
        newW = (uint32_t)((flex_t)Window_xSize * ((480.0*2.0)/Window_ySize));
        newH = 480*2;
    }

    if (newW > Window_xSize)
    {
        newW = Window_xSize;
        newH = Window_ySize;
    }

    if (newW < 640)
        newW = 640;
    if (newH < 480)
        newH = 480;

    stdDisplay_pCurVideoMode = &Video_renderSurface[modeIdx];
    
    stdDisplay_pCurVideoMode->format.format.bpp = 8;
    stdDisplay_pCurVideoMode->format.width_in_pixels = newW;
    stdDisplay_pCurVideoMode->format.width = newW;
    stdDisplay_pCurVideoMode->format.height = newH;
    
    _memcpy(&Video_otherBuf.format, &stdDisplay_pCurVideoMode->format, sizeof(Video_otherBuf.format));
    _memcpy(&Video_menuBuffer.format, &stdDisplay_pCurVideoMode->format, sizeof(Video_menuBuffer.format));
    
    _memcpy(&Video_overlayMapBuffer.format, &stdDisplay_pCurVideoMode->format, sizeof(Video_overlayMapBuffer.format));
    
    if (Video_bModeSet)
    {
        glDeleteTextures(1, &Video_menuTexId);
        glDeleteTextures(1, &Video_overlayTexId);
        if (Video_otherBuf.sdlSurface)
            SDL_FreeSurface(Video_otherBuf.sdlSurface);
        if (Video_menuBuffer.sdlSurface)
            SDL_FreeSurface(Video_menuBuffer.sdlSurface);
        if (Video_overlayMapBuffer.sdlSurface)
            SDL_FreeSurface(Video_overlayMapBuffer.sdlSurface);
        
        Video_otherBuf.sdlSurface = 0;
        Video_menuBuffer.sdlSurface = 0;
        Video_overlayMapBuffer.sdlSurface = 0;

#ifdef HW_VBUFFER
		std3D_FreeDrawSurface(&Video_otherBuf);
		std3D_FreeDrawSurface(&Video_menuBuffer);
		std3D_FreeDrawSurface(&Video_overlayMapBuffer);
#endif
    }

#ifdef HW_VBUFFER
//if(create_ddraw_surface)
	std3D_AllocDrawSurface(&Video_otherBuf, newW, newH);
	std3D_AllocDrawSurface(&Video_menuBuffer, newW, newH);
	std3D_AllocDrawSurface(&Video_overlayMapBuffer, newW, newH);
#endif

    SDL_Surface* otherSurface = SDL_CreateRGBSurface(0, newW, newH, 8,
                                        0,
                                        0,
                                        0,
                                        0);
    SDL_Surface* menuSurface = SDL_CreateRGBSurface(0, newW, newH, 8,
                                        0,
                                        0,
                                        0,
                                        0);
    SDL_Surface* overlaySurface = SDL_CreateRGBSurface(0, newW, newH, 8, 0, 0, 0, 0);
    
    if (palette)
    {
		if (stdDisplay_paGammaTable != NULL && stdDisplay_gammaIdx != 0)
		{
			memcpy(stdDisplay_tmpGammaPal, palette, 0x300);
			stdColor_GammaCorrect(&stdDisplay_gammaPalette[0].r, stdDisplay_tmpGammaPal, 256,
								  stdDisplay_paGammaTable[stdDisplay_gammaIdx - 1]);
		}
		else
		{
			memcpy(stdDisplay_gammaPalette, palette, 0x300);
		}

        const rdColor24* pal24 = (const rdColor24*)palette;
        SDL_Color* tmp = (SDL_Color*)malloc(sizeof(SDL_Color) * 256);
        for (int i = 0; i < 256; i++)
        {
            tmp[i].r = pal24[i].r;
            tmp[i].g = pal24[i].g;
            tmp[i].b = pal24[i].b;
            tmp[i].a = 0xFF;
        }
        
        SDL_SetPaletteColors(otherSurface->format->palette, tmp, 0, 256);
        SDL_SetPaletteColors(menuSurface->format->palette, tmp, 0, 256);
        SDL_SetPaletteColors(overlaySurface->format->palette, tmp, 0, 256);
        free(tmp);
    }
    
    //SDL_SetSurfacePalette(otherSurface, palette);
    //SDL_SetSurfacePalette(menuSurface, palette);
    
    Video_otherBuf.sdlSurface = otherSurface;
    Video_menuBuffer.sdlSurface = menuSurface;
    Video_overlayMapBuffer.sdlSurface = overlaySurface;
    
    Video_menuBuffer.format.width_in_bytes = menuSurface->pitch;
    Video_otherBuf.format.width_in_bytes = otherSurface->pitch;
    Video_overlayMapBuffer.format.width_in_bytes = overlaySurface->pitch;
    
    Video_menuBuffer.format.width_in_pixels = menuSurface->pitch;
    Video_otherBuf.format.width_in_pixels = otherSurface->pitch;
    Video_overlayMapBuffer.format.width_in_pixels = overlaySurface->pitch;
    Video_menuBuffer.format.width = newW;
    Video_otherBuf.format.width = newW;
    Video_overlayMapBuffer.format.width = newW;
    Video_menuBuffer.format.height = newH;
    Video_otherBuf.format.height = newH;
    Video_overlayMapBuffer.format.height = newH;
    
    Video_menuBuffer.format.format.bpp = 8;
    Video_otherBuf.format.format.bpp = 8;
    Video_overlayMapBuffer.format.format.bpp = 8;
    
    glGenTextures(1, &Video_menuTexId);
    glBindTexture(GL_TEXTURE_2D, Video_menuTexId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, newW, newH, 0, GL_RED, GL_UNSIGNED_BYTE, Video_menuBuffer.sdlSurface->pixels);
    
    glGenTextures(1, &Video_overlayTexId);
    glBindTexture(GL_TEXTURE_2D, Video_overlayTexId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, newW, newH, 0, GL_RED, GL_UNSIGNED_BYTE, Video_overlayMapBuffer.sdlSurface->pixels);
    

    Video_bModeSet = 1;
    
    return 1;
}

int stdDisplay_ClearRect(stdVBuffer *buf, int fillColor, rdRect *rect)
{
    return stdDisplay_VBufferFill(buf, fillColor, rect);
}



int stdDisplay_DDrawGdiSurfaceFlip()
{
    Window_SdlUpdate();
    return 1;
}

int stdDisplay_ddraw_waitforvblank()
{
    Window_SdlVblank();
    return 1;
}

int stdDisplay_SetMasterPalette(uint8_t* pal)
{
    rdColor24* pal24 = (rdColor24*)pal;
    
    memcpy(stdDisplay_masterPalette, pal24, sizeof(stdDisplay_masterPalette));
    
    return 1;
}

stdVBuffer* stdDisplay_VBufferNew(stdVBufferTexFmt *fmt, int create_ddraw_surface, int gpu_mem, const void* palette)
{
    stdVBuffer* out = (stdVBuffer*)std_pHS->alloc(sizeof(stdVBuffer));
    
    _memset(out, 0, sizeof(*out));
    
    _memcpy(&out->format, fmt, sizeof(out->format));
    
    // force 0 reads
    //out->format.width = 0;
    //out->format.width_in_bytes = 0;
    //out->surface_lock_alloc = std_pHS->alloc(texture_size_in_bytes);
    
    //if (fmt->format.g_bits == 6) // RGB565
    {
        fmt->format.r_bits = 0;
        fmt->format.g_bits = 0;
        fmt->format.b_bits = 0;
        fmt->format.r_shift = 0;
        fmt->format.g_shift = 0;
        fmt->format.b_shift = 0;
    }

    uint32_t rbitmask = ((1 << fmt->format.r_bits) - 1) << fmt->format.r_shift;
    uint32_t gbitmask = ((1 << fmt->format.g_bits) - 1) << fmt->format.g_shift;
    uint32_t bbitmask = ((1 << fmt->format.b_bits) - 1) << fmt->format.b_shift;
    uint32_t abitmask = 0;//((1 << fmt->format.a_bits) - 1) << fmt->format.a_shift;
    if (fmt->format.bpp == 8)
    {
        rbitmask = 0;
        gbitmask = 0;
        bbitmask = 0;
        abitmask = 0;
    }

    SDL_Surface* surface = SDL_CreateRGBSurface(0, fmt->width, fmt->height, fmt->format.bpp, rbitmask, gbitmask, bbitmask, abitmask);
    
    if (surface)
    {
        static int num = 0;
        //printf("Allocated VBuffer %u, w %u h %u bpp %u %x %x %x\n", num++, fmt->width, fmt->height, fmt->format.bpp, rbitmask, gbitmask, bbitmask);
        out->format.width_in_bytes = surface->pitch;
        out->format.width_in_pixels = fmt->width;
        out->format.texture_size_in_bytes = surface->pitch * fmt->height;
    }
    else
    {
        //printf("asdf\n");
        stdPlatform_Printf("Failed to allocate VBuffer! %s, w %u h %u bpp %u, rmask %x gmask %x bmask %x amask %x, %x %x %x, %x %x %x\n", SDL_GetError(), fmt->width, fmt->height, fmt->format.bpp, rbitmask, gbitmask, bbitmask, abitmask, fmt->format.r_bits, fmt->format.g_bits, fmt->format.b_bits, fmt->format.r_shift, fmt->format.g_shift, fmt->format.b_shift);
        assert(0);
    }
    //printf("Failed to allocate VBuffer! %s, w %u h %u bpp %u, rmask %x gmask %x bmask %x amask %x, %x %x %x, %x %x %x\n", SDL_GetError(), fmt->width, fmt->height, fmt->format.bpp, rbitmask, gbitmask, bbitmask, abitmask, fmt->format.r_bits, fmt->format.g_bits, fmt->format.b_bits, fmt->format.r_shift, fmt->format.g_shift, fmt->format.b_shift);
    
    out->sdlSurface = surface;

#ifdef HW_VBUFFER
	//memcpy(out->palette, palette, );
	//if(create_ddraw_surface)
		std3D_AllocDrawSurface(out, fmt->width, fmt->height);
#endif
    
    return out;
}

int stdDisplay_VBufferLock(stdVBuffer *buf)
{
    if (!buf) return 0;
	if (buf->bSurfaceLocked)
		return 0;
    SDL_LockSurface(buf->sdlSurface);
    buf->surface_lock_alloc = (char*)buf->sdlSurface->pixels;
    buf->bSurfaceLocked = 1;
	return 1;
}

void stdDisplay_VBufferUnlock(stdVBuffer *buf)
{
    if (!buf) return;
	if (!buf->bSurfaceLocked)
		return;
#ifdef HW_VBUFFER
	if (buf->device_surface && buf->surface_lock_alloc)
	{
		std3D_UploadDrawSurface(buf->device_surface, buf->format.width, buf->format.height, buf->surface_lock_alloc, buf->palette);
	}
   #endif
    buf->surface_lock_alloc = NULL;
    SDL_UnlockSurface(buf->sdlSurface);
	buf->bSurfaceLocked = 0;
}

int stdDisplay_VBufferCopy(stdVBuffer *vbuf, stdVBuffer *vbuf2, unsigned int blit_x, int blit_y, rdRect *rect, int alpha_maybe)
{
    if (!vbuf || !vbuf2) return 1;
    
    rdRect fallback = {0,0,vbuf2->format.width, vbuf2->format.height};
    if (!rect)
    {
        rect = &fallback;
        //memcpy(vbuf->sdlSurface->pixels, vbuf2->sdlSurface->pixels, 640*480);
        //return;
    }
    
    //if (vbuf == &Video_menuBuffer)
    //    stdPlatform_Printf("Vbuffer copy to menu %u,%u %ux%u %u,%u\n", rect->x, rect->y, rect->width, rect->height, blit_x, blit_y);
    
    if (vbuf->palette)
    {
        rdColor24* pal24 = (rdColor24*)vbuf->palette;
        SDL_Color* tmp = (SDL_Color*)malloc(sizeof(SDL_Color) * 256);
        for (int i = 0; i < 256; i++)
        {
            tmp[i].r = pal24[i].r;
            tmp[i].g = pal24[i].g;
            tmp[i].b = pal24[i].b;
            tmp[i].a = 0xFF;
        }
    
        SDL_SetPaletteColors(vbuf->sdlSurface->format->palette, tmp, 0, 256);
        free(tmp);
    }
    
    if (vbuf2->palette)
    {
        rdColor24* pal24 = (rdColor24*)vbuf2->palette;
        SDL_Color* tmp = (SDL_Color*)malloc(sizeof(SDL_Color) * 256);
        for (int i = 0; i < 256; i++)
        {
            tmp[i].r = pal24[i].r;
            tmp[i].g = pal24[i].g;
            tmp[i].b = pal24[i].b;
            tmp[i].a = 0xFF;
        }
        
        SDL_SetPaletteColors(vbuf2->sdlSurface->format->palette, tmp, 0, 256);
        free(tmp);
    }

    SDL_Rect dstRect = {(int)blit_x, (int)blit_y, (int)rect->width, (int)rect->height};
    SDL_Rect srcRect = {(int)rect->x, (int)rect->y, (int)rect->width, (int)rect->height};
    
    uint8_t* srcPixels = (uint8_t*)vbuf2->sdlSurface->pixels;
    uint8_t* dstPixels = (uint8_t*)vbuf->sdlSurface->pixels;
    uint32_t srcStride = vbuf2->format.width_in_bytes;
    uint32_t dstStride = vbuf->format.width_in_bytes;

#ifdef TARGET_SSE
	int self_copy = 0;
	int has_alpha = !(rect->width == 640) && (alpha_maybe & 1);

	// Handle self-copy case with temporary buffer
	if (dstPixels == srcPixels)
	{
		size_t buf_len = srcStride * dstRect.h;
		uint8_t* tempBuffer = (uint8_t*)_mm_malloc(buf_len, 16); // Aligned allocation
		SDL_Rect dstRect_inter = { 0, 0, rect->width, rect->height };

		// Process rows in blocks
		for (int j = 0; j < rect->height; j++)
		{
			int srcY = j + srcRect.y;
			if ((uint32_t)srcY >= (uint32_t)vbuf2->format.height) continue;

			uint8_t* srcRow = srcPixels + srcY * srcStride + srcRect.x;
			uint8_t* dstRow = tempBuffer + j * srcStride;

			int i = 0;
			// Process 16 bytes at a time
			for (; i + 15 < rect->width; i += 16)
			{
				// Load 16 pixels
				__m128i pixels = _mm_loadu_si128((__m128i*)(srcRow + i));

				if (has_alpha)
				{
					// Create mask for non-zero pixels
					__m128i zero = _mm_setzero_si128();
					__m128i mask = _mm_cmpeq_epi8(pixels, zero);

					// For self-copy, we just store all pixels (no alpha skip)
					_mm_store_si128((__m128i*)(dstRow + i), pixels);
				}
				else
				{
					_mm_store_si128((__m128i*)(dstRow + i), pixels);
				}
			}

			// Handle remaining pixels
			for (; i < rect->width; i++)
				dstRow[i] = srcRow[i];
		}

		srcPixels = tempBuffer;
		srcRect.x = 0;
		srcRect.y = 0;
		self_copy = 1;
	}

	// Main blitting loop with SSE optimization
	for (int j = 0; j < rect->height; j++)
	{
		int srcY = j + srcRect.y;
		int dstY = j + dstRect.y;

		if ((uint32_t)srcY >= (uint32_t)vbuf2->format.height) continue;
		if ((uint32_t)dstY >= (uint32_t)vbuf->format.height) continue;

		uint8_t* srcRow = srcPixels + srcY * srcStride + srcRect.x;
		uint8_t* dstRow = dstPixels + dstY * dstStride + dstRect.x;

		int i = 0;
		if (!has_alpha)
		{
			// Fast path: no alpha handling, copy entire rows
			for (; i + 15 < rect->width; i += 16)
			{
				__m128i pixels = _mm_loadu_si128((__m128i*)(srcRow + i));
				_mm_storeu_si128((__m128i*)(dstRow + i), pixels);
			}
		}
		else
		{
			 // Alpha handling path
			__m128i zero = _mm_setzero_si128();
			for (; i + 15 < rect->width; i += 16)
			{
				__m128i pixels = _mm_loadu_si128((__m128i*)(srcRow + i));
				__m128i mask = _mm_cmpeq_epi8(pixels, zero);

				// Load destination pixels
				__m128i dstPx = _mm_loadu_si128((__m128i*)(dstRow + i));

				// Blend: where mask is true (pixel==0), keep destination
				__m128i result = _mm_or_si128(
					_mm_and_si128(mask, dstPx),
					_mm_andnot_si128(mask, pixels)
				);

				_mm_storeu_si128((__m128i*)(dstRow + i), result);
			}
		}

		// Handle remaining pixels
		for (; i < rect->width; i++)
		{
			uint8_t pixel = srcRow[i];
			if (!(pixel == 0 && has_alpha))
				dstRow[i] = pixel;
		}
	}

	// Free temporary buffer if used
	if (self_copy)
		_mm_free(srcPixels);
	
#else
    int self_copy = 0;

    if (dstPixels == srcPixels)
    {
        size_t buf_len = srcStride * dstRect.w * dstRect.h;
        uint8_t* dstPixels = (uint8_t*)malloc(buf_len);
        int has_alpha = 0;//!(rect->width == 640);

        SDL_Rect dstRect_inter = {0, 0, rect->width, rect->height};

        for (int i = 0; i < rect->width; i++)
        {
            for (int j = 0; j < rect->height; j++)
            {
                if ((uint32_t)(i + srcRect.x) > (uint32_t)vbuf2->format.width) continue;
                if ((uint32_t)(j + srcRect.y) > (uint32_t)vbuf2->format.height) continue;
                
                uint8_t pixel = srcPixels[(i + srcRect.x) + ((j + srcRect.y)*srcStride)];

                if (!pixel && has_alpha) continue;
                if ((uint32_t)(i + dstRect_inter.x) > (uint32_t)vbuf->format.width) continue;
                if ((uint32_t)(j + dstRect_inter.y) > (uint32_t)vbuf->format.height) continue;

                dstPixels[(i + dstRect_inter.x) + ((j + dstRect_inter.y)*srcStride)] = pixel;
            }
        }
        
        srcPixels = dstPixels;
        srcRect.x = 0;
        srcRect.y = 0;

        self_copy = 1;
    }
    
    int once = 0;
    int has_alpha = !(rect->width == 640) && (alpha_maybe & 1);
    
    for (int i = 0; i < rect->width; i++)
    {
        for (int j = 0; j < rect->height; j++)
        {
            if ((uint32_t)(i + srcRect.x) >= (uint32_t)vbuf2->format.width) continue;
            if ((uint32_t)(j + srcRect.y) >= (uint32_t)vbuf2->format.height) continue;
            
            uint8_t pixel = srcPixels[(i + srcRect.x) + ((j + srcRect.y)*srcStride)];

            if (!pixel && has_alpha) continue;
            if ((uint32_t)(i + dstRect.x) >= (uint32_t)vbuf->format.width) continue;
            if ((uint32_t)(j + dstRect.y) >= (uint32_t)vbuf->format.height) continue;

            dstPixels[(i + dstRect.x) + ((j + dstRect.y)*dstStride)] = pixel;
        }
    }

    if (self_copy)
    {
        free(srcPixels);
    }
#endif

#ifdef HW_VBUFFER
	if (vbuf != vbuf2)
	{
		if (vbuf->device_surface && vbuf2->device_surface)
		{
			rdRect dstRect = { blit_x, blit_y, rect->width, rect->height };
			rdRect srcRect = { rect->x, rect->y, rect->width, rect->height };
			std3D_BlitDrawSurface(vbuf2->device_surface, &srcRect, vbuf->device_surface, &dstRect);
		}
	}
#endif
    
    //SDL_BlitSurface(vbuf2->sdlSurface, &srcRect, vbuf->sdlSurface, &dstRect); //TODO error check
    return 1;
}

int stdDisplay_VBufferFill(stdVBuffer *vbuf, int fillColor, rdRect *rect)
{    
	STD_BEGIN_PROFILER_LABEL();

    rdRect fallback = {0,0,vbuf->format.width, vbuf->format.height};
    if (!rect)
    {
        rect = &fallback;
    }
    
    //if (vbuf == &Video_menuBuffer)
    //    stdPlatform_Printf("Vbuffer fill to menu %u,%u %ux%u\n", rect->x, rect->y, rect->width, rect->height);

    SDL_Rect dstRect = {rect->x, rect->y, rect->width, rect->height};
    
    //printf("%x; %u %u %u %u\n", fillColor, rect->x, rect->y, rect->width, rect->height);

	// eebs: this is stupid slow
	//uint8_t* dstPixels = (uint8_t*)vbuf->sdlSurface->pixels;
    //uint32_t dstStride = vbuf->format.width_in_bytes;
    //uint32_t max_idx = dstStride * vbuf->format.height;
    //for (int i = 0; i < rect->width; i++)
    //{
    //    for (int j = 0; j < rect->height; j++)
    //    {
    //        uint32_t idx = (i + dstRect.x) + ((j + dstRect.y)*dstStride);
    //        if (idx > max_idx)
    //            continue;
    //        
    //        dstPixels[idx] = fillColor;
    //    }
    //}
    
	// eebs: about 1ms faster than above but still pretty shit, can maybe fix with the hw version
    SDL_FillRect(vbuf->sdlSurface, &dstRect, fillColor); //TODO error check

#ifdef HW_VBUFFER
	if (vbuf->device_surface)
		std3D_ClearDrawSurface(vbuf->device_surface, fillColor, rect);
#endif

	STD_END_PROFILER_LABEL();

    return 1;
}

int stdDisplay_VBufferSetColorKey(stdVBuffer *vbuf, int color)
{
    //DDCOLORKEY v3; // [esp+0h] [ebp-8h] BYREF

    if ( vbuf->bSurfaceLocked )
    {
        /*if ( vbuf->bSurfaceLocked == 1 )
        {
            v3.dwColorSpaceLowValue = color;
            v3.dwColorSpaceHighValue = color;
            vbuf->ddraw_surface->lpVtbl->SetColorKey(vbuf->ddraw_surface, 8, &v3);
            return 1;
        }*/
        vbuf->transparent_color = color;
    }
    else
    {
        vbuf->transparent_color = color;
    }
    return 1;
}

void stdDisplay_VBufferFree(stdVBuffer *vbuf)
{
    stdDisplay_VBufferUnlock(vbuf);
    SDL_FreeSurface(vbuf->sdlSurface);
#ifdef HW_VBUFFER
	std3D_FreeDrawSurface(vbuf);
#endif
    std_pHS->free(vbuf);
}

void stdDisplay_ddraw_surface_flip2()
{
}

void stdDisplay_RestoreDisplayMode()
{

}

stdVBuffer* stdDisplay_VBufferConvertColorFormat(void* a, stdVBuffer* b)
{
    return b;
}

int stdDisplay_GammaCorrect3(int gammaIndex)
{
	uint8_t convertedPalette[768];

	if (stdDisplay_paGammaTable == NULL)
	{
		memcpy(stdDisplay_gammaPalette, stdDisplay_tmpGammaPal, sizeof(rdColor24) * 256);
		return 0;
	}

	if (gammaIndex == 0)
	{
		memcpy(stdDisplay_gammaPalette, stdDisplay_tmpGammaPal, sizeof(rdColor24) * 256);
	}
	else
	{
		stdColor_GammaCorrect(
			&stdDisplay_gammaPalette[0].r,
			stdDisplay_tmpGammaPal,
			768,
			stdDisplay_paGammaTable[gammaIndex - 1]
		);
	}

	// jk usually sets palette to the window with GDI or D3D here
	memcpy(stdDisplay_masterPalette, stdDisplay_gammaPalette, sizeof(stdDisplay_masterPalette));

	stdDisplay_gammaIdx = gammaIndex;
	return 1;
}

int stdDisplay_SetCooperativeLevel(uint32_t a){return 0;}
int stdDisplay_DrawAndFlipGdi(uint32_t a){return 0;}
void stdDisplay_FreeBackBuffers()
{
}
#endif

void stdDisplay_GammaCorrect(const void *pPal)
{
	memcpy(stdDisplay_tmpGammaPal, pPal, 768);

	if (stdDisplay_paGammaTable != NULL && stdDisplay_gammaIdx != 0)
	{
		double gammaValue = stdDisplay_paGammaTable[stdDisplay_gammaIdx - 1];
		stdColor_GammaCorrect(
			&stdDisplay_gammaPalette[0].r,
			stdDisplay_tmpGammaPal,
			256,
			gammaValue
		);
	}
	else
	{
		memcpy(stdDisplay_gammaPalette, pPal, 768);
	}
}