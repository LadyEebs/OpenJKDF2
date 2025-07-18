#ifndef _RDROID_H
#define _RDROID_H

#include "jk.h"
#include "types.h"
#include "globals.h"

#define rdStartup_ADDR (0x0043A950)
#define rdShutdown_ADDR (0x0043A990)
#define rdOpen_ADDR (0x0043A9B0)
#define rdClose_ADDR (0x0043AA40)
#define rdSetRenderOptions_ADDR (0x0043AA60)
#define rdSetGeometryMode_ADDR (0x0043AA70)
#define rdSetLightingMode_ADDR (0x0043AA80)
#define rdSetTextureMode_ADDR (0x0043AA90)
#define rdSetSortingMethod_ADDR (0x0043AAA0)
#define rdSetOcclusionMethod_ADDR (0x0043AAB0)
#define rdSetZBufferMethod_ADDR (0x0043AAC0)
#define rdSetCullFlags_ADDR (0x0043AAD0)
#define rdSetProcFaceUserData_ADDR (0x0043AAE0)
#define rdGetRenderOptions_ADDR (0x0043AAF0)
#define rdGetGeometryMode_ADDR (0x0043AB00)
#define rdGetLightingMode_ADDR (0x0043AB10)
#define rdGetTextureMode_ADDR (0x0043AB20)
#define rdGetSortingMethod_ADDR (0x0043AB30)
#define rdGetOcclusionMethod_ADDR (0x0043AB40)
#define rdGetZBufferMethod_ADDR (0x0043AB50)
#define rdGetCullFlags_ADDR (0x0043AB60)
#define rdGetProcFaceUserData_ADDR (0x0043AB70)
#define rdSetMipDistances_ADDR (0x0043AB80)
#define rdSetColorEffects_ADDR (0x0043ABB0)
#define rdAdvanceFrame_ADDR (0x0043ABD0)
#define rdFinishFrame_ADDR (0x0043ABF0)
#define rdClearPostStatistics_ADDR (0x0043AC10)

extern int rdroid_curVertexColorMode;
#ifdef FOG
extern int rdroid_curFogEnabled;
extern rdVector4 rdroid_curFogColor;
extern float rdroid_curFogStartDepth;
extern float rdroid_curFogEndDepth;
#endif

int rdStartup(HostServices *p_hs);
void rdShutdown();
int rdOpen(int a1);
void rdClose();

void rdSetRenderOptions(int a1);
void rdSetGeometryMode(int a1);
void rdSetLightingMode(int a1);
void rdSetTextureMode(int a1);
void rdSetSortingMethod(int a1);
void rdSetOcclusionMethod(int a1);
void rdSetZBufferMethod(rdZBufferMethod_t val);
void rdSetCullFlags(int a1);
void rdSetProcFaceUserData(int a1);
void rdSetVertexColorMode(int a1);

#if defined(FOG) && !defined(RENDER_DROID2)
void rdSetFog(int active, const rdVector4* color, float startDepth, float endDepth);
#endif

int rdGetRenderOptions(void);
int rdGetGeometryMode(void);
int rdGetLightingMode(void);
int rdGetTextureMode(void);
int rdGetSortingMethod(void);
int rdGetOcclusionMethod(void);
int rdGetZBufferMethod(void);
int rdGetCullFlags(void);
int rdGetProcFaceUserData(void);
int rdGetVertexColorMode(void);

MATH_FUNC int rdSetMipDistances(rdVector4 *dists);
int rdSetColorEffects(stdPalEffect *effects);

void rdAdvanceFrame();
void rdFinishFrame();
void rdClearPostStatistics();

//#define  (*(int*)0x)

#ifdef RENDER_DROID2
// todo: the original rdProcEntry stuff was somewhat stateless, try to move away from stateful api?

#define RD_PACK_COLOR8(r, g, b, a)  (b | (g << 8) | (r << 16) | (a << 24))
#define RD_PACK_COLOR10(r, g, b, a) (b | (g << 10) | (r << 20) | (a << 30))
#define RD_PACK_COLOR8F(r, g, b, a)  (stdMath_ClampInt(b * 255, 0, 255) | (stdMath_ClampInt(g * 255, 0, 255) << 8) | (stdMath_ClampInt(r * 255, 0, 255) << 16) | (stdMath_ClampInt(a * 255, 0, 255) << 24))

void rdDepthRange(float znear, float zfar);

// todo: rdPushMatrix/rdPopMatrix?
void rdMatrixMode(rdMatrixMode_t mode);
void rdPerspective(float fov, float aspect, float nearPlane, float farPlane);
void rdOrthographic(float width, float height, float nearPlane, float farPlane);
void rdLookat(const rdVector3* pViewer, const rdVector3* pTarget, const rdVector3* pUp);
void rdTranslate(const rdVector3* pTranslation);
void rdRotate(const rdVector3* pRotation);
void rdScale(const rdVector4* pScaling);
void rdIdentity();
void rdTranspose();
void rdLoadMatrix34(const rdMatrix34* pMatrix);
void rdLoadMatrix(const rdMatrix44* pMatrix);
void rdPostMultiplyMatrix(const rdMatrix44* pMatrix);
void rdPreMultiplyMatrix(const rdMatrix44* pMatrix);
void rdGetMatrix(rdMatrix44* pOut, rdMatrixMode_t mode);
void rdResetMatrices();

// fog
void rdFogRange(float startDepth, float endDepth);
void rdFogColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void rdFogColorf(float r, float g, float b, float a);
void rdFogAnisotropy(float g);
void rdFogLightDir(float x, float y, float z);

// viewport
void rdViewport(float x, float y, float width, float height);
void rdGetViewport(rdViewportRect* pOut);

// scissor
void rdScissorMode(rdScissorMode_t mode);
void rdScissor(int x, int y, int width, int height);
void rdScissorf(float x, float y, float width, float height);

// primitives
int rdBeginPrimitive(rdPrimitiveType_t type);
void rdEndPrimitive();
void rdVertex3f(float x, float y, float z);
void rdColor4f(float r, float g, float b, float a);
void rdTexCoord2f(uint8_t i, float u, float v); // normalized
void rdTexCoord2i(uint8_t i, float u, float v); // unnormalized
void rdTexCoord3i(uint8_t i, float u, float v, float w); // unnormalized
void rdTexCoordScaled2i(uint8_t i, float u, float v, float w, float h); // kinda suckyyy...
void rdNormal3f(float x, float y, float z);
void rdVertex3v(const float* v);
void rdColor4v(const float* v);
void rdTexCoord2v(uint8_t i, const float* v);
void rdNormal3v(const float* v);
void rdTexOffset(uint8_t i, float u, float v);
void rdTexOffseti(uint8_t i, float u, float v);

// render state
void rdSetZBufferCompare(rdCompare_t compare);
void rdSetFogMode(rdFogMode_t mode);
void rdSetBlendEnabled(int enabled);
void rdSetBlendMode(rdBlend_t src, rdBlend_t dst);
void rdSetCullMode(rdCullMode_t mode);
void rdAlphaTestFunction(rdCompare_t mode);
void rdSetAlphaTestReference(uint8_t ref);
void rdSetConstantColorf(float r, float g, float b, float a);
void rdSetConstantColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void rdSetChromaKey(rdChromaKeyMode_t mode);
void rdSetChromaKeyValue(uint8_t r, uint8_t g, uint8_t b);

void rdSortOrder(int sortOrder);
void rdSortDistance(float distance);

// these names kinda suck, come up with better for per-primitive modes
void rdSetGeoMode(rdGeoMode_t a1);
void rdSetLightMode(rdLightMode_t a1);
void rdSetTexMode(rdTexMode_t a1);

void rdDitherMode(rdDitherMode_t mode);
void rdSetGlowIntensity(float intensity);

void rdSetDecalMode(rdDecalMode_t mode);
void rdSetOverbright(float overbright);

void rdStencilBit(uint8_t bit);
void rdStencilMode(uint8_t mode);

void rdColorMask(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// shader
void rdSetShader(rdShader* shader);
void rdSetShaderConstant(uint8_t idx, const rdVector4* value);
void rdSetShaderConstant4f(uint8_t idx, float x, float y, float z, float w);

// texture
int rdBindTexture(rdTexture* pTexture);

void rdTexFilterMode(rdTexFilter_t texFilter);
void rdTexGen(rdTexGen_t texGen);
void rdTexGenParams(float p0, float p1, float p2, float p3);
void rdTexClampMode(int modeU, int modeV);

// material
int rdBindMaterial(rdMaterial* pMaterial, int cel);

// lighting
int rdAddLight(rdLight* pLight, rdVector3* pPosition);
void rdAddOccluder(rdVector3* position, float radius);
void rdAddDecal(rdDecal* decal, rdMatrix34* matrix, rdVector3* color, rdVector3* scale, float angleFade);

void rdExtraLight(float extra);
void rdAmbientFlags(uint32_t flags);
void rdAmbientLight(float r, float g, float b);
void rdAmbientLightSH(rdAmbient* amb);

// shaders
int rdGenShader();
void rdDeleteShader(int id);
int rdCompileShader(int id, const char* path);

#endif

#endif // _RDROID_H
