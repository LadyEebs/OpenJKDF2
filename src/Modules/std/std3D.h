#ifndef _GL_STD3D_H
#define _GL_STD3D_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include "globals.h"

#include "Modules/rdroid/types.h"

#ifdef RENDER_DROID2

extern int std3D_bReinitHudElements;

// tmp
void std3D_BlitFramebuffer(int x, int y, int width, int height, void* pixels);

void std3D_SendVerticesToHardware(void* vertices, uint32_t count, uint32_t stride);
void std3D_SendIndicesToHardware(void* indices, uint32_t count, uint32_t stride);
void std3D_SetState(std3D_DrawCallState* pState, uint32_t updateBits);
void std3D_DrawElements(rdGeoMode_t geoMode, uint32_t count, uint32_t offset, uint32_t stride);
void std3D_PushDebugGroup(const char* name);
void std3D_PopDebugGroup();
void std3D_AdvanceFrame();

void std3D_ResetState();

// Added
int std3D_HasAlpha();
int std3D_HasModulateAlpha();
int std3D_HasAlphaFlatStippled();

int      std3D_Startup();
void     std3D_Shutdown();
int      std3D_StartScene();
int      std3D_EndScene();
void     std3D_ResetRenderList();
int      std3D_RenderListVerticesFinish();
void     std3D_DrawRenderList();
int      std3D_SetCurrentPalette(rdColor24 *a1, int a2);
void     std3D_GetValidDimension(unsigned int inW, unsigned int inH, unsigned int *outW, unsigned int *outH);
int      std3D_DrawOverlay();
void     std3D_UnloadAllTextures();
void     std3D_AddRenderListTris(rdTri *tris, unsigned int num_tris);
void     std3D_AddRenderListLines(rdLine* lines, uint32_t num_lines);
int      std3D_AddRenderListVertices(D3DVERTEX *vertex_array, int count);
void     std3D_UpdateFrameCount(rdDDrawSurface *surface);
void     std3D_PurgeTextureCache();
int      std3D_ClearZBuffer();
int      std3D_AddToTextureCache(stdVBuffer **vbuf, int numMips, rdDDrawSurface *texture, int is_alpha_tex, int no_alpha);
void     std3D_DrawMenu();
void     std3D_DrawSceneFbo();
void     std3D_FreeResources();
void     std3D_InitializeViewport(rdRect *viewRect);
int      std3D_GetValidDimensions(int a1, int a2, int a3, int a4);
int      std3D_FindClosestDevice(uint32_t index, int a2);
int      std3D_SetRenderList(intptr_t a1);
intptr_t std3D_GetRenderList();
int      std3D_CreateExecuteBuffer();

void std3D_PurgeUIEntry(int i, int idx);
void std3D_PurgeTextureEntry(int i);
void std3D_PurgeBitmapRefs(stdBitmap *pBitmap);
void std3D_PurgeSurfaceRefs(rdDDrawSurface *texture);
void std3D_UpdateSettings();
void std3D_Screenshot(const char* pFpath);

void std3D_ResetUIRenderList();
int  std3D_AddBitmapToTextureCache(stdBitmap *texture, int mipIdx, int is_alpha_tex, int no_alpha);
void std3D_DrawUIBitmapRGBA(stdBitmap* pBmp, int mipIdx, float dstX, float dstY, rdRect* srcRect, float scaleX, float scaleY, int bAlphaOverwrite, uint8_t color_r, uint8_t color_g, uint8_t color_b, uint8_t color_a);
void std3D_DrawUIBitmap(stdBitmap* pBmp, int mipIdx, float dstX, float dstY, rdRect* srcRect, float scale, int bAlphaOverwrite);
void std3D_DrawUIClearedRect(uint8_t palIdx, rdRect* dstRect);
void std3D_DrawUIClearedRectRGBA(uint8_t color_r, uint8_t color_g, uint8_t color_b, uint8_t color_a, rdRect* dstRect);
int  std3D_IsReady();

int  std3D_GenShader();
void std3D_DeleteShader(int id);
void std3D_UploadShader(rdShader* shader);

void std3D_FlushPostFX();

void std3D_PurgeDecals();
int  std3D_UploadDecalTexture(rdRectf* out, stdVBuffer* vbuf, rdDDrawSurface* pTexture);

void std3D_SendLightsToHardware(rdClusterLight* lights, uint32_t lightOffset, uint32_t numLights);
void std3D_SendOccludersToHardware(rdClusterOccluder* occluders, uint32_t occluderOffset, uint32_t numOccluders);
void std3D_SendDecalsToHardware(rdClusterDecal* decals, uint32_t decalOffset, uint32_t numDecals);
void std3D_SendClusterBitsToHardware(uint32_t* clusterBits, float znear, float zfar, uint32_t tileSizeX, uint32_t tileSizeY);

#ifdef HW_VBUFFER
std3D_DrawSurface* std3D_AllocDrawSurface(stdVBufferTexFmt* fmt, int32_t width, int32_t height);
void  std3D_FreeDrawSurface(std3D_DrawSurface* surface);
void  std3D_BlitDrawSurface(std3D_DrawSurface* src, rdRect* srcRect, std3D_DrawSurface* dst, rdRect* dstRect);
void  std3D_ClearDrawSurface(std3D_DrawSurface* surface, int fillColor, rdRect* rect);
void std3D_UploadDrawSurface(std3D_DrawSurface* src, int width, int height, void* pixels, uint8_t* palette);
#endif


#endif // RENDER_DROID2

#ifdef __cplusplus
}
#endif


#endif // _GL_STD3D_H
