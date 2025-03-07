﻿#include "Modules/std/std3D.h"

#include "types.h"
#include "globals.h"

#ifdef RENDER_DROID2

#include "Raster/rdCache.h"
#include "Win95/stdDisplay.h"
#include "Win95/Window.h"
#include "World/sithWorld.h"
#include "Engine/rdColormap.h"
#include "Main/jkGame.h"
#include "World/jkPlayer.h"
#include "General/stdBitmap.h"
#include "stdPlatform.h"
#include "Engine/rdClip.h"
#include "Primitives/rdMath.h"

#include "jk.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "Platform/GL/shader_utils.h"
#include "Platform/GL/jkgm.h"

#include "SDL2_helper.h"

#include "General/stdMath.h"
#include "General/stdHashTable.h"
#include "Modules/std/stdJob.h"
#include "Modules/std/stdProfiler.h"

#ifdef WIN32
// Force Optimus/AMD to use non-integrated GPUs by default.
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

#define TEX_MODE_TEST 0
#define TEX_MODE_WORLDPAL 1
#define TEX_MODE_BILINEAR 2
#define TEX_MODE_16BPP 5
#define TEX_MODE_BILINEAR_16BPP 6

// legacy names
#define TEX_SLOT_DIFFUSE         0
#define TEX_SLOT_EMISSIVE        1
#define TEX_SLOT_DISPLACEMENT    2

#define TEX_SLOT_TEX0			 0
#define TEX_SLOT_TEX1			 1
#define TEX_SLOT_TEX2			 2
#define TEX_SLOT_TEX3			 3
#define TEX_SLOT_WORLD_PAL       4
#define TEX_SLOT_WORLD_LIGHT_PAL 5
#define TEX_SLOT_CLUSTER_BUFFER  6
#define TEX_SLOT_DECAL_ATLAS     7
#define TEX_SLOT_DEPTH           8
#define TEX_SLOT_AO              9
#define TEX_SLOT_REFRACTION     10
#define TEX_SLOT_CLIP           11
#define TEX_SLOT_DITHER         12
#define TEX_SLOT_DIFFUSE_LIGHT  13

#define UBO_SLOT_LIGHTS        0
#define UBO_SLOT_OCCLUDERS     1
#define UBO_SLOT_DECALS        2
#define UBO_SLOT_SHARED        3
#define UBO_SLOT_FOG           4
#define UBO_SLOT_TEX           5
#define UBO_SLOT_MATERIAL      6
#define UBO_SLOT_SHADER        7
#define UBO_SLOT_SHADER_CONSTS 8

#define STD3D_MAX_RENDER_PASSES     3

#define CLUSTER_MAX_LIGHTS          256 // match RDCAMERA_MAX_LIGHTS/SITHREND_NUM_LIGHTS
#define CLUSTER_MAX_OCCLUDERS       128
#define CLUSTER_MAX_DECALS          256
#define CLUSTER_MAX_ITEMS           (CLUSTER_MAX_LIGHTS + CLUSTER_MAX_OCCLUDERS + CLUSTER_MAX_DECALS)
#define CLUSTER_BUCKETS_PER_CLUSTER (CLUSTER_MAX_ITEMS / 32)
#define CLUSTER_GRID_SIZE_X         16
#define CLUSTER_GRID_SIZE_Y         8
#define CLUSTER_GRID_SIZE_Z         24

#define CLUSTER_GRID_SIZE_XYZ (CLUSTER_GRID_SIZE_X * CLUSTER_GRID_SIZE_Y * CLUSTER_GRID_SIZE_Z)
#define CLUSTER_GRID_TOTAL_SIZE (CLUSTER_GRID_SIZE_X * CLUSTER_GRID_SIZE_Y * CLUSTER_GRID_SIZE_Z * CLUSTER_BUCKETS_PER_CLUSTER)

typedef struct std3DSimpleTexStage
{
    GLuint program;

    GLint attribute_coord3d;
    GLint attribute_v_color;
    GLint attribute_v_uv;
    GLint attribute_v_norm;

    GLint uniform_mvp;
	GLint uniform_proj;
	
	GLint uniform_tex;
    GLint uniform_tex2;
    GLint uniform_tex3;
	GLint uniform_tex4;
	GLint uniform_iResolution;

    GLint uniform_param1;
    GLint uniform_param2;
    GLint uniform_param3;

	GLint uniform_rt;
	GLint uniform_lt;
	GLint uniform_rb;
	GLint uniform_lb;

	GLint uniform_shared;
	GLint uniform_shader;
	GLint uniform_shaderConsts;
	GLint uniform_fog;
	GLint uniform_tex_block;
	GLint uniform_material;
	GLint uniform_lightbuf;
	GLint uniform_lights;
	GLint uniform_occluders;
	GLint uniform_decals;

	GLint uniform_tint;
	GLint uniform_filter;
	GLint uniform_fade;
	GLint uniform_add;
} std3DSimpleTexStage;

typedef struct std3DIntermediateFbo
{
    GLuint fbo;
    GLuint tex;

    GLuint rbo;
    int32_t w;
    int32_t h;

    int32_t iw;
    int32_t ih;
} std3DIntermediateFbo;

// todo: move to stdVBuffer/stdDisplay?
typedef struct std3DFramebuffer
{
	int32_t w;
	int32_t h;

	//GLuint rbo;
	GLuint zfbo;
    GLuint fbo;
	GLuint ntex;
	GLuint ztex;
	GLuint tex0; // color
    GLuint tex1; // emissive
} std3DFramebuffer;

std3DIntermediateFbo window;
std3DIntermediateFbo std3D_mainFbo;

int std3D_framebufferFlags = 0;

std3DIntermediateFbo deferred; // opaque lighting prepass
std3DIntermediateFbo refr; // refraction tex
std3DIntermediateFbo refrZ; // refraction z clipping tex
std3DIntermediateFbo ssaoDepth;
std3DIntermediateFbo ssao;
std3DIntermediateFbo bloomLayers[4];

GLint std3D_windowFbo = 0;
std3DFramebuffer std3D_framebuffer;

static bool has_initted = false;

static void* last_overlay = NULL;

int init_once = 0;
GLuint programMenu;
GLint attribute_coord3d, attribute_v_color, attribute_v_light, attribute_v_uv, attribute_v_norm;

GLint uniform_mvp, uniform_tex, uniform_texEmiss, uniform_displacement_map, uniform_tex_mode, uniform_blend_mode, uniform_worldPalette, uniform_worldPaletteLights;
GLint uniform_tint, uniform_filter, uniform_fade, uniform_add, uniform_emissiveFactor, uniform_albedoFactor;
GLint uniform_light_mult, uniform_displacement_factor, uniform_iResolution, uniform_enableDither;
#ifdef FOG
GLint uniform_fog, uniform_fog_color, uniform_fog_start, uniform_fog_end;
#endif

// fixme: this shouldn't exist, make matrix part of the render pass
rdMatrix44* renderPassProj = NULL;


float sliceScalingFactor;
float sliceBiasFactor;
uint32_t tileSizeX;
uint32_t tileSizeY;
GLuint cluster_buffer;
GLuint cluster_tbo;

// uniforms shared across draw lists during flush
typedef struct std3D_SharedUniforms
{
	rdVector4 sgBasis[8];
	rdVector4 mipDistances;

	float     timeSeconds;
	float     lightMult;
	rdVector2 resolution;

	rdVector2 clusterTileSizes;
	rdVector2 clusterScaleBias;

} std3D_SharedUniforms;

GLuint shared_ubo;

typedef struct std3D_FogUniforms
{
	rdVector4 fogColor;
	int32_t   fogEnabled;
	float     fogStartDepth;
	float     fogEndDepth;
	float     fogAnisotropy;
	rdVector4 fogLightDir;
} std3D_FogUniforms;

GLuint fog_ubo;

typedef struct std3D_DrawUniforms
{
	rdMatrix44 projeciton;
	rdMatrix44 modelMatrix;
} std3D_DrawUniforms;

typedef struct std3D_TextureUniforms
{
	int32_t   tex_mode;
	int32_t   uv_mode;
	int32_t   texgen;
	int32_t   numMips;

	rdVector2 texsize;
	rdVector2 uv_offset;

	rdVector4 texgen_params;

	rdVector4 padding;
} std3D_TextureUniforms;
GLuint tex_ubo;

typedef struct std3D_MaterialUniforms
{
	rdVector4 fillColor;
	rdVector4 albedo_factor;
	rdVector4 emissive_factor;
	rdVector4 specular_factor;

	float    displacement_factor;
	float    roughnessFactor;
	float    texPad1, texPad2;
} std3D_MaterialUniforms;
GLuint material_ubo;

typedef struct std3D_light
{
	rdVector4 position;
	rdVector4 direction_intensity;

	//rdVector4 color;
	uint32_t color;
	float radiusSq, pad2, pad3;

	int32_t   type;
	float     falloffMin;
	float     falloffMax;
	float     lux;
	
	float     angleX;
	float     cosAngleX;
	float     angleY;
	float     cosAngleY;
} std3D_light;

int lightsDirty = 0;

typedef struct std3D_LightUniforms
{
	uint32_t firstLight;
	uint32_t numLights;
	uint32_t lightPad0, lightPad1;
	std3D_light tmpLights[CLUSTER_MAX_LIGHTS];
} std3D_LightUniforms;
std3D_LightUniforms lightUniforms;

GLuint light_ubo;

typedef struct std3D_occluder
{
	rdVector4 position;
} std3D_occluder;

int occludersDirty = 0;


typedef struct std3D_OccluderUniforms
{
	uint32_t firstOccluder;
	uint32_t numOccluders;
	uint32_t occluderPad0, occluderPad1;
	std3D_occluder tmpOccluders[CLUSTER_MAX_OCCLUDERS];
} std3D_OccluderUniforms;
std3D_OccluderUniforms occluderUniforms;

GLuint occluder_ubo;

typedef struct std3D_decal
{
	rdMatrix44      decalMatrix;
	rdMatrix44      invDecalMatrix;
	rdVector4       uvScaleBias;
	rdVector4       posRad;
	rdVector4       color;
	uint32_t        flags;
	float           angleFade;
	float           padding0;
	float           padding1;
} std3D_decal;

int decalsDirty = 0;

typedef struct std3D_DecalUniforms
{
	uint32_t firstDecal;
	uint32_t numDecals;
	uint32_t decalPad0, decalPad1;
	std3D_decal tmpDecals[CLUSTER_MAX_DECALS];
} std3D_DecalUniforms;
std3D_DecalUniforms decalUniforms;


GLuint decal_ubo;

typedef struct std3D_decalAtlasNode
{
	char name[32];
	rdRect rect;
	struct std3D_decalAtlasNode* children[2];
	rdDDrawSurface* texture;
} std3D_decalAtlasNode;

#define DECAL_ATLAS_SIZE 1024

static int numAllocNodes = 0;
static std3D_decalAtlasNode nodePool[(DECAL_ATLAS_SIZE / 4) * (DECAL_ATLAS_SIZE / 4)];
static std3D_decalAtlasNode decalRootNode;
static stdHashTable* decalHashTable = NULL;

int std3D_InsertDecalTexture(rdRect* out, stdVBuffer* vbuf, rdDDrawSurface* pTexture);
void std3D_PurgeDecalAtlas();

#ifdef JOB_SYSTEM
#define USE_JOBS
#endif

#ifdef USE_JOBS
#include "SDL_atomic.h"
#endif

typedef struct std3D_Cluster
{
	int       lastUpdateFrame;
	rdVector3 minb;
	rdVector3 maxb;
} std3D_Cluster;

static uint32_t std3D_clusterBits[CLUSTER_GRID_TOTAL_SIZE];

#define STD3D_MAX_DRAW_CALLS 16384
#define STD3D_MAX_DRAW_CALL_VERTS (STD3D_MAX_DRAW_CALLS * 24)
#define STD3D_MAX_DRAW_CALL_INDICES (STD3D_MAX_DRAW_CALLS * 66)

typedef enum STD3D_DRAW_LIST
{
	DRAW_LIST_Z,
	DRAW_LIST_Z_ALPHATEST,
	DRAW_LIST_Z_REFRACTION,
	DRAW_LIST_COLOR_ZPREPASS,
	DRAW_LIST_COLOR_NOZPREPASS,
	DRAW_LIST_COLOR_ALPHABLEND,
	DRAW_LIST_COLOR_REFRACTION,
	DRAW_LIST_COUNT
} STD3D_DRAW_LIST;

// define the maximum number of staging buffers needed for a whole frame to prevent reuse (vbo/ibo)
#define STD3D_STAGING_COUNT (DRAW_LIST_COUNT * STD3D_MAX_RENDER_PASSES)

typedef struct std3D_DrawCallList
{
	int              bSorted;
	uint32_t         drawCallCount;
	uint32_t         drawCallIndexCount;
	uint32_t         drawCallVertexCount;
	std3D_DrawCall*  drawCalls;
	uint16_t*        drawCallIndices;
	rdVertex*        drawCallVertices;
	std3D_DrawCall** drawCallPtrs;
} std3D_DrawCallList;

// todo/fixme: we're not currently handling viewport changes mid-draw
typedef struct std3D_RenderPass
{
	char                name[32];
	rdRenderPassFlags_t flags;
	rdVector2           depthRange; // todo: move to draw call render state
	std3D_DrawCallList  drawCallLists[DRAW_LIST_COUNT];
	rdMatrix44          oldProj; // keep track of the global projection to avoid redundant cluster building if the matrix doesn't change over the course of several frames
	int                 clustersDirty;       // clusters need rebuilding/refilling
	int                 clusterFrustumFrame; // current frame for clusters, any cluster not matching will have its bounds updated
	int                 lastClusterFrustumFrame; // last frame for clusters, will be updated to clusterFrustumFrame after building
	std3D_Cluster       clusters[CLUSTER_GRID_TOTAL_SIZE]; // each render pass gets its own cluster state to avoid recomputing the cluster bounds every time the projection changes
} std3D_RenderPass;

// todo: likely better to just swap to a BeginRenderPass/EndRenderPass in rdroid and call flush in EndRenderPass so we can have as many as we want
std3D_RenderPass std3D_renderPasses[STD3D_MAX_RENDER_PASSES];

void std3D_initDrawList(std3D_DrawCallList* pList)
{
	pList->drawCalls = malloc(sizeof(std3D_DrawCall) * STD3D_MAX_DRAW_CALLS);
	pList->drawCallIndices = malloc(sizeof(uint16_t) * STD3D_MAX_DRAW_CALL_INDICES);
	pList->drawCallVertices = malloc(sizeof(rdVertex) * STD3D_MAX_DRAW_CALL_VERTS);
	pList->drawCallPtrs = malloc(sizeof(std3D_DrawCall*) * STD3D_MAX_DRAW_CALLS);
}

void std3D_freeDrawList(std3D_DrawCallList* pList)
{
	if (pList->drawCalls)
	{
		free(pList->drawCalls);
		pList->drawCalls = 0;
	}

	if (pList->drawCallIndices)
	{
		free(pList->drawCallIndices);
		pList->drawCallIndices = 0;
	}

	if (pList->drawCallVertices)
	{
		free(pList->drawCallVertices);
		pList->drawCallVertices = 0;
	}

	if (pList->drawCallPtrs)
	{
		free(pList->drawCallPtrs);
		pList->drawCallPtrs = 0;
	}
}

void std3D_initRenderPass(std3D_RenderPass* pRenderPass)
{
	memset(pRenderPass, 0, sizeof(std3D_RenderPass));
	for(int i = 0; i < DRAW_LIST_COUNT; ++i)
		std3D_initDrawList(&pRenderPass->drawCallLists[i]);
}

void std3D_freeRenderPass(std3D_RenderPass* pRenderPass)
{
	for (int i = 0; i < DRAW_LIST_COUNT; ++i)
		std3D_freeDrawList(&pRenderPass->drawCallLists[i]);
}

void std3D_FlushLights();
void std3D_FlushOccluders();
void std3D_FlushDecals();
void std3D_BuildClusters(std3D_RenderPass* pRenderPass, rdMatrix44* pProjection);

typedef enum STD3D_SHADER_ID
{
	SHADER_DEPTH,
	SHADER_DEPTH_ALPHATEST,

	SHADER_COLOR_UNLIT,
	SHADER_COLOR,
	SHADER_COLOR_SPEC,

	SHADER_COLOR_ALPHATEST_UNLIT,
	SHADER_COLOR_ALPHATEST,
	SHADER_COLOR_ALPHATEST_SPEC,

	SHADER_COLOR_ALPHABLEND_UNLIT,
	SHADER_COLOR_ALPHABLEND,
	SHADER_COLOR_ALPHABLEND_SPEC,

	SHADER_COLOR_REFRACTION,

	SHADER_COUNT
} STD3D_DRAW_LIST;

typedef struct std3D_worldStage
{
	GLuint program;
	GLint attribute_coord3d, attribute_v_color, attribute_v_light, attribute_v_uv, attribute_v_norm;
	GLint uniform_projection, uniform_modelMatrix, uniform_viewMatrix;
	GLint uniform_ambient_color, uniform_ambient_sg, uniform_ambient_sg_count;
	GLint uniform_geo_mode,  uniform_fillColor, uniform_textures, uniform_tex, uniform_texEmiss, uniform_displacement_map, uniform_texDecals, uniform_texz, uniform_texssao, uniform_texrefraction, uniform_texclip;
	GLint uniform_worldPalette, uniform_worldPaletteLights, uniform_dithertex, uniform_diffuse_light;
	GLint uniform_light_mode, uniform_ao_flags;
	GLint uniform_shared, uniform_shader, uniform_shaderConsts, uniform_fog, uniform_tex_block, uniform_material, uniform_lightbuf, uniform_lights, uniform_occluders, uniform_decals;
	GLint uniform_rightTop;
	GLint uniform_rt;
	GLint uniform_lt;
	GLint uniform_rb;
	GLint uniform_lb;

	GLuint vao[STD3D_STAGING_COUNT];
} std3D_worldStage;

std3D_worldStage worldStages[SHADER_COUNT];

GLint programMenu_attribute_coord3d, programMenu_attribute_v_color, programMenu_attribute_v_uv, programMenu_attribute_v_norm;
GLint programMenu_uniform_mvp, programMenu_uniform_tex, programMenu_uniform_displayPalette;

std3DSimpleTexStage std3D_uiProgram;
std3DSimpleTexStage std3D_texFboStage;
std3DSimpleTexStage std3D_deferredStage;
std3DSimpleTexStage std3D_postfxStage;
std3DSimpleTexStage std3D_bloomStage;
std3DSimpleTexStage std3D_ssaoStage;

std3DSimpleTexStage std3D_decalAtlasStage;
std3DIntermediateFbo decalAtlasFBO;

unsigned int vao;
GLuint blank_tex, blank_tex_white;
void* blank_data = NULL, *blank_data_white = NULL;
GLuint worldpal_texture;
void* worldpal_data = NULL;
GLuint worldpal_lights_texture;
void* worldpal_lights_data = NULL;
GLuint displaypal_texture;
void* displaypal_data = NULL;
GLuint tiledrand_texture;
rdVector3 tiledrand_data[4 * 4];

GLuint dither_texture;
uint8_t dither_data[4*4];

GLuint linearSampler[4];
GLuint nearestSampler[4];

size_t std3D_loadedUITexturesAmt = 0;
stdBitmap* std3D_aUIBitmaps[STD3D_MAX_TEXTURES] = {0};
GLuint std3D_aUITextures[STD3D_MAX_TEXTURES] = {0};
static rdUITri GL_tmpUITris[STD3D_MAX_UI_TRIS] = {0};
static size_t GL_tmpUITrisAmt = 0;
GLuint last_ui_tex = 0;
int last_ui_flags = 0;
static D3DVERTEX GL_tmpUIVertices[STD3D_MAX_UI_VERTICES] = {0};
static size_t GL_tmpUIVerticesAmt = 0;

rdDDrawSurface* std3D_aLoadedSurfaces[STD3D_MAX_TEXTURES] = {0};
GLuint std3D_aLoadedTextures[STD3D_MAX_TEXTURES] = {0};
size_t std3D_loadedTexturesAmt = 0;
static rdTri GL_tmpTris[STD3D_MAX_TRIS] = {0};
static size_t GL_tmpTrisAmt = 0;
static rdLine GL_tmpLines[STD3D_MAX_VERTICES] = {0};
static size_t GL_tmpLinesAmt = 0;
static D3DVERTEX GL_tmpVertices[STD3D_MAX_VERTICES] = {0};
static size_t GL_tmpVerticesAmt = 0;
static size_t rendered_tris = 0;

static void* loaded_colormap = NULL;

rdDDrawSurface* last_tex = NULL;
int last_flags = 0;

int bufferIdx;
GLuint world_vbo_all[STD3D_STAGING_COUNT];
GLuint world_ibo_triangle[STD3D_STAGING_COUNT];

D3DVERTEX* menu_data_all = NULL;
GLushort* menu_data_elements = NULL;
GLuint menu_vao;
GLuint menu_vbo_all;
GLuint menu_ibo_triangle;

extern int jkGuiBuildMulti_bRendering;

int std3D_bInitted = 0;
rdColormap std3D_ui_colormap;
int std3D_bReinitHudElements = 0;

static inline bool std3D_isIntegerFormat(GLuint format)
{
	switch (format)
	{
	case GL_R8UI:
	case GL_R16UI:
	case GL_R16I:
	case GL_R32UI:
	case GL_R32I:
		return true;
	default:
		return false;
	}
}

static inline GLuint std3D_getUploadFormat(GLuint format)
{
	switch (format)
	{
	case GL_R3_G3_B2:
		return GL_UNSIGNED_BYTE_3_3_2;
	case GL_RGB565:
		return GL_UNSIGNED_SHORT_5_6_5;
	case GL_RGBA4:
		return GL_UNSIGNED_SHORT_4_4_4_4;
	case GL_RGB5_A1:
		return GL_UNSIGNED_SHORT_5_5_5_1;
	case GL_R8:
	case GL_RG8:
	case GL_RGB8:
	case GL_RGBA8:
		return GL_UNSIGNED_BYTE;
	case GL_RGB10_A2:
		return GL_UNSIGNED_INT_2_10_10_10_REV;
	case GL_R8_SNORM:
	case GL_RG8_SNORM:
	case GL_RGB8_SNORM:
	case GL_RGBA8_SNORM:
		return GL_BYTE;
	case GL_R16:
	case GL_RG16:
	case GL_RGB16:
	case GL_RGBA16:
		return GL_UNSIGNED_SHORT;
	case GL_R16_SNORM:
	case GL_RG16_SNORM:
	case GL_RGB16_SNORM:
	case GL_RGBA16_SNORM:
		return GL_SHORT;
	case GL_R11F_G11F_B10F:
	case GL_R16F:
	case GL_RG16F:
	case GL_RGB16F:
	case GL_RGBA16F:
		return GL_HALF_FLOAT;
	case GL_R32F:
	case GL_RG32F:
	case GL_RGB32F:
	case GL_RGBA32F:
		return GL_FLOAT;
	case GL_R32UI:
	case GL_RGBA32UI:
		return GL_UNSIGNED_INT;
	case GL_R32I:
		return GL_INT;
	case GL_R16UI:
		return GL_UNSIGNED_SHORT;
	case GL_R16I:
		return GL_SHORT;
	case GL_R8UI:
		return GL_UNSIGNED_BYTE;
	case GL_RGB4:
	case GL_RGB5:
	default:
		return GL_UNSIGNED_BYTE;
	};
}

static inline uint8_t std3D_getNumChannels(GLuint format)
{
	switch (format)
	{
	case GL_R8:
	case GL_R8_SNORM:
	case GL_R16:
	case GL_R16_SNORM:
	case GL_R16F:
	case GL_R32F:
	case GL_R8UI:
	case GL_R16UI:
	case GL_R16I:
	case GL_R32UI:
	case GL_R32I:
	case GL_DEPTH_COMPONENT16:
	case GL_DEPTH_COMPONENT24:
	case GL_DEPTH_COMPONENT32:
	case GL_DEPTH_COMPONENT32F:
		return 1;
	case GL_RG8:
	case GL_RG8_SNORM:
	case GL_RG16:
	case GL_RG16_SNORM:
	case GL_RG16F:
	case GL_RG16UI:
	case GL_RG16I:
	case GL_RG32F:
		return 2;
	case GL_R3_G3_B2:
	case GL_RGB4:
	case GL_RGB5:
	case GL_RGB565:
	case GL_RGB8:
	case GL_RGB8_SNORM:
	case GL_RGB16:
	case GL_RGB16_SNORM:
	case GL_RGB16F:
	case GL_RGB32F:
	case GL_R11F_G11F_B10F:
	case GL_SRGB8:
		return 3;
	case GL_RGBA4:
	case GL_RGB5_A1:
	case GL_RGBA8:
	case GL_RGBA8_SNORM:
	case GL_RGB10_A2:
	case GL_RGBA16:
	case GL_RGBA16_SNORM:
	case GL_RGBA16F:
	case GL_RGBA32F:
	case GL_RGBA32UI:
	case GL_SRGB8_ALPHA8:
		return 4;
	default:
		break;
	}
	return 0;
}

static inline GLuint std3D_getImageFormat(GLuint format)
{
	static GLuint typeForChannels[] =
	{
		GL_RGB, // 0 channels
		GL_RED,
		GL_RG,
		GL_RGB,
		GL_RGBA
	};
	static GLuint intTypeForChannels[] =
	{
		GL_RGB_INTEGER, // 0 channels
		GL_RED_INTEGER,
		GL_RG_INTEGER,
		GL_RGB_INTEGER,
		GL_RGBA_INTEGER
	};
	bool isInteger = std3D_isIntegerFormat(format);
	int numChannels = std3D_getNumChannels(format);
	return isInteger ? intTypeForChannels[numChannels] : typeForChannels[numChannels];
}

static inline void std3D_pushDebugGroup(const char* name)
{
	if(GLEW_KHR_debug)
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
}

static inline void std3D_popDebugGroup()
{
	if(GLEW_KHR_debug)
		glPopDebugGroup();
}

inline void std3D_bindTexture(int type, int texId, int slot)
{
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(type, texId);
}

typedef struct std3D_Shader
{
	uint32_t version;
	uint32_t padding0;
	uint32_t texCount;
	uint32_t count;
	uint32_t texInstr[8];
	uint32_t instr[16];
} std3D_Shader;

GLint defaultShaderUBO;

typedef struct std3D_ShaderConsts
{
	rdVector4 c[8];
} std3D_ShaderConsts;
GLint shaderConstsUbo;

enum
{
	// 4 general purpose registers
	REG_GP0,
	REG_GP1,
	REG_GP2,
	REG_GP3,

	// 2 vertex color registers
	REG_V0,
	REG_V1,

	// 2 temporaries for argument storage
	//REG_TMP0,
	//REG_TMP1,

	REG_COUNT
};
static_assert(REG_COUNT <= 8, "REG_COUNT must not exceed 8 registers.");

enum
{
	REG_T0,
	REG_T1,
	REG_T2,
	REG_T3,

	REG_FP_COUNT
};
static_assert(REG_FP_COUNT <= 4, "REG_FP_COUNT must not exceed 4 registers.");

enum
{
	SRC_MOD_NONE   = 0x0, // no modifier
	SRC_MOD_NEGATE = 0x1, // -x
	SRC_MOD_BIAS   = 0x2, // x - 0.5
	SRC_MOD_BX2    = 0x4, // x * 2 - 1
	SRC_MOD_INVERT = 0x8, // 1 - x

	SRC_MOD_COUNT = 0x10
};
static_assert((SRC_MOD_COUNT>>3) <= 16, "SRC_MOD_COUNT must not exceed 4 bits.");

// only one at a time
enum
{
	DST_MOD_NONE = 0,
	DST_MOD_X2   = 1,
	DST_MOD_X4   = 2,
	DST_MOD_D2   = 3,
	DST_MOD_D4   = 4,

	DST_MOD_COUNT = 5
};
static_assert(DST_MOD_COUNT < 8, "DST_MOD_COUNT must not exceed 3 bits.");

typedef enum std3D_ShaderAluOp
{
	OP_NOP,		// no op
	OP_ADD,		// addition
	OP_BBD,     // blackbody
	OP_DP3,		// dot3
	OP_DP4,		// dot4
	OP_LDC,		// constant load
	OP_LRP,		// linear interpolation
	OP_MAD,		// multiply add
	OP_MAX,		// maximum
	OP_MIN,		// minimum
	OP_MOV,		// move
	OP_MUL,		// multiply
	MAX_ALU_OPS
} std3D_ShaderAluOp;
static_assert(MAX_ALU_OPS <= 16, "std3D_ShaderAluOp must not exceed 16 op codes.");


typedef enum std3D_ShaderTexOp
{
	// no op
	OP_TEX = 1,		// texture sample
	OP_TEXI,		// texture sample with emissive multiplier (todo: needed?)
	OP_TEXOPM,		// offset a UV slot with offset parallax mapping
	OP_TEXCOORD,	// convert UV to color
	MAX_TEX_OPS
} std3D_ShaderTexOp;
static_assert(MAX_TEX_OPS <= 16, "std3D_ShaderTexOp must not exceed 16 op codes.");

static const char* std3D_shaderOpKeywords[] =
{
	"nop",
	"add",
	"bbd",
	"dp3",
	"dp4",
	"ldc",
	"lrp",
	"mad",
	"max",
	"min",
	"mov",
	"mul",
	"pm",
	"sub",
	"tex",
	"texe",
};

static const int std3D_shaderOpOperands[] =
{
	0, // nop
	3, // add
	3, // dp3
	3, // dp4
	2, // ldc
	4, // lrp
	4, // mad
	3, // max
	3, // min
	2, // mov
	3, // mul
	3, // pm
	3, // sub
	3, // tex
	3, // texe
};

int std3D_GetOpCode(const char* keyword)
{
	for (int i = 0; i < MAX_ALU_OPS; ++i)
	{
		if (stricmp(keyword, std3D_shaderOpKeywords[i]) == 0)
			return i;
	}
	return -1;
}

// shader is just a handle around a buffer of code info
int std3D_GenShader()
{
	int buffer;
	glGenBuffers(1, &buffer);
	return buffer;
}

void std3D_DeleteShader(int id)
{
	glDeleteBuffers(1, &id);
}

// layout
// [src | mod, src | mod, src | mod, dest | mod, op]
// each src | mod is 7 bits
// the dest | mod is 6 bits
// op code is 4 bits
// there is a free bit
static uint32_t std3D_buildInstruction(
	uint32_t op,
	uint32_t dest,
	uint32_t modd,
	uint32_t src0,
	uint32_t mod0,
	uint32_t src1,
	uint32_t mod1,
	uint32_t src2,
	uint32_t mod2
)
{
	dest = (dest & 0x7) | ((modd & 0x7) << 3);
	src0 = (src0 & 0x7) | ((mod0 & 0xF) << 3);
	src1 = (src1 & 0x7) | ((mod1 & 0xF) << 3);
	src2 = (src2 & 0x7) | ((mod2 & 0xF) << 3);

	uint32_t instr;
	instr  = (op   & 0xF);
	instr |= (dest & 0x3F) << 4;
	instr |= (src0 & 0x7F) << 11;
	instr |= (src1 & 0x7F) << 18;
	instr |= (src2 & 0x7F) << 25;
	return instr;
}

//static void std3D_parseOperand(uint32_t op, const char* operand, uint32_t operandIndex, std3D_Shader* shader, uint32_t* src, uint32_t* mod)
//{
//	uint32_t reg = _atoi(operand + 1); // skip the token to get the index
//
//	// load ops work directly with load registers
//	// todo: error handling
//	if(op == OP_LDC)
//		return reg;
//	
//	if (operand[0] == 'c') // check the token
//	{
//		// insert a constant load instruction
//		//shader->instr[shader->count++] = std3D_buildInstruction(OP_LDC, REG_TMP0 + operandIndex, reg, 0, 0);
//		reg = REG_TMP0 + operandIndex; // source register is now the temporary
//	}
//	return reg;
//}

// todo: cache stuff?
int std3D_CompileShaderFromFile(int id, const char* path)
{
	if (!stdConffile_OpenRead(path))
	{
		stdPlatform_Printf("std3D: ERROR, Failed to open shader '%s'.\n", path);
		return 0;
	}
	
	int success = 1;
	/*
	std3D_Shader shader;
	memset(&shader, 0, sizeof(shader));

	// read the version number
	stdConffile_ReadLine();
	if (_sscanf(stdConffile_aLine, "version %d", &shader.version) != 1)
	{
		stdPlatform_Printf("std3D: ERROR, Failed to parse shader 's', couldn't read version number.\n");
		stdConffile_Close();
		return 0;
	}

	// parse the code
	while (stdConffile_ReadArgs())
	{
		if (!stdConffile_entry.numArgs)
			continue;

		uint32_t op, dest, src0, src1, src2;
		op = std3D_GetOpCode(stdConffile_entry.args[0].key);
		if (op < 0)
		{
			stdPlatform_Printf("std3D: ERROR, Failed to compile shader '%s'. Invalid operation '%s'.\n", path, stdConffile_entry.args[0].key);
			success = 0;
			break;
		}

		if (stdConffile_entry.numArgs < std3D_shaderOpOperands[op])
		{
			stdPlatform_Printf("std3D: ERROR, Failed to compile shader 's'."
							   "Invalid operands for operation '%s', got %d, expected %d.\n",
							   path,
							   stdConffile_entry.args[0].key,
							   stdConffile_entry.numArgs,
							   std3D_shaderOpOperands[op]);
			success = 0;
			break;
		}

		dest = _atoi(stdConffile_entry.args[1].value + 1); // skip the helper token

		// todo: handle errors better
		if (stdConffile_entry.numArgs > 2)
			src0 = std3D_parseOperand(op, stdConffile_entry.args[2].value, 0, &shader);
		else
			src0 = 0;

		if (stdConffile_entry.numArgs > 3)
			src1 = std3D_parseOperand(op, stdConffile_entry.args[3].value, 1, &shader);
		else
			src1 = 0;

		if (stdConffile_entry.numArgs > 4)
			src2 = std3D_parseOperand(op, stdConffile_entry.args[4].value, 2, &shader);
		else
			src2 = 0;

		int idx = shader.count++;
		if (idx >= 16)
		{
			stdPlatform_Printf("std3D: ERROR, Failed to compile shader '%s', instruction count exceeded.\n", path);
			break;
		}

		shader.instr[shader.count++] = std3D_buildInstruction(op, dest, src0, src1, src2);
	}

	if (success)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, id);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(std3D_Shader), &shader, GL_STATIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	stdConffile_Close();
	*/
	return success;
}

void std3D_initDefaultShader()
{
	defaultShaderUBO = std3D_GenShader();

	std3D_Shader shader;
	memset(&shader, 0, sizeof(shader));
	shader.version = 1;

	// displace the UVs with the displacement map
	shader.texInstr[shader.texCount++] = std3D_buildInstruction(OP_TEXOPM, REG_T0, DST_MOD_NONE, REG_T2, SRC_MOD_NONE, REG_T0, SRC_MOD_NONE, 0, 0);

	// sample tex0 from texcoord at temp1 to diffuse
	shader.texInstr[shader.texCount++] = std3D_buildInstruction(OP_TEX, REG_GP0, DST_MOD_NONE, REG_T0, SRC_MOD_NONE, REG_T0, SRC_MOD_NONE, 0, 0);

	// light sample tex1 from texcoord at temp1
	shader.texInstr[shader.texCount++] = std3D_buildInstruction(OP_TEXI, REG_GP1, DST_MOD_NONE, REG_T1, SRC_MOD_NONE, REG_T0, SRC_MOD_NONE, 0, 0);
	
	// sample specular tex3 from texcoord at temp1
	shader.texInstr[shader.texCount++] = std3D_buildInstruction(OP_TEX, REG_GP2, DST_MOD_NONE, REG_T3, SRC_MOD_NONE, REG_T0, SRC_MOD_NONE, 0, 0);

	// per pixel lighting to r2
	//shader.instr[shader.count++] = std3D_buildInstruction(OP_LIT, REG_GP2, DST_MOD_NONE, REG_V0, SRC_MOD_NONE, 0, 0, 0, 0);

	// multiply color0 with diffuse tex
	shader.instr[shader.count++] = std3D_buildInstruction(OP_MUL, REG_GP0, DST_MOD_NONE, REG_GP0, SRC_MOD_NONE, REG_V0, SRC_MOD_NONE, 0, 0);

	// multiply color1 with specular tex
	shader.instr[shader.count++] = std3D_buildInstruction(OP_MAD, REG_GP0, DST_MOD_NONE, REG_GP2, SRC_MOD_NONE, REG_V1, SRC_MOD_NONE, REG_GP0, SRC_MOD_NONE);
	//shader.instr[shader.count++] = std3D_buildInstruction(OP_ADD, REG_GP0, DST_MOD_NONE, REG_GP0, SRC_MOD_NONE, REG_V1, SRC_MOD_NONE, 0, 0);

	// add specular
	//shader.instr[shader.count++] = std3D_buildInstruction(OP_ADD, REG_GP0, DST_MOD_NONE, REG_GP0, SRC_MOD_NONE, REG_V1, SRC_MOD_NONE, 0, 0);

	// multiply color0 with diffuse tex and add specular
	//shader.instr[shader.count++] = std3D_buildInstruction(OP_MAD, REG_GP0, DST_MOD_NONE, REG_GP0, SRC_MOD_NONE, REG_V0, SRC_MOD_NONE, REG_V1, SRC_MOD_NONE);

	// take max of diffuse and emissive
	shader.instr[shader.count++] = std3D_buildInstruction(OP_MAX, REG_GP0, DST_MOD_NONE, REG_GP0, SRC_MOD_NONE, REG_GP1, SRC_MOD_NONE, 0, 0);
		
	glBindBuffer(GL_UNIFORM_BUFFER, defaultShaderUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(std3D_Shader), &shader, GL_STATIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void std3D_generateIntermediateFbo(int32_t width, int32_t height, std3DIntermediateFbo* pFbo, uint32_t format, int mipMaps, int useDepth, int rbo)
{
    // Generate the framebuffer
    memset(pFbo, 0, sizeof(*pFbo));

    pFbo->w = width;
    pFbo->h = height;
    pFbo->iw = width;
    pFbo->ih = height;

    glActiveTexture(GL_TEXTURE0);

    glGenFramebuffers(1, &pFbo->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, pFbo->fbo);
    
    // Set up our framebuffer texture
    glGenTextures(1, &pFbo->tex);
    glBindTexture(GL_TEXTURE_2D, pFbo->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, std3D_getImageFormat(format), std3D_getUploadFormat(format), NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mipMaps ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipMaps ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    //if(mipMaps)
    //{
    //    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
    //    glGenerateMipmap(GL_TEXTURE_2D);
    //}

    // Attach fbTex to our currently bound framebuffer fb
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pFbo->tex, 0);

    // Set up our render buffer
	if(useDepth)
	{
		if(rbo == 0)
		{
			glGenRenderbuffers(1, &pFbo->rbo);
			glBindRenderbuffer(GL_RENDERBUFFER, pFbo->rbo);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
    
			// Bind it to our framebuffer fb
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, pFbo->rbo);
		}
		else
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rbo, 0);
			//glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
		}
	}

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        stdPlatform_Printf("std3D: ERROR, Framebuffer is incomplete!\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void std3D_deleteIntermediateFbo(std3DIntermediateFbo* pFbo)
{
    glDeleteFramebuffers(1, &pFbo->fbo);
    glDeleteTextures(1, &pFbo->tex);
	if(pFbo->rbo)
		glDeleteRenderbuffers(1, &pFbo->rbo);
}

void std3D_generateFramebuffer(int32_t width, int32_t height, std3DFramebuffer* pFb)
{
    // Generate the framebuffer
    memset(pFb, 0, sizeof(*pFb));

    pFb->w = width;
    pFb->h = height;

	if (jkPlayer_enable32Bit)
		std3D_framebufferFlags |= 4;
	else
		std3D_framebufferFlags &= ~4;

    glActiveTexture(GL_TEXTURE0);

    glGenFramebuffers(1, &pFb->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, pFb->fbo);
    
    // Set up our framebuffer texture
	// we never really use the alpha channel, so for 32bit we use deep color (rgb10a20, and for 16bit we use high color (rgb5a1, to avoid green shift)
    glGenTextures(1, &pFb->tex0);
    glBindTexture(GL_TEXTURE_2D, pFb->tex0);
    glTexImage2D(GL_TEXTURE_2D, 0, /*jkPlayer_enable32Bit ? GL_RGB10_A2 : GL_RGB5_A1*/ GL_RG8, width, height, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);//GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);//GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Attach fbTex to our currently bound framebuffer fb
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pFb->tex0, 0);

	if(jkPlayer_enableBloom)
	{
		std3D_framebufferFlags |= 1;

		// Set up our emissive fb texture
		glGenTextures(1, &pFb->tex1);
		glBindTexture(GL_TEXTURE_2D, pFb->tex1);
		glTexImage2D(GL_TEXTURE_2D, 0, /*jkPlayer_enable32Bit ? GL_RGB10_A2 :*/ GL_RGB5_A1, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
		//glGenerateMipmap(GL_TEXTURE_2D);
		//glGenerateMipmap(GL_TEXTURE_2D);
    
		// Attach fbTex to our currently bound framebuffer fb
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, pFb->tex1, 0);
	}
	else
	{
		std3D_framebufferFlags &= ~1;
	}

	// Set up our depth buffer texture
	glGenTextures(1, &pFb->ztex);
	glBindTexture(GL_TEXTURE_2D, pFb->ztex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);//_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);//_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	//glGenerateMipmap(GL_TEXTURE_2D);
	
	//glTexImage2D(gl.TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, w, h, 0, GL_DEPTH_STENCIL,
				// GL_UNSIGNED_INT_24_8, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, pFb->ztex, 0);

    // Set up our render buffer
   // glGenRenderbuffers(1, &pFb->rbo);
   // glBindRenderbuffer(GL_RENDERBUFFER, pFb->rbo);
   // glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
   // glBindRenderbuffer(GL_RENDERBUFFER, 0);
   // 
   // // Bind it to our framebuffer fb
   // glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, pFb->rbo);
    
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        stdPlatform_Printf("std3D: ERROR, Framebuffer is incomplete!\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenFramebuffers(1, &pFb->zfbo);
	glBindFramebuffer(GL_FRAMEBUFFER, pFb->zfbo);

	// Attach fbTex to our currently bound framebuffer fb
	//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pFb->ztex, 0);
	
	// Set up our normal texture
	//glGenTextures(1, &pFb->ntex);
	//glBindTexture(GL_TEXTURE_2D, pFb->ntex);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);//_MIPMAP_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);//_MIPMAP_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pFb->ntex, 0);

	//glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, pFb->rbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, pFb->ztex, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		stdPlatform_Printf("std3D: ERROR, Framebuffer is incomplete!\n");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void std3D_generateExtraFramebuffers(int32_t width, int32_t height)
{
	glActiveTexture(GL_TEXTURE0);

	if (jkPlayer_enableSSAO)
	{
		int ssao_w = width > 320 ? width / 2 : width;
		int ssao_h = ssao_w * ((float)height / width);

		std3D_generateIntermediateFbo(width / 2, height / 2, &ssaoDepth, GL_R16F, 0, 0, 0);
		std3D_generateIntermediateFbo(width, height, &ssao, GL_R8, 0, 1, std3D_framebuffer.ztex);
		std3D_framebufferFlags |= 2;
	}
	else
		std3D_framebufferFlags &= ~2;

	if (jkPlayer_enableBloom)
	{
		int bloom_w = width > 320 ? 640 : 320;
		int bloom_h = bloom_w * ((float)height / width);

		std3D_generateIntermediateFbo(bloom_w, bloom_h, &bloomLayers[0], GL_R11F_G11F_B10F, 1, 0, 0);
		for (int i = 1; i < ARRAY_SIZE(bloomLayers); ++i)
			std3D_generateIntermediateFbo(bloomLayers[i - 1].w / 2, bloomLayers[i - 1].h / 2, &bloomLayers[i], GL_R11F_G11F_B10F, 1, 0, 0);
	}

	std3D_generateIntermediateFbo(width, height, &deferred, GL_RGBA8, 0, 0, 0);

	// the refraction buffers use the same depth-stencil as the main scene
	std3D_generateIntermediateFbo(width, height, &refr, GL_RGB5_A1, 0, 1, std3D_framebuffer.ztex);
	std3D_generateIntermediateFbo(width, height, &refrZ, GL_R32F, 0, 1, std3D_framebuffer.ztex);

	std3D_mainFbo.fbo = std3D_framebuffer.fbo;
	std3D_mainFbo.tex = std3D_framebuffer.tex1;
	//std3D_mainFbo.rbo = std3D_framebuffer.rbo;
	std3D_mainFbo.w = std3D_framebuffer.w;
	std3D_mainFbo.h = std3D_framebuffer.h;
	std3D_mainFbo.iw = std3D_framebuffer.w;
	std3D_mainFbo.ih = std3D_framebuffer.h;

	window.fbo = std3D_windowFbo;
	window.w = Window_xSize;
	window.h = Window_ySize;
	window.iw = Window_xSize;
	window.ih = Window_ySize;
}

void std3D_deleteFramebuffer(std3DFramebuffer* pFb)
{
    glDeleteFramebuffers(1, &pFb->fbo);
    glDeleteTextures(1, &pFb->tex0);
    glDeleteTextures(1, &pFb->tex1);
    //glDeleteRenderbuffers(1, &pFb->rbo);
	//glDeleteTextures(1, &pFb->ntex);
	glDeleteTextures(1, &pFb->ztex);
	glDeleteFramebuffers(1, &pFb->zfbo);
}

void std3D_deleteExtraFramebuffers()
{
	std3D_deleteIntermediateFbo(&ssao);
	std3D_deleteIntermediateFbo(&ssaoDepth);
	for (int i = 0; i < ARRAY_SIZE(bloomLayers); ++i)
		std3D_deleteIntermediateFbo(&bloomLayers[i]);

	std3D_deleteIntermediateFbo(&deferred);
	std3D_deleteIntermediateFbo(&refr);
	std3D_deleteIntermediateFbo(&refrZ);
}

#ifdef HW_VBUFFER
typedef struct std3D_DrawSurface
{
	stdVBufferTexFmt fmt;

	GLuint fbo;
	GLuint tex;

	GLuint rbo;
	int32_t w;
	int32_t h;

	int32_t iw;
	int32_t ih;
} std3D_DrawSurface;

// todo: use the format...
std3D_DrawSurface* std3D_AllocDrawSurface(stdVBufferTexFmt* fmt, int32_t width, int32_t height)
{
	std3D_DrawSurface* surface = malloc(sizeof(std3D_DrawSurface));
	
	// Generate the framebuffer
	memset(surface, 0, sizeof(std3D_DrawSurface));

	memcpy(&surface->fmt, fmt, sizeof(stdVBufferTexFmt));
	surface->w = width;
	surface->h = height;
	surface->iw = width;
	surface->ih = height;

	glActiveTexture(GL_TEXTURE0);

	glGenFramebuffers(1, &surface->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, surface->fbo);

	// Set up our framebuffer texture
	glGenTextures(1, &surface->tex);
	glBindTexture(GL_TEXTURE_2D, surface->tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, std3D_getImageFormat(GL_RGBA8), std3D_getUploadFormat(GL_RGBA8), NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	//if (mipMaps)
	//{
	//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
	//	glGenerateMipmap(GL_TEXTURE_2D);
	//}

	// Attach fbTex to our currently bound framebuffer fb
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, surface->tex, 0);

	// Set up our render buffer
	//if (useDepth)
	//{
	//	glGenRenderbuffers(1, &pFbo->rbo);
	//	glBindRenderbuffer(GL_RENDERBUFFER, pFbo->rbo);
	//	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	//	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	//
	//	// Bind it to our framebuffer fb
	//	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, pFbo->rbo);
	//}

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		stdPlatform_Printf("std3D: ERROR, Framebuffer is incomplete!\n");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return surface;
}

void std3D_FreeDrawSurface(std3D_DrawSurface* surface)
{
	if(!surface)
		return;

	glDeleteFramebuffers(1, &surface->fbo);
	glDeleteTextures(1, &surface->tex);

	if(surface->rbo)
		glDeleteRenderbuffers(1, &surface->rbo);

	free(surface);
}

void std3D_UploadDrawSurface(std3D_DrawSurface* src, int width, int height, void* pixels, uint8_t* palette)
{
	glBindTexture(GL_TEXTURE_2D, src->tex);

	uint8_t* image_8bpp = pixels;
	uint16_t* image_16bpp = pixels;
	uint8_t* pal = palette;

	// temp, currently all RGBA8
	uint8_t* image_data = malloc(width * height * 4);

	if (0)//src->fmt.format.colorMode)
	{
		for (int j = 0; j < height; j++)
		{
			for (int i = 0; i < width; i++)
			{
				uint32_t index = (i * height) + j;
				uint32_t val_rgba = 0x00000000;

				uint16_t val = image_16bpp[index];
				if (!src->fmt.format.g_bits == 6) // RGB565
				{
					uint8_t val_a1 = 1;
					uint8_t val_r5 = (val >> 11) & 0x1F;
					uint8_t val_g6 = (val >> 5) & 0x3F;
					uint8_t val_b5 = (val >> 0) & 0x1F;

					uint8_t val_a8 = val_a1 ? 0xFF : 0x0;
					uint8_t val_r8 = (val_r5 * 527 + 23) >> 6;
					uint8_t val_g8 = (val_g6 * 259 + 33) >> 6;
					uint8_t val_b8 = (val_b5 * 527 + 23) >> 6;

#ifdef __NOTDEF_TRANSPARENT_BLACK
					uint8_t transparent_r8 = (vbuf->transparent_color >> 16) & 0xFF;
					uint8_t transparent_g8 = (vbuf->transparent_color >> 8) & 0xFF;
					uint8_t transparent_b8 = (vbuf->transparent_color >> 0) & 0xFF;

					if (val_r8 == transparent_r8 && val_g8 == transparent_g8 && val_b8 == transparent_b8)
					{
						val_a1 = 0;
					}
#endif // __NOTDEF_TRANSPARENT_BLACK

					val_rgba |= (val_a8 << 24);
					val_rgba |= (val_b8 << 16);
					val_rgba |= (val_g8 << 8);
					val_rgba |= (val_r8 << 0);
				}
				else // RGB1555
				{
					uint8_t val_a1 = (val >> 15);
					uint8_t val_r5 = (val >> 10) & 0x1F;
					uint8_t val_g5 = (val >> 5) & 0x1F;
					uint8_t val_b5 = (val >> 0) & 0x1F;

					uint8_t val_a8 = val_a1 ? 0xFF : 0x0;
					uint8_t val_r8 = (val_r5 * 527 + 23) >> 6;
					uint8_t val_g8 = (val_g5 * 527 + 23) >> 6;
					uint8_t val_b8 = (val_b5 * 527 + 23) >> 6;

					val_rgba |= (val_a8 << 24);
					val_rgba |= (val_b8 << 16);
					val_rgba |= (val_g8 << 8);
					val_rgba |= (val_r8 << 0);
				}

				*(uint32_t*)(image_data + index * 4) = val_rgba;
			}
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, image_data);
	}
	else
	{
		for (int j = 0; j < height; j++)
		{
			for (int i = 0; i < width; i++)
			{
				uint32_t index = (i * height) + j;
				uint32_t val_rgba = 0xFF000000;

				if (pal)
				{
					uint8_t val = image_8bpp[index];
					val_rgba |= (pal[(val * 3) + 2] << 16);
					val_rgba |= (pal[(val * 3) + 1] << 8);
					val_rgba |= (pal[(val * 3) + 0] << 0);
				}
				else
				{
					uint8_t val = image_8bpp[index];
					rdColor24* pal_master = (rdColor24*)stdDisplay_masterPalette;//stdDisplay_gammaPalette;
					rdColor24* color = &pal_master[val];
					val_rgba |= (color->r << 16);
					val_rgba |= (color->g << 8);
					val_rgba |= (color->b << 0);
				}

				*(uint32_t*)(image_data + index * 4) = val_rgba;
			}
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, image_data);
	}

	free(image_data);
}

void std3D_BlitDrawSurface(std3D_DrawSurface* src, rdRect* srcRect, std3D_DrawSurface* dst, rdRect* dstRect)
{
	if(!src || !dst || !srcRect || !dstRect)
		return;

	std3D_pushDebugGroup("std3D_BlitDrawSurface");
		
	glBindFramebuffer(GL_READ_FRAMEBUFFER, src->fbo);
	if(glGetError() == GL_INVALID_OPERATION)
		printf("fuckkk\n");
	
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst->fbo);
	if (glGetError() == GL_INVALID_OPERATION)
		printf("fuckkk\n");

	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	int srcX0 = srcRect->x;
	int srcX1 = srcRect->x + srcRect->width;
	int srcY0 = srcRect->y;
	int srcY1 = srcRect->y + srcRect->height;

	int dstX0 = dstRect->x;
	int dstX1 = dstRect->x + dstRect->width;
	int dstY0 = dstRect->y;
	int dstY1 = dstRect->y + dstRect->height;

	glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	std3D_popDebugGroup();
}

void std3D_ClearDrawSurface(std3D_DrawSurface* surface, int fillColor, rdRect* rect)
{
	std3D_pushDebugGroup("std3D_ClearDrawSurface");

	std3DIntermediateFbo* pFb = (std3DIntermediateFbo*)surface;
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pFb->fbo);

	// it's very unclear what vbuffer fill color format is... might match the format of the fb?
	float a = ((fillColor >> 24) & 0xFF) / 255.0f;
	float r = ((fillColor >> 16) & 0xFF) / 255.0f;
	float g = ((fillColor >> 8) & 0xFF) / 255.0f;
	float b = ((fillColor >> 0) & 0xFF) / 255.0f;

	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	
	std3D_popDebugGroup();
}
#endif

void std3D_swapFramebuffers()
{
}

GLuint std3D_loadProgram(const char* fpath_base, const char* userDefines)
{
    GLuint out;
    GLint link_ok = GL_FALSE;
    
    char* tmp_vert = malloc(strlen(fpath_base) + 32);
    char* tmp_frag = malloc(strlen(fpath_base) + 32);
    
    strcpy(tmp_vert, fpath_base);
    strcat(tmp_vert, "_v.glsl");
    
    strcpy(tmp_frag, fpath_base);
    strcat(tmp_frag, "_f.glsl");
    
    GLuint vs, fs;
    if ((vs = load_shader_file(tmp_vert, GL_VERTEX_SHADER, userDefines))   == 0) return 0;
    if ((fs = load_shader_file(tmp_frag, GL_FRAGMENT_SHADER, userDefines)) == 0) return 0;
    
    free(tmp_vert);
    free(tmp_frag);
    
    out = glCreateProgram();
    glAttachShader(out, vs);
    glAttachShader(out, fs);
    glLinkProgram(out);
    glGetProgramiv(out, GL_LINK_STATUS, &link_ok);
    if (!link_ok) 
    {
        print_log(out);
        return 0;
    }
    
    return out;
}

GLint std3D_tryFindAttribute(GLuint program, const char* attribute_name)
{
    GLint out = glGetAttribLocation(program, attribute_name);
    if (out == -1) {
        stdPlatform_Printf("std3D: Could not bind attribute %s!\n", attribute_name);
    }
    return out;
}

GLint std3D_tryFindUniform(GLuint program, const char* uniform_name)
{
    GLint out = glGetUniformLocation(program, uniform_name);
    if (out == -1) {
        stdPlatform_Printf("std3D: Could not bind uniform %s!\n", uniform_name);
    }
    return out;
}

bool std3D_loadSimpleTexProgram(const char* fpath_base, std3DSimpleTexStage* pOut)
{
    if (!pOut) return false;
    if ((pOut->program = std3D_loadProgram(fpath_base, "")) == 0) return false;
    
    pOut->attribute_coord3d = std3D_tryFindAttribute(pOut->program, "coord3d");
    pOut->attribute_v_color = std3D_tryFindAttribute(pOut->program, "v_color");
    pOut->attribute_v_uv = std3D_tryFindAttribute(pOut->program, "v_uv");
    pOut->uniform_mvp = std3D_tryFindUniform(pOut->program, "mvp");
	pOut->uniform_proj = std3D_tryFindUniform(pOut->program, "projMatrix");
    pOut->uniform_iResolution = std3D_tryFindUniform(pOut->program, "iResolution");
    pOut->uniform_tex = std3D_tryFindUniform(pOut->program, "tex");
    pOut->uniform_tex2 = std3D_tryFindUniform(pOut->program, "tex2");
    pOut->uniform_tex3 = std3D_tryFindUniform(pOut->program, "tex3");
	pOut->uniform_tex4 = std3D_tryFindUniform(pOut->program, "tex4");

    pOut->uniform_param1 = std3D_tryFindUniform(pOut->program, "param1");
    pOut->uniform_param2 = std3D_tryFindUniform(pOut->program, "param2");
    pOut->uniform_param3 = std3D_tryFindUniform(pOut->program, "param3");

	pOut->uniform_rt = std3D_tryFindUniform(pOut->program, "cameraRT");
	pOut->uniform_lt = std3D_tryFindUniform(pOut->program, "cameraLT");
	pOut->uniform_rb = std3D_tryFindUniform(pOut->program, "cameraRB");
	pOut->uniform_lb = std3D_tryFindUniform(pOut->program, "cameraLB");

	pOut->uniform_tint = std3D_tryFindUniform(pOut->program, "colorEffects_tint");
	pOut->uniform_filter = std3D_tryFindUniform(pOut->program, "colorEffects_filter");
	pOut->uniform_fade = std3D_tryFindUniform(pOut->program, "colorEffects_fade");
	pOut->uniform_add = std3D_tryFindUniform(pOut->program, "colorEffects_add");
	
	pOut->uniform_lightbuf = std3D_tryFindUniform(pOut->program, "clusterBuffer");
	pOut->uniform_shared = glGetUniformBlockIndex(pOut->program, "sharedBlock");
	pOut->uniform_shader = glGetUniformBlockIndex(pOut->program, "shaderBlock");
	pOut->uniform_shaderConsts = glGetUniformBlockIndex(pOut->program, "shaderConstantsBlock");
	pOut->uniform_fog = glGetUniformBlockIndex(pOut->program, "fogBlock");
	pOut->uniform_tex_block = glGetUniformBlockIndex(pOut->program, "textureBlock");
	pOut->uniform_material = glGetUniformBlockIndex(pOut->program, "materialBlock");
	pOut->uniform_lights = glGetUniformBlockIndex(pOut->program, "lightBlock");
	pOut->uniform_occluders = glGetUniformBlockIndex(pOut->program, "occluderBlock");
	pOut->uniform_decals = glGetUniformBlockIndex(pOut->program, "decalBlock");

	glUniformBlockBinding(pOut->program, pOut->uniform_lights, UBO_SLOT_LIGHTS);
	glUniformBlockBinding(pOut->program, pOut->uniform_occluders, UBO_SLOT_OCCLUDERS);
	glUniformBlockBinding(pOut->program, pOut->uniform_decals, UBO_SLOT_DECALS);
	glUniformBlockBinding(pOut->program, pOut->uniform_shared, UBO_SLOT_SHARED);
	glUniformBlockBinding(pOut->program, pOut->uniform_fog, UBO_SLOT_FOG);
	glUniformBlockBinding(pOut->program, pOut->uniform_tex_block, UBO_SLOT_TEX);
	glUniformBlockBinding(pOut->program, pOut->uniform_material, UBO_SLOT_MATERIAL);
	glUniformBlockBinding(pOut->program, pOut->uniform_shader, UBO_SLOT_SHADER);
	glUniformBlockBinding(pOut->program, pOut->uniform_shaderConsts, UBO_SLOT_SHADER_CONSTS);

    return true;
}

int std3D_loadWorldStage(std3D_worldStage* pStage, int isZPass, const char* defines)
{
	if ((pStage->program = std3D_loadProgram(isZPass ? "shaders/depth" : "shaders/world", defines)) == 0) return 0;

	pStage->attribute_coord3d = std3D_tryFindAttribute(pStage->program, "coord3d");
	pStage->attribute_v_color = std3D_tryFindAttribute(pStage->program, "v_color");
	pStage->attribute_v_light = std3D_tryFindAttribute(pStage->program, "v_light");
	pStage->attribute_v_uv    = std3D_tryFindAttribute(pStage->program, "v_uv");
	pStage->attribute_v_norm  = std3D_tryFindAttribute(pStage->program, "v_normal");

	pStage->uniform_projection = std3D_tryFindUniform(pStage->program, "projMatrix");
	pStage->uniform_modelMatrix = std3D_tryFindUniform(pStage->program, "modelMatrix");
	pStage->uniform_viewMatrix = std3D_tryFindUniform(pStage->program, "viewMatrix");
	pStage->uniform_ambient_color = std3D_tryFindUniform(pStage->program, "ambientColor");
	pStage->uniform_ambient_sg = std3D_tryFindUniform(pStage->program, "ambientSG");
	pStage->uniform_ambient_sg_count = std3D_tryFindUniform(pStage->program, "ambientNumSG");
	pStage->uniform_fillColor = std3D_tryFindUniform(pStage->program, "fillColor");
	pStage->uniform_textures = std3D_tryFindUniform(pStage->program, "textures");
	pStage->uniform_tex = std3D_tryFindUniform(pStage->program, "tex");
	pStage->uniform_texEmiss = std3D_tryFindUniform(pStage->program, "texEmiss");
	pStage->uniform_worldPalette = std3D_tryFindUniform(pStage->program, "worldPalette");
	pStage->uniform_worldPaletteLights = std3D_tryFindUniform(pStage->program, "worldPaletteLights");
	pStage->uniform_displacement_map = std3D_tryFindUniform(pStage->program, "displacement_map");
	pStage->uniform_texDecals = std3D_tryFindUniform(pStage->program, "decalAtlas");
	pStage->uniform_texz = std3D_tryFindUniform(pStage->program, "ztex");
	pStage->uniform_texssao = std3D_tryFindUniform(pStage->program, "ssaotex");
	pStage->uniform_texrefraction = std3D_tryFindUniform(pStage->program, "refrtex");
	pStage->uniform_texclip = std3D_tryFindUniform(pStage->program, "cliptex");
	pStage->uniform_geo_mode = std3D_tryFindUniform(pStage->program, "geoMode");
	pStage->uniform_dithertex = std3D_tryFindUniform(pStage->program, "dithertex");
	pStage->uniform_diffuse_light = std3D_tryFindUniform(pStage->program, "diffuseLightTex");
	pStage->uniform_light_mode = std3D_tryFindUniform(pStage->program, "lightMode");
	pStage->uniform_ao_flags = std3D_tryFindUniform(pStage->program, "aoFlags");

	pStage->uniform_lightbuf = std3D_tryFindUniform(pStage->program, "clusterBuffer");
	pStage->uniform_shared = glGetUniformBlockIndex(pStage->program, "sharedBlock");
	pStage->uniform_shader = glGetUniformBlockIndex(pStage->program, "shaderBlock");
	pStage->uniform_shaderConsts = glGetUniformBlockIndex(pStage->program, "shaderConstantsBlock");
	pStage->uniform_fog = glGetUniformBlockIndex(pStage->program, "fogBlock");
	pStage->uniform_tex_block = glGetUniformBlockIndex(pStage->program, "textureBlock");
	pStage->uniform_material = glGetUniformBlockIndex(pStage->program, "materialBlock");
	pStage->uniform_lights = glGetUniformBlockIndex(pStage->program, "lightBlock");
	pStage->uniform_occluders = glGetUniformBlockIndex(pStage->program, "occluderBlock");
	pStage->uniform_decals = glGetUniformBlockIndex(pStage->program, "decalBlock");

	pStage->uniform_rightTop = std3D_tryFindUniform(pStage->program, "rightTop");
	pStage->uniform_rt = std3D_tryFindUniform(pStage->program, "cameraRT");
	pStage->uniform_lt = std3D_tryFindUniform(pStage->program, "cameraLT");
	pStage->uniform_rb = std3D_tryFindUniform(pStage->program, "cameraRB");
	pStage->uniform_lb = std3D_tryFindUniform(pStage->program, "cameraLB");

	return 1;
}

void std3D_setupMenuVAO()
{
	glGenVertexArrays(1, &menu_vao);
	glBindVertexArray(menu_vao);

	glBindBuffer(GL_ARRAY_BUFFER, menu_vbo_all);
	glVertexAttribPointer(
		programMenu_attribute_coord3d, // attribute
		3,                 // number of elements per vertex, here (x,y,z)
		GL_FLOAT,          // the type of each element
		GL_FALSE,          // normalize fixed-point data?
		sizeof(D3DVERTEX),                 // data stride
		(GLvoid*)offsetof(D3DVERTEX, x)                  // offset of first element
	);

	glVertexAttribPointer(
		programMenu_attribute_v_color, // attribute
		4,                 // number of elements per vertex, here (R,G,B,A)
		GL_UNSIGNED_BYTE,  // the type of each element
		GL_TRUE,          // normalize fixed-point data?
		sizeof(D3DVERTEX),                 // no extra data between each position
		(GLvoid*)offsetof(D3DVERTEX, color) // offset of first element
	);

	/*glVertexAttribPointer(
		std3D_texFboStage.attribute_v_light, // attribute
		1,                 // number of elements per vertex, here (L)
		GL_FLOAT,  // the type of each element
		GL_FALSE,          // normalize fixed-point data?
		sizeof(D3DVERTEX),                 // no extra data between each position
		(GLvoid*)offsetof(D3DVERTEX, lightLevel) // offset of first element
	);*/

	glVertexAttribPointer(
		programMenu_attribute_v_uv,    // attribute
		2,                 // number of elements per vertex, here (U,V)
		GL_FLOAT,          // the type of each element
		GL_FALSE,          // take our values as-is
		sizeof(D3DVERTEX),                 // no extra data between each position
		(GLvoid*)offsetof(D3DVERTEX, tu)                  // offset of first element
	);

	glEnableVertexAttribArray(programMenu_attribute_coord3d);
	glEnableVertexAttribArray(programMenu_attribute_v_color);
	glEnableVertexAttribArray(programMenu_attribute_v_uv);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, menu_ibo_triangle);

	glBindVertexArray(vao);
}

void std3D_setupUBOs()
{
	memset(&lightUniforms,    0,    sizeof(std3D_LightUniforms));
	memset(&occluderUniforms, 0, sizeof(std3D_OccluderUniforms));
	memset(&decalUniforms,    0,    sizeof(std3D_DecalUniforms));

	// shared buffer
	glGenBuffers(1, &shared_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, shared_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(std3D_SharedUniforms), NULL, GL_DYNAMIC_DRAW);

	glGenBuffers(1, &shaderConstsUbo);
	glBindBuffer(GL_UNIFORM_BUFFER, shaderConstsUbo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(std3D_ShaderConsts), NULL, GL_DYNAMIC_DRAW);

	// fog buffer
	glGenBuffers(1, &fog_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, fog_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(std3D_FogUniforms), NULL, GL_DYNAMIC_DRAW);

	// texture buffer
	glGenBuffers(1, &tex_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, tex_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(std3D_TextureUniforms), NULL, GL_DYNAMIC_DRAW);

	// material buffer	
	glGenBuffers(1, &material_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, material_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(std3D_MaterialUniforms), NULL, GL_DYNAMIC_DRAW);

	// light buffer
	glGenBuffers(1, &light_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, light_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(std3D_LightUniforms), NULL, GL_DYNAMIC_DRAW);

	// occluder buffer
	glGenBuffers(1, &occluder_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, occluder_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(std3D_OccluderUniforms), NULL, GL_DYNAMIC_DRAW);

	// decal buffer
	glGenBuffers(1, &decal_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, decal_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(std3D_DecalUniforms), NULL, GL_DYNAMIC_DRAW);

	// cluster buffer
	//int maxsize;
	//glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxsize);
	//printf("MAX TEX BUFFER %d\n",  maxsize);
	glGenBuffers(1, &cluster_buffer);
	glBindBuffer(GL_TEXTURE_BUFFER, cluster_buffer);
	glBufferData(GL_TEXTURE_BUFFER, sizeof(std3D_clusterBits), NULL, GL_DYNAMIC_DRAW);

	glGenTextures(1, &cluster_tbo);
	glBindTexture(GL_TEXTURE_BUFFER, cluster_tbo);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, cluster_buffer);

	glBindBuffer(GL_TEXTURE_BUFFER, 0);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
}

void std3D_setupLightingUBO(std3D_worldStage* pStage)
{
	glUniformBlockBinding(pStage->program, pStage->uniform_lights, UBO_SLOT_LIGHTS);
	glUniformBlockBinding(pStage->program, pStage->uniform_occluders, UBO_SLOT_OCCLUDERS);
	glUniformBlockBinding(pStage->program, pStage->uniform_decals, UBO_SLOT_DECALS);
	glUniformBlockBinding(pStage->program, pStage->uniform_shared, UBO_SLOT_SHARED);
	glUniformBlockBinding(pStage->program, pStage->uniform_fog, UBO_SLOT_FOG);
	glUniformBlockBinding(pStage->program, pStage->uniform_tex_block, UBO_SLOT_TEX);
	glUniformBlockBinding(pStage->program, pStage->uniform_material, UBO_SLOT_MATERIAL);
	glUniformBlockBinding(pStage->program, pStage->uniform_shader, UBO_SLOT_SHADER);
	glUniformBlockBinding(pStage->program, pStage->uniform_shaderConsts, UBO_SLOT_SHADER_CONSTS);
}

void std3D_setupDrawCallVAO(std3D_worldStage* pStage)
{
	glGenVertexArrays(STD3D_STAGING_COUNT, pStage->vao);
	for (int i = 0; i < STD3D_STAGING_COUNT; ++i)
	{
		glBindVertexArray(pStage->vao[i]);

		glBindBuffer(GL_ARRAY_BUFFER, world_vbo_all[i]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, world_ibo_triangle[i]);

		glVertexAttribPointer(
			pStage->attribute_coord3d, // attribute
			3,                 // number of elements per vertex, here (x,y,z)
			GL_FLOAT,          // the type of each element
			GL_FALSE,          // normalize fixed-point data?
			sizeof(rdVertex),                 // data stride
			(GLvoid*)offsetof(rdVertex, x)                  // offset of first element
		);
		glEnableVertexAttribArray(pStage->attribute_coord3d);

		glVertexAttribPointer(
			pStage->attribute_v_norm, // attribute
			GL_BGRA,                 // number of elements per vertex, here (x,y,z)
			GL_UNSIGNED_INT_2_10_10_10_REV,          // the type of each element
			GL_TRUE,          // normalize fixed-point data?
			sizeof(rdVertex), // data stride
			(GLvoid*)offsetof(rdVertex, norm10a2) // offset of first element
		);
		glEnableVertexAttribArray(pStage->attribute_v_norm);

		for (int i = 0; i < RD_NUM_COLORS; ++i)
		{
			glVertexAttribPointer(
				pStage->attribute_v_color + i, // attribute
				4,                 // number of elements per vertex, here (R,G,B,A)
				GL_UNSIGNED_BYTE,  // the type of each element
				GL_TRUE,          // normalize fixed-point data?
				sizeof(rdVertex),                 // no extra data between each position
				(GLvoid*)offsetof(rdVertex, colors[i]) // offset of first element
			);
			glEnableVertexAttribArray(pStage->attribute_v_color + i);
		}

		for (int i = 0; i < RD_NUM_COLORS; ++i)
		{
			glVertexAttribPointer(
				pStage->attribute_v_uv + i,    // attribute
				4,                 // number of elements per vertex, here (U,V,R,Q)
				GL_FLOAT,          // the type of each element
				GL_FALSE,          // take our values as-is
				sizeof(rdVertex),                 // no extra data between each position
				(GLvoid*)offsetof(rdVertex, texcoords[i])                  // offset of first element
			);
			glEnableVertexAttribArray(pStage->attribute_v_uv + i);
		}
	}
	glBindVertexArray(vao);
}

int init_resources()
{
    stdPlatform_Printf("std3D: OpenGL init...\n");

    std3D_bReinitHudElements = 1;

    memset(std3D_aUITextures, 0, sizeof(std3D_aUITextures));

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &std3D_windowFbo);

    int32_t tex_w = Window_xSize;
    int32_t tex_h = Window_ySize;

    std3D_generateFramebuffer(tex_w, tex_h, &std3D_framebuffer);
	std3D_generateExtraFramebuffers(tex_w, tex_h);

    if ((programMenu = std3D_loadProgram("shaders/menu", "")) == 0) return false;

	if (!std3D_loadWorldStage(&worldStages[SHADER_DEPTH], 1, "Z_PREPASS")) return false;
	if (!std3D_loadWorldStage(&worldStages[SHADER_DEPTH_ALPHATEST], 1, "Z_PREPASS;ALPHA_DISCARD")) return false;
	if (!std3D_loadWorldStage(&worldStages[SHADER_COLOR], 0, "")) return false;
	if (!std3D_loadWorldStage(&worldStages[SHADER_COLOR_SPEC], 0, "SPECULAR")) return false;
	if (!std3D_loadWorldStage(&worldStages[SHADER_COLOR_UNLIT], 0, "UNLIT")) return false;
	if (!std3D_loadWorldStage(&worldStages[SHADER_COLOR_ALPHATEST], 0, "ALPHA_DISCARD")) return false;
	if (!std3D_loadWorldStage(&worldStages[SHADER_COLOR_ALPHATEST_SPEC], 0, "ALPHA_DISCARD;SPECULAR")) return false;
	if (!std3D_loadWorldStage(&worldStages[SHADER_COLOR_ALPHATEST_UNLIT], 0, "ALPHA_DISCARD;UNLIT")) return false;
	if (!std3D_loadWorldStage(&worldStages[SHADER_COLOR_ALPHABLEND], 0, "ALPHA_DISCARD;ALPHA_BLEND")) return false;
	if (!std3D_loadWorldStage(&worldStages[SHADER_COLOR_ALPHABLEND_SPEC], 0, "ALPHA_DISCARD;ALPHA_BLEND;SPECULAR")) return false;
	if (!std3D_loadWorldStage(&worldStages[SHADER_COLOR_ALPHABLEND_UNLIT], 0, "ALPHA_DISCARD;ALPHA_BLEND;UNLIT")) return false;
	if (!std3D_loadWorldStage(&worldStages[SHADER_COLOR_REFRACTION], 0, "ALPHA_DISCARD;ALPHA_BLEND;REFRACTION")) return false;

    if (!std3D_loadSimpleTexProgram("shaders/ui", &std3D_uiProgram)) return false;
    if (!std3D_loadSimpleTexProgram("shaders/texfbo", &std3D_texFboStage)) return false;
	if (!std3D_loadSimpleTexProgram("shaders/deferred", &std3D_deferredStage)) return false;
	if (!std3D_loadSimpleTexProgram("shaders/postfx", &std3D_postfxStage)) return false;
	if (!std3D_loadSimpleTexProgram("shaders/bloom", &std3D_bloomStage)) return false;
	if (!std3D_loadSimpleTexProgram("shaders/ssao", &std3D_ssaoStage)) return false;
	if (!std3D_loadSimpleTexProgram("shaders/decal_insert", &std3D_decalAtlasStage)) return false;

	std3D_generateIntermediateFbo(DECAL_ATLAS_SIZE, DECAL_ATLAS_SIZE, &decalAtlasFBO, GL_RGBA8, 0, 0, 0);

	decalRootNode.rect.x = 0.0f;
	decalRootNode.rect.y = 0.0f;
	decalRootNode.rect.width = DECAL_ATLAS_SIZE;
	decalRootNode.rect.height = DECAL_ATLAS_SIZE;
   
    programMenu_attribute_coord3d = std3D_tryFindAttribute(programMenu, "coord3d");
    programMenu_attribute_v_color = std3D_tryFindAttribute(programMenu, "v_color");
    programMenu_attribute_v_uv = std3D_tryFindAttribute(programMenu, "v_uv");
    programMenu_uniform_mvp = std3D_tryFindUniform(programMenu, "mvp");
    programMenu_uniform_tex = std3D_tryFindUniform(programMenu, "tex");
    programMenu_uniform_displayPalette = std3D_tryFindUniform(programMenu, "displayPalette");
   
	// samplers
	int wrap[2] = { GL_REPEAT, GL_CLAMP_TO_EDGE };
	glGenSamplers(4, linearSampler);
	glGenSamplers(4, nearestSampler);
	for (int i = 0; i < 4; ++i)
	{
		int wrapX = wrap[i % 2];
		int wrapY = wrap[i / 2];

		glSamplerParameteri(linearSampler[i], GL_TEXTURE_WRAP_S, wrapX);
		glSamplerParameteri(linearSampler[i], GL_TEXTURE_WRAP_T, wrapY);
		glSamplerParameteri(linearSampler[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(linearSampler[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		
		glSamplerParameteri(nearestSampler[i], GL_TEXTURE_WRAP_S, wrapX);
		glSamplerParameteri(nearestSampler[i], GL_TEXTURE_WRAP_T, wrapY);
		glSamplerParameteri(nearestSampler[i], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(nearestSampler[i], GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	}

    // Blank texture
    glGenTextures(1, &blank_tex);
    blank_data = jkgm_alloc_aligned(0x400);
    memset(blank_data, 0x0, 0x400);
    
    glBindTexture(GL_TEXTURE_2D, blank_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, blank_data);

    // Blank texture
    glGenTextures(1, &blank_tex_white);
    blank_data_white = jkgm_alloc_aligned(0x400);
    memset(blank_data_white, 0xFF, 0x400);
    
    glBindTexture(GL_TEXTURE_2D, blank_tex_white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, blank_data_white);

    // World palette
    glGenTextures(1, &worldpal_texture);
    worldpal_data = jkgm_alloc_aligned(0x300);
    memset(worldpal_data, 0xFF, 0x300);
    
    glBindTexture(GL_TEXTURE_2D, worldpal_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //glPixelStorei(GL_PACK_ALIGNMENT, 1);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 256, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, worldpal_data);

    // World palette lights
    glGenTextures(1, &worldpal_lights_texture);
    worldpal_lights_data = jkgm_alloc_aligned(0x4000);
    memset(worldpal_lights_data, 0xFF, 0x4000);
    
    glBindTexture(GL_TEXTURE_2D, worldpal_lights_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //glPixelStorei(GL_PACK_ALIGNMENT, 1);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 256, 0x40, 0, GL_RED, GL_UNSIGNED_BYTE, worldpal_lights_data);
    
    
    // Display palette
    glGenTextures(1, &displaypal_texture);
    displaypal_data = jkgm_alloc_aligned(0x400);
    memset(displaypal_data, 0xFF, 0x300);
    
    glBindTexture(GL_TEXTURE_2D, displaypal_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //glPixelStorei(GL_PACK_ALIGNMENT, 1);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 256, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, displaypal_data);

    // Tiled random
    glGenTextures(1, &tiledrand_texture);

    memset(tiledrand_data, 0, 4 * 4 * sizeof(rdVector3));
    for (int i = 0; i < 4*4; i++)
    {
        tiledrand_data[i].x = (_frand() * 2.0) - 1.0;
        tiledrand_data[i].y = (_frand() * 2.0) - 1.0;
		tiledrand_data[i].z = (_frand() * 2.0) - 1.0;
		rdVector_Normalize3Acc(&tiledrand_data[i]);
    }

    glBindTexture(GL_TEXTURE_2D, tiledrand_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, tiledrand_data);

	// Dither texture
	const uint8_t DITHER_LUT[16] = {
			0, 4, 1, 5,
			6, 2, 7, 3,
			1, 5, 0, 4,
			7, 3, 6, 2
	};
	glGenTextures(1, &dither_texture);

	glBindTexture(GL_TEXTURE_2D, dither_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 4, 4, 0, GL_RED, GL_UNSIGNED_BYTE, DITHER_LUT);

    glGenVertexArrays( 1, &vao );
    glBindVertexArray( vao ); 

    menu_data_all = malloc(STD3D_MAX_UI_VERTICES * sizeof(D3DVERTEX));
    menu_data_elements = malloc(sizeof(GLushort) * 3 * STD3D_MAX_UI_TRIS);

    glGenBuffers(1, &menu_vbo_all);
    glGenBuffers(1, &menu_ibo_triangle);

	std3D_setupMenuVAO();

	// generate staging buffers
	bufferIdx = 0;
	glGenBuffers(STD3D_STAGING_COUNT, world_vbo_all);
	glGenBuffers(STD3D_STAGING_COUNT, world_ibo_triangle);
	for (int i = 0; i < STD3D_STAGING_COUNT; ++i)
	{
		glBindBuffer(GL_ARRAY_BUFFER, world_vbo_all[i]);
		glBufferData(GL_ARRAY_BUFFER, STD3D_MAX_DRAW_CALLS * sizeof(rdVertex), NULL, GL_STREAM_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, world_ibo_triangle[i]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, STD3D_MAX_DRAW_CALL_INDICES * sizeof(uint16_t), NULL, GL_STREAM_DRAW);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	for (int i = 0; i < STD3D_MAX_RENDER_PASSES; ++i)
		std3D_initRenderPass(&std3D_renderPasses[i]);
	
	std3D_setupUBOs();
	for(int i = 0; i < SHADER_COUNT; ++i)
	{
		std3D_setupDrawCallVAO(&worldStages[i]);
		std3D_setupLightingUBO(&worldStages[i]);
	}

	std3D_initDefaultShader();

    has_initted = true;
    return true;
}

void std3D_ResetDrawCalls();

int std3D_Startup()
{
    if (std3D_bInitted) {
        return 1;
    }

#ifdef TARGET_CAN_JKGM
    jkgm_startup();
#endif

    memset(&std3D_ui_colormap, 0, sizeof(std3D_ui_colormap));
    rdColormap_LoadEntry("misc\\cmp\\UIColormap.cmp", &std3D_ui_colormap);

	for (int i = 0; i < STD3D_MAX_RENDER_PASSES; ++i)
		memset(&std3D_renderPasses[i], 0, sizeof(std3D_RenderPass));

    std3D_bReinitHudElements = 1;

    std3D_bInitted = 1;
    return 1;
}

void std3D_Shutdown()
{
    if (!std3D_bInitted) {
        return;
    }

    std3D_bReinitHudElements = 0;

    rdColormap_FreeEntry(&std3D_ui_colormap);
    std3D_bInitted = 0;
}

void std3D_FreeResources()
{
    std3D_PurgeTextureCache();

    glDeleteProgram(programMenu);
	std3D_deleteExtraFramebuffers();
	std3D_deleteFramebuffer(&std3D_framebuffer);
    glDeleteTextures(1, &blank_tex);
    glDeleteTextures(1, &blank_tex_white);
    glDeleteTextures(1, &worldpal_texture);
    glDeleteTextures(1, &worldpal_lights_texture);
    glDeleteTextures(1, &displaypal_texture);
    if (blank_data)
        jkgm_aligned_free(blank_data);
    if (blank_data_white)
        jkgm_aligned_free(blank_data_white);
    if (worldpal_data)
        jkgm_aligned_free(worldpal_data);
    if (worldpal_lights_data)
        jkgm_aligned_free(worldpal_lights_data);
    if (displaypal_data)
        jkgm_aligned_free(displaypal_data);

	glDeleteSamplers(4, linearSampler);
	glDeleteSamplers(4, nearestSampler);

    blank_data = NULL;
    blank_data_white = NULL;
    worldpal_data = NULL;
    worldpal_lights_data = NULL;
    displaypal_data = NULL;

    if (menu_data_all)
        free(menu_data_all);
    menu_data_all = NULL;

    if (menu_data_elements)
        free(menu_data_elements);
    menu_data_elements = NULL;

    loaded_colormap = NULL;

	glDeleteTextures(1, &tiledrand_texture);
	glDeleteTextures(1, &dither_texture);

    glDeleteBuffers(STD3D_STAGING_COUNT, world_vbo_all);
    glDeleteBuffers(STD3D_STAGING_COUNT, world_ibo_triangle);

    glDeleteBuffers(1, &menu_vbo_all);

	for(int i = 0; i < SHADER_COUNT; ++i)
	{
		glDeleteProgram(worldStages[i].program);
		glDeleteVertexArrays(3, worldStages[i].vao);
	}
	glDeleteBuffers(1, &tex_ubo);
	glDeleteBuffers(1, &material_ubo);
	glDeleteBuffers(1, &shared_ubo);
	glDeleteBuffers(1, &shaderConstsUbo);
	glDeleteBuffers(1, &light_ubo);
	glDeleteBuffers(1, &occluder_ubo);
	glDeleteBuffers(1, &decal_ubo);
	glDeleteBuffers(1, &cluster_buffer);
	glDeleteTextures(1, &cluster_tbo);

	for (int i = 0; i < STD3D_MAX_RENDER_PASSES; ++i)
		std3D_freeRenderPass(&std3D_renderPasses[i]);

	std3D_DeleteShader(defaultShaderUBO);

    std3D_bReinitHudElements = 1;

    has_initted = false;
}

void std3D_useProgram(int program)
{
	static int last_program = -1;
	if (program != last_program)
	{
		glUseProgram(program);
		last_program = program;
	}
}

int std3D_StartScene()
{
    if (Main_bHeadless) return 1;

    //printf("Begin draw\n");
    if (!has_initted)
    {
        if (!init_resources()) {
            stdPlatform_Printf("std3D: Failed to init resources, exiting...");
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to init resources, exiting...", NULL);
            exit(-1);
        }
    }

	std3D_pushDebugGroup("std3D_StartScene");
    
    rendered_tris = 0;
    
    std3D_swapFramebuffers();
    
    double supersample_level = jkPlayer_ssaaMultiple; // Can also be set lower
    int32_t tex_w = (int32_t)((double)Window_xSize * supersample_level);
    int32_t tex_h = (int32_t)((double)Window_ySize * supersample_level);
	tex_w = (tex_w < 320 ? 320 : tex_w);
	tex_h = tex_w * (float)Window_ySize / Window_xSize;

    if (tex_w != std3D_framebuffer.w || tex_h != std3D_framebuffer.h 
        || (((std3D_framebufferFlags & 1) == 1) != jkPlayer_enableBloom)
		|| (((std3D_framebufferFlags & 2) == 2) != jkPlayer_enableSSAO)
		|| (((std3D_framebufferFlags & 4) == 4) != jkPlayer_enable32Bit))
    {
		std3D_deleteExtraFramebuffers();
        std3D_deleteFramebuffer(&std3D_framebuffer);
        std3D_generateFramebuffer(tex_w, tex_h, &std3D_framebuffer);
		std3D_generateExtraFramebuffers(tex_w, tex_h);
    }

	glClearColor(0.0, 0.0, 0.0, 1.0);

	// clear the window buffer
	glBindFramebuffer(GL_FRAMEBUFFER, window.fbo);
	glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, std3D_framebuffer.fbo);
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    glCullFace(GL_FRONT);
    //glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);

	GLuint clearBits = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
	if(jkGuiBuildMulti_bRendering)
		clearBits |= GL_COLOR_BUFFER_BIT;

#ifdef STENCIL_BUFFER
	glClearStencil(0);
	glStencilMask(0xFF);
#endif
	glClear(clearBits);

    if (jkGuiBuildMulti_bRendering && rdColormap_pCurMap && loaded_colormap != rdColormap_pCurMap)
    {
        glBindTexture(GL_TEXTURE_2D, worldpal_texture);
        memcpy(worldpal_data, rdColormap_pCurMap->colors, 0x300);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGB, GL_UNSIGNED_BYTE, worldpal_data);
    
        if (rdColormap_pCurMap->lightlevel)
        {
            glBindTexture(GL_TEXTURE_2D, worldpal_lights_texture);
            memcpy(worldpal_lights_data, rdColormap_pCurMap->lightlevel, 0x4000);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 0x40, GL_RED, GL_UNSIGNED_BYTE, worldpal_lights_data);
        }

        loaded_colormap = rdColormap_pCurMap;
    }
    else if (sithWorld_pCurrentWorld && sithWorld_pCurrentWorld->colormaps && loaded_colormap != sithWorld_pCurrentWorld->colormaps)
    {
        glBindTexture(GL_TEXTURE_2D, worldpal_texture);
        memcpy(worldpal_data, sithWorld_pCurrentWorld->colormaps->colors, 0x300);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGB, GL_UNSIGNED_BYTE, worldpal_data);
    
        if (sithWorld_pCurrentWorld->colormaps->lightlevel)
        {
            glBindTexture(GL_TEXTURE_2D, worldpal_lights_texture);
            memcpy(worldpal_lights_data, sithWorld_pCurrentWorld->colormaps->lightlevel, 0x4000);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 0x40, GL_RED, GL_UNSIGNED_BYTE, worldpal_lights_data);
        }

        loaded_colormap = sithWorld_pCurrentWorld->colormaps;
    }

    if (memcmp(displaypal_data, stdDisplay_masterPalette, 0x300))
    {
        glBindTexture(GL_TEXTURE_2D, displaypal_texture);
        memcpy(displaypal_data, stdDisplay_masterPalette, 0x300);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGB, GL_UNSIGNED_BYTE, displaypal_data);
    }

	std3D_ResetDrawCalls();

	std3D_popDebugGroup();

    return 1;
}

int std3D_EndScene()
{
    if (Main_bHeadless) {
        last_tex = NULL;
        last_flags = 0;
        return 1;
    }

    last_tex = NULL;
    last_flags = 0;
    return 1;
}

void std3D_ResetUIRenderList()
{
    rendered_tris += GL_tmpUITrisAmt;

    GL_tmpUIVerticesAmt = 0;
    GL_tmpUITrisAmt = 0;
    //GL_tmpLinesAmt = 0;
    
    //memset(GL_tmpTris, 0, sizeof(GL_tmpTris));
    //memset(GL_tmpVertices, 0, sizeof(GL_tmpVertices));
}


int std3D_RenderListVerticesFinish()
{
    return 1;
}

void std3D_DrawMenuSubrect(float x, float y, float w, float h, float dstX, float dstY, float scale)
{
    //double tex_w = (double)Window_xSize;
    //double tex_h = (double)Window_ySize;
    double tex_w = Video_menuBuffer.format.width;
    double tex_h = Video_menuBuffer.format.height;

    float w_dst = w;
    float h_dst = h;

    if (scale == 0.0)
    {
        w_dst = (w / tex_w) * (double)Window_xSize;
        h_dst = (h / tex_h) * (double)Window_ySize;

        dstX = (dstX / tex_w) * (double)Window_xSize;
        dstY = (dstY / tex_h) * (double)Window_ySize;

        scale = 1.0;
    }

    double u1 = (x / tex_w);
    double u2 = ((x+w) / tex_w);
    double v1 = (y / tex_h);
    double v2 = ((y+h) / tex_h);

    GL_tmpVertices[GL_tmpVerticesAmt+0].x = dstX;
    GL_tmpVertices[GL_tmpVerticesAmt+0].y = dstY;
    GL_tmpVertices[GL_tmpVerticesAmt+0].z = 0.0;
    GL_tmpVertices[GL_tmpVerticesAmt+0].tu = u1;
    GL_tmpVertices[GL_tmpVerticesAmt+0].tv = v1;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+0].nx = 0;
    GL_tmpVertices[GL_tmpVerticesAmt+0].color = 0xFFFFFFFF;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+0].nz = 0;
    
    GL_tmpVertices[GL_tmpVerticesAmt+1].x = dstX;
    GL_tmpVertices[GL_tmpVerticesAmt+1].y = dstY + (scale * h_dst);
    GL_tmpVertices[GL_tmpVerticesAmt+1].z = 0.0;
    GL_tmpVertices[GL_tmpVerticesAmt+1].tu = u1;
    GL_tmpVertices[GL_tmpVerticesAmt+1].tv = v2;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+1].nx = 0;
    GL_tmpVertices[GL_tmpVerticesAmt+1].color = 0xFFFFFFFF;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+1].nz = 0;
    
    GL_tmpVertices[GL_tmpVerticesAmt+2].x = dstX + (scale * w_dst);
    GL_tmpVertices[GL_tmpVerticesAmt+2].y = dstY + (scale * h_dst);
    GL_tmpVertices[GL_tmpVerticesAmt+2].z = 0.0;
    GL_tmpVertices[GL_tmpVerticesAmt+2].tu = u2;
    GL_tmpVertices[GL_tmpVerticesAmt+2].tv = v2;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+2].nx = 0;
    GL_tmpVertices[GL_tmpVerticesAmt+2].color = 0xFFFFFFFF;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+2].nz = 0;
    
    GL_tmpVertices[GL_tmpVerticesAmt+3].x = dstX + (scale * w_dst);
    GL_tmpVertices[GL_tmpVerticesAmt+3].y = dstY;
    GL_tmpVertices[GL_tmpVerticesAmt+3].z = 0.0;
    GL_tmpVertices[GL_tmpVerticesAmt+3].tu = u2;
    GL_tmpVertices[GL_tmpVerticesAmt+3].tv = v1;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+3].nx = 0;
    GL_tmpVertices[GL_tmpVerticesAmt+3].color = 0xFFFFFFFF;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+3].nz = 0;
    
    GL_tmpTris[GL_tmpTrisAmt+0].v1 = GL_tmpVerticesAmt+1;
    GL_tmpTris[GL_tmpTrisAmt+0].v2 = GL_tmpVerticesAmt+0;
    GL_tmpTris[GL_tmpTrisAmt+0].v3 = GL_tmpVerticesAmt+2;
    
    GL_tmpTris[GL_tmpTrisAmt+1].v1 = GL_tmpVerticesAmt+0;
    GL_tmpTris[GL_tmpTrisAmt+1].v2 = GL_tmpVerticesAmt+3;
    GL_tmpTris[GL_tmpTrisAmt+1].v3 = GL_tmpVerticesAmt+2;
    
    GL_tmpVerticesAmt += 4;
    GL_tmpTrisAmt += 2;
}

void std3D_DrawMenuSubrect2(float x, float y, float w, float h, float dstX, float dstY, float scale)
{
    //double tex_w = (double)Window_xSize;
    //double tex_h = (double)Window_ySize;
    double tex_w = Video_menuBuffer.format.width;
    double tex_h = Video_menuBuffer.format.height;

    float w_dst = w;
    float h_dst = h;

    if (scale == 0.0)
    {
        w_dst = (w / tex_w) * (double)Window_xSize;
        h_dst = (h / tex_h) * (double)Window_ySize;

        dstX = (dstX / tex_w) * (double)Window_xSize;
        dstY = (dstY / tex_h) * (double)Window_ySize;

        scale = 1.0;
    }

    double u1 = (x / tex_w);
    double u2 = ((x+w) / tex_w);
    double v1 = (y / tex_h);
    double v2 = ((y+h) / tex_h);

    GL_tmpVertices[GL_tmpVerticesAmt+0].x = dstX;
    GL_tmpVertices[GL_tmpVerticesAmt+0].y = dstY;
    GL_tmpVertices[GL_tmpVerticesAmt+0].z = 0.0;
    GL_tmpVertices[GL_tmpVerticesAmt+0].tu = u1;
    GL_tmpVertices[GL_tmpVerticesAmt+0].tv = v1;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+0].nx = 0;
    GL_tmpVertices[GL_tmpVerticesAmt+0].color = 0x000000FF;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+0].nz = 0;
    
    GL_tmpVertices[GL_tmpVerticesAmt+1].x = dstX;
    GL_tmpVertices[GL_tmpVerticesAmt+1].y = dstY + (scale * h_dst);
    GL_tmpVertices[GL_tmpVerticesAmt+1].z = 0.0;
    GL_tmpVertices[GL_tmpVerticesAmt+1].tu = u1;
    GL_tmpVertices[GL_tmpVerticesAmt+1].tv = v2;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+1].nx = 0;
    GL_tmpVertices[GL_tmpVerticesAmt+1].color = 0x000000FF;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+1].nz = 0;
    
    GL_tmpVertices[GL_tmpVerticesAmt+2].x = dstX + (scale * w_dst);
    GL_tmpVertices[GL_tmpVerticesAmt+2].y = dstY + (scale * h_dst);
    GL_tmpVertices[GL_tmpVerticesAmt+2].z = 0.0;
    GL_tmpVertices[GL_tmpVerticesAmt+2].tu = u2;
    GL_tmpVertices[GL_tmpVerticesAmt+2].tv = v2;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+2].nx = 0;
    GL_tmpVertices[GL_tmpVerticesAmt+2].color = 0x000000FF;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+2].nz = 0;
    
    GL_tmpVertices[GL_tmpVerticesAmt+3].x = dstX + (scale * w_dst);
    GL_tmpVertices[GL_tmpVerticesAmt+3].y = dstY;
    GL_tmpVertices[GL_tmpVerticesAmt+3].z = 0.0;
    GL_tmpVertices[GL_tmpVerticesAmt+3].tu = u2;
    GL_tmpVertices[GL_tmpVerticesAmt+3].tv = v1;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+3].nx = 0;
    GL_tmpVertices[GL_tmpVerticesAmt+3].color = 0x000000FF;
    *(uint32_t*)&GL_tmpVertices[GL_tmpVerticesAmt+3].nz = 0;
    
    GL_tmpTris[GL_tmpTrisAmt+0].v1 = GL_tmpVerticesAmt+1;
    GL_tmpTris[GL_tmpTrisAmt+0].v2 = GL_tmpVerticesAmt+0;
    GL_tmpTris[GL_tmpTrisAmt+0].v3 = GL_tmpVerticesAmt+2;
    
    GL_tmpTris[GL_tmpTrisAmt+1].v1 = GL_tmpVerticesAmt+0;
    GL_tmpTris[GL_tmpTrisAmt+1].v2 = GL_tmpVerticesAmt+3;
    GL_tmpTris[GL_tmpTrisAmt+1].v3 = GL_tmpVerticesAmt+2;
    
    GL_tmpVerticesAmt += 4;
    GL_tmpTrisAmt += 2;
}

static rdDDrawSurface* test_idk = NULL;
void std3D_DrawSimpleTex(std3DSimpleTexStage* pStage, std3DIntermediateFbo* pFbo, GLuint texId, GLuint texId2, GLuint texId3, float param1, float param2, float param3, int gen_mips, const char* debugName);
void std3D_DrawMapOverlay();
void std3D_DrawUIRenderList();

void std3D_DrawMenu()
{
    if (Main_bHeadless) return;

	std3D_pushDebugGroup("std3D_DrawMenu");

    //printf("Draw menu\n");
 //   std3D_DrawSceneFbo();
    //glFlush();

	glBindFramebuffer(GL_FRAMEBUFFER, window.fbo);
	if (!jkGame_isDDraw)// || jkGuiBuildMulti_bRendering)
	{
		//glDisable(GL_DEPTH_TEST);
		//glClear(GL_COLOR_BUFFER_BIT);
	}

    glBindFramebuffer(GL_FRAMEBUFFER, std3D_windowFbo);
    glDepthMask(GL_TRUE);
    glCullFace(GL_FRONT);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_ALWAYS);
	std3D_useProgram(programMenu);
    
    float menu_w, menu_h, menu_u, menu_v, menu_x;
    menu_w = (double)Window_xSize;
    menu_h = (double)Window_ySize;
    menu_u = 1.0;
    menu_v = 1.0;
    menu_x = 0.0;
    
    int bFixHudScale = 0;

    double fake_windowW = (double)Window_xSize;
    double fake_windowH = (double)Window_ySize;

    if (!jkGame_isDDraw && !jkGuiBuildMulti_bRendering && !jkCutscene_isRendering)
    {
        //menu_w = 640.0;
        //menu_h = 480.0;

        // Stretch screen
        menu_u = (1.0 / Video_menuBuffer.format.width) * 640.0;
        menu_v = (1.0 / Video_menuBuffer.format.height) * 480.0;

        // Keep 4:3 aspect
        menu_x = (menu_w - (menu_h * (640.0 / 480.0))) / 2.0;
        menu_w = (menu_h * (640.0 / 480.0));
    }
    else if (jkCutscene_isRendering) {
        bFixHudScale = 1;

        //menu_w = 640.0;
        //menu_h = 480.0;

        menu_w = Video_menuBuffer.format.width;
        menu_h = Video_menuBuffer.format.height;

        // For ultrawide screens, limit the width to 16:9
        if (Window_xSize > Window_ySize && ((double)Window_xSize / (double)Window_ySize) > (Main_bMotsCompat ? (16.0/9.0) : (21.0/9.0))) {
            fake_windowW = fake_windowH * (16.0/9.0);
        }

        // Keep 4:3 aspect
        menu_x = (menu_w - (menu_h * (640.0 / 480.0))) / 2.0;

    }
    else if (jkGuiBuildMulti_bRendering)
    {
        bFixHudScale = 1;

        // Stretch screen
        menu_u = (1.0 / Video_menuBuffer.format.width) * 640.0;
        menu_v = (1.0 / Video_menuBuffer.format.height) * 480.0;

        // Keep 4:3 aspect
        menu_x = (menu_w - (menu_h * (640.0 / 480.0))) / 2.0;
        menu_w = (menu_h * (640.0 / 480.0));
    }
    else
    {
        bFixHudScale = 0;

        menu_w = Video_menuBuffer.format.width;
        menu_h = Video_menuBuffer.format.height;
    }

    if (!bFixHudScale)
    {
        GL_tmpVertices[0].x = menu_x;
        GL_tmpVertices[0].y = 0.0;
        GL_tmpVertices[0].z = 0.0;
        GL_tmpVertices[0].tu = 0.0;
        GL_tmpVertices[0].tv = 0.0;
        *(uint32_t*)&GL_tmpVertices[0].nx = 0;
        GL_tmpVertices[0].color = 0xFFFFFFFF;
        *(uint32_t*)&GL_tmpVertices[0].nz = 0;
        
        GL_tmpVertices[1].x = menu_x;
        GL_tmpVertices[1].y = menu_h;
        GL_tmpVertices[1].z = 0.0;
        GL_tmpVertices[1].tu = 0.0;
        GL_tmpVertices[1].tv = menu_v;
        *(uint32_t*)&GL_tmpVertices[1].nx = 0;
        GL_tmpVertices[1].color = 0xFFFFFFFF;
        *(uint32_t*)&GL_tmpVertices[1].nz = 0;
        
        GL_tmpVertices[2].x = menu_x + menu_w;
        GL_tmpVertices[2].y = menu_h;
        GL_tmpVertices[2].z = 0.0;
        GL_tmpVertices[2].tu = menu_u;
        GL_tmpVertices[2].tv = menu_v;
        *(uint32_t*)&GL_tmpVertices[2].nx = 0;
        GL_tmpVertices[2].color = 0xFFFFFFFF;
        *(uint32_t*)&GL_tmpVertices[2].nz = 0;
        
        GL_tmpVertices[3].x = menu_x + menu_w;
        GL_tmpVertices[3].y = 0.0;
        GL_tmpVertices[3].z = 0.0;
        GL_tmpVertices[3].tu = menu_u;
        GL_tmpVertices[3].tv = 0.0;
        *(uint32_t*)&GL_tmpVertices[3].nx = 0;
        GL_tmpVertices[3].color = 0xFFFFFFFF;
        *(uint32_t*)&GL_tmpVertices[3].nz = 0;
        
        GL_tmpTris[0].v1 = 1;
        GL_tmpTris[0].v2 = 0;
        GL_tmpTris[0].v3 = 2;
        
        GL_tmpTris[1].v1 = 0;
        GL_tmpTris[1].v2 = 3;
        GL_tmpTris[1].v3 = 2;
        
        GL_tmpVerticesAmt = 4;
        GL_tmpTrisAmt = 2;
    }
    else if (jkGuiBuildMulti_bRendering)
    {
        GL_tmpVerticesAmt = 0;
        GL_tmpTrisAmt = 0;

        // Main View
        std3D_DrawMenuSubrect(0, 0, 640, 480, menu_x, 0, menu_w/640.0);
    }
    else if (jkCutscene_isRendering)
    {
        GL_tmpVerticesAmt = 0;
        GL_tmpTrisAmt = 0;

        glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);

        int video_height = Main_bMotsCompat ? 350 : 300;
        int subs_y = 350;
        int subs_h = 130;
        if (Main_bMotsCompat) {
            subs_y = 400;
            subs_h = 80;
        }

        float partial_menu_w = (menu_h * (640.0 / 480.0));
        float upscale = fake_windowW/640.0;
        float upscale2 = (fake_windowH - (50 + video_height * upscale))/((double)subs_h);
        float upscale3 = 1.0;//Window_ySize/480.0;

        if (upscale2 < 1.0) {
            upscale2 = 1.0;
            if (fake_windowH > 480.0) {
                upscale2 = 2.0;
            }
        }
        if (upscale2 > upscale) {
            upscale2 = upscale;
        }

        float shift_y = ((double)Window_ySize - fake_windowH) / 2.0;
        float shift_x = ((double)Window_xSize - fake_windowW) / 2.0;

        float sub_width = 640*upscale2;
        float sub_x = (fake_windowW - sub_width) / 2.0;

        float pause_width = 640*upscale3;
        float pause_x = (fake_windowW - pause_width) / 2.0;

        //printf("%f %f, %f %f %f, %d %d\n", sub_x, pause_x, upscale, upscale2, upscale3, Window_xSize, Window_ySize);

        // Main View
        std3D_DrawMenuSubrect(0, 50, 640, video_height, shift_x + 0, shift_y + 50, upscale);

        // Subtitles
        if (jkCutscene_dword_55B750) {
            

            // Some monitors might not have a bottom black bar, so draw an outline
            std3D_DrawMenuSubrect2(0, subs_y, 640, subs_h, shift_x + sub_x-2, shift_y + fake_windowH - (subs_h*upscale2), upscale2);
            std3D_DrawMenuSubrect2(0, subs_y, 640, subs_h, shift_x + sub_x+2, shift_y + fake_windowH - (subs_h*upscale2), upscale2);
            std3D_DrawMenuSubrect2(0, subs_y, 640, subs_h, shift_x + sub_x, shift_y + fake_windowH - (subs_h*upscale2) - 2, upscale2);
            std3D_DrawMenuSubrect2(0, subs_y, 640, subs_h, shift_x + sub_x, shift_y + fake_windowH - (subs_h*upscale2) + 2, upscale2);

            //std3D_DrawMenuSubrect2(0, subs_y, 640, subs_h, shift_x + sub_x-2, shift_y + fake_windowH - (subs_h*upscale2) -2, upscale2);
            //std3D_DrawMenuSubrect2(0, subs_y, 640, subs_h, shift_x + sub_x+2, shift_y + fake_windowH - (subs_h*upscale2) +2, upscale2);
            //std3D_DrawMenuSubrect2(0, subs_y, 640, subs_h, shift_x + sub_x+2, shift_y + fake_windowH - (subs_h*upscale2) - 2, upscale2);
            //std3D_DrawMenuSubrect2(0, subs_y, 640, subs_h, shift_x + sub_x-2, shift_y + fake_windowH - (subs_h*upscale2) + 2, upscale2);

            std3D_DrawMenuSubrect(0, subs_y, 640, subs_h, shift_x + sub_x, shift_y + fake_windowH - (subs_h*upscale2), upscale2);
        }

        // Paused
        std3D_DrawMenuSubrect(0, 10, 640, 40, shift_x + pause_x, shift_y + 0*upscale, upscale3);
    }
    else
    {
        GL_tmpVerticesAmt = 0;
        GL_tmpTrisAmt = 0;

        // Main View
        std3D_DrawMenuSubrect(0, 128, menu_w, menu_h-256, 0, 128, 0.0);

        float hudScale = Window_ySize / 480.0;

        /*if (menu_w >= 3600)
            hudScale = 4;
        else if (menu_w >= 1800)
            hudScale = 3;
        else if (menu_w >= 1200)
            hudScale = 2;*/

        // Left and Right HUD
        std3D_DrawMenuSubrect(0, menu_h - 64, 64, 64, 0, Window_ySize - 64*hudScale, hudScale);
        std3D_DrawMenuSubrect(menu_w - 64, menu_h - 64, 64, 64, Window_xSize - 64*hudScale, Window_ySize - 64*hudScale, hudScale);

        // Items
        std3D_DrawMenuSubrect((menu_w / 2) - 128, menu_h - 64, 256, 64, (Window_xSize / 2) - (128*hudScale), Window_ySize - 64*hudScale, hudScale);

        // Text
        float textScale = hudScale;
        if (jkDev_BMFontHeight > 11) {
            textScale *= 11.0 / (float)jkDev_BMFontHeight;
        }
        float textWidth = menu_w - (48*2);
        float textHeight = jkDev_BMFontHeight * 5.5;
        float destTextWidth = textWidth * textScale;
        std3D_DrawMenuSubrect(48, 0, menu_w - (48*2), textHeight, (Window_xSize / 2) - (destTextWidth / 2), 0, textScale);

        // Active forcepowers/items
        std3D_DrawMenuSubrect(menu_w - 48, 0, 48, 128, Window_xSize - (48*hudScale), 0, hudScale);
    }

    glActiveTexture(GL_TEXTURE0 + 4);
    glBindTexture(GL_TEXTURE_2D, blank_tex);
    glActiveTexture(GL_TEXTURE0 + 3);
    glBindTexture(GL_TEXTURE_2D, blank_tex);
    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, blank_tex);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, blank_tex);
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, blank_tex);
    
    glActiveTexture(GL_TEXTURE0 + 0);
#ifdef HW_VBUFFER
    glBindTexture(GL_TEXTURE_2D, Video_menuBuffer.device_surface->tex);
#else   
	glBindTexture(GL_TEXTURE_2D,  Video_menuTexId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Video_menuBuffer.format.width, Video_menuBuffer.format.height, GL_RED, GL_UNSIGNED_BYTE, Video_menuBuffer.sdlSurface->pixels);
#endif

    //GLushort data_elements[32 * 3];
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, displaypal_texture);

    glActiveTexture(GL_TEXTURE0 + 0);
    glUniform1i(programMenu_uniform_tex, 0);
    glUniform1i(programMenu_uniform_displayPalette, 1);

    D3DVERTEX* vertexes = GL_tmpVertices;

	glBindVertexArray(menu_vao);
	glBindBuffer(GL_ARRAY_BUFFER, menu_vbo_all);
	glBufferData(GL_ARRAY_BUFFER, GL_tmpVerticesAmt * sizeof(D3DVERTEX), GL_tmpVertices, GL_STREAM_DRAW);

    {

    float maxX, maxY, scaleX, scaleY, width, height;

    scaleX = 1.0/((double)Window_xSize / 2.0);
    scaleY = 1.0/((double)Window_ySize / 2.0);
    maxX = 1.0;
    maxY = 1.0;
    width = Window_xSize;
    height = Window_ySize;
    
    float d3dmat[16] = {
       maxX*scaleX,      0,                                          0,      0, // right
       0,                                       -maxY*scaleY,               0,      0, // up
       0,                                       0,                                          1,     0, // forward
       -(width/2)*scaleX,  (height/2)*scaleY,     -1,      1  // pos
    };
    
    glUniformMatrix4fv(programMenu_uniform_mvp, 1, GL_FALSE, d3dmat);
    glViewport(0, 0, width, height);

    }
    
    rdTri* tris = GL_tmpTris;
    
    rdDDrawSurface* last_tex = (void*)-1;
    int last_tex_idx = 0;
    //GLushort* data_elements = malloc(sizeof(GLushort) * 3 * GL_tmpTrisAmt);
    for (int j = 0; j < GL_tmpTrisAmt; j++)
    {
        menu_data_elements[(j*3)+0] = tris[j].v1;
        menu_data_elements[(j*3)+1] = tris[j].v2;
        menu_data_elements[(j*3)+2] = tris[j].v3;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, menu_ibo_triangle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GL_tmpTrisAmt * 3 * sizeof(GLushort), menu_data_elements, GL_STREAM_DRAW);

    int tris_size = 0;  
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &tris_size);
    glDrawElements(GL_TRIANGLES, tris_size / sizeof(GLushort), GL_UNSIGNED_SHORT, 0);

    std3D_DrawMapOverlay();
    std3D_DrawUIRenderList();

	glBindVertexArray(vao);

    last_flags = 0;

	std3D_popDebugGroup();
}

void std3D_DrawMapOverlay()
{
    if (Main_bHeadless) return;

    //glFlush();

    glBindFramebuffer(GL_FRAMEBUFFER, std3D_windowFbo);
    glDepthMask(GL_TRUE);
    glCullFace(GL_FRONT);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_ALWAYS);
	std3D_useProgram(programMenu);
    
    float menu_w = (double)Window_xSize;
    float menu_h = (double)Window_ySize;

    if (!jkGame_isDDraw)
    {
        return;
    }

    menu_w = Video_menuBuffer.format.width;
    menu_h = Video_menuBuffer.format.height;

    GL_tmpVerticesAmt = 0;
    GL_tmpTrisAmt = 0;

    // Main View
    std3D_DrawMenuSubrect(0, 0, menu_w, menu_h, 0, 0, 0.0);

    glActiveTexture(GL_TEXTURE0 + 4);
    glBindTexture(GL_TEXTURE_2D, blank_tex);
    glActiveTexture(GL_TEXTURE0 + 3);
    glBindTexture(GL_TEXTURE_2D, blank_tex);
    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, blank_tex);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, blank_tex);
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, blank_tex);
    
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, Video_overlayTexId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Video_overlayMapBuffer.format.width, Video_overlayMapBuffer.format.height, GL_RED, GL_UNSIGNED_BYTE, Video_overlayMapBuffer.sdlSurface->pixels);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, displaypal_texture);

    glActiveTexture(GL_TEXTURE0 + 0);
    glUniform1i(programMenu_uniform_tex, 0);
    glUniform1i(programMenu_uniform_displayPalette, 1);

    D3DVERTEX* vertexes = GL_tmpVertices;
	glBindVertexArray(menu_vao);
	glBindBuffer(GL_ARRAY_BUFFER, menu_vbo_all);
	glBufferData(GL_ARRAY_BUFFER, GL_tmpVerticesAmt * sizeof(D3DVERTEX), GL_tmpVertices, GL_STREAM_DRAW);
    
	{

    float maxX, maxY, scaleX, scaleY, width, height;

    scaleX = 1.0/((double)Window_xSize / 2.0);
    scaleY = 1.0/((double)Window_ySize / 2.0);
    maxX = 1.0;
    maxY = 1.0;
    width = Window_xSize;
    height = Window_ySize;
    
    float d3dmat[16] = {
       maxX*scaleX,      0,                                          0,      0, // right
       0,                                       -maxY*scaleY,               0,      0, // up
       0,                                       0,                                          1,     0, // forward
       -(width/2)*scaleX,  (height/2)*scaleY,     -1,      1  // pos
    };
    
    glUniformMatrix4fv(programMenu_uniform_mvp, 1, GL_FALSE, d3dmat);
    glViewport(0, 0, width, height);

    }
    
    rdTri* tris = GL_tmpTris;
    
    rdDDrawSurface* last_tex = (void*)-1;
    int last_tex_idx = 0;
    //GLushort* data_elements = malloc(sizeof(GLushort) * 3 * GL_tmpTrisAmt);
    for (int j = 0; j < GL_tmpTrisAmt; j++)
    {
        menu_data_elements[(j*3)+0] = tris[j].v1;
        menu_data_elements[(j*3)+1] = tris[j].v2;
        menu_data_elements[(j*3)+2] = tris[j].v3;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, menu_ibo_triangle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GL_tmpTrisAmt * 3 * sizeof(GLushort), menu_data_elements, GL_STREAM_DRAW);

    int tris_size = 0;  
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &tris_size);
    glDrawElements(GL_TRIANGLES, tris_size / sizeof(GLushort), GL_UNSIGNED_SHORT, 0);

	glBindVertexArray(vao);
}

void std3D_DrawUIBitmapRGBA(stdBitmap* pBmp, int mipIdx, float dstX, float dstY, rdRect* srcRect, float scaleX, float scaleY, int bAlphaOverwrite, uint8_t color_r, uint8_t color_g, uint8_t color_b, uint8_t color_a)
{
    float internalWidth = Video_menuBuffer.format.width;
    float internalHeight = Video_menuBuffer.format.height;

    if (!pBmp) return;
    if (!pBmp->abLoadedToGPU[mipIdx]) {
        std3D_AddBitmapToTextureCache(pBmp, mipIdx, !(pBmp->palFmt & 1), 0);
    }

    if (jkGuiBuildMulti_bRendering) {
        internalWidth = 640.0;
        internalHeight = 480.0;
    }

    double scaleX_ = (double)Window_xSize/(double)internalWidth;
    double scaleY_ = (double)Window_ySize/(double)internalHeight;

    dstX *= scaleX_;
    dstY *= scaleY_;

    //double tex_w = (double)Window_xSize;
    //double tex_h = (double)Window_ySize;
    double tex_w = pBmp->mipSurfaces[0]->format.width;
    double tex_h = pBmp->mipSurfaces[0]->format.height;

    double w = tex_w;
    double h = tex_h;
    double x = 0;
    double y = 0;

    if (srcRect) {
        x = srcRect->x;
        y = srcRect->y;
        w = srcRect->width;
        h = srcRect->height;
    }

    float w_dst = w;
    float h_dst = h;

    if (scaleX == 0.0 && scaleY == 0.0)
    {
        w_dst = (w / tex_w) * (double)Window_xSize;
        h_dst = (h / tex_h) * (double)Window_ySize;

        dstX = (dstX / tex_w) * (double)Window_xSize;
        dstY = (dstY / tex_h) * (double)Window_ySize;

        scaleX = 1.0;
        scaleY = 1.0;
    }

    double dstScaleX = scaleX;
    double dstScaleY = scaleY;
    dstScaleX *= scaleX_;
    dstScaleY *= scaleY_;

    double u1 = (x / tex_w);
    double u2 = ((x+w) / tex_w);
    double v1 = (y / tex_h);
    double v2 = ((y+h) / tex_h);

    uint32_t color = 0;

    color |= (color_r << 0);
    color |= (color_g << 8);
    color |= (color_b << 16);
    color |= (color_a << 24);

    if (GL_tmpUIVerticesAmt + 4 > STD3D_MAX_UI_VERTICES) {
        return;
    }
    if (GL_tmpUITrisAmt + 2 > STD3D_MAX_UI_TRIS) {
        return;
    }

    if (dstY + (dstScaleY * h_dst) < 0.0 || dstX + (dstScaleX * w_dst) < 0.0) {
        return;
    }
    if (dstY > Window_ySize || dstX > Window_xSize) {
        return;
    }

    GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].x = dstX;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].y = dstY;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].z = 0.0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].tu = u1;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].tv = v1;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].nx = 0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].color = color;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].nz = 0;
    
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].x = dstX;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].y = dstY + (dstScaleY * h_dst);
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].z = 0.0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].tu = u1;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].tv = v2;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].nx = 0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].color = color;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].nz = 0;
    
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].x = dstX + (dstScaleX * w_dst);
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].y = dstY + (dstScaleY * h_dst);
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].z = 0.0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].tu = u2;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].tv = v2;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].nx = 0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].color = color;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].nz = 0;
    
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].x = dstX + (dstScaleX * w_dst);
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].y = dstY;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].z = 0.0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].tu = u2;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].tv = v1;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].nx = 0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].color = color;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].nz = 0;
    
    GL_tmpUITris[GL_tmpUITrisAmt+0].v1 = GL_tmpUIVerticesAmt+1;
    GL_tmpUITris[GL_tmpUITrisAmt+0].v2 = GL_tmpUIVerticesAmt+0;
    GL_tmpUITris[GL_tmpUITrisAmt+0].v3 = GL_tmpUIVerticesAmt+2;
    GL_tmpUITris[GL_tmpUITrisAmt+0].flags = bAlphaOverwrite;
    GL_tmpUITris[GL_tmpUITrisAmt+0].texture = pBmp->aTextureIds[mipIdx];
    
    GL_tmpUITris[GL_tmpUITrisAmt+1].v1 = GL_tmpUIVerticesAmt+0;
    GL_tmpUITris[GL_tmpUITrisAmt+1].v2 = GL_tmpUIVerticesAmt+3;
    GL_tmpUITris[GL_tmpUITrisAmt+1].v3 = GL_tmpUIVerticesAmt+2;
    GL_tmpUITris[GL_tmpUITrisAmt+1].flags = bAlphaOverwrite;
    GL_tmpUITris[GL_tmpUITrisAmt+1].texture = pBmp->aTextureIds[mipIdx];
    
    GL_tmpUIVerticesAmt += 4;
    GL_tmpUITrisAmt += 2;
}

void std3D_DrawUIBitmap(stdBitmap* pBmp, int mipIdx, float dstX, float dstY, rdRect* srcRect, float scale, int bAlphaOverwrite)
{
    std3D_DrawUIBitmapRGBA(pBmp, mipIdx, dstX, dstY, srcRect, scale, scale, bAlphaOverwrite, 0xFF, 0xFF, 0xFF, 0xFF);
}

void std3D_DrawUIClearedRect(uint8_t palIdx, rdRect* dstRect)
{
    if (!displaypal_data) return;
    uint32_t color = 0;
    uint8_t color_r = ((uint8_t*)displaypal_data)[(palIdx*3) + 0];
    uint8_t color_g = ((uint8_t*)displaypal_data)[(palIdx*3) + 1];
    uint8_t color_b = ((uint8_t*)displaypal_data)[(palIdx*3) + 2];

    std3D_DrawUIClearedRectRGBA(color_r, color_g, color_b, 0xFF, dstRect);
}

void std3D_DrawUIClearedRectRGBA(uint8_t color_r, uint8_t color_g, uint8_t color_b, uint8_t color_a, rdRect* dstRect)
{
    if (!has_initted) return;
    if (!dstRect) return;
    double dstX = dstRect->x;
    double dstY = dstRect->y;

    float internalWidth = Video_menuBuffer.format.width;
    float internalHeight = Video_menuBuffer.format.height;
    if (!internalWidth || !internalHeight) return;

    if (jkGuiBuildMulti_bRendering) {
        internalWidth = 640.0;
        internalHeight = 480.0;
    }

    double scaleX = (double)Window_xSize/(double)internalWidth;
    double scaleY = (double)Window_ySize/(double)internalHeight;

    dstX *= scaleX;
    dstY *= scaleY;

    //double tex_w = (double)Window_xSize;
    //double tex_h = (double)Window_ySize;
    double tex_w = dstRect->width;
    double tex_h = dstRect->height;
    if (!tex_w || !tex_h) return;

    double w = tex_w;
    double h = tex_h;
    double x = 0;
    double y = 0;

    float w_dst = w;
    float h_dst = h;
    double scale = 1.0;

    double dstScaleX = scale;
    double dstScaleY = scale;
    dstScaleX *= scaleX;
    dstScaleY *= scaleY;

    double u1 = (x / tex_w);
    double u2 = ((x+w) / tex_w);
    double v1 = (y / tex_h);
    double v2 = ((y+h) / tex_h);

    uint32_t color = 0;

    color |= (color_r << 0);
    color |= (color_g << 8);
    color |= (color_b << 16);
    color |= (color_a << 24);
    if (GL_tmpUIVerticesAmt + 4 > STD3D_MAX_UI_VERTICES) {
        return;
    }
    if (GL_tmpUITrisAmt + 2 > STD3D_MAX_UI_TRIS) {
        return;
        return;
    }

    GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].x = dstX;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].y = dstY;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].z = 0.0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].tu = u1;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].tv = v1;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].nx = 0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].color = color;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+0].nz = 0;
    
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].x = dstX;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].y = dstY + (dstScaleY * h_dst);
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].z = 0.0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].tu = u1;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].tv = v2;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].nx = 0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].color = color;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+1].nz = 0;
    
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].x = dstX + (dstScaleX * w_dst);
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].y = dstY + (dstScaleY * h_dst);
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].z = 0.0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].tu = u2;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].tv = v2;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].nx = 0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].color = color;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+2].nz = 0;
    
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].x = dstX + (dstScaleX * w_dst);
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].y = dstY;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].z = 0.0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].tu = u2;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].tv = v1;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].nx = 0;
    GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].color = color;
    *(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt+3].nz = 0;
    
    GL_tmpUITris[GL_tmpUITrisAmt+0].v1 = GL_tmpUIVerticesAmt+1;
    GL_tmpUITris[GL_tmpUITrisAmt+0].v2 = GL_tmpUIVerticesAmt+0;
    GL_tmpUITris[GL_tmpUITrisAmt+0].v3 = GL_tmpUIVerticesAmt+2;
    GL_tmpUITris[GL_tmpUITrisAmt+0].flags = 0;
    GL_tmpUITris[GL_tmpUITrisAmt+0].texture = blank_tex_white;
    
    GL_tmpUITris[GL_tmpUITrisAmt+1].v1 = GL_tmpUIVerticesAmt+0;
    GL_tmpUITris[GL_tmpUITrisAmt+1].v2 = GL_tmpUIVerticesAmt+3;
    GL_tmpUITris[GL_tmpUITrisAmt+1].v3 = GL_tmpUIVerticesAmt+2;
    GL_tmpUITris[GL_tmpUITrisAmt+1].flags = 0;
    GL_tmpUITris[GL_tmpUITrisAmt+1].texture = blank_tex_white;
    
    GL_tmpUIVerticesAmt += 4;
    GL_tmpUITrisAmt += 2;
}

void std3D_DrawUIRenderList()
{
    if (Main_bHeadless) return;
    if (!GL_tmpUITrisAmt) return;

    //glFlush();

    //printf("Draw render list\n");
    glBindFramebuffer(GL_FRAMEBUFFER, std3D_windowFbo);
    glDepthMask(GL_TRUE);
    glCullFace(GL_FRONT);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_ALWAYS);
	std3D_useProgram(std3D_uiProgram.program); // TODO: simpler shader
    
    last_ui_tex = 0;
    last_ui_flags = -1;

    // Generate vertices list
    D3DVERTEX* vertexes = GL_tmpUIVertices;

    float maxX, maxY, scaleX, scaleY, width, height;

    float internalWidth = Window_xSize;//Video_menuBuffer.format.width;
    float internalHeight = Window_ySize;//Video_menuBuffer.format.height;

    if (jkGuiBuildMulti_bRendering) {
        internalWidth = 640.0;
        internalHeight = 480.0;
    }

    maxX = 1.0;
    maxY = 1.0;
    scaleX = 1.0/((double)internalWidth / 2.0);
    scaleY = 1.0/((double)internalHeight / 2.0);
    width = Window_xSize;
    height = Window_ySize;

    if (jkGuiBuildMulti_bRendering) {
        width = 640;
        height = 480;
    }

    // JKDF2's vertical FOV is fixed with their projection, for whatever reason. 
    // This ends up resulting in the view looking squished vertically at wide/ultrawide aspect ratios.
    // To compensate, we zoom the y axis here.
    // I also went ahead and fixed vertical displays in the same way because it seems to look better.
    float zoom_yaspect = 1.0;
    float zoom_xaspect = 1.0;
    
    float shift_add_x = 0;
    float shift_add_y = 0;
    
    glUniform1i(std3D_uiProgram.uniform_tex, 0);
    glUniform1i(std3D_uiProgram.uniform_tex2, 1);
    glUniform1i(std3D_uiProgram.uniform_tex3, 2);

    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, blank_tex_white);
    
    {
    
    float d3dmat[16] = {
       maxX*scaleX,      0,                                          0,      0, // right
       0,                                       -maxY*scaleY,               0,      0, // up
       0,                                       0,                                          1,     0, // forward
       -(width/2)*scaleX,  (height/2)*scaleY,     -1,      1  // pos
    };
    
    glUniformMatrix4fv(std3D_uiProgram.uniform_mvp, 1, GL_FALSE, d3dmat);
    glViewport(0, 0, width, height);
    glUniform2f(std3D_uiProgram.uniform_iResolution, internalWidth, internalHeight);

    float param1 = 1.0;
    float param2 = 1.0;

    glUniform1f(std3D_uiProgram.uniform_param1, param1);
    glUniform1f(std3D_uiProgram.uniform_param2, param2);
    glUniform1f(std3D_uiProgram.uniform_param3, jkPlayer_gamma);
    
    }

    rdUITri* tris = GL_tmpUITris;
    
	glBindVertexArray(menu_vao);
	glBindBuffer(GL_ARRAY_BUFFER, menu_vbo_all);
	glBufferData(GL_ARRAY_BUFFER, GL_tmpUIVerticesAmt * sizeof(D3DVERTEX), GL_tmpUIVertices, GL_STREAM_DRAW);
  
    int last_flags = 0;
    int last_tex_idx = 0;
    //GLushort* menu_data_elements = malloc(sizeof(GLushort) * 3 * GL_tmpTrisAmt);
    for (int j = 0; j < GL_tmpUITrisAmt; j++)
    {
        menu_data_elements[(j*3)+0] = tris[j].v1;
        menu_data_elements[(j*3)+1] = tris[j].v2;
        menu_data_elements[(j*3)+2] = tris[j].v3;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, menu_ibo_triangle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GL_tmpUITrisAmt * 3 * sizeof(GLushort), menu_data_elements, GL_STREAM_DRAW);
    
    int do_batch = 0;

    int tex_id = tris[0].texture;
    glActiveTexture(GL_TEXTURE0 + 0);
    if (tex_id == 0)
        glBindTexture(GL_TEXTURE_2D, blank_tex_white);
    else
        glBindTexture(GL_TEXTURE_2D, tex_id);

    if (tris[0].flags) {
        glUniform1f(std3D_uiProgram.uniform_param1, 1.0);
    }
    else {
        glUniform1f(std3D_uiProgram.uniform_param1, 0.0);
    }
    
    for (int j = 0; j < GL_tmpUITrisAmt; j++)
    {
        if (tris[j].texture != last_ui_tex || tris[j].flags != last_ui_flags)
        {
            do_batch = 1;
        }
        
        if (do_batch)
        {
            int num_tris_batch = j - last_tex_idx;

            if (num_tris_batch)
            {
                //printf("batch %u~%u\n", last_tex_idx, j);
                glDrawElements(GL_TRIANGLES, num_tris_batch * 3, GL_UNSIGNED_SHORT, (GLvoid*)((intptr_t)&menu_data_elements[last_tex_idx * 3] - (intptr_t)&menu_data_elements[0]));
            }

            int tex_id = tris[j].texture;
            glActiveTexture(GL_TEXTURE0 + 0);
            if (tex_id == 0)
                glBindTexture(GL_TEXTURE_2D, blank_tex_white);
            else
                glBindTexture(GL_TEXTURE_2D, tex_id);

            if (tris[j].flags) {
                glUniform1f(std3D_uiProgram.uniform_param1, 1.0);
            }
            else {
                glUniform1f(std3D_uiProgram.uniform_param1, 0.0);
            }
            
            last_ui_tex = tris[j].texture;
            last_ui_flags = tris[j].flags;
            last_tex_idx = j;

            do_batch = 0;
        }
        //printf("tri %u: %u,%u,%u\n", j, tris[j].v1, tris[j].v2, tris[j].v3);
        
        
        /*int vert = tris[j].v1;
        stdPlatform_Printf("%u: %f %f %f, %f %f %f, %f %f\n", vert, vertexes[vert].x, vertexes[vert].y, vertexes[vert].z,
                                      vertexes[vert].nx, vertexes[vert].ny, vertexes[vert].nz,
                                      vertexes[vert].tu, vertexes[vert].tv);
        
        vert = tris[j].v2;
        stdPlatform_Printf("%u: %f %f %f, %f %f %f, %f %f\n", vert, vertexes[vert].x, vertexes[vert].y, vertexes[vert].z,
                                      vertexes[vert].nx, vertexes[vert].ny, vertexes[vert].nz,
                                      vertexes[vert].tu, vertexes[vert].tv);
        
        vert = tris[j].v3;
        stdPlatform_Printf("%u: %f %f %f, %f %f %f, %f %f\n", vert, vertexes[vert].x, vertexes[vert].y, vertexes[vert].z,
                                      vertexes[vert].nx, vertexes[vert].ny, vertexes[vert].nz,
                                      vertexes[vert].tu, vertexes[vert].tv);*/
    }
    
    int remaining_batch = GL_tmpUITrisAmt - last_tex_idx;

    if (remaining_batch)
    {
        glDrawElements(GL_TRIANGLES, remaining_batch * 3, GL_UNSIGNED_SHORT, (GLvoid*)((intptr_t)&menu_data_elements[last_tex_idx * 3] - (intptr_t)&menu_data_elements[0]));
    }

    // Done drawing    
    glBindTexture(GL_TEXTURE_2D, blank_tex_white);

	glBindVertexArray(vao);

    std3D_ResetUIRenderList();
}

// todo: clean this shit up
void std3D_DrawSimpleTex(std3DSimpleTexStage* pStage, std3DIntermediateFbo* pFbo, GLuint texId, GLuint texId2, GLuint texId3, float param1, float param2, float param3, int gen_mips, const char* debugName)
{
	std3D_pushDebugGroup(debugName);

    glBindFramebuffer(GL_FRAMEBUFFER, pFbo->fbo);
    glDepthFunc(GL_ALWAYS);
	glDisable(GL_CULL_FACE);
	std3D_useProgram(pStage->program);
    
	glBindVertexArray(vao);

    float menu_w, menu_h, menu_u, menu_v, menu_x;
    menu_w = (double)pFbo->w;
    menu_h = (double)pFbo->h;
    menu_u = 1.0;
    menu_v = 1.0;
    menu_x = 0.0;
	    
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, texId);
    if (gen_mips)
        glGenerateMipmap(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, texId2 ? texId2 : blank_tex);
    if (texId2 && gen_mips)
        glGenerateMipmap(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, texId3 ? texId3 : blank_tex);
    if (texId3 && gen_mips)
        glGenerateMipmap(GL_TEXTURE_2D);

    GLushort data_elements[32 * 3];

    D3DVERTEX* vertexes = GL_tmpVertices;
    
    glUniform1i(pStage->uniform_tex, 0);
    glUniform1i(pStage->uniform_tex2, 1);
    glUniform1i(pStage->uniform_tex3, 2);
	glUniform1i(pStage->uniform_tex4, 3);

	glUniform3f(pStage->uniform_tint, rdroid_curColorEffects.tint.x, rdroid_curColorEffects.tint.y, rdroid_curColorEffects.tint.z);
	if (rdroid_curColorEffects.filter.x || rdroid_curColorEffects.filter.y || rdroid_curColorEffects.filter.z)
		glUniform3f(pStage->uniform_filter, rdroid_curColorEffects.filter.x ? 1.0 : 0.25, rdroid_curColorEffects.filter.y ? 1.0 : 0.25, rdroid_curColorEffects.filter.z ? 1.0 : 0.25);
	else
		glUniform3f(pStage->uniform_filter, 1.0, 1.0, 1.0);
	glUniform1f(pStage->uniform_fade, rdroid_curColorEffects.fade);
	glUniform3f(pStage->uniform_add, (float)rdroid_curColorEffects.add.x / 255.0f, (float)rdroid_curColorEffects.add.y / 255.0f, (float)rdroid_curColorEffects.add.z / 255.0f);

    {

    float maxX, maxY, scaleX, scaleY, width, height;

    scaleX = 1.0/((double)pFbo->w / 2.0);
    scaleY = 1.0/((double)pFbo->h / 2.0);
    maxX = 1.0;
    maxY = 1.0;
    width = pFbo->w;
    height = pFbo->h;
    
    float d3dmat[16] = {
       maxX*scaleX,      0,                                          0,      0, // right
       0,                                       -maxY*scaleY,               0,      0, // up
       0,                                       0,                                          1,     0, // forward
       -(width/2)*scaleX,  (height/2)*scaleY,     -1,      1  // pos
    };
    
    glUniformMatrix4fv(pStage->uniform_mvp, 1, GL_FALSE, d3dmat);
    glViewport(0, 0, width, height);
    glUniform2f(pStage->uniform_iResolution, pFbo->iw, pFbo->ih);

    glUniform1f(pStage->uniform_param1, param1);
    glUniform1f(pStage->uniform_param2, param2);
    glUniform1f(pStage->uniform_param3, param3);

	glUniform1i(pStage->uniform_tex, TEX_SLOT_DIFFUSE);
	glUniform1i(pStage->uniform_lightbuf, TEX_SLOT_CLUSTER_BUFFER);

	if(renderPassProj)
		glUniformMatrix4fv(pStage->uniform_proj, 1, GL_FALSE, (float*)renderPassProj);

	//glUniform3fv(pStage->uniform_rt, 1, (float*)&rdCamera_pCurCamera->pClipFrustum->rt);
	//glUniform3fv(pStage->uniform_lt, 1, (float*)&rdCamera_pCurCamera->pClipFrustum->lt);
	//glUniform3fv(pStage->uniform_rb, 1, (float*)&rdCamera_pCurCamera->pClipFrustum->rb);
	//glUniform3fv(pStage->uniform_lb, 1, (float*)&rdCamera_pCurCamera->pClipFrustum->lb);
    }
    
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	std3D_popDebugGroup();
}

int std3D_SetCurrentPalette(rdColor24 *a1, int a2)
{
    return 1;
}

void std3D_GetValidDimension(unsigned int inW, unsigned int inH, unsigned int *outW, unsigned int *outH)
{
    // TODO hack for JKE? I don't know what they're doing
    *outW = inW > 256 ? 256 : inW;
    *outH = inH > 256 ? 256 : inH;
}

int std3D_DrawOverlay()
{
    return 1;
}

void std3D_UnloadAllTextures()
{
#ifndef SDL2_RENDER
    if (!Main_bHeadless) {
        glDeleteTextures(std3D_loadedTexturesAmt, std3D_aLoadedTextures);
    }
    std3D_loadedTexturesAmt = 0;
#else
    std3D_UpdateSettings();
#endif
}

void std3D_AddRenderListTris(rdTri *tris, unsigned int num_tris)
{
    if (Main_bHeadless) return;

    if (GL_tmpTrisAmt + num_tris > STD3D_MAX_TRIS)
    {
        return;
    }
    
    memcpy(&GL_tmpTris[GL_tmpTrisAmt], tris, sizeof(rdTri) * num_tris);
    
    GL_tmpTrisAmt += num_tris;
}

void std3D_AddRenderListLines(rdLine* lines, uint32_t num_lines)
{
    if (Main_bHeadless) return;

    if (GL_tmpLinesAmt + num_lines > STD3D_MAX_VERTICES)
    {
        return;
    }
    
    memcpy(&GL_tmpLines[GL_tmpLinesAmt], lines, sizeof(rdLine) * num_lines);
    GL_tmpLinesAmt += num_lines;
}

int std3D_AddRenderListVertices(D3DVERTEX *vertices, int count)
{
    if (Main_bHeadless) return 1;

    if (GL_tmpVerticesAmt + count >= STD3D_MAX_VERTICES)
    {
        return 0;
    }
    
    memcpy(&GL_tmpVertices[GL_tmpVerticesAmt], vertices, sizeof(D3DVERTEX) * count);
    
    GL_tmpVerticesAmt += count;
    
    return 1;
}

void std3D_AddRenderListUITris(rdUITri *tris, unsigned int num_tris)
{
    if (Main_bHeadless) return;

    if (GL_tmpUITrisAmt + num_tris > STD3D_MAX_TRIS)
    {
        return;
    }
    
    memcpy(&GL_tmpUITris[GL_tmpUITrisAmt], tris, sizeof(rdUITri) * num_tris);
    
    GL_tmpUITrisAmt += num_tris;
}

int std3D_ClearZBuffer()
{
    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, std3D_framebuffer.fbo);
    glClear(GL_DEPTH_BUFFER_BIT);
    return 1;
}

void rgb_to_hsv(float r, float g, float b, float* h, float* s, float* v)
{
	float min, max, delta;

	min = fminf(fminf(r, g), b);
	max = fmaxf(fmaxf(r, g), b);
	*v = max;
	delta = max - min;

	if (max != 0.0f)
	{
		*s = delta / max;
	}
	else
	{
		*s = 0.0f;
	}

	if (delta == 0.0f)
	{
		*h = 0.0f;
	}
	else
	{
		if (r == max)
		{
			*h = (g - b) / delta;  // Red is max
		}
		else if (g == max)
		{
			*h = (b - r) / delta + 2.0f;  // Green is max
		}
		else
		{
			*h = (r - g) / delta + 4.0f;  // Blue is max
		}
		*h *= 60.0f;
		if (*h < 0.0f)
		{
			*h += 360.0f;
		}
	}
}

float color_to_roughness(uint8_t ir, uint8_t ig, uint8_t ib)
{
	float r = (float)ir / 255.0f;
	float g = (float)ig / 255.0f;
	float b = (float)ib / 255.0f;

	float h, s, v;
	rgb_to_hsv(r, g, b, &h, &s, &v);

	float roughness = 1.0f - s;  // Inverse relation to saturation
	roughness *= (1.0f - v); // Brighter colors result in less roughness

	return roughness;
}

float calculate_saturation(float r, float g, float b)
{
	float min_val = fminf(fminf(r, g), b) / 255.0;
	float max_val = fmaxf(fmaxf(r, g), b) / 255.0;
	float delta = max_val - min_val;

	if (max_val == 0.0f) return 0.0f;
	return delta / max_val;
}

float calculate_luminance(float r, float g, float b)
{
	return 0.299f * r + 0.587f * g + 0.114f * b;
}

float generate_specular(float r, float g, float b, float specular_scale)
{
	float luminance = calculate_luminance(r/255.0, g/255.0, b/255.0);

	//luminance = stdMath_Clamp((luminance - 0.8f) * 4.0 + 0.8f, 0.0f, 1.0f);

	float maxl = 0.8;
	float minl = 0.4;
	//luminance = (luminance - minl) / (maxl - minl);
	//luminance = stdMath_Clamp(luminance, 0.0f, 1.0f);

	// sigmoid contrast
//	luminance = 1.0f / (1.0f + (expf(-(luminance - 0.5f) * 12.0f)));

	//luminance = (1.0f - luminance * luminance) * powf(_frand(), 16.0f);

	//luminance = 1.0f / (luminance * 100.0f + 1.0f);

	luminance = powf(luminance, 64.0) * 64.0;

	luminance = stdMath_Clamp(luminance, 0.0f, 1.0f);

	return luminance;// 1.-powf(luminance, 8.0f) * specular_scale;
}

int std3D_AddToTextureCache(stdVBuffer** vbuf, int numMips, rdDDrawSurface *texture, int is_alpha_tex, int no_alpha)
{
    if (Main_bHeadless) return 1;
    if (!vbuf || !*vbuf || !texture) return 1;
    if (texture->texture_loaded) return 1;

    if (std3D_loadedTexturesAmt >= STD3D_MAX_TEXTURES) {
        stdPlatform_Printf("ERROR: Texture cache exhausted!! Ask ShinyQuagsire to increase the size.\n");
        return 1;
    }
    //printf("Add to texture cache\n");
    
	GLuint displacement_texture = 0;
	GLuint specular_texture = 0;
	GLuint emissive_texture = 0;

    GLuint image_texture;
    glGenTextures(1, &image_texture);
    uint8_t* image_8bpp = (*vbuf)->sdlSurface->pixels;
    uint16_t* image_16bpp = (*vbuf)->sdlSurface->pixels;
    uint8_t* pal = (*vbuf)->palette;
    
    uint32_t width, height;
    width = (*vbuf)->format.width;
    height = (*vbuf)->format.height;

	glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, numMips-1);

	if (jkPlayer_enableTextureFilter && texture->is_16bit)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	}

	stdVBuffer** vbufIter = vbuf;

    if ((*vbufIter)->format.format.colorMode)
    {
        texture->is_16bit = 1;
		if ((*vbufIter)->format.format.bpp == 32)
		{
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			if (!is_alpha_tex)
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image_8bpp);
			else
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_8bpp);
		}
		else
		{
			glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
			if (!is_alpha_tex)
			{
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, width, height, 0,  GL_RGB, GL_UNSIGNED_SHORT_5_6_5, image_8bpp);
			}
			else
			{
				if((*vbufIter)->format.format.g_bits == 4) // todo: make this an alpha check or something
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, width, height, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, image_8bpp);
				else
				{
					glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, width, height, 0,  GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, image_8bpp);
				}
			}
		}

		for(int mip = 1; mip < numMips; ++mip)
		{
			++vbufIter;

			image_8bpp = (*vbufIter)->sdlSurface->pixels;
			width = (*vbufIter)->format.width;
			height = (*vbufIter)->format.height;

			if (!is_alpha_tex)
			{
				glTexImage2D(GL_TEXTURE_2D, mip, GL_RGB565, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, image_8bpp);
			}
			else
			{
				if ((*vbufIter)->format.format.g_bits == 4) // todo: make this an alpha check or something
					glTexImage2D(GL_TEXTURE_2D, mip, GL_RGBA4, width, height, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, image_8bpp);
				else
				{
					glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
					glTexImage2D(GL_TEXTURE_2D, mip, GL_RGB5_A1, width, height, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, image_8bpp);
				}
			}
		}

		// generate the remaining mips
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, numMips - 1);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    }
    else {

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		
		//texture->is_16bit = 0;
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image_8bpp);

		texture->is_16bit = 0;

		// it's much faster on GPU side to simply convert to rgba instead of manually bilinear filtering
		uint16_t* image_data = (uint16_t*)malloc(width * height * 2);
		texture->pDataDepthConverted = (void*)image_data;

		float avg = 0.0;
		float max_lum = 0.0;
		float min_lum = 1.0;

		for (int j = 0; j < height; j++)
		{
			for (int i = 0; i < width; i++)
			{
				uint32_t index = (i * height) + j;
				uint16_t val_rgba = 0x0000;

				uint8_t val = image_8bpp[index];
				
				rdColor24 color;
				if (pal)
				{
					color.r = pal[(val * 3) + 2];
					color.g = pal[(val * 3) + 1];
					color.b = pal[(val * 3) + 0];
				}
				else
				{
					rdColor24* pal_master = sithWorld_pCurrentWorld ? (rdColor24*)sithWorld_pCurrentWorld->colormaps->colors : stdDisplay_gammaPalette;
					color = pal_master[val];
				}

				float lum = calculate_luminance(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
				avg += lum;
				max_lum = fmax(lum, max_lum);
				min_lum = fmin(lum, min_lum);

				if(is_alpha_tex)
				{
					val_rgba |= (val != (*vbufIter)->transparent_color) ? 1 : 0x0000;

					val_rgba |= ((((uint32_t)color.r >> 3) & 0x1F) << 11);
					val_rgba |= ((((uint32_t)color.g >> 3) & 0x1F) << 6);
					val_rgba |= ((((uint32_t)color.b >> 3) & 0x1F) << 1);
				}
				else
				{
					val_rgba |= ((((uint32_t)color.r >> 3) & 0x1F) << 11);
					val_rgba |= ((((uint32_t)color.g >> 2) & 0x3F) << 5);
					val_rgba |= ((((uint32_t)color.b >> 3) & 0x1F) << 0);
				}

				image_data[index] = val_rgba;
			}
		}

		avg /= width * height;

		glTexImage2D(GL_TEXTURE_2D, 0, is_alpha_tex ? GL_RGB5_A1 : GL_RGB565, width, height, 0, is_alpha_tex ? GL_RGBA : GL_RGB, is_alpha_tex ? GL_UNSIGNED_SHORT_5_5_5_1 : GL_UNSIGNED_SHORT_5_6_5, image_data);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, numMips - 1);
		for (int mip = 1; mip < numMips; ++mip)
		{
			++vbufIter;

			image_8bpp = (*vbufIter)->sdlSurface->pixels;
			pal = (*vbufIter)->palette;

			width = (*vbufIter)->format.width;
			height = (*vbufIter)->format.height;

			for (int j = 0; j < height; j++)
			{
				for (int i = 0; i < width; i++)
				{
					uint32_t index = (i * height) + j;
					uint16_t val_rgba = 0x0000;

					uint8_t val = image_8bpp[index];

					rdColor24 color;
					if (pal)
					{
						color.r = pal[(val * 3) + 2];
						color.g = pal[(val * 3) + 1];
						color.b = pal[(val * 3) + 0];
					}
					else
					{
						rdColor24* pal_master = sithWorld_pCurrentWorld ? (rdColor24*)sithWorld_pCurrentWorld->colormaps->colors : stdDisplay_gammaPalette;
						color = pal_master[val];
					}

					if (is_alpha_tex)
					{
						val_rgba |= (val != (*vbufIter)->transparent_color) ? 1 : 0x0000;
						val_rgba |= ((((uint32_t)color.r >> 3) & 0x1F) << 11);
						val_rgba |= ((((uint32_t)color.g >> 3) & 0x1F) << 6);
						val_rgba |= ((((uint32_t)color.b >> 3) & 0x1F) << 1);
					}
					else
					{
						val_rgba |= ((((uint32_t)color.r >> 3) & 0x1F) << 11);
						val_rgba |= ((((uint32_t)color.g >> 2) & 0x3F) << 5);
						val_rgba |= ((((uint32_t)color.b >> 3) & 0x1F) << 0);
					}

					image_data[index] = val_rgba;
				}
			}

			glTexImage2D(GL_TEXTURE_2D, mip, is_alpha_tex ? GL_RGB5_A1 : GL_RGB565, width, height, 0, is_alpha_tex ? GL_RGBA : GL_RGB, is_alpha_tex ? GL_UNSIGNED_SHORT_5_5_5_1 : GL_UNSIGNED_SHORT_5_6_5, image_data);
			//glTexImage2D(GL_TEXTURE_2D, mip, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image_8bpp);
		}

		// generate the remaining mips
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, numMips - 1);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);

		// auto generate a specular texture from the indices
		if(1)
		{
			glGenTextures(1, &specular_texture);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, specular_texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			
			GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_RED };
			glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

			if (jkPlayer_enableTextureFilter)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			}
			else
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			}

			vbufIter = vbuf;

			image_8bpp = (*vbufIter)->sdlSurface->pixels;
			pal = (*vbufIter)->palette;

			width = (*vbufIter)->format.width;
			height = (*vbufIter)->format.height;

			const uint32_t DITHER_LUT[16] = {
				0, 4, 1, 5,
				6, 2, 7, 3,
				1, 5, 0, 4,
				7, 3, 6, 2
			};

			for (int j = 0; j < height; j++)
			{
				for (int i = 0; i < width; i++)
				{
					uint32_t index = (i * height) + j;
					uint8_t val = image_8bpp[index];

					//uint32_t state = val * 747796405u + 2891336453u;
					//uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
					//val = ((word >> 22u) ^ word) & 0xFFu;


					int wrap_x = (i) & 3;
					int wrap_y = (j) & 3;
					int wrap_index = wrap_x + wrap_y * 4;

					//uint32_t state = val * 747796405u + 2891336453u;
					//val = (state >> 24u) & 0xFFu;

					//val = powf((float)val / 255.0, 0.5f) * 255;

					rdColor24 color;
					if (pal)
					{
						color.r = pal[(val * 3) + 2];
						color.g = pal[(val * 3) + 1];
						color.b = pal[(val * 3) + 0];
					}
					else
					{
						rdColor24* pal_master = sithWorld_pCurrentWorld ? (rdColor24*)sithWorld_pCurrentWorld->colormaps->colors : stdDisplay_gammaPalette;
						color = pal_master[val];
					}

					//val = color_to_roughness(color.r, color.g, color.b) * 255;
					//val *= (DITHER_LUT[wrap_index] / 8.0f)*0.5+0.5;
					//val = calculate_saturation(color.r, color.g, color.b) * 255;
				//	val = generate_specular(color.r, color.g, color.b, 1.0f) * 255;

					float l = calculate_luminance(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
				//	l /= avg;
				
				//	l = 1.0f / (1.0f + (expf(-(l - 0.5f) * 14.0f)));
				//	l = powf(l / 0.5, 4.0f) * 0.5;

					//if(avg < 0.1)
					//	l = max_lum - l;

					l = (l - min_lum) / (max_lum - min_lum);

					//float mid = (max_lum + min_lum) * 0.5;
					//if (max_lum < 0.3)
						//l = 1.0f - l;

					//l = stdMath_Sqrt(l);
					//l = powf(l, 0.25f);
					//l = powf(l, 1.0f) * avg * 2.0f;
					val = l * 255;

					//uint32_t state = val * 747796405u + 2891336453u;
					//val = (state >> 24u) & 0xFFu;

					((uint8_t*)image_data)[index] = val;
				}
			}

			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image_data);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, numMips - 1);
			for (int mip = 1; mip < numMips; ++mip)
			{
				++vbufIter;

				image_8bpp = (*vbufIter)->sdlSurface->pixels;
				pal = (*vbufIter)->palette;

				width = (*vbufIter)->format.width;
				height = (*vbufIter)->format.height;

				for (int j = 0; j < height; j++)
				{
					for (int i = 0; i < width; i++)
					{
						uint32_t index = (i * height) + j;
						uint8_t val = image_8bpp[index];

						//uint32_t state = val * 747796405u + 2891336453u;
						//uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
						//val = ((word >> 22u) ^ word) & 0xFFu;

						int wrap_x = (i) & 3;
						int wrap_y = (j) & 3;
						int wrap_index = wrap_x + wrap_y * 4;

						//uint32_t state = val * 747796405u + 2891336453u;
						//val = (state >> 24u) & 0xFFu;

						//val = powf((float)val / 255.0, 0.5f) * 255;

						rdColor24 color;
						if (pal)
						{
							color.r = pal[(val * 3) + 2];
							color.g = pal[(val * 3) + 1];
							color.b = pal[(val * 3) + 0];
						}
						else
						{
							rdColor24* pal_master = sithWorld_pCurrentWorld ? (rdColor24*)sithWorld_pCurrentWorld->colormaps->colors : stdDisplay_gammaPalette;
							color = pal_master[val];
						}

						//val = color_to_roughness(color.r, color.g, color.b) * 255;
						//val *= (DITHER_LUT[wrap_index] / 8.0f) * 0.5 + 0.5;
						//val = calculate_saturation(color.r, color.g, color.b) * 255;
					//	val = generate_specular(color.r, color.g, color.b, 1.0f) * 255;

						//uint32_t state = val * 747796405u + 2891336453u;
					//	val = (state >> 24u) & 0xFFu;

						float l = calculate_luminance(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
					//	l /= avg;
						//if (avg < 0.1)
							//l = max_lum - l;

					//	l = 1.0f / (1.0f + (expf(-(l - 0.5f) * 14.0f)));

					//	l = powf(l / 0.5, 4.0f) * 0.5;
						l = (l - min_lum) / (max_lum - min_lum);

						//float mid = (max_lum + min_lum) * 0.5;
						//if (max_lum < 0.3)
						//	l = 1.0f - l;

						//l = powf(l, 1.0f) * avg * 2.0f;
						//l = powf(l, 0.25f);
						val = l * 255;

						((uint8_t*)image_data)[index] = val;
					}
				}

				glTexImage2D(GL_TEXTURE_2D, mip, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image_data);
				//glTexImage2D(GL_TEXTURE_2D, mip, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image_8bpp);
			}

			// generate the remaining mips
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, numMips - 1);
			glGenerateMipmap(GL_TEXTURE_2D);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		}

		// auto generate a displacement texture from the indices
		if (0)
		{
			glGenTextures(1, &displacement_texture);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, displacement_texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);

			GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_RED };
			glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

			if (jkPlayer_enableTextureFilter)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			}
			else
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			}

			vbufIter = vbuf;

			image_8bpp = (*vbufIter)->sdlSurface->pixels;
			pal = (*vbufIter)->palette;

			width = (*vbufIter)->format.width;
			height = (*vbufIter)->format.height;

			for (int j = 0; j < height; j++)
			{
				for (int i = 0; i < width; i++)
				{
					uint32_t index = (i * height) + j;
					uint8_t val = image_8bpp[index];

					rdColor24 color;
					if (pal)
					{
						color.r = pal[(val * 3) + 2];
						color.g = pal[(val * 3) + 1];
						color.b = pal[(val * 3) + 0];
					}
					else
					{
						rdColor24* pal_master = sithWorld_pCurrentWorld ? (rdColor24*)sithWorld_pCurrentWorld->colormaps->colors : stdDisplay_gammaPalette;
						color = pal_master[val];
					}

					float l = calculate_luminance(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
					l = (l - min_lum) / (max_lum - min_lum);
					l = 1.0f - powf(l, 2.0f);

					val = l * 255;

					//uint32_t state = val * 747796405u + 2891336453u;
					//val = (state >> 24u) & 0xFFu;

					((uint8_t*)image_data)[index] = val;
				}
			}

			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image_data);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, numMips - 1);
			for (int mip = 1; mip < numMips; ++mip)
			{
				++vbufIter;

				image_8bpp = (*vbufIter)->sdlSurface->pixels;
				pal = (*vbufIter)->palette;

				width = (*vbufIter)->format.width;
				height = (*vbufIter)->format.height;

				for (int j = 0; j < height; j++)
				{
					for (int i = 0; i < width; i++)
					{
						uint32_t index = (i * height) + j;
						uint8_t val = image_8bpp[index];

						rdColor24 color;
						if (pal)
						{
							color.r = pal[(val * 3) + 2];
							color.g = pal[(val * 3) + 1];
							color.b = pal[(val * 3) + 0];
						}
						else
						{
							rdColor24* pal_master = sithWorld_pCurrentWorld ? (rdColor24*)sithWorld_pCurrentWorld->colormaps->colors : stdDisplay_gammaPalette;
							color = pal_master[val];
						}

					
						float l = calculate_luminance(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
						l = (l - min_lum) / (max_lum - min_lum);
						l = 1.0f - powf(l, 2.0f);

						val = l * 255;

						((uint8_t*)image_data)[index] = val;
					}
				}

				glTexImage2D(GL_TEXTURE_2D, mip, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image_data);
				//glTexImage2D(GL_TEXTURE_2D, mip, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image_8bpp);
			}

			// generate the remaining mips
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, numMips - 1);
			glGenerateMipmap(GL_TEXTURE_2D);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		}
	
		// auto-generate an emissive texture from the light palette
		uint8_t* light = sithWorld_pCurrentWorld ? (uint8_t*)sithWorld_pCurrentWorld->colormaps->lightlevel : NULL;

		if(light && !pal)
		{
			glGenTextures(1, &emissive_texture);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, emissive_texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);

			if (jkPlayer_enableTextureFilter)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			}
			else
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			}

			vbufIter = vbuf;

			image_8bpp = (*vbufIter)->sdlSurface->pixels;
			pal = (*vbufIter)->palette;

			width = (*vbufIter)->format.width;
			height = (*vbufIter)->format.height;

			for (int j = 0; j < height; j++)
			{
				for (int i = 0; i < width; i++)
				{
					uint32_t index = (i * height) + j;
					uint16_t val_rgba = 0x0000;

					//val_rgba |= (index != (*vbufIter)->transparent_color) ? 0xFF000000 : 0x00000000;

					uint8_t val = light[image_8bpp[index]];
						
					rdColor24* pal_master = sithWorld_pCurrentWorld ? (rdColor24*)sithWorld_pCurrentWorld->colormaps->colors : stdDisplay_gammaPalette;
					rdColor24 color = pal_master[val];

					val_rgba |= (((uint32_t)color.r >> 3) << 11);
					val_rgba |= (((uint32_t)color.g >> 2) << 5);
					val_rgba |= (((uint32_t)color.b >> 3) << 0);

					image_data[index] = val_rgba;
				}
			}

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, image_data);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, numMips - 1);
			for (int mip = 1; mip < numMips; ++mip)
			{
				++vbufIter;

				image_8bpp = (*vbufIter)->sdlSurface->pixels;
				pal = (*vbufIter)->palette;

				width = (*vbufIter)->format.width;
				height = (*vbufIter)->format.height;

				for (int j = 0; j < height; j++)
				{
					for (int i = 0; i < width; i++)
					{
						uint32_t index = (i * height) + j;
						uint16_t val_rgba = 0x0000;

						//val_rgba |= (index != (*vbufIter)->transparent_color) ? 0xFF000000 : 0x00000000;

						uint8_t val = light[image_8bpp[index]];
						rdColor24* pal_master = (rdColor24*)sithWorld_pCurrentWorld->colormaps->colors;//stdDisplay_gammaPalette;
						rdColor24 color = pal_master[val];

						val_rgba |= (((uint32_t)color.r >> 3u) << 11);
						val_rgba |= (((uint32_t)color.g >> 2u) << 5);
						val_rgba |= (((uint32_t)color.b >> 3u) << 0);

						image_data[index] = val_rgba;
					}
				}

				glTexImage2D(GL_TEXTURE_2D, mip, GL_RGB565, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, image_data);
				//glTexImage2D(GL_TEXTURE_2D, mip, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image_8bpp);
			}
		
			// generate the remaining mips
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, numMips - 1);
			glGenerateMipmap(GL_TEXTURE_2D);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		}

        //texture->pDataDepthConverted = NULL;
    }
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    
    std3D_aLoadedSurfaces[std3D_loadedTexturesAmt] = texture;
    std3D_aLoadedTextures[std3D_loadedTexturesAmt++] = image_texture;
    /*ext->surfacebuf = image_data;
    ext->surfacetex = image_texture;
    ext->surfacepaltex = pal_texture;*/
    
    texture->texture_id = image_texture;
	texture->specular_texture_id = specular_texture;
    texture->emissive_texture_id = emissive_texture;
    texture->displacement_texture_id = displacement_texture;
    texture->texture_loaded = 1;
    texture->emissive_factor[0] = 1.0;
    texture->emissive_factor[1] = 1.0;
    texture->emissive_factor[2] = 1.0;
    texture->albedo_factor[0] = 1.0;
    texture->albedo_factor[1] = 1.0;
    texture->albedo_factor[2] = 1.0;
    texture->albedo_factor[3] = 1.0;
    texture->displacement_factor = 0.0;// 0.05;
    texture->albedo_data = NULL;
    texture->displacement_data = NULL;
    texture->emissive_data = NULL;

    glBindTexture(GL_TEXTURE_2D, blank_tex);
    
    return 1;
}

int std3D_GetBitmapCacheIdx()
{
    for (int i = 0; i < STD3D_MAX_TEXTURES; i++)
    {
        if (!std3D_aUIBitmaps[i]) {
            return i;
        }
    }
    return -1;
}

int std3D_AddBitmapToTextureCache(stdBitmap *texture, int mipIdx, int is_alpha_tex, int no_alpha)
{
    if (Main_bHeadless) return 1;
    if (!has_initted) return 0;
    if (!texture) return 1;
    if (mipIdx >= texture->numMips) return 1;
    if (!texture->abLoadedToGPU || texture->abLoadedToGPU[mipIdx]) return 1;

    stdVBuffer *vbuf = texture->mipSurfaces[mipIdx];
     if (!vbuf) return 1;

    int cacheIdx = std3D_GetBitmapCacheIdx();
    if (cacheIdx < 0) {
        stdPlatform_Printf("ERROR: Texture cache exhausted!! Ask ShinyQuagsire to increase the size.\n");
        return 1;
    }
    //printf("Add to texture cache\n");
    
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    uint8_t* image_8bpp = vbuf->sdlSurface->pixels;
    uint16_t* image_16bpp = vbuf->sdlSurface->pixels;
    uint8_t* pal = vbuf->palette;
    
    uint32_t width, height;
    width = vbuf->format.width;
    height = vbuf->format.height;

    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    /*if (jkPlayer_enableTextureFilter && texture->is_16bit)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    else*/
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    if (vbuf->format.format.colorMode || texture->format.bpp == 16)
    {
        texture->is_16bit = 1;

#if 0
        if (!is_alpha_tex)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0,  GL_RGB, GL_UNSIGNED_SHORT_5_6_5, image_8bpp);
        else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,  GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, image_8bpp);
#endif
        uint32_t tex_width, tex_height, tex_row_stride;
        uint32_t row_stride = vbuf->format.width_in_bytes / 2;
        tex_width = width;//vbuf->format.width_in_bytes / 2;
        tex_height = height;
        tex_row_stride = width;

        void* image_data = malloc(tex_width*tex_height*4);
        memset(image_data, 0, tex_width*tex_height*4);
    
        //uint32_t index = 0;
        for (int j = 0; j < height; j++)
        {
            for (int i = 0; i < width; i++)
            {
                uint32_t index = (j*row_stride) + i;
                uint32_t tex_index = (j*tex_row_stride) + i;
                uint32_t val_rgba = 0x00000000;
                
                uint16_t val = image_16bpp[index];
                if (vbuf->format.format.r_bits == 5 && vbuf->format.format.g_bits == 6 && vbuf->format.format.b_bits == 5) // RGB565
                {
                    uint8_t val_a1 = 1;
                    uint8_t val_r5 = (val >> 11) & 0x1F;
                    uint8_t val_g6 = (val >> 5) & 0x3F;
                    uint8_t val_b5 = (val >> 0) & 0x1F;

                    uint8_t val_a8 = val_a1 ? 0xFF : 0x0;
                    uint8_t val_r8 = ( val_r5 * 527 + 23 ) >> 6;
                    uint8_t val_g8 = ( val_g6 * 259 + 33 ) >> 6;
                    uint8_t val_b8 = ( val_b5 * 527 + 23 ) >> 6;

                    //uint8_t transparent_r8 = (vbuf->transparent_color >> 16) & 0xFF;
                    //uint8_t transparent_g8 = (vbuf->transparent_color >> 8) & 0xFF;
                    //uint8_t transparent_b8 = (vbuf->transparent_color >> 0) & 0xFF;

                    uint8_t transparent_r5 = (vbuf->transparent_color >> 11) & 0x1F;
                    uint8_t transparent_g6 = (vbuf->transparent_color >> 5) & 0x3F;
                    uint8_t transparent_b5 = (vbuf->transparent_color >> 0) & 0x1F;

                    uint8_t transparent_r8 = ( transparent_r5 * 527 + 23 ) >> 6;
                    uint8_t transparent_g8 = ( transparent_g6 * 259 + 33 ) >> 6;
                    uint8_t transparent_b8 = ( transparent_b5 * 527 + 23 ) >> 6;

                    //
                    if (vbuf->transparent_color && val_r5 == transparent_r5 && val_g6 == transparent_g6 && val_b5 == transparent_b5) {
                        val_a8 = 0;
                        //val_r8 = 0;
                        //val_g8 = 0;
                        //val_b8 = 0;
                    }

                    val_rgba |= (val_a8 << 24);
                    val_rgba |= (val_b8 << 16);
                    val_rgba |= (val_g8 << 8);
                    val_rgba |= (val_r8 << 0);

#if 0
                    val_rgba = 0xFF000000;
                    val_rgba |= (transparent_b8 << 16);
                    val_rgba |= (transparent_g8 << 8);
                    val_rgba |= (transparent_r8 << 0);
#endif
                }
                else if (vbuf->format.format.r_bits == 5 && vbuf->format.format.g_bits == 5 && vbuf->format.format.b_bits == 5) // RGB1555
                {
                    uint8_t val_a1 = (val >> 15);
                    uint8_t val_r5 = (val >> 10) & 0x1F;
                    uint8_t val_g5 = (val >> 5) & 0x1F;
                    uint8_t val_b5 = (val >> 0) & 0x1F;

                    uint8_t val_a8 = val_a1 ? 0xFF : 0x0;
                    uint8_t val_r8 = ( val_r5 * 527 + 23 ) >> 6;
                    uint8_t val_g8 = ( val_g5 * 527 + 23 ) >> 6;
                    uint8_t val_b8 = ( val_b5 * 527 + 23 ) >> 6;

                    uint8_t transparent_a1 = (vbuf->transparent_color >> 15) & 0x1;
                    uint8_t transparent_r5 = (vbuf->transparent_color >> 10) & 0x1F;
                    uint8_t transparent_g5 = (vbuf->transparent_color >> 5) & 0x1F;
                    uint8_t transparent_b5 = (vbuf->transparent_color >> 0) & 0x1F;

                    uint8_t transparent_r8 = ( transparent_r5 * 527 + 23 ) >> 6;
                    uint8_t transparent_g8 = ( transparent_g5 * 527 + 23 ) >> 6;
                    uint8_t transparent_b8 = ( transparent_b5 * 527 + 23 ) >> 6;

#if 0
                    //vbuf->transparent_color && 
                    if (val_a1 == transparent_a1 && val_r5 == transparent_r5 && val_g5 == transparent_g5 && val_b5 == transparent_b5) {
                        val_a8 = 0;
                        //val_r8 = 0;
                        //val_g8 = 0;
                        //val_b8 = 0;
                    }
#endif

                    val_rgba |= (val_a8 << 24);
                    val_rgba |= (val_b8 << 16);
                    val_rgba |= (val_g8 << 8);
                    val_rgba |= (val_r8 << 0);
#if 0
                    val_rgba = 0xFF000000;
                    val_rgba |= (transparent_b8 << 16);
                    val_rgba |= (transparent_g8 << 8);
                    val_rgba |= (transparent_r8 << 0);
#endif
                }
                else {
                    stdPlatform_Printf("wtf is this %u %u %u %u\n", vbuf->format.format.unk_40, vbuf->format.format.r_bits, vbuf->format.format.g_bits, vbuf->format.format.b_bits);
                }
                    
                ((uint32_t*)image_data)[tex_index] = val_rgba;
            }
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

        texture->paDataDepthConverted[mipIdx] = image_data;
    }
    else {
        texture->is_16bit = 0;
#if 1
        uint32_t tex_width, tex_height, tex_row_stride;
        uint32_t row_stride = vbuf->format.width_in_bytes;
        tex_width = width;//vbuf->format.width_in_bytes / 2;
        tex_height = height;
        tex_row_stride = width;

        void* image_data = malloc(tex_width*tex_height*4);
        memset(image_data, 0, tex_width*tex_height*4);

        void* palette_data = texture->palette;//displaypal_data;

        if (!palette_data) 
        {
            palette_data = std3D_ui_colormap.colors;//jkGui_stdBitmaps[2]->palette;
            pal = NULL;//palette_data;
        }
        else {
            pal = NULL;//texture->palette;
        }
    
        for (int j = 0; j < height; j++)
        {
            for (int i = 0; i < width; i++)
            {
                uint32_t index = (j*row_stride) + i;
                uint32_t tex_index = (j*tex_row_stride) + i;
                uint32_t val_rgba = 0x00000000;
                
                if (pal)
                {
                    uint8_t val = image_8bpp[index];
                    val_rgba |= (pal[(val * 3) + 2] << 16);
                    val_rgba |= (pal[(val * 3) + 1] << 8);
                    val_rgba |= (pal[(val * 3) + 0] << 0);
                    val_rgba |= (0xFF << 24);

                    if (!val) {
                        val_rgba = 0;
                    }
                }
                else
                {
                    uint8_t val = image_8bpp[index];
#if 0
                    if (sithWorld_pCurrentWorld && sithWorld_pCurrentWorld->colormaps && sithWorld_pCurrentWorld->colormaps->colors)
                    {
                        rdColor24* pal_master = (rdColor24*)sithWorld_pCurrentWorld->colormaps->colors;//stdDisplay_gammaPalette;
                        rdColor24* color = &pal_master[val];
                        val_rgba |= (color->r << 16);
                        val_rgba |= (color->g << 8);
                        val_rgba |= (color->b << 0);
                        val_rgba |= (0xFF << 24);
                        stdPlatform_Printf("%x %x\n", val_rgba, val);
                    }
                    else {
                        val_rgba = 0xFFFFFFFF; // HACK
                    }
#endif

                    if (palette_data)
                    {
                        uint8_t color_r = ((uint8_t*)palette_data)[(val*3) + 0];
                        uint8_t color_g = ((uint8_t*)palette_data)[(val*3) + 1];
                        uint8_t color_b = ((uint8_t*)palette_data)[(val*3) + 2];

                        val_rgba |= (0xFF << 24);
                        val_rgba |= (color_b << 16);
                        val_rgba |= (color_g << 8);
                        val_rgba |= (color_r << 0);
                    }
                    else {
                        val_rgba = 0xFFFFFF00; // HACK
                    }
                    

                    if (!val) {
                        val_rgba = 0;
                    }
                }
                
                ((uint32_t*)image_data)[tex_index] = val_rgba;
            }
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

        texture->paDataDepthConverted[mipIdx] = image_data;
#endif
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image_8bpp);
    }
    
    std3D_aUIBitmaps[cacheIdx] = texture;
    std3D_aUITextures[cacheIdx] = image_texture;
    std3D_loadedUITexturesAmt++;
    /*ext->surfacebuf = image_data;
    ext->surfacetex = image_texture;
    ext->surfacepaltex = pal_texture;*/
    
    texture->aTextureIds[mipIdx] = image_texture;
    texture->abLoadedToGPU[mipIdx] = 1;

    glBindTexture(GL_TEXTURE_2D, blank_tex);

    return 1;
}

void std3D_UpdateFrameCount(rdDDrawSurface *surface)
{
}

// Added helpers
void std3D_UpdateSettings()
{
    jk_printf("Updating texture cache...\n");
    for (int i = 0; i < STD3D_MAX_TEXTURES; i++)
    {
        rdDDrawSurface* tex = std3D_aLoadedSurfaces[i];
        if (!tex) continue;

        if (!std3D_aLoadedTextures[i]) continue;
        glBindTexture(GL_TEXTURE_2D, std3D_aLoadedTextures[i]);

		if (jkPlayer_enableTextureFilter && tex->is_16bit)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		}

        if (tex->emissive_texture_id != 0) {
            glBindTexture(GL_TEXTURE_2D, tex->emissive_texture_id);
            
            if (jkPlayer_enableTextureFilter)
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            }
            else
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            }
        }

        if (tex->displacement_texture_id != 0) {
            glBindTexture(GL_TEXTURE_2D, tex->displacement_texture_id);
            
            if (jkPlayer_enableTextureFilter)
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            }
            else
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            }
        }

		if (tex->specular_texture_id != 0)
		{
			glBindTexture(GL_TEXTURE_2D, tex->specular_texture_id);

			if (jkPlayer_enableTextureFilter)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			}
			else
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			}
		}
    }

    glBindTexture(GL_TEXTURE_2D, blank_tex);
}

// Added
void std3D_Screenshot(const char* pFpath)
{
#ifdef TARGET_CAN_JKGM
    //if (!std3D_pFb) return;

    uint8_t* data = malloc(window.w * window.h * 3 * sizeof(uint8_t));
    glBindFramebuffer(GL_FRAMEBUFFER, window.fbo);
    glReadPixels(0, 0, window.w, window.h, GL_RGB, GL_UNSIGNED_BYTE, data);
    jkgm_write_png(pFpath, window.w, window.h, data);
    free(data);
#endif
}

int std3D_HasAlpha()
{
    return 1;
}

int std3D_HasModulateAlpha()
{
    return 1;
}

int std3D_HasAlphaFlatStippled()
{
    return 1;
}

void std3D_PurgeBitmapRefs(stdBitmap *pBitmap)
{
    for (int i = 0; i < STD3D_MAX_TEXTURES; i++)
    {
        int texId = std3D_aUITextures[i];
        stdBitmap* tex = std3D_aUIBitmaps[i];
        if (!tex) continue;
        if (tex != pBitmap) continue;

        for (int j = 0; j < tex->numMips; j++)
        {
            if (tex->aTextureIds[j] == texId) {
                std3D_PurgeUIEntry(i, j);
                break;
            }
        }
    }
}

void std3D_PurgeSurfaceRefs(rdDDrawSurface *texture)
{
    for (int i = 0; i < STD3D_MAX_TEXTURES; i++)
    {
        rdDDrawSurface* tex = std3D_aLoadedSurfaces[i];
        if (!tex) continue;
        if (tex != texture) continue;

        std3D_PurgeTextureEntry(i);
    }
}

void std3D_PurgeTextureEntry(int i) {
    if (std3D_aLoadedTextures[i]) {
        glDeleteTextures(1, &std3D_aLoadedTextures[i]);
        std3D_aLoadedTextures[i] = 0;
    }

    rdDDrawSurface* tex = std3D_aLoadedSurfaces[i];
    if (!tex) return;

    if (tex->pDataDepthConverted != NULL) {
        free(tex->pDataDepthConverted);
        tex->pDataDepthConverted = NULL;
    }

    if (tex->albedo_data != NULL) {
        //jkgm_aligned_free(tex->albedo_data);
        tex->albedo_data = NULL;
    }

    if (tex->emissive_data != NULL) {
        //jkgm_aligned_free(tex->emissive_data);
        tex->emissive_data = NULL;
    }

    if (tex->displacement_data != NULL) {
        //jkgm_aligned_free(tex->displacement_data);
        tex->displacement_data = NULL;
    }

    if (tex->emissive_texture_id != 0) {
        glDeleteTextures(1, &tex->emissive_texture_id);
        tex->emissive_texture_id = 0;
    }

	if (tex->specular_texture_id != 0) {
		glDeleteTextures(1, &tex->specular_texture_id);
		tex->specular_texture_id = 0;
	}

    if (tex->displacement_texture_id != 0) {
        glDeleteTextures(1, &tex->displacement_texture_id);
        tex->displacement_texture_id = 0;
    }

    tex->emissive_factor[0] = 0.0;
    tex->emissive_factor[1] = 0.0;
    tex->emissive_factor[2] = 0.0;
    tex->albedo_factor[0] = 1.0;
    tex->albedo_factor[1] = 1.0;
    tex->albedo_factor[2] = 1.0;
    tex->albedo_factor[3] = 1.0;
    tex->displacement_factor = 0.0;

    tex->texture_loaded = 0;
    tex->texture_id = 0;

    std3D_aLoadedSurfaces[i] = NULL;
}

void std3D_PurgeUIEntry(int i, int idx) {
    if (std3D_aUITextures[i]) {
        glDeleteTextures(1, &std3D_aUITextures[i]);
        std3D_aUITextures[i] = 0;
    }

    stdBitmap* tex = std3D_aUIBitmaps[i];
    if (!tex) return;

    tex->abLoadedToGPU[idx] = 0;
    tex->aTextureIds[idx] = 0;
    free(tex->paDataDepthConverted[idx]);
    tex->paDataDepthConverted[idx] = NULL;

    std3D_aUIBitmaps[i] = NULL;
    std3D_loadedUITexturesAmt--;
}

void std3D_PurgeTextureCache()
{
    if (Main_bHeadless) {
        std3D_loadedTexturesAmt = 0;
        return;
    }

    if (!std3D_loadedTexturesAmt) {
        jk_printf("Skipping texture cache purge, nothing loaded.\n");
        return;
    }

    jk_printf("Purging texture cache... %x\n", std3D_loadedTexturesAmt);
    for (int i = 0; i < std3D_loadedTexturesAmt; i++)
    {
        std3D_PurgeTextureEntry(i);
    }
    std3D_loadedTexturesAmt = 0;

	std3D_PurgeDecalAtlas();
}

void std3D_InitializeViewport(rdRect *viewRect)
{
    std3D_rectViewIdk.x = viewRect->x;
    std3D_rectViewIdk.y = viewRect->y;
    std3D_rectViewIdk.width = viewRect->width;
	std3D_rectViewIdk.height = viewRect->height;

	// this looks like some kind of viewport matrix?
    memset(std3D_aViewIdk, 0, sizeof(std3D_aViewIdk));
    std3D_aViewIdk[0] = (float)std3D_rectViewIdk.x;
    std3D_aViewIdk[1] = (float)std3D_rectViewIdk.y;
	std3D_aViewIdk[8] = std3D_aViewIdk[16] = (float)(viewRect->width + std3D_rectViewIdk.x);
	std3D_aViewIdk[9] = std3D_aViewIdk[1];
	std3D_aViewIdk[17] = std3D_aViewIdk[25] = (float)(viewRect->height + std3D_rectViewIdk.y);
	std3D_aViewIdk[24] = std3D_aViewIdk[0];

	// this looks like some kind of screen quad?
	std3D_aViewTris[0].v1 = 0;
    std3D_aViewTris[0].v2 = 1;
    std3D_aViewTris[0].texture = 0;
    std3D_aViewTris[0].v3 = 2;
    std3D_aViewTris[0].flags = 0x8200;
    std3D_aViewTris[1].v1 = 0;
    std3D_aViewTris[1].v2 = 2;
    std3D_aViewTris[1].v3 = 3;
    std3D_aViewTris[1].texture = 0;
    std3D_aViewTris[1].flags = 0x8200;
}

int std3D_GetValidDimensions(int a1, int a2, int a3, int a4)
{
    int result; // eax

    std3D_gpuMaxTexSizeMaybe = a1;
    result = a4;
    std3D_dword_53D66C = a2;
    std3D_dword_53D670 = a3;
    std3D_dword_53D674 = a4;
    return result;
}

int std3D_FindClosestDevice(uint32_t index, int a2)
{
    return 0;
}

int std3D_SetRenderList(intptr_t a1)
{
    std3D_renderList = a1;
    return std3D_CreateExecuteBuffer();
}

intptr_t std3D_GetRenderList()
{
    return std3D_renderList;
}

int std3D_CreateExecuteBuffer()
{
    return 1;
}

int std3D_IsReady()
{
    return has_initted;
}

void std3D_DrawDecal(stdVBuffer* vbuf, rdDDrawSurface* texture, rdVector3* verts, rdMatrix44* decalMatrix, rdVector3* color, uint32_t flags, float angleFade)
{
	if (Main_bHeadless) return;

	if (decalUniforms.numDecals >= CLUSTER_MAX_DECALS)
		return;

	decalsDirty = 1;
	std3D_renderPasses[0].clustersDirty = std3D_renderPasses[1].clustersDirty = std3D_renderPasses[2].clustersDirty = 1;

	rdRect uvScaleBias;
	if (!std3D_InsertDecalTexture(&uvScaleBias, vbuf, texture))
		return;

	std3D_decal* decal = &decalUniforms.tmpDecals[decalUniforms.numDecals++];
	decal->uvScaleBias.x = (float)uvScaleBias.x / DECAL_ATLAS_SIZE;
	decal->uvScaleBias.y = (float)uvScaleBias.y / DECAL_ATLAS_SIZE;
	decal->uvScaleBias.z = (float)uvScaleBias.width / DECAL_ATLAS_SIZE;
	decal->uvScaleBias.w = (float)uvScaleBias.height / DECAL_ATLAS_SIZE;
	rdVector_Copy3((rdVector3*)&decal->posRad, (rdVector3*)&decalMatrix->vD);

	//rdVector3 diag;
	//diag.x = decalMatrix->vA.x;
	//diag.y = decalMatrix->vB.y;
	//diag.z = decalMatrix->vC.z;
	//decal->posRad.w = rdVector_Len3(&diag);
	decal->posRad.w = rdVector_Len3(verts) * 0.5f;

	rdMatrix_Copy44(&decal->decalMatrix, decalMatrix);
	rdMatrix_Invert44(&decal->invDecalMatrix, decalMatrix);

	rdVector_Copy3((rdVector3*)&decal->color, color);
	decal->color.w = 1.0f;
	decal->flags = flags;
	decal->angleFade = angleFade;
}

void std3D_DrawOccluder(rdVector3* position, float radius, rdVector3* verts)
{
	if (Main_bHeadless) return;

	if (occluderUniforms.numOccluders >= CLUSTER_MAX_OCCLUDERS)
		return;

	occludersDirty = 1;
	std3D_renderPasses[0].clustersDirty = std3D_renderPasses[1].clustersDirty = std3D_renderPasses[2].clustersDirty = 1;

	std3D_occluder* occ = &occluderUniforms.tmpOccluders[occluderUniforms.numOccluders++];
	occ->position.x = position->x;
	occ->position.y = position->y;
	occ->position.z = position->z;
	occ->position.w = radius;
}

void std3D_BlitFramebuffer(int x, int y, int width, int height, void* pixels)
{
	glBindFramebuffer(GL_FRAMEBUFFER, std3D_framebuffer.fbo);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

void std3D_SetRenderPass(const char* name, int8_t renderPass, rdRenderPassFlags_t renderPassFlags)
{
	strcpy_s(std3D_renderPasses[renderPass].name, 32, name);
	std3D_renderPasses[renderPass].flags = renderPassFlags;
}

void std3D_SetDepthRange(int8_t renderPass, float znearNorm, float zfarNorm)
{
	std3D_renderPasses[renderPass].depthRange.x = znearNorm;
	std3D_renderPasses[renderPass].depthRange.y = zfarNorm;
}

int std3D_HasDepthWrites(std3D_DrawCallState* pState)
{
	return pState->stateBits.zMethod == RD_ZBUFFER_READ_WRITE || pState->stateBits.zMethod == RD_ZBUFFER_NOREAD_WRITE;
}

GLuint std3D_PrimitiveForGeoMode(rdGeoMode_t geoMode)
{
	switch (geoMode)
	{
	case RD_GEOMODE_VERTICES:
		return GL_POINTS;
	case RD_GEOMODE_WIREFRAME:
		return GL_LINES;
	case RD_GEOMODE_SOLIDCOLOR:
	case RD_GEOMODE_TEXTURED:
	case RD_GEOMODE_NOTRENDERED:
	default:
		return GL_TRIANGLES;
	}
}

uint64_t std3D_GetSortKey(std3D_DrawCall* pDrawCall)
{
	int textureID = pDrawCall->state.textureState.pTexture ? pDrawCall->state.textureState.pTexture->texture_id : 0;

	uint64_t offset = 0ull;
	
	uint64_t sortKey = 0;
	sortKey |= pDrawCall->state.stateBits.data & 0xFFFFFFFF;
	sortKey |= ((uint64_t)textureID & 0xFFFF) << 32ull;
	sortKey |= ((uint64_t)pDrawCall->shaderID & 0x4) << 48ull;
	sortKey |= ((uint64_t)pDrawCall->state.header.sortOrder & 0xFF) << 52ull;

	return sortKey;
}

int std3D_GetShaderID(std3D_DrawCallState* pState)
{
	int alphaTest = pState->stateBits.alphaTest & 1;
	int blending  = pState->stateBits.blend & 1;
	int lighting  = pState->stateBits.lightMode >= RD_LIGHTMODE_DIFFUSE;
	int specular  = pState->stateBits.lightMode == RD_LIGHTMODE_SPECULAR;

	// todo: clean this up by using some array indexing or something
	if (blending)
		return SHADER_COLOR_ALPHABLEND_UNLIT + lighting + specular;

	if (alphaTest)
		return SHADER_COLOR_ALPHATEST_UNLIT + lighting + specular;

	return SHADER_COLOR_UNLIT + lighting + specular;
}

void std3D_AddListVertices(std3D_DrawCall* pDrawCall, rdPrimitiveType_t type, std3D_DrawCallList* pList, rdVertex* paVertices, int numVertices)
{
	int firstIndex = pList->drawCallIndexCount;
	int firstVertex = pList->drawCallVertexCount;

	// copy the vertices
	memcpy(&pList->drawCallVertices[firstVertex], paVertices, sizeof(rdVertex) * numVertices);
	pList->drawCallVertexCount += numVertices;

	// generate indices
	if (numVertices <= 3)
	{
		// single triangle fast path
		pList->drawCallIndices[pList->drawCallIndexCount++] = firstVertex + 0;
		pList->drawCallIndices[pList->drawCallIndexCount++] = firstVertex + 1;
		pList->drawCallIndices[pList->drawCallIndexCount++] = firstVertex + 2;
	}
	else if (type == RD_PRIMITIVE_TRIANGLES)
	{
		// generate triangle indices directly
		int tris = numVertices / 3;
		for (int i = 0; i < tris; ++i)
		{
			pList->drawCallIndices[pList->drawCallIndexCount++] = firstVertex + i * 3 + 0;
			pList->drawCallIndices[pList->drawCallIndexCount++] = firstVertex + i * 3 + 1;
			pList->drawCallIndices[pList->drawCallIndexCount++] = firstVertex + i * 3 + 2;
		}
	}
	else if (type == RD_PRIMITIVE_TRIANGLE_FAN)
	{
		// build indices from a single corner vertex
		int verts = numVertices - 2;
		for (int i = 0; i < verts; i++)
		{
			pList->drawCallIndices[pList->drawCallIndexCount++] = firstVertex + 0;
			pList->drawCallIndices[pList->drawCallIndexCount++] = firstVertex + i + 1;
			pList->drawCallIndices[pList->drawCallIndexCount++] = firstVertex + i + 2;
		}
	}
	else if (type == RD_PRIMITIVE_POLYGON)
	{
		// build indices through simple triangulation
		int verts = numVertices - 2;
		int i1 = 0;
		int i2 = 1;
		int i3 = numVertices - 1;
		for (int i = 0; i < verts; ++i)
		{
			pList->drawCallIndices[pList->drawCallIndexCount++] = firstVertex + i1;
			pList->drawCallIndices[pList->drawCallIndexCount++] = firstVertex + i2;
			pList->drawCallIndices[pList->drawCallIndexCount++] = firstVertex + i3;
			if ((i & 1) != 0)
				i1 = i3--;
			else
				i1 = i2++;
		}
	}

	pDrawCall->firstIndex = firstIndex;
	pDrawCall->numIndices = pList->drawCallIndexCount - firstIndex;
}

void std3D_AddListDrawCall(rdPrimitiveType_t type, std3D_DrawCallList* pList, std3D_DrawCallState* pDrawCallState, int shaderID, rdVertex* paVertices, int numVertices)
{
	if (pList->drawCallCount + 1 > STD3D_MAX_DRAW_CALLS)
		return; // todo: flush here?

	if (pList->drawCallVertexCount + numVertices > STD3D_MAX_DRAW_CALL_VERTS)
		return; // todo: flush here?

	std3D_DrawCall* pDrawCall = &pList->drawCalls[pList->drawCallCount++];
	pDrawCall->sortKey = std3D_GetSortKey(pDrawCallState);
	pDrawCall->state = *pDrawCallState;
	pDrawCall->shaderID = shaderID;

	std3D_AddListVertices(pDrawCall, type, pList, paVertices, numVertices);
}

void std3D_AddZListDrawCall(rdPrimitiveType_t type, std3D_DrawCallList* pList, std3D_DrawCallState* pDrawCallState, rdVertex* paVertices, int numVertices)
{
	if (pList->drawCallCount + 1 > STD3D_MAX_DRAW_CALLS)
		return; // todo: flush here?

	if (pList->drawCallVertexCount + numVertices > STD3D_MAX_DRAW_CALL_VERTS)
		return; // todo: flush here?

	std3D_DrawCall* pDrawCall = &pList->drawCalls[pList->drawCallCount++];
	pDrawCall->state = *pDrawCallState;
	pDrawCall->shaderID = pDrawCallState->stateBits.alphaTest ? SHADER_DEPTH_ALPHATEST : SHADER_DEPTH;

	// z lists can ignore blending, fog, lighting and possibly textures
	pDrawCall->state.stateBits.fogMode = 0;
	pDrawCall->state.stateBits.blend = 0;
	pDrawCall->state.stateBits.srdBlend = 1;
	pDrawCall->state.stateBits.dstBlend = 0;
	pDrawCall->state.stateBits.lightMode = 0;

	memset(&pDrawCall->state.fogState, 0, sizeof(std3D_FogState));
	memset(&pDrawCall->state.lightingState, 0, sizeof(std3D_LightingState));
	if(!pDrawCallState->stateBits.alphaTest)// && pDrawCallState->stateBits.texGen == RD_TEXGEN_NONE) // non-alpha test with no texgen doesn't require textures
	{
		pDrawCall->state.stateBits.geoMode = 2;
		pDrawCall->state.stateBits.ditherMode = 0;
		pDrawCall->state.stateBits.texMode = 0;
		pDrawCall->state.stateBits.texGen = 0;
		pDrawCall->state.stateBits.texFilter = 0;
		pDrawCall->state.stateBits.alphaTest = 0;
		pDrawCall->state.stateBits.chromaKey = 0;
		memset(&pDrawCall->state.textureState, 0, sizeof(std3D_TextureState));
		memset(&pDrawCall->state.materialState, 0, sizeof(std3D_MaterialState));
	}
	
	pDrawCall->sortKey = std3D_GetSortKey(&pDrawCall->state);

	std3D_AddListVertices(pDrawCall, type, pList, paVertices, numVertices);
}

void std3D_AddDrawCall(rdPrimitiveType_t type, std3D_DrawCallState* pDrawCallState, rdVertex* paVertices, int numVertices)
{
	if (Main_bHeadless)
		return;

	int renderPass = stdMath_ClampInt(pDrawCallState->header.renderPass, 0, STD3D_MAX_RENDER_PASSES - 1);

	int alphaTest = pDrawCallState->stateBits.alphaTest & 1;
	int blending = pDrawCallState->stateBits.blend & 1;
	int lighting = pDrawCallState->stateBits.lightMode >= RD_LIGHTMODE_DIFFUSE;
	int specular = pDrawCallState->stateBits.lightMode == RD_LIGHTMODE_SPECULAR;
	
	int writesZ = std3D_HasDepthWrites(pDrawCallState);
	int isWater = pDrawCallState->stateBits.texGen == RD_TEXGEN_WATER;

	int listIndex;
	int shaderID;

	if (blending && isWater)
	{
		std3D_AddZListDrawCall(type, &std3D_renderPasses[renderPass].drawCallLists[DRAW_LIST_Z_REFRACTION], pDrawCallState, paVertices, numVertices);
		
		// water surfaces are refractive and split the render into 2
		// in order for this to work correctly, they MUST write Z so that the content above them clips correctly
		if(pDrawCallState->stateBits.zMethod == RD_ZBUFFER_NOREAD_NOWRITE)
			pDrawCallState->stateBits.zMethod = RD_ZBUFFER_NOREAD_WRITE;
		else if (pDrawCallState->stateBits.zMethod == RD_ZBUFFER_READ_NOWRITE)
			pDrawCallState->stateBits.zMethod = RD_ZBUFFER_READ_WRITE;

		// since the surface will be refractive/read the refraction buffer for composite, we can disable actual alpha blending
		pDrawCallState->stateBits.blend = 0;
		pDrawCallState->stateBits.srdBlend = 1;
		pDrawCallState->stateBits.dstBlend = 0;

		// use simple lighting on the refractive surfaces for now
		pDrawCallState->stateBits.lightMode = RD_LIGHTMODE_DIFFUSE;

		shaderID = SHADER_COLOR_REFRACTION;
		listIndex = DRAW_LIST_COLOR_REFRACTION;
	}
	else if (!blending && writesZ && !alphaTest) // add to z-prepass if applicable
	{
		std3D_AddZListDrawCall(type, &std3D_renderPasses[renderPass].drawCallLists[DRAW_LIST_Z + alphaTest], pDrawCallState, paVertices, numVertices);

		// the forward pass can now do a simple equal test with no writes
		pDrawCallState->header.sortOrder            = 0; // render first
		pDrawCallState->stateBits.zCompare          = RD_COMPARE_EQUAL;
		pDrawCallState->stateBits.zMethod           = RD_ZBUFFER_READ_NOWRITE;
		pDrawCallState->stateBits.alphaTest         = 0;
		pDrawCallState->stateBits.chromaKey         = 0;
		pDrawCallState->textureState.alphaRef       = 0;
		pDrawCallState->textureState.chromaKeyColor = 0;

		shaderID  = SHADER_COLOR_UNLIT + lighting + specular;
		listIndex = DRAW_LIST_COLOR_ZPREPASS;
	}
	else	
	{
		shaderID = std3D_GetShaderID(pDrawCallState);
		
		//pDrawCallState->stateBits.blend = 0;
		//pDrawCallState->stateBits.srdBlend = 1;
		//pDrawCallState->stateBits.dstBlend = 0;

		listIndex = blending ? DRAW_LIST_COLOR_ALPHABLEND : DRAW_LIST_COLOR_NOZPREPASS;
	}

	std3D_AddListDrawCall(type, &std3D_renderPasses[renderPass].drawCallLists[listIndex], pDrawCallState, shaderID, paVertices, numVertices);
}

void std3D_ResetDrawCalls()
{
	for(int j = 0; j < STD3D_MAX_RENDER_PASSES; ++j)
	{
		std3D_RenderPass* pRenderPass = &std3D_renderPasses[j];
		for (int i = 0; i < DRAW_LIST_COUNT; ++i)
		{
			std3D_DrawCallList* pList = &pRenderPass->drawCallLists[i];
			pList->bSorted = 0;
			pList->drawCallCount = 0;
			pList->drawCallIndexCount = 0;
			pList->drawCallVertexCount = 0;

			std3D_DrawCall* drawCalls = pList->drawCalls;
			std3D_DrawCall** drawCallPtrs = pList->drawCallPtrs;

			for (int k = 0; k < STD3D_MAX_DRAW_CALLS; ++k)
				drawCallPtrs[k] = &drawCalls[k];
		}
	}
}

void std3D_UpdateSharedUniforms()
{
	std3D_SharedUniforms uniforms;

	// uniforms shared across draw lists during flush
	uniforms.timeSeconds = sithTime_curSeconds;

	extern float rdroid_overbright;
	uniforms.lightMult = 1.0f / rdroid_overbright;//jkGuiBuildMulti_bRendering ? 0.85 : (jkPlayer_enableBloom ? 0.9 : 0.85);
	
	rdVector_Set2(&uniforms.clusterTileSizes, (float)tileSizeX, (float)tileSizeY);
	rdVector_Set2(&uniforms.clusterScaleBias, sliceScalingFactor, sliceBiasFactor);	
	rdVector_Set2(&uniforms.resolution, std3D_framebuffer.w, std3D_framebuffer.h);

	extern rdVector4 rdroid_sgBasis[8]; //eww
	memcpy(uniforms.sgBasis, rdroid_sgBasis, sizeof(rdVector4)*8);

	float mipScale = 1.0 / rdCamera_GetMipmapScalar();
	rdVector_Set4(&uniforms.mipDistances, mipScale * rdroid_aMipDistances.x, mipScale * rdroid_aMipDistances.y, mipScale * rdroid_aMipDistances.z, mipScale * rdroid_aMipDistances.w);

	// this one isn't really used so let's store the bias in it
	uniforms.mipDistances.w = (float)jkPlayer_mipBias;

	glBindBuffer(GL_UNIFORM_BUFFER, shared_ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(std3D_SharedUniforms), &uniforms);	
}

int std3D_DrawCallCompareSortKey(std3D_DrawCall** a, std3D_DrawCall** b)
{
	if ((*a)->sortKey > (*b)->sortKey)
		return 1;
	if ((*a)->sortKey < (*b)->sortKey)
		return -1;
	return 0;
}

int std3D_DrawCallCompareDepth(std3D_DrawCall** a, std3D_DrawCall** b)
{
	if ((*a)->state.header.sortDistance > (*b)->state.header.sortDistance)
		return 1;
	if ((*a)->state.header.sortDistance < (*b)->state.header.sortDistance)
		return -1;
	return 0;
}

// todo: track state bits and only apply necessary changes
void std3D_SetRasterState(std3D_worldStage* pStage, std3D_DrawCallState* pState)
{
	std3D_RasterState* pRasterState = &pState->rasterState;

	glViewport(pRasterState->viewport.x, pRasterState->viewport.y, pRasterState->viewport.width, pRasterState->viewport.height);
	if(pState->stateBits.scissorMode == RD_SCISSOR_ENABLED)
		glEnable(GL_SCISSOR_TEST);
	else
		glDisable(GL_SCISSOR_TEST);
	glScissor(pRasterState->scissor.x, pRasterState->scissor.y, pRasterState->scissor.width, pRasterState->scissor.height);

	if(pState->stateBits.cullMode == RD_CULL_MODE_NONE)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);

	//glFrontFace(pState->stateBits.cullMode == RD_CULL_MODE_CW_ONLY ? GL_CW : GL_CCW);
	glFrontFace(GL_CCW);
	glCullFace(pState->stateBits.cullMode == RD_CULL_MODE_BACK ? GL_BACK : GL_FRONT);

	glUniform1i(pStage->uniform_geo_mode, pState->stateBits.geoMode + 1);

	if(pState->stateBits.ditherMode > 0)
		std3D_bindTexture(GL_TEXTURE_2D, dither_texture, TEX_SLOT_DITHER);
	else
		std3D_bindTexture(GL_TEXTURE_2D, blank_tex, TEX_SLOT_DITHER);

	std3D_bindTexture(GL_TEXTURE_2D, deferred.tex, TEX_SLOT_DIFFUSE_LIGHT);
}

void std3D_SetFogState(std3D_worldStage* pStage, std3D_DrawCallState* pState)
{
	std3D_FogUniforms fog;
	fog.fogEnabled = pState->stateBits.fogMode;
	fog.fogStartDepth = pState->fogState.startDepth;
	fog.fogEndDepth = pState->fogState.endDepth;
	fog.fogAnisotropy = pState->fogState.anisotropy;
	fog.fogLightDir.x = pState->fogState.lightDir.x;
	fog.fogLightDir.y = pState->fogState.lightDir.y;
	fog.fogLightDir.z = pState->fogState.lightDir.z;
	fog.fogLightDir.w = rdVector_Normalize3Acc((rdVector3*)&fog.fogLightDir) > 0.0;

	float a = ((pState->fogState.color >> 24) & 0xFF) / 255.0f;
	float r = ((pState->fogState.color >> 16) & 0xFF) / 255.0f;
	float g = ((pState->fogState.color >> 8) & 0xFF) / 255.0f;
	float b = ((pState->fogState.color >> 0) & 0xFF) / 255.0f;
	rdVector_Set4(&fog.fogColor, r, g, b, a);

	glBindBuffer(GL_UNIFORM_BUFFER, fog_ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(std3D_FogUniforms), &fog);
}

int std3D_SetBlendState(std3D_worldStage* pStage, std3D_DrawCallState* pState)
{
	if (pState->stateBits.blend == 0)
		glDisable(GL_BLEND);
	else
		glEnable(GL_BLEND);


	static GLuint glBlendForRdBlend[] =
	{
		GL_ZERO,                // RD_BLEND_ZERO
		GL_ONE,                 // RD_BLEND_ONE
		GL_DST_COLOR,           // RD_BLEND_DSTCOLOR
		GL_ONE_MINUS_DST_COLOR, // RD_BLEND_INVDSTCOLOR
		GL_SRC_ALPHA,           // RD_BLEND_SRCALPHA
		GL_ONE_MINUS_SRC_ALPHA, // RD_BLEND_INVSRCALPHA
		GL_DST_ALPHA,           // RD_BLEND_DSTALPHA
		GL_ONE_MINUS_DST_ALPHA, // RD_BLEND_INVDSTALPHA
	};
	glBlendFunc(glBlendForRdBlend[pState->stateBits.srdBlend], glBlendForRdBlend[pState->stateBits.dstBlend]);
}

void std3D_SetDepthStencilState(std3D_DrawCallState* pState)
{
	if (pState->stateBits.zMethod == RD_ZBUFFER_NOREAD_NOWRITE)
	{
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
	else if (pState->stateBits.zMethod == RD_ZBUFFER_READ_NOWRITE)
	{
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
	else if (pState->stateBits.zMethod == RD_ZBUFFER_NOREAD_WRITE)
	{
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
	}
	else
	{
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
	}

	static const GLuint gl_compares[] =
	{
		GL_ALWAYS,
		GL_LESS,
		GL_LEQUAL,
		GL_GREATER,
		GL_GEQUAL,
		GL_EQUAL,
		GL_NOTEQUAL,
		GL_NEVER
	};
	glDepthFunc(gl_compares[pState->stateBits.zCompare]);
}

void std3D_SetTransformState(std3D_worldStage* pStage, std3D_DrawCallState* pState)
{
	renderPassProj = &pState->transformState.proj;

	float fov = 2.0 * atanf(1.0 / renderPassProj->vC.y);
	float aspect = renderPassProj->vC.y / renderPassProj->vA.x;
	float znear = -renderPassProj->vD.z / (renderPassProj->vB.z + 1.0f);
	float zfar = -renderPassProj->vD.z / (renderPassProj->vB.z - 1.0f);

	float T = znear * tanf(0.5 * fov);
	float R = aspect * T;

	glUniformMatrix4fv(pStage->uniform_projection, 1, GL_FALSE, (float*)renderPassProj);
	glUniform2f(pStage->uniform_rightTop, R, T);

	glUniformMatrix4fv(pStage->uniform_viewMatrix, 1, GL_FALSE, (float*)&pState->transformState.view);
	glUniformMatrix4fv(pStage->uniform_modelMatrix, 1, GL_FALSE, (float*)&pState->transformState.modelView);
}

void std3D_SetTextureState(std3D_worldStage* pStage, std3D_DrawCallState* pState)
{
	std3D_TextureState* pTexState = &pState->textureState;
	rdDDrawSurface* pTexture = (pState->stateBits.geoMode + 1 == RD_GEOMODE_TEXTURED) ? pState->textureState.pTexture : NULL;

	std3D_TextureUniforms tex;
	tex.uv_mode = pState->stateBits.texMode;
	tex.texgen = pState->stateBits.texGen;
	tex.numMips = pTexState->numMips;
	rdVector_Set2(&tex.uv_offset, pTexState->texOffset.x, pTexState->texOffset.y);
	rdVector_Set4(&tex.texgen_params, pTexState->texGenParams.x, pTexState->texGenParams.y, pTexState->texGenParams.z, pTexState->texGenParams.w);		

	if(pTexture)
	{
		int tex_id = pTexture->texture_id ? pTexture->texture_id : blank_tex_white;
		glActiveTexture(GL_TEXTURE0 + TEX_SLOT_DIFFUSE);
		glBindTexture(GL_TEXTURE_2D, tex_id);

		int clampX = (pTexState->flags & RD_FF_TEX_CLAMP_X) ? 1 : 0;
		int clampY = (pTexState->flags & RD_FF_TEX_CLAMP_Y) ? 1 : 0;

		if ((pState->stateBits.texFilter == RD_TEXFILTER_BILINEAR))// && pTexture->is_16bit)
			glBindSampler(TEX_SLOT_DIFFUSE, linearSampler[clampX + clampY * 2]);
		else
			glBindSampler(TEX_SLOT_DIFFUSE, nearestSampler[clampX + clampY * 2]);

		int emiss_tex_id = pTexture->emissive_texture_id;
		glActiveTexture(GL_TEXTURE0 + TEX_SLOT_EMISSIVE);
		glBindTexture(GL_TEXTURE_2D, emiss_tex_id ? emiss_tex_id : blank_tex);
		
		if ((pState->stateBits.texFilter == RD_TEXFILTER_BILINEAR))// && pTexture->is_16bit)
			glBindSampler(TEX_SLOT_EMISSIVE, linearSampler[clampX + clampY * 2]);
		else
			glBindSampler(TEX_SLOT_EMISSIVE, nearestSampler[clampX + clampY * 2]);

		int displace_tex_id = pTexture->displacement_texture_id;
		glActiveTexture(GL_TEXTURE0 + TEX_SLOT_DISPLACEMENT);
		glBindTexture(GL_TEXTURE_2D, displace_tex_id ? displace_tex_id : blank_tex);
		
		//if ((pState->stateBits.texFilter == RD_TEXFILTER_BILINEAR))// && pTexture->is_16bit)
			glBindSampler(TEX_SLOT_DISPLACEMENT, linearSampler[clampX + clampY * 2]);
		//else
		//	glBindSampler(TEX_SLOT_DISPLACEMENT, nearestSampler[clampX + clampY * 2]);

		int spec_tex_id = pTexture->specular_texture_id;
		glActiveTexture(GL_TEXTURE0 + TEX_SLOT_TEX3);
		glBindTexture(GL_TEXTURE_2D, spec_tex_id ? spec_tex_id : blank_tex);

		if ((pState->stateBits.texFilter == RD_TEXFILTER_BILINEAR))
			glBindSampler(TEX_SLOT_EMISSIVE, linearSampler[clampX + clampY * 2]);
		else
			glBindSampler(TEX_SLOT_EMISSIVE, nearestSampler[clampX + clampY * 2]);


		glActiveTexture(GL_TEXTURE0 + 0);



		if (tex_id == 0)
		{
			tex.tex_mode = TEX_MODE_TEST;
		}
		else
		{
			if (/*!jkPlayer_enableTextureFilter || */ (pState->stateBits.texFilter == RD_TEXFILTER_NEAREST))
				tex.tex_mode = pTexture->is_16bit ? TEX_MODE_16BPP : TEX_MODE_WORLDPAL;
			else
				tex.tex_mode = pTexture->is_16bit ? TEX_MODE_BILINEAR_16BPP : TEX_MODE_BILINEAR;
		}

		rdVector_Set2(&tex.texsize, pTexture->width, pTexture->height);
	}
	else
	{
		glActiveTexture(GL_TEXTURE0 + TEX_SLOT_EMISSIVE);
		glBindTexture(GL_TEXTURE_2D, blank_tex); // emissive
		glActiveTexture(GL_TEXTURE0 + TEX_SLOT_DISPLACEMENT);
		glBindTexture(GL_TEXTURE_2D, blank_tex); // displace
		glActiveTexture(GL_TEXTURE0 + TEX_SLOT_DIFFUSE);
		glBindTexture(GL_TEXTURE_2D, blank_tex_white);
		glActiveTexture(GL_TEXTURE0);

		tex.tex_mode = TEX_MODE_TEST;
		rdVector_Set2(&tex.texsize, 1, 1);
	}

	glBindBuffer(GL_UNIFORM_BUFFER, tex_ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(std3D_TextureUniforms), &tex);
}

#define RD_SET_COLOR4(target, color) \
	rdVector_Set4(target, ((color >> 16) & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f, ((color >> 0) & 0xFF) / 255.0f, ((color >> 24) & 0xFF) / 255.0f);

void std3D_SetMaterialState(std3D_worldStage* pStage, std3D_DrawCallState* pState)
{
	std3D_MaterialState* pMaterialState = &pState->materialState;
	rdDDrawSurface* pTexture = (pState->stateBits.geoMode + 1 == RD_GEOMODE_TEXTURED) ? pState->textureState.pTexture : NULL;

	std3D_MaterialUniforms tex;
	RD_SET_COLOR4(&tex.fillColor, pMaterialState->fillColor);

	//if (pTexture)
	{
		RD_SET_COLOR4(&tex.emissive_factor, pMaterialState->emissive);
		RD_SET_COLOR4(&tex.albedo_factor, pMaterialState->albedo);
		tex.displacement_factor = pMaterialState->displacement;
	}
	//else
	//{
	//	RD_SET_COLOR4(&tex.emissive_factor, 0xFF000000);
	//	RD_SET_COLOR4(&tex.albedo_factor, 0xFFFFFFFF);
	//	tex.displacement_factor = 0.0;
	//}

	// todo: expose
	rdVector_Set4(&tex.specular_factor, 0.2209f, 0.2209f, 0.2209f, 0.2209f); // 0.04 in linear, but we're working in srgb
	tex.roughnessFactor = stdMath_Sqrt(2.0f / 24.0f);

	glBindBuffer(GL_UNIFORM_BUFFER, material_ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(std3D_MaterialUniforms), &tex);
}

void std3D_SetLightingState(std3D_worldStage* pStage, std3D_DrawCallState* pState)
{
	glUniform1i(pStage->uniform_light_mode, pState->stateBits.lightMode);
	glUniform1i(pStage->uniform_ao_flags, pState->lightingState.ambientFlags);

	float r = ((pState->lightingState.ambientColor >> 20) & 0x3FF) / 255.0f;
	float g = ((pState->lightingState.ambientColor >> 10) & 0x3FF) / 255.0f;
	float b = ((pState->lightingState.ambientColor >>  0) & 0x3FF) / 255.0f;
	glUniform3f(pStage->uniform_ambient_color, r, g, b);

	rdVector4 sgs[8];
	for (int i = 0; i < 8; ++i)
	{
		float r = ((pState->lightingState.ambientLobes[i] >> 20) & 0x3FF) / 255.0f;
		float g = ((pState->lightingState.ambientLobes[i] >> 10) & 0x3FF) / 255.0f;
		float b = ((pState->lightingState.ambientLobes[i] >> 0) & 0x3FF) / 255.0f;
		float a = ((pState->lightingState.ambientLobes[i] >> 30) & 0x2);
		rdVector_Set4(&sgs[i], r, g, b, a);
	}
	glUniform4fv(pStage->uniform_ambient_sg, 8, &sgs[0].x);

	//glUniform1uiv(pStage->uniform_ambient_sg, 8, &pState->lightingState.ambientLobes[0]);
	glUniform1ui(pStage->uniform_ambient_sg_count, 8);
}

void std3D_SetShaderState(std3D_worldStage* pStage, std3D_DrawCallState* pState)
{
	// todo
	//glBindBuffer(GL_UNIFORM_BUFFER, shaderConstsUbo);
	//glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(std3D_ShaderConsts), &uniforms);
}

void std3D_BindStage(std3D_worldStage* pStage)
{
	std3D_useProgram(pStage->program);

	glBindVertexArray(pStage->vao[bufferIdx]);
	glBindBuffer(GL_ARRAY_BUFFER, world_vbo_all[bufferIdx]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, world_ibo_triangle[bufferIdx]);

	GLuint texids[] = {
		TEX_SLOT_TEX0,
		TEX_SLOT_TEX1,
		TEX_SLOT_TEX2,
		TEX_SLOT_TEX3,
	};

	glUniform1iv(pStage->uniform_textures, 4, texids);

	glUniform1i(pStage->uniform_tex,                TEX_SLOT_DIFFUSE);
	glUniform1i(pStage->uniform_worldPalette,       TEX_SLOT_WORLD_PAL);
	glUniform1i(pStage->uniform_worldPaletteLights, TEX_SLOT_WORLD_LIGHT_PAL);
	glUniform1i(pStage->uniform_texEmiss,           TEX_SLOT_EMISSIVE);
	glUniform1i(pStage->uniform_displacement_map,   TEX_SLOT_DISPLACEMENT);
	glUniform1i(pStage->uniform_lightbuf,           TEX_SLOT_CLUSTER_BUFFER);
	glUniform1i(pStage->uniform_texDecals,          TEX_SLOT_DECAL_ATLAS);
	glUniform1i(pStage->uniform_texz,               TEX_SLOT_DEPTH);
	glUniform1i(pStage->uniform_texssao,            TEX_SLOT_AO);
	glUniform1i(pStage->uniform_texrefraction,      TEX_SLOT_REFRACTION);
	glUniform1i(pStage->uniform_texclip,            TEX_SLOT_CLIP);
	glUniform1i(pStage->uniform_dithertex,          TEX_SLOT_DITHER);
	glUniform1i(pStage->uniform_diffuse_light,      TEX_SLOT_DIFFUSE_LIGHT);
}

typedef int(*std3D_SortFunc)(std3D_DrawCall*, std3D_DrawCall*);

enum STD3D_DIRTYBIT
{
	STD3D_STATEBITS = 0x1,
	STD3D_SHADER    = 0x2,
	STD3D_TRANSFORM = 0x4,
	STD3D_RASTER    = 0x8,
	STD3D_TEXTURE   = 0x10,
	STD3D_FOG       = 0x20,
	STD3D_LIGHTING  = 0x40,
};

static uint16_t std3D_drawCallIndicesSorted[STD3D_MAX_DRAW_CALL_INDICES];

uint16_t* std3D_SortDrawCallIndices(std3D_DrawCallList* pList)
{
	STD_BEGIN_PROFILER_LABEL();
	int drawCallIndexCount = 0;
	for (int i = 0; i < pList->drawCallCount; ++i)
	{
		memcpy(&std3D_drawCallIndicesSorted[drawCallIndexCount], &pList->drawCallIndices[pList->drawCallPtrs[i]->firstIndex], sizeof(uint16_t) * pList->drawCallPtrs[i]->numIndices);
		drawCallIndexCount += pList->drawCallPtrs[i]->numIndices;
	}
	STD_END_PROFILER_LABEL();
	return std3D_drawCallIndicesSorted;
}

void std3D_SortDrawCallList(std3D_DrawCallList* pList, std3D_SortFunc sortFunc)
{
	STD_BEGIN_PROFILER_LABEL();
	//if(!pList->bSorted)
		_qsort(pList->drawCallPtrs, pList->drawCallCount, sizeof(std3D_DrawCall*), (int(__cdecl*)(const void*, const void*))sortFunc);
	pList->bSorted = 1;
	STD_END_PROFILER_LABEL();
}

void std3D_FlushDrawCallList(std3D_RenderPass* pRenderPass, std3D_DrawCallList* pList, std3D_SortFunc sortFunc, const char* debugName)
{
	if (!pList->drawCallCount)
		return;

	STD_BEGIN_PROFILER_LABEL();
	std3D_pushDebugGroup(debugName);

	if (!(pRenderPass->flags & RD_RENDERPASS_NO_CLUSTERING))
	{
		if (rdMatrix_Compare44(&pRenderPass->oldProj, &pList->drawCalls[0].state.transformState.proj) != 0)
		{
			pRenderPass->clustersDirty = 1;
			pRenderPass->clusterFrustumFrame++;
			rdMatrix_Copy44(&pRenderPass->oldProj, &pList->drawCalls[0].state.transformState.proj);
		}

		if(pRenderPass->clustersDirty)
			std3D_BuildClusters(pRenderPass, &pList->drawCalls[0].state.transformState.proj);
	}

	// sort draw calls to reduce state changes and maximize batching
	uint16_t* indexArray;
	if(sortFunc)
	{
		std3D_SortDrawCallList(pList, sortFunc);
		
		// batching needs to follow the draw order, but index array becomes disjointed after sorting
		// build a sorted list of indices to ensure sequential access during batching
		indexArray = std3D_SortDrawCallIndices(pList);
	}
	else
	{
		indexArray = pList->drawCallIndices;
	}
	
	std3D_DrawCall* pDrawCall = pList->drawCallPtrs[0];
	std3D_DrawCallState* pState = &pDrawCall->state;

	std3D_DrawCallState lastState;
	memcpy(&lastState, &pDrawCall->state, sizeof(std3D_DrawCallState));

	int lastShader = pDrawCall->shaderID;
	std3D_worldStage* pStage = &worldStages[pDrawCall->shaderID];

	// increase the buffer counter for next upload
	bufferIdx = (bufferIdx + 1) % STD3D_STAGING_COUNT;

	std3D_BindStage(pStage);
	glBufferSubData(GL_ARRAY_BUFFER, 0, pList->drawCallVertexCount * sizeof(rdVertex), pList->drawCallVertices);
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, pList->drawCallIndexCount * sizeof(uint16_t), indexArray);
	
	int last_tex = pState->textureState.pTexture ? pState->textureState.pTexture->texture_id : blank_tex_white;
	std3D_SetRasterState(pStage, pState);
	std3D_SetFogState(pStage, pState);
	std3D_SetBlendState(pStage, pState);
	std3D_SetDepthStencilState(pState);
	std3D_SetTextureState(pStage, pState);
	std3D_SetMaterialState(pStage, pState);
	std3D_SetLightingState(pStage, pState);	
	std3D_SetTransformState(pStage, pState);
	
	int batchIndices = 0;
	int indexOffset = 0;
	for (int j = 0; j < pList->drawCallCount; j++)
	{
		pDrawCall = pList->drawCallPtrs[j];
		pState = &pDrawCall->state;

		int texid = pState->textureState.pTexture ? pState->textureState.pTexture->texture_id : blank_tex_white;
		
		uint32_t dirtyBits = 0;
		dirtyBits |= (lastShader != pDrawCall->shaderID) ? STD3D_SHADER : 0;
		dirtyBits |= (last_tex != texid) ? STD3D_TEXTURE : 0;
		dirtyBits |= (lastState.stateBits.data != pDrawCall->state.stateBits.data) ? STD3D_STATEBITS : 0; // todo: this probably triggers too many updates, make it more granular
		dirtyBits |= (memcmp(&lastState.fogState, &pDrawCall->state.fogState, sizeof(std3D_FogState)) != 0) ? STD3D_FOG : 0;
		dirtyBits |= (memcmp(&lastState.rasterState, &pDrawCall->state.rasterState, sizeof(std3D_RasterState)) != 0) ? STD3D_RASTER : 0;		
		dirtyBits |= (memcmp(&lastState.textureState, &pDrawCall->state.textureState, sizeof(std3D_TextureState)) != 0) ? STD3D_TEXTURE : 0;
		dirtyBits |= (memcmp(&lastState.materialState, &pDrawCall->state.materialState, sizeof(std3D_MaterialState)) != 0) ? STD3D_TEXTURE : 0;
		dirtyBits |= (memcmp(&lastState.lightingState, &pDrawCall->state.lightingState, sizeof(std3D_LightingState)) != 0) ? STD3D_LIGHTING : 0;
		dirtyBits |= (memcmp(&lastState.transformState, &pDrawCall->state.transformState, sizeof(std3D_TransformState)) != 0) ? STD3D_TRANSFORM : 0;

		if (dirtyBits)
		{
			//glDrawArrays(std3D_PrimitiveForGeoMode(lastState.raster.geoMode), vertexOffset, batch_verts);
			glDrawElements(std3D_PrimitiveForGeoMode(lastState.stateBits.geoMode + 1), batchIndices, GL_UNSIGNED_SHORT, (void*)(indexOffset * sizeof(uint16_t)));

			if (lastShader != pDrawCall->shaderID)//dirtyBits & STD3D_SHADER)
			{
				pStage = &worldStages[pDrawCall->shaderID];
				std3D_BindStage(pStage);
			}

			if((dirtyBits & STD3D_RASTER) || (dirtyBits & STD3D_STATEBITS))
				std3D_SetRasterState(pStage, pState);

			if ((dirtyBits & STD3D_FOG) || (dirtyBits & STD3D_STATEBITS))
				std3D_SetFogState(pStage, pState);

			if (dirtyBits & STD3D_STATEBITS)
			{
				std3D_SetBlendState(pStage, pState);
				std3D_SetDepthStencilState(pState);
			}

			if ((dirtyBits & STD3D_TEXTURE) || (dirtyBits & STD3D_STATEBITS))
			{
				std3D_SetTextureState(pStage, pState);

				// todo: material bits
				std3D_SetMaterialState(pStage, pState);
			}
			
			if ((dirtyBits & STD3D_LIGHTING) || (dirtyBits & STD3D_STATEBITS))
				std3D_SetLightingState(pStage, pState);
			
			//if ((dirtyBits & STD3D_TRANSFORM)) // fixme: not working?
				std3D_SetTransformState(pStage, pState);
		
			// if the projection matrix changed then all lighting is invalid, rebuild clusters and assign lights
			// perhaps all of this would be better if we just flushed the pipeline on matrix change instead

			if (!(pRenderPass->flags & RD_RENDERPASS_NO_CLUSTERING))
			{
				if (rdMatrix_Compare44(&lastState.transformState.proj, &pDrawCall->state.transformState.proj) != 0)
				{
					stdPlatform_Printf("std3D: Warning, clusters are being rebuilt twice within a draw list flush!\n");

					rdMatrix_Copy44(&pRenderPass->oldProj, &pDrawCall->state.transformState.proj);
					pRenderPass->clusterFrustumFrame++;
					pRenderPass->clustersDirty = 1;
					std3D_BuildClusters(pRenderPass, &pDrawCall->state.transformState.proj);
					std3D_UpdateSharedUniforms();
				}
			}

			last_tex = texid;
			lastShader = pDrawCall->shaderID;
			memcpy(&lastState, &pDrawCall->state, sizeof(std3D_DrawCallState));

			indexOffset += batchIndices;
			batchIndices = 0;
		}

		batchIndices += pDrawCall->numIndices;
	}

	if (batchIndices)
		glDrawElements(std3D_PrimitiveForGeoMode(pDrawCall->state.stateBits.geoMode + 1), batchIndices, GL_UNSIGNED_SHORT, (void*)(indexOffset * sizeof(uint16_t)));

	glBindSampler(0, 0);
	glBindSampler(TEX_SLOT_DIFFUSE, 0);
	glBindSampler(TEX_SLOT_EMISSIVE, 0);
	glBindSampler(TEX_SLOT_DISPLACEMENT, 0);

	std3D_popDebugGroup();
	STD_END_PROFILER_LABEL();
}

void std3D_DoSSAO()
{
	std3D_pushDebugGroup("SSAO");

	// downscale the depth buffer with lower precision
	std3D_DrawSimpleTex(&std3D_texFboStage, &ssaoDepth, std3D_framebuffer.ztex, 0, 0, 1.0, 1.0, 1.0, 0, "Z Downscale");

	// enable depth testing to prevent running SSAO on empty pixels
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GREATER);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);

	glBindFramebuffer(GL_FRAMEBUFFER, ssao.fbo);

	glClearColor(1,1,1,1);
	glClear(GL_COLOR_BUFFER_BIT);

	std3D_useProgram(std3D_ssaoStage.program);

	std3D_bindTexture(GL_TEXTURE_2D, std3D_framebuffer.ztex,          0);
	std3D_bindTexture(GL_TEXTURE_2D, ssaoDepth.tex, 1);
	std3D_bindTexture(GL_TEXTURE_2D, tiledrand_texture,        2);

	glUniform1i(std3D_ssaoStage.uniform_tex,  0);
	glUniform1i(std3D_ssaoStage.uniform_tex2, 1);
	glUniform1i(std3D_ssaoStage.uniform_tex3, 2);

	glViewport(0, 0, ssao.w, ssao.h);
	glUniform2f(std3D_ssaoStage.uniform_iResolution, ssao.iw, ssao.ih);
	
	glDrawArrays(GL_TRIANGLES, 0, 3);
	
	std3D_bindTexture(GL_TEXTURE_2D, 0, 0);
	std3D_bindTexture(GL_TEXTURE_2D, 0, 1);
	std3D_bindTexture(GL_TEXTURE_2D, 0, 2);

	std3D_popDebugGroup();
}

void std3D_DoDeferredLighting()
{
	std3D_pushDebugGroup("Deferred Lighting");

	// enable depth testing to prevent running on empty pixels
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GREATER);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);

	glBindFramebuffer(GL_FRAMEBUFFER, deferred.fbo);

	//glClearColor(1, 1, 1, 1);
	//glClear(GL_COLOR_BUFFER_BIT);

	std3D_useProgram(std3D_deferredStage.program);

	std3D_bindTexture(GL_TEXTURE_2D, std3D_framebuffer.ztex, 0);
	std3D_bindTexture(GL_TEXTURE_2D, std3D_framebuffer.ntex, 1);
	std3D_bindTexture(GL_TEXTURE_2D, tiledrand_texture, 2);

	glUniform1i(std3D_deferredStage.uniform_tex, 0);
	glUniform1i(std3D_deferredStage.uniform_tex2, 1);
	glUniform1i(std3D_deferredStage.uniform_tex3, 2);
	glUniform1i(std3D_deferredStage.uniform_tex4, 3);
	glUniform1i(std3D_deferredStage.uniform_lightbuf, TEX_SLOT_CLUSTER_BUFFER);

	glViewport(0, 0, deferred.w, deferred.h);
	glUniform2f(std3D_deferredStage.uniform_iResolution, deferred.iw, deferred.ih);

	if (renderPassProj)
		glUniformMatrix4fv(std3D_deferredStage.uniform_proj, 1, GL_FALSE, (float*)renderPassProj);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	std3D_bindTexture(GL_TEXTURE_2D, 0, 0);
	std3D_bindTexture(GL_TEXTURE_2D, 0, 1);
	std3D_bindTexture(GL_TEXTURE_2D, 0, 2);

	std3D_popDebugGroup();
}

void std3D_setupWorldTextures()
{
	GLint aotex = jkPlayer_enableSSAO ? ssao.tex : blank_tex_white;
	std3D_bindTexture(    GL_TEXTURE_2D,         blank_tex_white,         TEX_SLOT_TEX0);
	std3D_bindTexture(    GL_TEXTURE_2D,         blank_tex_white,         TEX_SLOT_TEX1);
	std3D_bindTexture(    GL_TEXTURE_2D,         blank_tex_white,         TEX_SLOT_TEX2);
	std3D_bindTexture(    GL_TEXTURE_2D,         blank_tex_white,         TEX_SLOT_TEX3);
		
	//std3D_bindTexture(    GL_TEXTURE_2D,         blank_tex_white,         TEX_SLOT_DIFFUSE);
	//std3D_bindTexture(    GL_TEXTURE_2D,               blank_tex,        TEX_SLOT_EMISSIVE);
	//std3D_bindTexture(    GL_TEXTURE_2D,               blank_tex,    TEX_SLOT_DISPLACEMENT);
	std3D_bindTexture(    GL_TEXTURE_2D,        worldpal_texture,       TEX_SLOT_WORLD_PAL);
	std3D_bindTexture(    GL_TEXTURE_2D, worldpal_lights_texture, TEX_SLOT_WORLD_LIGHT_PAL);
	std3D_bindTexture(GL_TEXTURE_BUFFER,             cluster_tbo,  TEX_SLOT_CLUSTER_BUFFER);
	std3D_bindTexture(    GL_TEXTURE_2D,       decalAtlasFBO.tex,     TEX_SLOT_DECAL_ATLAS);
	std3D_bindTexture(    GL_TEXTURE_2D,  std3D_framebuffer.ztex,           TEX_SLOT_DEPTH);
	std3D_bindTexture(    GL_TEXTURE_2D,                   aotex,              TEX_SLOT_AO);
	std3D_bindTexture(    GL_TEXTURE_2D,                refr.tex,      TEX_SLOT_REFRACTION);
	std3D_bindTexture(    GL_TEXTURE_2D,               blank_tex,            TEX_SLOT_CLIP);
	std3D_bindTexture(    GL_TEXTURE_2D,          dither_texture,          TEX_SLOT_DITHER);
}

void std3D_FlushRefractionZDrawCalls(std3D_RenderPass* pRenderPass)
{
	std3D_pushDebugGroup("std3D_FlushRefractionZDrawCalls");

	glBindFramebuffer(GL_FRAMEBUFFER, refrZ.fbo);

	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilFunc(GL_ALWAYS, 0xFF, 0xFF);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	//std3D_FlushDrawCallList(pRenderPass, &pRenderPass->drawCallLists[DRAW_LIST_Z], std3D_DrawCallCompareSortKey, "Z Prepass");
	//std3D_FlushDrawCallList(pRenderPass, &pRenderPass->drawCallLists[DRAW_LIST_Z_ALPHATEST], std3D_DrawCallCompareSortKey, "Z Prepass Alphatest");
	std3D_FlushDrawCallList(pRenderPass, &pRenderPass->drawCallLists[DRAW_LIST_Z_REFRACTION], std3D_DrawCallCompareSortKey, "Refraction Z");
	
	glDisable(GL_STENCIL_TEST);

	std3D_popDebugGroup();
}

void std3D_FlushZDrawCalls(std3D_RenderPass* pRenderPass)
{
	std3D_pushDebugGroup("std3D_FlushZDrawCalls");

	glBindFramebuffer(GL_FRAMEBUFFER, std3D_framebuffer.zfbo);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	// clear the depth buffer if requested
	if (pRenderPass->flags & RD_RENDERPASS_CLEAR_DEPTH)
	{
		glDepthMask(GL_TRUE);
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	std3D_setupWorldTextures();

	//glColorMask(0,0,0,0);

	std3D_FlushDrawCallList(pRenderPass,           &pRenderPass->drawCallLists[DRAW_LIST_Z], std3D_DrawCallCompareSortKey,           "Z Prepass");
	std3D_FlushDrawCallList(pRenderPass, &pRenderPass->drawCallLists[DRAW_LIST_Z_ALPHATEST], std3D_DrawCallCompareSortKey, "Z Prepass Alphatest");

	//glColorMask(1,1,1,1);

	std3D_popDebugGroup();
}

void std3D_FlushColorDrawCallsRefraction(std3D_RenderPass* pRenderPass)
{
	std3D_pushDebugGroup("std3D_FlushColorDrawCallsRefraction");

	std3D_setupWorldTextures();
	std3D_bindTexture(GL_TEXTURE_2D, refrZ.tex, TEX_SLOT_CLIP);

	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_EQUAL, 0xFF, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	glBindFramebuffer(GL_FRAMEBUFFER, refr.fbo);
	GLenum bufs[] = { GL_COLOR_ATTACHMENT0 };//, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(ARRAYSIZE(bufs), bufs);

	std3D_FlushDrawCallList(pRenderPass,   &pRenderPass->drawCallLists[DRAW_LIST_COLOR_ZPREPASS], std3D_DrawCallCompareSortKey,    "Color ZPrepass");
	std3D_FlushDrawCallList(pRenderPass, &pRenderPass->drawCallLists[DRAW_LIST_COLOR_NOZPREPASS], std3D_DrawCallCompareSortKey, "Color No ZPrepass");
	std3D_FlushDrawCallList(pRenderPass, &pRenderPass->drawCallLists[DRAW_LIST_COLOR_ALPHABLEND],   std3D_DrawCallCompareDepth,  "Color Alphablend");
	
	glDisable(GL_STENCIL_TEST);

	std3D_popDebugGroup();
}

void std3D_FlushColorDrawCalls(std3D_RenderPass* pRenderPass)
{
	std3D_pushDebugGroup("std3D_FlushColorDrawCalls");
	
	std3D_setupWorldTextures();

	glBindFramebuffer(GL_FRAMEBUFFER, std3D_framebuffer.fbo);
	GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(ARRAYSIZE(bufs), bufs);

	std3D_FlushDrawCallList(pRenderPass,   &pRenderPass->drawCallLists[DRAW_LIST_COLOR_ZPREPASS], std3D_DrawCallCompareSortKey,    "Color ZPrepass");
	std3D_FlushDrawCallList(pRenderPass, &pRenderPass->drawCallLists[DRAW_LIST_COLOR_NOZPREPASS], std3D_DrawCallCompareSortKey, "Color No ZPrepass");
	std3D_FlushDrawCallList(pRenderPass, &pRenderPass->drawCallLists[DRAW_LIST_COLOR_ALPHABLEND],   std3D_DrawCallCompareDepth,  "Color Alphablend");

	std3D_popDebugGroup();
}

void std3D_FlushRefractionDrawCalls(std3D_RenderPass* pRenderPass)
{
	std3D_pushDebugGroup("std3D_FlushRefractionDrawCalls");

	std3D_setupWorldTextures();

	glBindFramebuffer(GL_FRAMEBUFFER, std3D_framebuffer.fbo);
	GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(ARRAYSIZE(bufs), bufs);

	std3D_FlushDrawCallList(pRenderPass, &pRenderPass->drawCallLists[DRAW_LIST_COLOR_REFRACTION], std3D_DrawCallCompareDepth, "Color Refraction");

	std3D_popDebugGroup();
}

void std3D_FlushDeferred(std3D_RenderPass* pRenderPass)
{
	STD_BEGIN_PROFILER_LABEL();
	std3D_pushDebugGroup("std3D_FlushDeferred");
	
	if (pRenderPass->flags & RD_RENDERPASS_AMBIENT_OCCLUSION)
		std3D_DoSSAO();

	// deferred is a lot more expensive at 4k than cramming it all into 1 shader
	//std3D_DrawSimpleTex(&std3D_deferredStage, &deferred, std3D_framebuffer.ztex, std3D_framebuffer.ntex, 0, 1.0f, 1.0f, 1.0f, 0, "Deferred Opaque");
	//if (!(pRenderPass->flags & RD_RENDERPASS_NO_CLUSTERING))
	//	std3D_DoDeferredLighting();

	std3D_popDebugGroup();
	STD_END_PROFILER_LABEL();
}

// writes directly to the final window framebuffer
void std3D_DoBloom()
{
	if (!jkPlayer_enableBloom)
		return;

	STD_BEGIN_PROFILER_LABEL();

	// todo: cvars
	const float bloom_intensity = 1.0f;// 1.0f;
	const float bloom_gamma = 1.0f;// 1.5f;
	const float blendLerp = 0.6f;
	const float uvScale = 1.0f; // debug for the kernel radius

	std3D_pushDebugGroup("Bloom");

	// downscale layers using a simple gaussian filter
	std3D_DrawSimpleTex(&std3D_bloomStage, &bloomLayers[0], std3D_framebuffer.tex1, 0, 0, uvScale, 1.0, 1.0, 0, "Bloom Downscale");
	for (int i = 1; i < ARRAY_SIZE(bloomLayers); ++i)
		std3D_DrawSimpleTex(&std3D_bloomStage, &bloomLayers[i], bloomLayers[i - 1].tex, 0, 0, uvScale, 1.0, 1.0, 0, "Bloom Downscale");

	// upscale layers and blend upward
	glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendFunc(GL_ONE, GL_ONE);
	for (int i = ARRAY_SIZE(bloomLayers) - 2; i >= 0; --i)
		std3D_DrawSimpleTex(&std3D_bloomStage, &bloomLayers[i], bloomLayers[i + 1].tex, 0, 0, uvScale, blendLerp, 1.0, 0, "Bloom Upscale");

	std3D_popDebugGroup();
	
	STD_END_PROFILER_LABEL();
}

void std3D_FlushPostFX()
{
	STD_BEGIN_PROFILER_LABEL();

	std3D_pushDebugGroup("std3D_FlushPostFX");

	glBindFramebuffer(GL_FRAMEBUFFER, window.fbo);
	//glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	//if (!jkGame_isDDraw && !jkGuiBuildMulti_bRendering)
		//return;

	std3D_DoBloom();

	glDisable(GL_BLEND);
	std3D_DrawSimpleTex(&std3D_postfxStage, &window, std3D_framebuffer.tex0, bloomLayers[0].tex, 0, (rdCamera_pCurCamera->flags & 0x1) ? sithTime_curSeconds : -1.0, jkPlayer_enableDithering, jkPlayer_gamma, 0, "Final Output");

	std3D_popDebugGroup();

	STD_END_PROFILER_LABEL();
}

// fixme: flush this when rdCache_Flush is called instead of trying to lump everything into a bunch of draw lists?
void std3D_FlushDrawCalls()
{
	if (Main_bHeadless) return;
	if (!has_initted) return;

	STD_BEGIN_PROFILER_LABEL();
	std3D_pushDebugGroup("std3D_FlushDrawCalls");

	glViewport(0, 0, std3D_framebuffer.w, std3D_framebuffer.h);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	glCullFace(GL_BACK);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);

	glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SLOT_LIGHTS, light_ubo);
	glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SLOT_OCCLUDERS, occluder_ubo);
	glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SLOT_DECALS, decal_ubo);
	glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SLOT_SHARED, shared_ubo);
	glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SLOT_FOG, fog_ubo);
	glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SLOT_TEX, tex_ubo);
	glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SLOT_MATERIAL, material_ubo);
	glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SLOT_SHADER, defaultShaderUBO);

	for (int j = 0; j < STD3D_MAX_RENDER_PASSES; ++j)
	{
		std3D_RenderPass* pRenderPass = &std3D_renderPasses[j];

		std3D_pushDebugGroup(pRenderPass->name);

		std3D_UpdateSharedUniforms();

		// fill the depth buffer with opaque draw calls
		std3D_FlushZDrawCalls(pRenderPass);

		// do opaque-only deferred stuff
		//if (pRenderPass->flags & RD_RENDERPASS_AMBIENT_OCCLUSION)
			std3D_FlushDeferred(pRenderPass);

		// draw refraction passes
		int hasRefraction = 0;
		if (pRenderPass->flags & RD_RENDERPASS_REFRACTION)
			hasRefraction = pRenderPass->drawCallLists[DRAW_LIST_Z_REFRACTION].drawCallCount > 0;

		if (hasRefraction)
		{
			if (j == 0)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, refr.fbo);
				glClear(GL_COLOR_BUFFER_BIT);
			}
			else
			{
				glDepthMask(GL_FALSE);
				std3D_DrawSimpleTex(&std3D_texFboStage, &refr, std3D_framebuffer.tex0, 0, 0, 1.0, 1.0, 1.0, 0, "Refraction Blit");
			}

			// fill the refraction depth buffer, this includes refracted objects
			std3D_FlushRefractionZDrawCalls(pRenderPass);

			// draw color with clipping against the water depth and stencil
			std3D_FlushColorDrawCallsRefraction(pRenderPass);

			// generate the mip chain
			//glActiveTexture(GL_TEXTURE0 + TEX_SLOT_REFRACTION);
			//glBindTexture(GL_TEXTURE_2D, refr.tex);
			//glGenerateMipmap(GL_TEXTURE_2D);

			// draw refraction surfaces
			std3D_FlushRefractionDrawCalls(pRenderPass);
		}
		
		// draw scene normally
		// for refraction case, the depth buffer will clip out anything below the refraction surfaces
		std3D_FlushColorDrawCalls(pRenderPass);

		std3D_popDebugGroup();
	}

	std3D_FlushPostFX();

	std3D_ResetDrawCalls();

	glBindVertexArray(vao);

	glBindTexture(GL_TEXTURE_2D, worldpal_texture);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);
	glCullFace(GL_FRONT);
	glBindSampler(0, 0);
	glBindSampler(TEX_SLOT_DIFFUSE, 0);
	glBindSampler(TEX_SLOT_EMISSIVE, 0);
	glBindSampler(TEX_SLOT_DISPLACEMENT, 0);

	std3D_popDebugGroup();
	STD_END_PROFILER_LABEL();
}

void std3D_ClearLights()
{
	lightsDirty = 1;
	std3D_renderPasses[0].clustersDirty = std3D_renderPasses[1].clustersDirty = std3D_renderPasses[2].clustersDirty = 1;
	lightUniforms.numLights = 0;
}

int std3D_AddLight(rdLight* light, rdVector3* position, float intensity)
{
	if(lightUniforms.numLights >= CLUSTER_MAX_LIGHTS || !light->active)
		return 0;

	lightsDirty = 1;
	std3D_renderPasses[0].clustersDirty = std3D_renderPasses[1].clustersDirty = std3D_renderPasses[2].clustersDirty = 1;

	std3D_light* light3d = &lightUniforms.tmpLights[lightUniforms.numLights++];
	light3d->type = light->type;
	light3d->position.x = position->x;
	light3d->position.y = position->y;
	light3d->position.z = position->z;
	light3d->direction_intensity.x = light->direction.x;
	light3d->direction_intensity.y = light->direction.y;
	light3d->direction_intensity.z = light->direction.z;
	light3d->direction_intensity.w = light->intensity * intensity;


	uint32_t r = (uint32_t)fmin(light->color.x * intensity * 1023.0, 1023.0);
	uint32_t g = (uint32_t)fmin(light->color.y * intensity * 1023.0, 1023.0);
	uint32_t b = (uint32_t)fmin(light->color.z * intensity * 1023.0, 1023.0);
	uint32_t a = 1;

	light3d->color = (r << 20u) | (g << 10u) | (b << 0u);

	//light3d->color.x = light->color.x * intensity;
	//light3d->color.y = light->color.y * intensity;
	//light3d->color.z = light->color.z * intensity;
	//light3d->color.w = fmin(light->color.x, fmin(light->color.y, light->color.z));
#ifdef JKM_LIGHTING
	light3d->angleX = light->angleX;
	light3d->cosAngleX = light->cosAngleX;
	light3d->angleY = light->angleY;
	light3d->cosAngleY = light->cosAngleY;
	light3d->lux = light->lux;
#endif
	light3d->radiusSq = light->falloffMin * light->falloffMin;
	light3d->falloffMin = light->falloffMin;
	light3d->falloffMax = light->falloffMax;
	return 1;
}

void std3D_FlushLights()
{
	if (lightsDirty)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, light_ubo);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(std3D_LightUniforms), &lightUniforms, GL_DYNAMIC_DRAW);
		lightsDirty = 0;
	}
}

void std3D_ClearOccluders()
{
	occludersDirty = 1;
	std3D_renderPasses[0].clustersDirty = std3D_renderPasses[1].clustersDirty = std3D_renderPasses[2].clustersDirty = 1;
	occluderUniforms.numOccluders = 0;
}

void std3D_ClearDecals()
{
	decalsDirty = 1;
	std3D_renderPasses[0].clustersDirty = std3D_renderPasses[1].clustersDirty = std3D_renderPasses[2].clustersDirty = 1;
	decalUniforms.numDecals= 0;

	// tmp debug
	std3D_PurgeDecalAtlas();
}

void std3D_FlushOccluders()
{
	if (occludersDirty)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, occluder_ubo);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(std3D_OccluderUniforms), &occluderUniforms, GL_DYNAMIC_DRAW);
		occludersDirty = 0;
	}
}

void std3D_PurgeDecalAtlas()
{
	decalRootNode.children[0] = decalRootNode.children[1] = NULL;
	numAllocNodes = 0;
	if(decalHashTable)
	{
		stdHashTable_Free(decalHashTable);
		decalHashTable = 0;
	}
}

std3D_decalAtlasNode* std3D_AllocDecalNode()
{
	if (numAllocNodes >= (DECAL_ATLAS_SIZE/4)*(DECAL_ATLAS_SIZE/4))
	{
		stdPlatform_Printf("std3D: ERROR, Decal node pool is exhausted!\n");
		return NULL;
	}
	std3D_decalAtlasNode* node = &nodePool[numAllocNodes++];
	memset(node, 0, sizeof(std3D_decalAtlasNode));
	return node;
}

std3D_decalAtlasNode* std3D_InsertDecal(std3D_decalAtlasNode* parent, const rdRect* bound, rdDDrawSurface* tex)
{
	std3D_decalAtlasNode* newNode;
	if (parent->children[0]) // if not a leaf, insert into children
	{
		newNode = std3D_InsertDecal(parent->children[0], bound, tex);
		if (newNode)
			return newNode;

		return std3D_InsertDecal(parent->children[1], bound, tex);
	}
	else
	{
		// already have one
		if (parent->texture)
			return NULL;

		// doesn't fit
		if (parent->rect.width < bound->width || parent->rect.height < bound->height)
			return NULL;

		if (parent->rect.width == bound->width && parent->rect.height == bound->height)
		{
			sprintf_s(parent->name, 32, "decalTex%d", tex->texture_id);
			parent->texture = tex;
			return parent;
		}

		parent->children[0] = std3D_AllocDecalNode();
		parent->children[1] = std3D_AllocDecalNode();

		float dw = parent->rect.width - bound->width;
		float dh = parent->rect.height - bound->height;
		if (dw > dh)
		{
			parent->children[0]->rect = parent->rect;
			parent->children[1]->rect = parent->rect;
			parent->children[0]->rect.width = bound->width;
			parent->children[1]->rect.x = parent->rect.x + bound->width;
		}
		else
		{
			parent->children[0]->rect = parent->rect;
			parent->children[1]->rect = parent->rect;
			parent->children[0]->rect.height = bound->height;
			parent->children[1]->rect.y = parent->rect.y + bound->height;
		}
		return std3D_InsertDecal(parent->children[0], bound, tex);
	}
}

int std3D_InsertDecalTexture(rdRect* out, stdVBuffer* vbuf, rdDDrawSurface* pTexture)
{
	if(!decalHashTable)
		decalHashTable = stdHashTable_New(256); // todo: move

	char tmpName[32];
	sprintf_s(tmpName, 32, "decalTex%d", pTexture->texture_id);

	int32_t index = -1;
	std3D_decalAtlasNode* findNode = (std3D_decalAtlasNode*)stdHashTable_GetKeyVal(decalHashTable, tmpName);
	if (findNode)
	{
		*out = findNode->rect;
		return 1;
	}
	else
	{
		rdRect rect;
		rect.x = 0;
		rect.y = 0;
		rect.width = vbuf->format.width;
		rect.height = vbuf->format.height;

		std3D_decalAtlasNode* node = std3D_InsertDecal(&decalRootNode, &rect, pTexture);
		if (node)
		{
			std3D_pushDebugGroup("std3D_InsertDecalTexture");

			glBindFramebuffer(GL_FRAMEBUFFER, decalAtlasFBO.fbo);
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
			glDepthFunc(GL_ALWAYS);
			glDisable(GL_CULL_FACE);
			glDisable(GL_BLEND);
			glDepthMask(GL_FALSE);
			glDisable(GL_DEPTH_TEST);
			std3D_useProgram(std3D_decalAtlasStage.program);

			glBindVertexArray(vao);

			glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_2D, pTexture->texture_id);
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_2D, worldpal_texture);

			glUniform1i(std3D_decalAtlasStage.uniform_tex, 0);
			glUniform1i(std3D_decalAtlasStage.uniform_tex2, 1);
			glUniform1f(std3D_decalAtlasStage.uniform_param1, pTexture && pTexture->is_16bit ? TEX_MODE_16BPP : TEX_MODE_WORLDPAL);

			glViewport(node->rect.x, node->rect.y, node->rect.width, node->rect.height);

			glDrawArrays(GL_TRIANGLES, 0, 3);

			stdHashTable_SetKeyVal(decalHashTable, node->name, node);
	
			std3D_popDebugGroup();
			
			*out = rect;
		}
		else
		{
			stdPlatform_Printf("std3D: ERROR, Decal texture atlas out of space!\n");
			return 0;
		}
	}
}

void std3D_FlushDecals()
{
	if (decalsDirty)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, decal_ubo);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(std3D_DecalUniforms), &decalUniforms, GL_DYNAMIC_DRAW);
		decalsDirty = 0;
	}
}

void std3D_UpdateClipRegionRoot(float nc, float lc, float lz, float Radius, float CameraScale, float* ClipMin, float* ClipMax)
{
	float nz = (Radius - nc * lc) / lz;
	float pz = (lc * lc + lz * lz - Radius * Radius) / (lz - (nz / nc) * lc);
	if (pz > 0.0f)
	{
		float c = -nz * CameraScale / nc;
		if (nc > 0.0f)
			*ClipMin = fmax(*ClipMin, c);
		else
			*ClipMax = fmin(*ClipMax, c);
	}
}

void std3D_UpdateClipRegion(float lc, float lz, float Radius, float CameraScale, float* ClipMin, float* ClipMax)
{
	float rSq = Radius * Radius;
	float lcSqPluslzSq = lc * lc + lz * lz;
	float d = rSq * lc * lc - lcSqPluslzSq * (rSq - lz * lz);
	if (d > 0.0f)
	{
		float a = Radius * lc;
		float b = stdMath_Sqrt(d);
		float nx0 = (a + b) / lcSqPluslzSq;
		float nx1 = (a - b) / lcSqPluslzSq;
		std3D_UpdateClipRegionRoot(nx0, lc, lz, Radius, CameraScale, ClipMin, ClipMax);
		std3D_UpdateClipRegionRoot(nx1, lc, lz, Radius, CameraScale, ClipMin, ClipMax);
	}
}

int std3D_ComputeClipRegion(const rdVector3* Center, float Radius, rdMatrix44* pProjection, float Near, rdVector4* ClipRegion)
{
	rdVector_Set4(ClipRegion, 1.0f, 1.0f, 0.0f, 0.0f);
	if ((Center->y + Radius) >= Near)
	{
		rdVector2 ClipMin = { -1.0f, -1.0f };
		rdVector2 ClipMax = { +1.0f, +1.0f };
		std3D_UpdateClipRegion(Center->x, Center->y, Radius, pProjection->vA.x, &ClipMin.x, &ClipMax.x);
		std3D_UpdateClipRegion(-Center->z, Center->y, Radius, pProjection->vC.y, &ClipMin.y, &ClipMax.y);
		rdVector_Set4(ClipRegion, ClipMin.x, ClipMin.y, ClipMax.x, ClipMax.y);
		return 1;
	}
	return 0;
}

int std3D_ComputeBoundingBox(const rdVector3* Center, float Radius, rdMatrix44* pProjection, float Near, rdVector4* Bounds)
{
	rdVector4 bounds;
	int clipped = std3D_ComputeClipRegion(Center, Radius, pProjection, Near, &bounds);

	Bounds->x = 0.5f *  bounds.x + 0.5f;
	Bounds->y = 0.5f * -bounds.w + 0.5f;
	Bounds->z = 0.5f *  bounds.z + 0.5f;
	Bounds->w = 0.5f * -bounds.y + 0.5f;

	return clipped;
}

// unfortunately this is way slower than the frustum approach...
void std3D_BuildCluster(std3D_Cluster* pCluster, int x, int y, int z, float znear, float zfar)
{
	float z0 = (float)(z + 0) / CLUSTER_GRID_SIZE_Z;
	float z1 = (float)(z + 1) / CLUSTER_GRID_SIZE_Z;
	z0 = znear * powf(zfar / znear, z0) / zfar; // linear 0-1
	z1 = znear * powf(zfar / znear, z1) / zfar; // linear 0-1

	float v0 = (float)(y + 0) / CLUSTER_GRID_SIZE_Y;
	float v1 = (float)(y + 1) / CLUSTER_GRID_SIZE_Y;

	float u0 = (float)(x + 0) / CLUSTER_GRID_SIZE_X;
	float u1 = (float)(x + 1) / CLUSTER_GRID_SIZE_X;

	// calculate the corners of the cluster
	rdVector3 corners[8];
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[0], u0, v0, z0);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[1], u1, v0, z0);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[2], u0, v1, z0);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[3], u0, v0, z1);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[4], u1, v1, z0);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[5], u0, v1, z1);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[6], u1, v0, z1);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[7], u1, v1, z1);

	// calculate the AABB of the cluster
	rdVector_Set3(&pCluster->minb,  10000.0f,  10000.0f,  10000.0f);
	rdVector_Set3(&pCluster->maxb, -10000.0f, -10000.0f, -10000.0f);
	for (int c = 0; c < 8; ++c)
	{
		pCluster->minb.x = fmin(pCluster->minb.x, corners[c].x);
		pCluster->minb.y = fmin(pCluster->minb.y, corners[c].y);
		pCluster->minb.z = fmin(pCluster->minb.z, corners[c].z);
		pCluster->maxb.x = fmax(pCluster->maxb.x, corners[c].x);
		pCluster->maxb.y = fmax(pCluster->maxb.y, corners[c].y);
		pCluster->maxb.z = fmax(pCluster->maxb.z, corners[c].z);
	}
}

void std3D_AssignItemToClusters(std3D_RenderPass* pRenderPass, int itemIndex, rdVector3* pPosition, float radius, rdMatrix44* pProjection, float znear, float zfar, rdMatrix44* boxMat)
{
	// use a tight screen space bounding rect to determine which tiles the item needs to be assigned to
	rdVector4 rect;
	int clipped = std3D_ComputeBoundingBox(pPosition, radius, pProjection, znear, &rect); // todo: this seems to be a bit expensive, would it be better to use a naive box?
	if (rect.x < rect.z && rect.y < rect.w)
	{
		// linear depth for near and far edges of the light
		float zMin = fmax(0.0f, (pPosition->y - radius));
		float zMax = fmax(0.0f, (pPosition->y + radius));

		// skip if out of depth range
		if(zMin > pRenderPass->depthRange.y * zfar) // zmin is further than the range max
			return;
		if (zMax < pRenderPass->depthRange.x * zfar) // zmax is closer than the range min
			return;

		// non linear depth distribution
		int zStartIndex = (int)floorf(fmax(0.0f, logf(zMin) * sliceScalingFactor + sliceBiasFactor));
		int zEndIndex = (int)ceilf(fmax(0.0f, logf(zMax) * sliceScalingFactor + sliceBiasFactor));

		int yStartIndex = (int)floorf(rect.y * (float)CLUSTER_GRID_SIZE_Y);
		int yEndIndex = (int)ceilf(rect.w * (float)CLUSTER_GRID_SIZE_Y);

		int xStartIndex = (int)floorf(rect.x * (float)CLUSTER_GRID_SIZE_X);
		int xEndIndex = (int)ceilf(rect.z * (float)CLUSTER_GRID_SIZE_X);

		if ((zStartIndex < 0 && zEndIndex < 0) || (zStartIndex >= (int)CLUSTER_GRID_SIZE_Z && zEndIndex >= (int)CLUSTER_GRID_SIZE_Z))
			return;

		if ((yStartIndex < 0 && yEndIndex < 0) || (yStartIndex >= (int)CLUSTER_GRID_SIZE_Y && yEndIndex >= (int)CLUSTER_GRID_SIZE_Y))
			return;

		if ((xStartIndex < 0 && xEndIndex < 0) || (xStartIndex >= (int)CLUSTER_GRID_SIZE_X && xEndIndex >= (int)CLUSTER_GRID_SIZE_X))
			return;

		zStartIndex = stdMath_ClampInt(zStartIndex, 0, CLUSTER_GRID_SIZE_Z - 1);
		zEndIndex = stdMath_ClampInt(zEndIndex, 0, CLUSTER_GRID_SIZE_Z - 1);

		yStartIndex = stdMath_ClampInt(yStartIndex, 0, CLUSTER_GRID_SIZE_Y - 1);
		yEndIndex = stdMath_ClampInt(yEndIndex, 0, CLUSTER_GRID_SIZE_Y - 1);

		xStartIndex = stdMath_ClampInt(xStartIndex, 0, CLUSTER_GRID_SIZE_X - 1);
		xEndIndex = stdMath_ClampInt(xEndIndex, 0, CLUSTER_GRID_SIZE_X - 1);

		for (uint32_t z = zStartIndex; z <= zEndIndex; ++z)
		{
			for (uint32_t y = yStartIndex; y <= yEndIndex; ++y)
			{
				for (uint32_t x = xStartIndex; x <= xEndIndex; ++x)
				{
					uint32_t clusterID = x + y * CLUSTER_GRID_SIZE_X + z * CLUSTER_GRID_SIZE_X * CLUSTER_GRID_SIZE_Y;
					uint32_t tile_bucket_index = clusterID * CLUSTER_BUCKETS_PER_CLUSTER;

					// note: updating the cluster bounds is by far the most expensive part of this entire thing, avoid doing it!
				#ifndef USE_JOBS
					if (pRenderPass->clusters[clusterID].lastUpdateFrame != pRenderPass->clusterFrustumFrame)
					{
						std3D_BuildCluster(&pRenderPass->clusters[clusterID], x, y, z, znear, zfar);
						pRenderPass->clusters[clusterID].lastUpdateFrame = pRenderPass->clusterFrustumFrame;
					}
				#endif

					// todo: spotlight
					int intersects;
					if(boxMat)
						intersects = rdMath_IntersectAABB_OBB(&pRenderPass->clusters[clusterID].minb, &pRenderPass->clusters[clusterID].maxb, boxMat);
					else
						intersects = rdMath_IntersectAABB_Sphere(&pRenderPass->clusters[clusterID].minb, &pRenderPass->clusters[clusterID].maxb, pPosition, radius);

					if (intersects)
					{
						const uint32_t bucket_index = itemIndex / 32;
						const uint32_t bucket_place = itemIndex % 32;
						
						// SDL doesn't have proper atomic bit operations so fucking do it ourselves I guess
				#ifdef USE_JOBS
					#ifdef _WIN32
						_InterlockedOr((long*)&std3D_clusterBits[tile_bucket_index + bucket_index], (1 << bucket_place));
					#else // hopefully this is ok for Linux?
						__sync_or_and_fetch(&std3D_clusterBits[tile_bucket_index + bucket_index], (1 << bucket_place));
					#endif
				#else
						std3D_clusterBits[tile_bucket_index + bucket_index] |= (1 << bucket_place);
				#endif
					}
				}
			}
		}
	}
}

#ifdef USE_JOBS
std3D_RenderPass* std3D_jobRenderPass;
rdMatrix44* std3D_jobProjectionMat;

void stdJob_BuildClustersJob(uint32_t jobIndex, uint32_t groupIndex)
{
	int z = jobIndex / (CLUSTER_GRID_SIZE_X * CLUSTER_GRID_SIZE_Y);
	int y = (jobIndex % (CLUSTER_GRID_SIZE_X * CLUSTER_GRID_SIZE_Y)) / CLUSTER_GRID_SIZE_X;
	int x = jobIndex % CLUSTER_GRID_SIZE_X;

	float znear = -std3D_jobProjectionMat->vD.z / (std3D_jobProjectionMat->vB.z + 1.0f);
	float zfar = -std3D_jobProjectionMat->vD.z / (std3D_jobProjectionMat->vB.z - 1.0f);

	if (std3D_jobRenderPass->clusters[jobIndex].lastUpdateFrame != std3D_jobRenderPass->clusterFrustumFrame)
	{
		std3D_BuildCluster(&std3D_jobRenderPass->clusters[jobIndex], x, y, z, znear, zfar);
		std3D_jobRenderPass->clusters[jobIndex].lastUpdateFrame = std3D_jobRenderPass->clusterFrustumFrame;
	}
}

void stdJob_AssignLightsToClustersJob(uint32_t jobIndex, uint32_t groudIndex)
{
	if (jobIndex >= lightUniforms.numLights)
		return;

	float znear = -std3D_jobProjectionMat->vD.z / (std3D_jobProjectionMat->vB.z + 1.0f);
	float zfar = -std3D_jobProjectionMat->vD.z / (std3D_jobProjectionMat->vB.z - 1.0f);

	std3D_AssignItemToClusters(
		std3D_jobRenderPass,
		lightUniforms.firstLight + jobIndex,
		(rdVector3*)&lightUniforms.tmpLights[jobIndex].position,
		lightUniforms.tmpLights[jobIndex].falloffMin,
		std3D_jobProjectionMat,
		znear,
		zfar,
		NULL
	);
}

void stdJob_AssignOccludersToClustersJob(uint32_t jobIndex, uint32_t groupIndex)
{
	if (jobIndex >= occluderUniforms.numOccluders)
		return;

	float znear = -std3D_jobProjectionMat->vD.z / (std3D_jobProjectionMat->vB.z + 1.0f);
	float zfar = -std3D_jobProjectionMat->vD.z / (std3D_jobProjectionMat->vB.z - 1.0f);

	std3D_AssignItemToClusters(
		std3D_jobRenderPass,
		occluderUniforms.firstOccluder + jobIndex,
		(rdVector3*)&occluderUniforms.tmpOccluders[jobIndex].position,
		occluderUniforms.tmpOccluders[jobIndex].position.w,
		std3D_jobProjectionMat,
		znear,
		zfar,
		NULL
	);
}

void stdJob_AssignDecalsToClustersJob(uint32_t jobIndex, uint32_t groupIndex)
{
	if (jobIndex >= decalUniforms.numDecals)
		return;

	float znear = -std3D_jobProjectionMat->vD.z / (std3D_jobProjectionMat->vB.z + 1.0f);
	float zfar = -std3D_jobProjectionMat->vD.z / (std3D_jobProjectionMat->vB.z - 1.0f);
	
	std3D_AssignItemToClusters(
		std3D_jobRenderPass,
		decalUniforms.firstDecal + jobIndex,
		(rdVector3*)&decalUniforms.tmpDecals[jobIndex].posRad,
		decalUniforms.tmpDecals[jobIndex].posRad.w,
		std3D_jobProjectionMat,
		znear,
		zfar,
		&decalUniforms.tmpDecals[jobIndex].decalMatrix);
}
#endif

void std3D_BuildClusters(std3D_RenderPass* pRenderPass, rdMatrix44* pProjection)
{
	STD_BEGIN_PROFILER_LABEL();

#ifdef USE_JOBS
	std3D_jobRenderPass = pRenderPass;
	std3D_jobProjectionMat = pProjection;
#endif

	pRenderPass->clustersDirty = 0;

	// pull the near/far from the projection matrix
	// note: common sources list this as [3][2] and [2][2] but we have a rotated projection matrix, so we use [1][2]
	float znear = -pProjection->vD.z / (pProjection->vB.z + 1.0f);
	float zfar  = -pProjection->vD.z / (pProjection->vB.z - 1.0f);

	// scale and bias factor for non-linear cluster distribution
	sliceScalingFactor = (float)CLUSTER_GRID_SIZE_Z / logf(zfar / znear);
	sliceBiasFactor    = -((float)CLUSTER_GRID_SIZE_Z * logf(znear) / logf(zfar / znear));

	// ratio of tile to pixel
	tileSizeX = (uint32_t)ceilf((float)std3D_framebuffer.w / (float)CLUSTER_GRID_SIZE_X);
	tileSizeY = (uint32_t)ceilf((float)std3D_framebuffer.h / (float)CLUSTER_GRID_SIZE_Y);
	
	//int64_t time = Linux_TimeUs();

#ifdef USE_JOBS
	lightUniforms.firstLight = 0;
	occluderUniforms.firstOccluder = lightUniforms.firstLight + lightUniforms.numLights;
	decalUniforms.firstDecal = occluderUniforms.firstOccluder + occluderUniforms.numOccluders;

	// todo: do we need to atomically clear this?
	// maybe this should also be in a dispatch
	//for (int i = 0; i < ARRAY_SIZE(std3D_clusterBits); ++i)
		//SDL_AtomicSet(&std3D_clusterBits[i], 0);
	memset(std3D_clusterBits, 0, sizeof(std3D_clusterBits));

	// build all clusters in parallel
	if(pRenderPass->lastClusterFrustumFrame != pRenderPass->clusterFrustumFrame)
	{
		stdJob_Dispatch(CLUSTER_GRID_SIZE_XYZ, 64, stdJob_BuildClustersJob);
		//printf("%lld us to build custers for frame %d\n", Linux_TimeUs() - time, rdroid_frameTrue);

		// wait for clusters to finish building
		stdJob_Wait();

		pRenderPass->lastClusterFrustumFrame = pRenderPass->clusterFrustumFrame;
	}
	
	//time = Linux_TimeUs();

	if (!lightUniforms.numLights && !occluderUniforms.numOccluders && !decalUniforms.numDecals)
		return;

	// now assign items to clusters in parallel
	if (lightUniforms.numLights)
		stdJob_Dispatch(lightUniforms.numLights, 8, stdJob_AssignLightsToClustersJob);
	
	if (occluderUniforms.numOccluders)
		stdJob_Dispatch(occluderUniforms.numOccluders, 8, stdJob_AssignOccludersToClustersJob);
	
	if (decalUniforms.numDecals)
		stdJob_Dispatch(decalUniforms.numDecals, 8, stdJob_AssignDecalsToClustersJob);

	// wait on the result before uploading to GPU
	stdJob_Wait();

	//printf("%lld us to assign items to clusters for frame %d\n", Linux_TimeUs() - time, rdroid_frameTrue);
#else
	// clean slate
	memset(std3D_clusterBits, 0, sizeof(std3D_clusterBits));

	// assign lights
	lightUniforms.firstLight = 0;
	int64_t lighTime = Linux_TimeUs();
	for (int i = 0; i < lightUniforms.numLights; ++i)
	{
		std3D_AssignItemToClusters(pRenderPass, lightUniforms.firstLight + i, (rdVector3*)&lightUniforms.tmpLights[i].position, lightUniforms.tmpLights[i].falloffMin, pProjection, znear, zfar, NULL);
	}
	//printf("\t%lld us to assign lights to custers for frame %d\n", Linux_TimeUs() - lighTime, rdroid_frameTrue);

	// assign occluders
	occluderUniforms.firstOccluder = lightUniforms.firstLight + lightUniforms.numLights;
	int64_t occluderTime = Linux_TimeUs();
	for (int i = 0; i < occluderUniforms.numOccluders; ++i)
	{
		std3D_AssignItemToClusters(pRenderPass, occluderUniforms.firstOccluder + i, (rdVector3*)&occluderUniforms.tmpOccluders[i].position, occluderUniforms.tmpOccluders[i].position.w, pProjection, znear, zfar, NULL);
	}
	//printf("\t%lld us to assign occluders to custers for frame %d\n", Linux_TimeUs() - occluderTime, rdroid_frameTrue);

	// assign decals
	decalUniforms.firstDecal = occluderUniforms.firstOccluder + occluderUniforms.numOccluders;
	int64_t decalTime = Linux_TimeUs();
	for (int i = 0; i < decalUniforms.numDecals; ++i)
	{
		std3D_AssignItemToClusters(pRenderPass, decalUniforms.firstDecal + i, (rdVector3*)&decalUniforms.tmpDecals[i].posRad, decalUniforms.tmpDecals[i].posRad.w, pProjection, znear, zfar, &decalUniforms.tmpDecals[i].decalMatrix);
	}
	//printf("\t%lld us to assign decals to custers for frame %d\n", Linux_TimeUs() - decalTime, rdroid_frameTrue);

	//printf("%lld us to assign items to clusters for frame %d\n", Linux_TimeUs() - time, rdroid_frameTrue);
#endif

	std3D_FlushLights();
	std3D_FlushOccluders();
	std3D_FlushDecals();

	// todo: map buffer instead of storing to tmp then uploading?
	glBindBuffer(GL_TEXTURE_BUFFER, cluster_buffer);
	glBufferSubData(GL_TEXTURE_BUFFER, 0, sizeof(std3D_clusterBits), (void*)std3D_clusterBits);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);

	STD_END_PROFILER_LABEL();
}

#endif