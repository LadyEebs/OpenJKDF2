﻿#include "Platform/std3D.h"

#if !defined(RENDER_DROID2) && defined(SDL2_RENDER)

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

typedef struct std3DSimpleTexStage
{
    GLuint program;

    GLint attribute_coord3d;
    GLint attribute_v_color;
    GLint attribute_v_uv;
    GLint attribute_v_norm;

    GLint uniform_mvp;
    GLint uniform_tex;
    GLint uniform_tex2;
    GLint uniform_tex3;
	GLint uniform_tex4;
	GLint uniform_iResolution;

    GLint uniform_param1;
    GLint uniform_param2;
    GLint uniform_param3;
#ifdef VIEW_SPACE_GBUFFER
	GLint uniform_rt;
	GLint uniform_lt;
	GLint uniform_rb;
	GLint uniform_lb;
#endif
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

// LadyEebs updated gbuffer:
// color/forward render: 2-4 bytes per pixel
// emissive: 2-4 bytes per pixel
// depth: 2-4 bytes per pixel
// normal: 2 bytes per pixel
// diffuse: 2 bytes per pixel
//
// total: 10/16 bytes per pixel vs the original 28 bytes per pixel
// note: the depth can also be replaced with renderbuffer depth readback and linearization to save 2-4 bytes
typedef struct std3DFramebuffer
{
    GLuint fbo;
    GLuint tex0; // color (transparencies, etc)
    GLuint tex1; // emissive
    GLuint tex2; // position/linear depth (if VIEW_SPACE_GBUFFER)
    GLuint tex3; // normals
#ifdef VIEW_SPACE_GBUFFER
	GLuint tex4; // diffuse
#endif

    std3DIntermediateFbo window;
    std3DIntermediateFbo main;

	std3DIntermediateFbo postfx; // temporary composite space for postfx

    int enable_extra;
    std3DIntermediateFbo blur1;
    std3DIntermediateFbo blur2;
    std3DIntermediateFbo blur3;
    std3DIntermediateFbo blur4;

#ifdef NEW_BLOOM
	//std3DIntermediateFbo blur5;
	//std3DIntermediateFbo blur6;
	//std3DIntermediateFbo blur7;
	//std3DIntermediateFbo blur8;
#endif
    //std3DIntermediateFbo blurBlend;

    std3DIntermediateFbo ssaoBlur1;
    std3DIntermediateFbo ssaoBlur2;
    //std3DIntermediateFbo ssaoBlur3;
	std3DIntermediateFbo halfDepth;

#if defined(DECAL_RENDERING)
	std3DIntermediateFbo decalLight;
#endif

    GLuint rbo;
    int32_t w;
    int32_t h;
} std3DFramebuffer;

GLint std3D_windowFbo = 0;
std3DFramebuffer std3D_framebuffers[2];
std3DFramebuffer *std3D_pFb = NULL;

static bool has_initted = false;

static void* last_overlay = NULL;

static int std3D_activeFb = 1;

int init_once = 0;
GLuint programDefault, programMenu;
GLint attribute_coord3d, attribute_v_color, attribute_v_light, attribute_v_uv, attribute_v_norm;
#ifdef VIEW_SPACE_GBUFFER
GLint attribute_coordVS;
#endif
GLint uniform_mvp, uniform_tex, uniform_texEmiss, uniform_displacement_map, uniform_tex_mode, uniform_blend_mode, uniform_worldPalette, uniform_worldPaletteLights;
GLint uniform_tint, uniform_filter, uniform_fade, uniform_add, uniform_emissiveFactor, uniform_albedoFactor;
GLint uniform_light_mult, uniform_displacement_factor, uniform_iResolution, uniform_enableDither;
#ifdef FOG
GLint uniform_fog, uniform_fog_color, uniform_fog_start, uniform_fog_end;
#endif

GLint programMenu_attribute_coord3d, programMenu_attribute_v_color, programMenu_attribute_v_uv, programMenu_attribute_v_norm;
GLint programMenu_uniform_mvp, programMenu_uniform_tex, programMenu_uniform_displayPalette;

std3DSimpleTexStage std3D_uiProgram;
std3DSimpleTexStage std3D_texFboStage;
std3DSimpleTexStage std3D_blurStage;
std3DSimpleTexStage std3D_ssaoStage;
std3DSimpleTexStage std3D_ssaoMixStage;
std3DSimpleTexStage std3D_postfxStage;
#ifdef NEW_BLOOM
std3DSimpleTexStage std3D_bloomStage;
#endif

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
rdVector3* tiledrand_data = NULL;

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

D3DVERTEX* world_data_all = NULL;
GLushort* world_data_elements = NULL;
GLuint world_vao;
GLuint world_vbo_all;
GLuint world_ibo_triangle;

D3DVERTEX* menu_data_all = NULL;
GLushort* menu_data_elements = NULL;
GLuint menu_vao;
GLuint menu_vbo_all;
GLuint menu_ibo_triangle;

extern int jkGuiBuildMulti_bRendering;

int std3D_bInitted = 0;
rdColormap std3D_ui_colormap;
int std3D_bReinitHudElements = 0;

#ifdef DEFERRED_FRAMEWORK
typedef struct std3D_deferredStage
{
	GLuint vao;
	GLuint program;
	GLint attribute_coord3d;
	GLint uniform_texDepth, uniform_texLight, uniform_texDiffuse, uniform_texNormal; // gbuffer textures
	GLint uniform_tex, uniform_worldPalette, uniform_texmode; // optional texture
	GLint uniform_mvp, uniform_iResolution; // projection stuff
	GLint uniform_tint, uniform_filter, uniform_fade, uniform_add; // for emissive, todo: remove me
	GLint uniform_flags, uniform_objectMatrix, uniform_position, uniform_radius, uniform_color; // general volume params
	GLint uniform_rt, uniform_lt, uniform_rb, uniform_lb; // frustum corner rays for position reconstruction
} std3D_deferredStage;

static GLuint deferred_vbo;
static GLuint deferred_ibo;
static std3D_deferredStage std3D_stencilStage;
static int canUseDepthStencil = 1; // true until zbuffer is cleared mid-frame, which invalidates depth content

void std3D_setupDeferred()
{
	// triangle indices for deferred cube
	rdTri deferred_tmpTris[12];
	deferred_tmpTris[0].v1 = 0;
	deferred_tmpTris[0].v2 = 1;
	deferred_tmpTris[0].v3 = 2;
	deferred_tmpTris[1].v1 = 2;
	deferred_tmpTris[1].v2 = 3;
	deferred_tmpTris[1].v3 = 0;
	deferred_tmpTris[2].v1 = 1;
	deferred_tmpTris[2].v2 = 5;
	deferred_tmpTris[2].v3 = 6;
	deferred_tmpTris[3].v1 = 6;
	deferred_tmpTris[3].v2 = 2;
	deferred_tmpTris[3].v3 = 1;
	deferred_tmpTris[4].v1 = 7;
	deferred_tmpTris[4].v2 = 6;
	deferred_tmpTris[4].v3 = 5;
	deferred_tmpTris[5].v1 = 5;
	deferred_tmpTris[5].v2 = 4;
	deferred_tmpTris[5].v3 = 7;
	deferred_tmpTris[6].v1 = 4;
	deferred_tmpTris[6].v2 = 0;
	deferred_tmpTris[6].v3 = 3;
	deferred_tmpTris[7].v1 = 3;
	deferred_tmpTris[7].v2 = 7;
	deferred_tmpTris[7].v3 = 4;
	deferred_tmpTris[8].v1 = 4;
	deferred_tmpTris[8].v2 = 5;
	deferred_tmpTris[8].v3 = 1;
	deferred_tmpTris[9].v1 = 1;
	deferred_tmpTris[9].v2 = 0;
	deferred_tmpTris[9].v3 = 4;
	deferred_tmpTris[10].v1 = 3;
	deferred_tmpTris[10].v2 = 2;
	deferred_tmpTris[10].v3 = 6;
	deferred_tmpTris[11].v1 = 6;
	deferred_tmpTris[11].v2 = 7;
	deferred_tmpTris[11].v3 = 3;

	GLushort data_elements[12 * 3];
	rdTri* tris = deferred_tmpTris;
	for (int j = 0; j < 12; j++)
	{
		data_elements[(j * 3) + 0] = tris[j].v1;
		data_elements[(j * 3) + 1] = tris[j].v2;
		data_elements[(j * 3) + 2] = tris[j].v3;
	}

	glGenBuffers(1, &deferred_ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, deferred_ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 12 * 3 * sizeof(GLushort), data_elements, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glGenBuffers(1, &deferred_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, deferred_vbo);
	glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(D3DVERTEX), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void std3D_freeDeferred()
{
	glDeleteBuffers(1, &deferred_vbo);
	glDeleteBuffers(1, &deferred_ibo);
}

#endif

#ifdef DECAL_RENDERING
int lightBufferDirty = 0;
std3D_deferredStage std3D_decalStage;
#endif

#ifdef PARTICLE_LIGHTS
std3D_deferredStage std3D_lightStage;
#endif

#ifdef SPHERE_AO
std3D_deferredStage std3D_occluderStage[2]; // 0 = 32 bit, 1 = 16 bit
#endif

static bool std3D_isIntegerFormat(GLuint format)
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

static GLuint std3D_getUploadFormat(GLuint format)
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

static uint8_t std3D_getNumChannels(GLuint format)
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

static GLuint std3D_getImageFormat(GLuint format)
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

static void std3D_PushDebugGroup(const char* name)
{
	if(GLEW_KHR_debug)
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
}

static void std3D_PopDebugGroup()
{
	if(GLEW_KHR_debug)
		glPopDebugGroup();
}

void std3D_generateIntermediateFbo(int32_t width, int32_t height, std3DIntermediateFbo* pFbo, uint32_t format, int mipMaps, int useDepth)
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    if(mipMaps)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    // Attach fbTex to our currently bound framebuffer fb
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pFbo->tex, 0);

    // Set up our render buffer
	if(useDepth)
	{
		glGenRenderbuffers(1, &pFbo->rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, pFbo->rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
    
		// Bind it to our framebuffer fb
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, pFbo->rbo);
	}

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        stdPlatform_Printf("std3D: ERROR, Framebuffer is incomplete!\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void std3D_deleteIntermediateFbo(std3DIntermediateFbo* pFbo)
{
    glDeleteFramebuffers(1, &pFbo->fbo);
    glDeleteTextures(1, &pFbo->tex);
    glDeleteRenderbuffers(1, &pFbo->rbo);
}

void std3D_generateFramebuffer(int32_t width, int32_t height, std3DFramebuffer* pFb)
{
    // Generate the framebuffer
    memset(pFb, 0, sizeof(*pFb));

    pFb->w = width;
    pFb->h = height;

    glActiveTexture(GL_TEXTURE0);

    glGenFramebuffers(1, &pFb->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, pFb->fbo);
    
    // Set up our framebuffer texture
	// we never really use the alpha channel, so for 32bit we use deep color (rgb10a20, and for 16bit we use high color (rgb5a1, to avoid green shift)
    glGenTextures(1, &pFb->tex0);
    glBindTexture(GL_TEXTURE_2D, pFb->tex0);
    glTexImage2D(GL_TEXTURE_2D, 0, jkPlayer_enable32Bit ? GL_RGB10_A2 : GL_RGB5_A1, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	if(jkPlayer_enable32Bit)
		pFb->enable_extra |= 4;
	else
		pFb->enable_extra &= ~4;

    // Attach fbTex to our currently bound framebuffer fb
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pFb->tex0, 0);

    // Set up our emissive fb texture
    glGenTextures(1, &pFb->tex1);
    glBindTexture(GL_TEXTURE_2D, pFb->tex1);
    glTexImage2D(GL_TEXTURE_2D, 0, jkPlayer_enable32Bit ? GL_RGB10_A2 : GL_RGB5_A1, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
    glGenerateMipmap(GL_TEXTURE_2D);
    //glGenerateMipmap(GL_TEXTURE_2D);
    
    // Attach fbTex to our currently bound framebuffer fb
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, pFb->tex1, 0);

#ifdef VIEW_SPACE_GBUFFER
	// Set up our depth fb texture
    glGenTextures(1, &pFb->tex2);
    glBindTexture(GL_TEXTURE_2D, pFb->tex2);
    glTexImage2D(GL_TEXTURE_2D, 0, jkPlayer_enable32Bit ? GL_R32F : GL_R16F, width, height, 0, GL_RED, GL_FLOAT, NULL); // juse use 16 or 32 based on jkPlayer_enable32Bit because why not
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Attach fbTex to our currently bound framebuffer fb
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, pFb->tex2, 0);

    // Set up our normal fb texture
	glGenTextures(1, &pFb->tex3);
    glBindTexture(GL_TEXTURE_2D, pFb->tex3);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, width, height, 0, GL_RG, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Attach fbTex to our currently bound framebuffer fb
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, pFb->tex3, 0);

	// diffuse color buffer
	glGenTextures(1, &pFb->tex4);
	glBindTexture(GL_TEXTURE_2D, pFb->tex4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Attach fbTex to our currently bound framebuffer fb
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, pFb->tex4, 0);
#else
	// Set up our position fb texture
	glGenTextures(1, &pFb->tex2);
	glBindTexture(GL_TEXTURE_2D, pFb->tex2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Attach fbTex to our currently bound framebuffer fb
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, pFb->tex2, 0);

	glGenTextures(1, &pFb->tex3);
	glBindTexture(GL_TEXTURE_2D, pFb->tex3);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif

    // Set up our render buffer
    glGenRenderbuffers(1, &pFb->rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, pFb->rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    
    // Bind it to our framebuffer fb
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, pFb->rbo);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        stdPlatform_Printf("std3D: ERROR, Framebuffer is incomplete!\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

#ifdef DECAL_RENDERING
	std3D_generateIntermediateFbo(width, height, &pFb->decalLight, jkPlayer_enable32Bit ? GL_RGB10_A2 : GL_RGB5_A1, 0, 0);
#endif

    if (jkPlayer_enableSSAO)
    {
#ifdef NEW_SSAO
		std3D_generateIntermediateFbo(width / 2, height / 2, &pFb->ssaoBlur1, GL_R8, 0, 0);
		std3D_generateIntermediateFbo(pFb->ssaoBlur1.w, pFb->ssaoBlur1.h, &pFb->ssaoBlur2, GL_R8, 0, 0);
		std3D_generateIntermediateFbo(pFb->ssaoBlur2.w, pFb->ssaoBlur2.h, &pFb->halfDepth, GL_R16F, 0, 0);
#else
        std3D_generateIntermediateFbo(width, height, &pFb->ssaoBlur1, GL_R8, 0, 0);
        std3D_generateIntermediateFbo(pFb->ssaoBlur1.w/2, pFb->ssaoBlur1.h/2, &pFb->ssaoBlur2, GL_R8, 0, 0);
#endif
		//std3D_generateIntermediateFbo(pFb->ssaoBlur2.w/2, pFb->ssaoBlur2.h/2, &pFb->ssaoBlur3, GL_R8, 0, 0);

        pFb->enable_extra |= 2;
    }
	else
		pFb->enable_extra &= ~2;

    if (jkPlayer_enableBloom)
    {
        pFb->enable_extra |= 1;
	#ifdef NEW_BLOOM
		std3D_generateIntermediateFbo(width / 4, height / 4, &pFb->blur1, GL_RGBA16F, 0, 0);
		std3D_generateIntermediateFbo(pFb->blur1.w / 2, pFb->blur1.h / 2, &pFb->blur2, GL_RGBA16F, 0, 0);
		std3D_generateIntermediateFbo(pFb->blur2.w / 2, pFb->blur2.h / 2, &pFb->blur3, GL_RGBA16F, 0, 0);
		std3D_generateIntermediateFbo(pFb->blur3.w / 2, pFb->blur3.h / 2, &pFb->blur4, GL_RGBA16F, 0, 0);
		//std3D_generateIntermediateFbo(pFb->blur4.w / 2, pFb->blur4.h / 2, &pFb->blur5,GL_RGBA16F, 0, 0);
		//std3D_generateIntermediateFbo(pFb->blur5.w / 2, pFb->blur5.h / 2, &pFb->blur6,GL_RGBA16F, 0, 0);
		//std3D_generateIntermediateFbo(pFb->blur6.w / 2, pFb->blur6.h / 2, &pFb->blur7,GL_RGBA16F, 0, 0);
		//std3D_generateIntermediateFbo(pFb->blur7.w / 2, pFb->blur7.h / 2, &pFb->blur8,GL_RGBA16F, 0, 0);
	#else
        std3D_generateIntermediateFbo(width, height, &pFb->blur1, GL_RGBA16F, 1, 0);
        //std3D_generateIntermediateFbo(width, height, &pFb->blurBlend, 1);
        std3D_generateIntermediateFbo(pFb->blur1.w/4, pFb->blur1.h/4, &pFb->blur2, GL_RGBA16F, 1, 0);
        std3D_generateIntermediateFbo(pFb->blur2.w/4, pFb->blur2.h/4, &pFb->blur3, GL_RGBA16F, 1, 0);
        std3D_generateIntermediateFbo(pFb->blur3.w/4, pFb->blur3.h/4, &pFb->blur4, GL_RGBA16F, 1, 0);
	#endif

        /*pFb->blur1.iw = width;
        pFb->blur1.ih = height;
        pFb->blur2.iw = width;
        pFb->blur2.ih = height;
        pFb->blur3.iw = width;
        pFb->blur3.ih = height;
        pFb->blur4.iw = width;
        pFb->blur4.ih = height;*/
    }
	else
		pFb->enable_extra &= ~1;

	std3D_generateIntermediateFbo(width, height, &pFb->postfx, GL_RGB10_A2, 0, 0);

    pFb->main.fbo = pFb->fbo;
    pFb->main.tex = pFb->tex1;
    pFb->main.rbo = pFb->rbo;
    pFb->main.w = pFb->w;
    pFb->main.h = pFb->h;
    pFb->main.iw = pFb->w;
    pFb->main.ih = pFb->h;

    pFb->window.fbo = std3D_windowFbo;
    pFb->window.w = Window_xSize;
    pFb->window.h = Window_ySize;
    pFb->window.iw = Window_xSize;
    pFb->window.ih = Window_ySize;
}

void std3D_deleteFramebuffer(std3DFramebuffer* pFb)
{
    glDeleteFramebuffers(1, &pFb->fbo);
    glDeleteTextures(1, &pFb->tex0);
    glDeleteTextures(1, &pFb->tex1);
    glDeleteTextures(1, &pFb->tex2);
    glDeleteTextures(1, &pFb->tex3);
#ifdef VIEW_SPACE_GBUFFER
	glDeleteTextures(1, &pFb->tex4);
#endif
    glDeleteRenderbuffers(1, &pFb->rbo);

    std3D_deleteIntermediateFbo(&pFb->blur1);
    std3D_deleteIntermediateFbo(&pFb->blur2);
    std3D_deleteIntermediateFbo(&pFb->blur3);
    std3D_deleteIntermediateFbo(&pFb->blur4);
    //std3D_deleteIntermediateFbo(&pFb->blurBlend);
#ifdef NEW_BLOOM
	//std3D_deleteIntermediateFbo(&pFb->blur5);
	//std3D_deleteIntermediateFbo(&pFb->blur6);
	//std3D_deleteIntermediateFbo(&pFb->blur7);
	//std3D_deleteIntermediateFbo(&pFb->blur8);
#endif

    std3D_deleteIntermediateFbo(&pFb->ssaoBlur1);
    std3D_deleteIntermediateFbo(&pFb->ssaoBlur2);
	std3D_deleteIntermediateFbo(&pFb->halfDepth);
    //std3D_deleteIntermediateFbo(&pFb->ssaoBlur3);

	std3D_deleteIntermediateFbo(&pFb->postfx);

#ifdef DECAL_RENDERING
	std3D_deleteIntermediateFbo(&pFb->decalLight);
#endif
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

	std3D_PushDebugGroup("std3D_BlitDrawSurface");
		
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

	std3D_PopDebugGroup();
}

void std3D_ClearDrawSurface(std3D_DrawSurface* surface, int fillColor, rdRect* rect)
{
	std3D_PushDebugGroup("std3D_ClearDrawSurface");

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
	
	std3D_PopDebugGroup();
}
#endif

void std3D_swapFramebuffers()
{
    if (std3D_activeFb == 2)
    {
        std3D_activeFb = 1;
        std3D_pFb = &std3D_framebuffers[0];
    }
    else
    {
        std3D_activeFb = 2;
        std3D_pFb = &std3D_framebuffers[1];
    }
}

GLuint std3D_loadProgram(const char* fpath_base, const char* userDefines)
{
    GLuint out;
    GLint link_ok = GL_FALSE;
    
    char* tmp_vert = (char*)malloc(strlen(fpath_base) + 32);
    char* tmp_frag = (char*)malloc(strlen(fpath_base) + 32);
    
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
    pOut->uniform_iResolution = std3D_tryFindUniform(pOut->program, "iResolution");
    pOut->uniform_tex = std3D_tryFindUniform(pOut->program, "tex");
    pOut->uniform_tex2 = std3D_tryFindUniform(pOut->program, "tex2");
    pOut->uniform_tex3 = std3D_tryFindUniform(pOut->program, "tex3");
	pOut->uniform_tex4 = std3D_tryFindUniform(pOut->program, "tex4");

    pOut->uniform_param1 = std3D_tryFindUniform(pOut->program, "param1");
    pOut->uniform_param2 = std3D_tryFindUniform(pOut->program, "param2");
    pOut->uniform_param3 = std3D_tryFindUniform(pOut->program, "param3");

#ifdef VIEW_SPACE_GBUFFER
	pOut->uniform_rt = std3D_tryFindUniform(pOut->program, "cameraRT");
	pOut->uniform_lt = std3D_tryFindUniform(pOut->program, "cameraLT");
	pOut->uniform_rb = std3D_tryFindUniform(pOut->program, "cameraRB");
	pOut->uniform_lb = std3D_tryFindUniform(pOut->program, "cameraLB");
#endif

    return true;
}

#ifdef DEFERRED_FRAMEWORK
bool std3D_loadDeferredProgram(const char* fpath_base, std3D_deferredStage* pOut, const char* defines)
{
	if (!pOut) return false;
	if ((pOut->program = std3D_loadProgram(fpath_base, defines)) == 0) return false;

	pOut->attribute_coord3d = std3D_tryFindAttribute(pOut->program, "coord3d");
	
	pOut->uniform_mvp = std3D_tryFindUniform(pOut->program, "mvp");
	pOut->uniform_iResolution = std3D_tryFindUniform(pOut->program, "iResolution");
	
	pOut->uniform_texDepth = std3D_tryFindUniform(pOut->program, "texDepth");
	pOut->uniform_texLight = std3D_tryFindUniform(pOut->program, "texLight");
	pOut->uniform_texDiffuse = std3D_tryFindUniform(pOut->program, "texDiffuse");
	pOut->uniform_texNormal = std3D_tryFindUniform(pOut->program, "texNormal");
	
	pOut->uniform_worldPalette = std3D_tryFindUniform(pOut->program, "texPalette");
	pOut->uniform_tex = std3D_tryFindUniform(pOut->program, "tex");
	pOut->uniform_texmode = std3D_tryFindUniform(pOut->program, "texMode");

	pOut->uniform_tint = std3D_tryFindUniform(pOut->program, "colorEffects_tint");
	pOut->uniform_filter = std3D_tryFindUniform(pOut->program, "colorEffects_filter");
	pOut->uniform_fade = std3D_tryFindUniform(pOut->program, "colorEffects_fade");
	pOut->uniform_add = std3D_tryFindUniform(pOut->program, "colorEffects_add");

	pOut->uniform_rt = std3D_tryFindUniform(pOut->program, "cameraRT");
	pOut->uniform_lt = std3D_tryFindUniform(pOut->program, "cameraLT");
	pOut->uniform_rb = std3D_tryFindUniform(pOut->program, "cameraRB");
	pOut->uniform_lb = std3D_tryFindUniform(pOut->program, "cameraLB");

	pOut->uniform_flags = std3D_tryFindUniform(pOut->program, "volumeFlags");
	pOut->uniform_position = std3D_tryFindUniform(pOut->program, "volumePosition");
	pOut->uniform_radius = std3D_tryFindUniform(pOut->program, "volumeRadius");
	pOut->uniform_color = std3D_tryFindUniform(pOut->program, "volumeColor");
	pOut->uniform_objectMatrix = std3D_tryFindUniform(pOut->program, "volumeInvMatrix");

	glGenVertexArrays(1, &pOut->vao);
	glBindVertexArray(pOut->vao);

	glBindBuffer(GL_ARRAY_BUFFER, deferred_vbo);

	glVertexAttribPointer(
		pOut->attribute_coord3d, // attribute
		3, // number of elements per vertex, here (x,y,z)
		GL_FLOAT, // the type of each element
		GL_FALSE, // normalize fixed-point data?
		sizeof(D3DVERTEX), // data stride
		(GLvoid*)offsetof(D3DVERTEX, x) // offset of first element
	);
	glEnableVertexAttribArray(pOut->attribute_coord3d);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, deferred_ibo);

	glBindVertexArray(vao);

	return true;
}
#endif

void std3D_setupWorldVAO()
{
	glGenVertexArrays(1, &world_vao);
	glBindVertexArray(world_vao);

	// Describe our vertices array to OpenGL (it can't guess its format automatically)
	glBindBuffer(GL_ARRAY_BUFFER, world_vbo_all);
	glVertexAttribPointer(
		attribute_coord3d, // attribute
		3,                 // number of elements per vertex, here (x,y,z)
		GL_FLOAT,          // the type of each element
		GL_FALSE,          // normalize fixed-point data?
		sizeof(D3DVERTEX),                 // data stride
		(GLvoid*)offsetof(D3DVERTEX, x)                  // offset of first element
	);

	glVertexAttribPointer(
		attribute_v_color, // attribute
		4,                 // number of elements per vertex, here (R,G,B,A)
		GL_UNSIGNED_BYTE,  // the type of each element
		GL_TRUE,          // normalize fixed-point data?
		sizeof(D3DVERTEX),                 // no extra data between each position
		(GLvoid*)offsetof(D3DVERTEX, color) // offset of first element
	);

	glVertexAttribPointer(
		attribute_v_light, // attribute
		1,                 // number of elements per vertex, here (L)
		GL_FLOAT,  // the type of each element
		GL_FALSE,          // normalize fixed-point data?
		sizeof(D3DVERTEX),                 // no extra data between each position
		(GLvoid*)offsetof(D3DVERTEX, lightLevel) // offset of first element
	);

	glVertexAttribPointer(
		attribute_v_uv,    // attribute
		2,                 // number of elements per vertex, here (U,V)
		GL_FLOAT,          // the type of each element
		GL_FALSE,          // take our values as-is
		sizeof(D3DVERTEX),                 // no extra data between each position
		(GLvoid*)offsetof(D3DVERTEX, tu)                  // offset of first element
	);

#ifdef VIEW_SPACE_GBUFFER
	glVertexAttribPointer(
		attribute_coordVS, // attribute
		3,                 // number of elements per vertex, here (x,y,z)
		GL_FLOAT,          // the type of each element
		GL_FALSE,          // normalize fixed-point data?
		sizeof(D3DVERTEX),                 // data stride
		(GLvoid*)offsetof(D3DVERTEX, vx)                  // offset of first element
	);
#endif

	glEnableVertexAttribArray(attribute_coord3d);
	glEnableVertexAttribArray(attribute_v_color);
	glEnableVertexAttribArray(attribute_v_light);
	glEnableVertexAttribArray(attribute_v_uv);

#ifdef VIEW_SPACE_GBUFFER
	glEnableVertexAttribArray(attribute_coordVS);
#endif

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, world_ibo_triangle);

	glBindVertexArray(vao);
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

int init_resources()
{
    stdPlatform_Printf("std3D: OpenGL init...\n");

    std3D_bReinitHudElements = 1;

    memset(std3D_aUITextures, 0, sizeof(std3D_aUITextures));

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &std3D_windowFbo);

    int32_t tex_w = Window_xSize;
    int32_t tex_h = Window_ySize;

    std3D_generateFramebuffer(tex_w, tex_h, &std3D_framebuffers[0]);
    std3D_generateFramebuffer(tex_w, tex_h, &std3D_framebuffers[1]);

    std3D_activeFb = 1;
    std3D_pFb = &std3D_framebuffers[0];
    
    if ((programDefault = std3D_loadProgram(/*"shaders/default"*/"shaders/software", "")) == 0) return false;
    if ((programMenu = std3D_loadProgram("shaders/menu", "")) == 0) return false;

    if (!std3D_loadSimpleTexProgram("shaders/ui", &std3D_uiProgram)) return false;
    if (!std3D_loadSimpleTexProgram("shaders/texfbo", &std3D_texFboStage)) return false;
    if (!std3D_loadSimpleTexProgram("shaders/blur", &std3D_blurStage)) return false;
    if (!std3D_loadSimpleTexProgram("shaders/ssao", &std3D_ssaoStage)) return false;
    if (!std3D_loadSimpleTexProgram("shaders/ssao_mix", &std3D_ssaoMixStage)) return false;
	if (!std3D_loadSimpleTexProgram("shaders/postfx/postfx", &std3D_postfxStage)) return false;
#ifdef NEW_BLOOM
	if (!std3D_loadSimpleTexProgram("shaders/postfx/bloom", &std3D_bloomStage)) return false;
#endif

#ifdef DEFERRED_FRAMEWORK
	std3D_setupDeferred();
#endif

#ifdef DECAL_RENDERING
	if (!std3D_loadDeferredProgram("shaders/decal", &std3D_decalStage, "")) return false;
#endif
#ifdef PARTICLE_LIGHTS
	if (!std3D_loadDeferredProgram("shaders/light", &std3D_lightStage, "")) return false;
#endif
#ifdef SPHERE_AO
	if (!std3D_loadDeferredProgram("shaders/occ", &std3D_occluderStage[0], "TRUECOLOR")) return false;
	if (!std3D_loadDeferredProgram("shaders/occ", &std3D_occluderStage[1], "HIGHCOLOR")) return false;
#endif

#ifdef DEFERRED_FRAMEWORK
	if (!std3D_loadDeferredProgram("shaders/stencil", &std3D_stencilStage, "")) return false;
#endif

    // Attributes/uniforms
    attribute_coord3d = std3D_tryFindAttribute(programDefault, "coord3d");
#ifdef VIEW_SPACE_GBUFFER
	attribute_coordVS = std3D_tryFindAttribute(programDefault, "coordVS");
#endif
    attribute_v_color = std3D_tryFindAttribute(programDefault, "v_color");
    attribute_v_light = std3D_tryFindAttribute(programDefault, "v_light");
    attribute_v_uv = std3D_tryFindAttribute(programDefault, "v_uv");
    uniform_mvp = std3D_tryFindUniform(programDefault, "mvp");
    uniform_tex = std3D_tryFindUniform(programDefault, "tex");
    uniform_texEmiss = std3D_tryFindUniform(programDefault, "texEmiss");
    uniform_worldPalette = std3D_tryFindUniform(programDefault, "worldPalette");
    uniform_worldPaletteLights = std3D_tryFindUniform(programDefault, "worldPaletteLights");
    uniform_displacement_map = std3D_tryFindUniform(programDefault, "displacement_map");
    uniform_tex_mode = std3D_tryFindUniform(programDefault, "tex_mode");
    uniform_blend_mode = std3D_tryFindUniform(programDefault, "blend_mode");
    uniform_tint = std3D_tryFindUniform(programDefault, "colorEffects_tint");
    uniform_filter = std3D_tryFindUniform(programDefault, "colorEffects_filter");
    uniform_fade = std3D_tryFindUniform(programDefault, "colorEffects_fade");
    uniform_add = std3D_tryFindUniform(programDefault, "colorEffects_add");
    uniform_emissiveFactor = std3D_tryFindUniform(programDefault, "emissiveFactor");
    uniform_albedoFactor = std3D_tryFindUniform(programDefault, "albedoFactor");
    uniform_light_mult = std3D_tryFindUniform(programDefault, "light_mult");
    uniform_displacement_factor = std3D_tryFindUniform(programDefault, "displacement_factor");
    uniform_iResolution = std3D_tryFindUniform(programDefault, "iResolution");
	uniform_enableDither = std3D_tryFindUniform(programDefault, "enableDither");
#ifdef FOG
	uniform_fog = std3D_tryFindUniform(programDefault, "fogEnabled");
	uniform_fog_color = std3D_tryFindUniform(programDefault, "fogColor");
	uniform_fog_start = std3D_tryFindUniform(programDefault, "fogStart");
	uniform_fog_end = std3D_tryFindUniform(programDefault, "fogEnd");
#endif
    
    programMenu_attribute_coord3d = std3D_tryFindAttribute(programMenu, "coord3d");
    programMenu_attribute_v_color = std3D_tryFindAttribute(programMenu, "v_color");
    programMenu_attribute_v_uv = std3D_tryFindAttribute(programMenu, "v_uv");
    programMenu_uniform_mvp = std3D_tryFindUniform(programMenu, "mvp");
    programMenu_uniform_tex = std3D_tryFindUniform(programMenu, "tex");
    programMenu_uniform_displayPalette = std3D_tryFindUniform(programMenu, "displayPalette");
   
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
    // FLEXTODO
    glGenTextures(1, &tiledrand_texture);
    if (tiledrand_data) {
        free(tiledrand_data);
    }
    tiledrand_data = (rdVector3*)malloc(3 * 4 * 4 * sizeof(float));
    memset(tiledrand_data, 0, 3 * 4 * 4 * sizeof(float));

    for (int i = 0; i < 4*4; i++)
    {
        tiledrand_data[i].x = (_frand() * 2.0) - 1.0;
        tiledrand_data[i].y = (_frand() * 2.0) - 1.0;
        tiledrand_data[i].z = 0.0;
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

    glGenVertexArrays( 1, &vao );
    glBindVertexArray( vao ); 

    world_data_all = (D3DVERTEX*)malloc(STD3D_MAX_VERTICES * sizeof(D3DVERTEX));
    world_data_elements = (GLushort*)malloc(sizeof(GLushort) * 3 * STD3D_MAX_TRIS);

    menu_data_all = (D3DVERTEX*)malloc(STD3D_MAX_UI_VERTICES * sizeof(D3DVERTEX));
    menu_data_elements = (GLushort*)malloc(sizeof(GLushort) * 3 * STD3D_MAX_UI_TRIS);

    glGenBuffers(1, &world_vbo_all);
    glGenBuffers(1, &world_ibo_triangle);

    glGenBuffers(1, &menu_vbo_all);
    glGenBuffers(1, &menu_ibo_triangle);

	std3D_setupWorldVAO();
	std3D_setupMenuVAO();

    has_initted = true;
    return true;
}

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
    std3D_PurgeEntireTextureCache();

    glDeleteProgram(programDefault);
    glDeleteProgram(programMenu);
    std3D_deleteFramebuffer(&std3D_framebuffers[0]);
    std3D_deleteFramebuffer(&std3D_framebuffers[1]);
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

    blank_data = NULL;
    blank_data_white = NULL;
    worldpal_data = NULL;
    worldpal_lights_data = NULL;
    displaypal_data = NULL;

    if (world_data_all)
        free(world_data_all);
    world_data_all = NULL;

    if (world_data_elements)
        free(world_data_elements);
    world_data_elements = NULL;

    if (menu_data_all)
        free(menu_data_all);
    menu_data_all = NULL;

    if (menu_data_elements)
        free(menu_data_elements);
    menu_data_elements = NULL;

    loaded_colormap = NULL;

    glDeleteBuffers(1, &world_vbo_all);
    glDeleteBuffers(1, &world_ibo_triangle);

    glDeleteBuffers(1, &menu_vbo_all);

#ifdef DECAL_RENDERING
	glDeleteProgram(std3D_decalStage.program);
#endif
#ifdef PARTICLE_LIGHTS
	glDeleteProgram(std3D_lightStage.program);
#endif
#ifdef SPHERE_AO
	glDeleteProgram(std3D_occluderStage[0].program);
	glDeleteProgram(std3D_occluderStage[1].program);
#endif
#ifdef DEFERRED_FRAMEWORK
	glDeleteProgram(std3D_stencilStage.program);
	std3D_freeDeferred();
#endif
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

    ++std3D_frameCount;

    //printf("Begin draw\n");
    if (!has_initted)
    {
        if (!init_resources()) {
            stdPlatform_Printf("std3D: Failed to init resources, exiting...");
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to init resources, exiting...", NULL);
            exit(-1);
        }
    }
    
    rendered_tris = 0;
    
    std3D_swapFramebuffers();
    
    double supersample_level = jkPlayer_ssaaMultiple; // Can also be set lower
    int32_t tex_w = (int32_t)((double)Window_xSize * supersample_level);
    int32_t tex_h = (int32_t)((double)Window_ySize * supersample_level);

    if (tex_w != std3D_pFb->w || tex_h != std3D_pFb->h 
        || (((std3D_pFb->enable_extra & 1) == 1) != jkPlayer_enableBloom)
        || (((std3D_pFb->enable_extra & 2) == 2) != jkPlayer_enableSSAO)
		|| (((std3D_pFb->enable_extra & 4) == 4) != jkPlayer_enable32Bit))
    {
        std3D_deleteFramebuffer(std3D_pFb);
        std3D_generateFramebuffer(tex_w, tex_h, std3D_pFb);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->fbo);
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    glCullFace(GL_FRONT);
    //glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);

    // Technically this should be from Clear2
    glClearColor(0.0, 0.0, 0.0, 1.0);

	GLuint clearBits = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
	if(jkGuiBuildMulti_bRendering)
		clearBits |= GL_COLOR_BUFFER_BIT;

#ifdef STENCIL_BUFFER
	glClearStencil(0);
	glStencilMask(0xFF);
#endif
	glClear(clearBits);

#ifdef DEFERRED_FRAMEWORK
	canUseDepthStencil = 1;
#endif

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

#if 0
    // New random values
    glBindTexture(GL_TEXTURE_2D, tiledrand_texture);
    for (int i = 0; i < 4*4; i++)
    {
        tiledrand_data[i].x = (_frand() * 2.0) - 1.0;
        tiledrand_data[i].y = (_frand() * 2.0) - 1.0;
        tiledrand_data[i].z = 0.0;
        rdVector_Normalize3Acc(&tiledrand_data[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGB, GL_FLOAT, tiledrand_data);
    }
#endif

#ifdef DECAL_RENDERING
	lightBufferDirty = 1;
#endif

    return 1;
}

int std3D_EndScene()
{
    if (Main_bHeadless) {
        last_tex = NULL;
        last_flags = 0;
        std3D_ResetRenderList();
        return 1;
    }

    //printf("End draw\n");
    last_tex = NULL;
    last_flags = 0;
    std3D_ResetRenderList();
    //printf("%u tris\n", rendered_tris);
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

void std3D_ResetRenderList()
{
    rendered_tris += GL_tmpTrisAmt;

    GL_tmpVerticesAmt = 0;
    GL_tmpTrisAmt = 0;
    GL_tmpLinesAmt = 0;
    
    //memset(GL_tmpTris, 0, sizeof(GL_tmpTris));
    //memset(GL_tmpVertices, 0, sizeof(GL_tmpVertices));
}

int std3D_RenderListVerticesFinish()
{
    return 1;
}

void std3D_DrawMenuSubrect(flex_t x, flex_t y, flex_t w, flex_t h, flex_t dstX, flex_t dstY, flex_t scale)
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

void std3D_DrawMenuSubrect2(flex_t x, flex_t y, flex_t w, flex_t h, flex_t dstX, flex_t dstY, flex_t scale)
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
void std3D_DrawSimpleTex(std3DSimpleTexStage* pStage, std3DIntermediateFbo* pFbo, GLuint texId, GLuint texId2, GLuint texId3, flex_t param1, flex_t param2, flex_t param3, int gen_mips);
void std3D_DrawMapOverlay();
void std3D_DrawUIRenderList();

void std3D_DrawMenu()
{
    if (Main_bHeadless) return;

	std3D_PushDebugGroup("std3D_DrawMenu");

    //printf("Draw menu\n");
    std3D_DrawSceneFbo();
    //glFlush();

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
    
    rdDDrawSurface* last_tex = (rdDDrawSurface*)(intptr_t)-1;
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

	std3D_PopDebugGroup();
}

void std3D_DrawMapOverlay()
{
#ifndef TILE_SW_RASTER
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
    
    rdDDrawSurface* last_tex = (rdDDrawSurface*)(intptr_t)-1;
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
#endif
}


void std3D_DrawUIBitmapRGBAZ(stdBitmap* pBmp, int mipIdx, float dstX, float dstY, rdRect* srcRect, float scaleX, float scaleY, int bAlphaOverwrite, uint8_t color_r, uint8_t color_g, uint8_t color_b, uint8_t color_a, float depth)
{
	float internalWidth = Video_menuBuffer.format.width;
	float internalHeight = Video_menuBuffer.format.height;

	if (!pBmp) return;
	if (!pBmp->abLoadedToGPU[mipIdx])
	{
		std3D_AddBitmapToTextureCache(pBmp, mipIdx, !(pBmp->palFmt & 1), 0);
	}

	if (jkGuiBuildMulti_bRendering)
	{
		internalWidth = 640.0;
		internalHeight = 480.0;
	}

	double scaleX_ = (double)Window_xSize / (double)internalWidth;
	double scaleY_ = (double)Window_ySize / (double)internalHeight;

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

	if (srcRect)
	{
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
	double u2 = ((x + w) / tex_w);
	double v1 = (y / tex_h);
	double v2 = ((y + h) / tex_h);

	uint32_t color = 0;

	color |= (color_r << 0);
	color |= (color_g << 8);
	color |= (color_b << 16);
	color |= (color_a << 24);

	if (GL_tmpUIVerticesAmt + 4 > STD3D_MAX_UI_VERTICES)
	{
		return;
	}
	if (GL_tmpUITrisAmt + 2 > STD3D_MAX_UI_TRIS)
	{
		return;
	}

	if (dstY + (dstScaleY * h_dst) < 0.0 || dstX + (dstScaleX * w_dst) < 0.0)
	{
		return;
	}
	if (dstY > Window_ySize || dstX > Window_xSize)
	{
		return;
	}

	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 0].x = dstX;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 0].y = dstY;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 0].z = depth;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 0].tu = u1;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 0].tv = v1;
	*(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt + 0].nx = 0;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 0].color = color;
	*(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt + 0].nz = 0;

	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 1].x = dstX;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 1].y = dstY + (dstScaleY * h_dst);
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 1].z = depth;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 1].tu = u1;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 1].tv = v2;
	*(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt + 1].nx = 0;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 1].color = color;
	*(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt + 1].nz = 0;

	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 2].x = dstX + (dstScaleX * w_dst);
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 2].y = dstY + (dstScaleY * h_dst);
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 2].z = depth;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 2].tu = u2;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 2].tv = v2;
	*(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt + 2].nx = 0;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 2].color = color;
	*(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt + 2].nz = 0;

	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 3].x = dstX + (dstScaleX * w_dst);
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 3].y = dstY;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 3].z = depth;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 3].tu = u2;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 3].tv = v1;
	*(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt + 3].nx = 0;
	GL_tmpUIVertices[GL_tmpUIVerticesAmt + 3].color = color;
	*(uint32_t*)&GL_tmpUIVertices[GL_tmpUIVerticesAmt + 3].nz = 0;

	GL_tmpUITris[GL_tmpUITrisAmt + 0].v1 = GL_tmpUIVerticesAmt + 1;
	GL_tmpUITris[GL_tmpUITrisAmt + 0].v2 = GL_tmpUIVerticesAmt + 0;
	GL_tmpUITris[GL_tmpUITrisAmt + 0].v3 = GL_tmpUIVerticesAmt + 2;
	GL_tmpUITris[GL_tmpUITrisAmt + 0].flags = bAlphaOverwrite;
	GL_tmpUITris[GL_tmpUITrisAmt + 0].texture = pBmp->aTextureIds[mipIdx];
	GL_tmpUITris[GL_tmpUITrisAmt + 0].bm = pBmp;

	GL_tmpUITris[GL_tmpUITrisAmt + 1].v1 = GL_tmpUIVerticesAmt + 0;
	GL_tmpUITris[GL_tmpUITrisAmt + 1].v2 = GL_tmpUIVerticesAmt + 3;
	GL_tmpUITris[GL_tmpUITrisAmt + 1].v3 = GL_tmpUIVerticesAmt + 2;
	GL_tmpUITris[GL_tmpUITrisAmt + 1].flags = bAlphaOverwrite;
	GL_tmpUITris[GL_tmpUITrisAmt + 1].texture = pBmp->aTextureIds[mipIdx];
	GL_tmpUITris[GL_tmpUITrisAmt + 1].bm = pBmp;

	GL_tmpUIVerticesAmt += 4;
	GL_tmpUITrisAmt += 2;
}

void std3D_DrawUIBitmapRGBA(stdBitmap* pBmp, int mipIdx, float dstX, float dstY, rdRect* srcRect, float scaleX, float scaleY, int bAlphaOverwrite, uint8_t color_r, uint8_t color_g, uint8_t color_b, uint8_t color_a)
{
	std3D_DrawUIBitmapRGBAZ(pBmp, mipIdx, dstX, dstY, srcRect, scaleX, scaleY, bAlphaOverwrite, color_r, color_g, color_b, color_a, 0.5f);
}

void std3D_DrawUIBitmap(stdBitmap* pBmp, int mipIdx, float dstX, float dstY, rdRect* srcRect, float scale, int bAlphaOverwrite)
{
	std3D_DrawUIBitmapRGBA(pBmp, mipIdx, dstX, dstY, srcRect, scale, scale, bAlphaOverwrite, 0xFF, 0xFF, 0xFF, 0xFF);
}

void std3D_DrawUIBitmapZ(stdBitmap* pBmp, int mipIdx, float dstX, float dstY, rdRect* srcRect, float scaleX, float scaleY, int bAlphaOverwrite, float depth)
{
	std3D_DrawUIBitmapRGBAZ(pBmp, mipIdx, dstX, dstY, srcRect, scaleX, scaleY, bAlphaOverwrite, 0xFF, 0xFF, 0xFF, 0xFF, depth);
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

void std3D_DrawSimpleTex(std3DSimpleTexStage* pStage, std3DIntermediateFbo* pFbo, GLuint texId, GLuint texId2, GLuint texId3, flex_t param1, flex_t param2, flex_t param3, int gen_mips)
{
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

#ifdef VIEW_SPACE_GBUFFER
	glUniform3fv(pStage->uniform_rt, 1, (float*)&rdCamera_pCurCamera->pClipFrustum->rt);
	glUniform3fv(pStage->uniform_lt, 1, (float*)&rdCamera_pCurCamera->pClipFrustum->lt);
	glUniform3fv(pStage->uniform_rb, 1, (float*)&rdCamera_pCurCamera->pClipFrustum->rb);
	glUniform3fv(pStage->uniform_lb, 1, (float*)&rdCamera_pCurCamera->pClipFrustum->lb);
#endif
    }
    
    rdTri* tris = GL_tmpTris;
    glEnableVertexAttribArray(pStage->attribute_coord3d);
    glEnableVertexAttribArray(pStage->attribute_v_color);
    glEnableVertexAttribArray(pStage->attribute_v_uv);

    glBindBuffer(GL_ARRAY_BUFFER, menu_vbo_all);
    glBufferData(GL_ARRAY_BUFFER, GL_tmpVerticesAmt * sizeof(D3DVERTEX), GL_tmpVertices, GL_STREAM_DRAW);
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

    glVertexAttribPointer(
        programMenu_attribute_v_uv,    // attribute
        2,                 // number of elements per vertex, here (U,V)
        GL_FLOAT,          // the type of each element
        GL_FALSE,          // take our values as-is
        sizeof(D3DVERTEX),                 // no extra data between each position
        (GLvoid*)offsetof(D3DVERTEX, tu)                  // offset of first element
    );

    glEnableVertexAttribArray(pStage->attribute_coord3d);
    glEnableVertexAttribArray(pStage->attribute_v_color);
    glEnableVertexAttribArray(pStage->attribute_v_uv);
    
    rdDDrawSurface* last_tex = (rdDDrawSurface*)(intptr_t)-1;
    int last_tex_idx = 0;
    //GLushort* data_elements = malloc(sizeof(GLushort) * 3 * GL_tmpTrisAmt);
    for (int j = 0; j < GL_tmpTrisAmt; j++)
    {
        data_elements[(j*3)+0] = tris[j].v1;
        data_elements[(j*3)+1] = tris[j].v2;
        data_elements[(j*3)+2] = tris[j].v3;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, menu_ibo_triangle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GL_tmpTrisAmt * 3 * sizeof(GLushort), data_elements, GL_STREAM_DRAW);

    int tris_size;  
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &tris_size);
    glDrawElements(GL_TRIANGLES, tris_size / sizeof(GLushort), GL_UNSIGNED_SHORT, 0);

    glDisableVertexAttribArray(pStage->attribute_v_uv);
    glDisableVertexAttribArray(pStage->attribute_v_color);
    glDisableVertexAttribArray(pStage->attribute_coord3d);
    
    //free(data_elements);
        
    //glBindTexture(GL_TEXTURE_2D, 0);
}

void std3D_DrawSceneFbo()
{
	std3D_PushDebugGroup("std3D_DrawSceneFbo");

    //printf("Draw scene FBO\n");
    glEnable(GL_BLEND);
    
    glBlendEquation(GL_FUNC_ADD);

    glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->window.fbo);
    glClear( GL_COLOR_BUFFER_BIT );
	glDisable(GL_DEPTH_TEST);

    static float frameNum = 1.0;
    //frameNum += (rand() % 16);

	int draw_ssao = jkPlayer_enableSSAO;
    int draw_bloom = jkPlayer_enableBloom;

    float add_luma = (((float)rdroid_curColorEffects.add.x / 255.0f) * 0.2125)
                     + (((float)rdroid_curColorEffects.add.y / 255.0f)* 0.7154)
                     + (((float)rdroid_curColorEffects.add.z / 255.0f) * 0.0721); // FLEXTODO

    // HACK: Force blinding shouldn't show the SSAO
    if (add_luma >= 0.7) {
        draw_ssao = 0;
    }

    if (!jkGame_isDDraw && !jkGuiBuildMulti_bRendering)
    {
        return;
    }

    if (jkGuiBuildMulti_bRendering) {
        draw_ssao = 0;
        //draw_bloom = 0;
    }

#ifndef NEW_BLOOM // don't need to clear
    if (draw_bloom)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->blur1.fbo);
        glClear( GL_COLOR_BUFFER_BIT );
        glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->blur2.fbo);
        glClear( GL_COLOR_BUFFER_BIT );
        glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->blur3.fbo);
        glClear( GL_COLOR_BUFFER_BIT );
        glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->blur4.fbo);
        glClear( GL_COLOR_BUFFER_BIT );
        //glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->blurBlend.fbo);
        //glClear( GL_COLOR_BUFFER_BIT );
    }
#endif

    // Clear SSAO stuff
	// disabled, unnecessary we're going to overwrite the contents with ssao result anyway
    if (draw_ssao)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->ssaoBlur1.fbo);
        //glClear( GL_COLOR_BUFFER_BIT );
        glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->ssaoBlur2.fbo);
        //glClear( GL_COLOR_BUFFER_BIT );
    }

	glDisable(GL_BLEND);

    float rad_scale = (float)std3D_pFb->w / 640.0;
    if (!draw_ssao)
    {
		std3D_PushDebugGroup("PostFX Blit");

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->postfx, std3D_pFb->tex0, 0, 0, 1.0, 1.0, 1.0, 0);
        //std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->postfx, std3D_pFb->tex1, 0, 0, 0.0, 1.0, 1.0, 0); // test emission output

		std3D_PopDebugGroup();
    }
    else
    {
		std3D_PushDebugGroup("SSAO");

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		GLuint depthOrPosTex = std3D_pFb->tex2;
	#ifdef VIEW_SPACE_GBUFFER
		glActiveTexture(GL_TEXTURE0 + 3);
		glBindTexture(GL_TEXTURE_2D, std3D_pFb->tex4);

		// downscale the depth buffer - note: this is a naive point downsample, could do better
		std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->halfDepth, std3D_pFb->tex2, 0, 0, 1.0, 1.0, 1.0, 0);
		depthOrPosTex = std3D_pFb->halfDepth.tex;
	#endif

		std3D_DrawSimpleTex(&std3D_ssaoStage, &std3D_pFb->ssaoBlur1, depthOrPosTex, std3D_pFb->tex3, tiledrand_texture, frameNum, 0.0, 0.0, 0); // test ssao output
        std3D_DrawSimpleTex(&std3D_blurStage, &std3D_pFb->ssaoBlur2, std3D_pFb->ssaoBlur1.tex, 0, 0, 14.0, 3.0, 1.0 * rad_scale, 0);
        //std3D_DrawSimpleTex(&std3D_blurStage, &std3D_pFb->ssaoBlur3, std3D_pFb->ssaoBlur2.tex, 0, 0, 8.0, 3.0, 4.0);

        glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
        std3D_DrawSimpleTex(&std3D_ssaoMixStage, &std3D_pFb->postfx, std3D_pFb->ssaoBlur2.tex, std3D_pFb->tex0, 0, 0.0, 0.0, 1.0, 0);

		std3D_PopDebugGroup();
    }

    glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
    //if (!draw_bloom)
        //std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->postfx, std3D_pFb->tex1, 0, 0, 1.0, 1.0, 1.0, 0);

    if (draw_bloom)
    {
		float bloom_intensity = 1.0;

	#ifdef NEW_BLOOM
		#ifdef CLASSIC_EMISSIVE
			bloom_intensity = 1.0f;
		#else
			bloom_intensity = 3.0f;
		#endif

		std3D_PushDebugGroup("Bloom");

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		// downscale
		float uvScale = 1.0f;// 0.25f; // source tex is 4x bigger
		std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur1, std3D_pFb->tex1, 0, 0, uvScale, 1.0, 1.0, 0);
		std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur2, std3D_pFb->blur1.tex, 0, 0, uvScale, 1.0, 1.0, 0);
		std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur3, std3D_pFb->blur2.tex, 0, 0, uvScale, 1.0, 1.0, 0);
		std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur4, std3D_pFb->blur3.tex, 0, 0, uvScale, 1.0, 1.0, 0);
		//std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur5, std3D_pFb->blur4.tex, 0, 0, uvScale, 1.0, 1.0, 0);
		//std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur6, std3D_pFb->blur5.tex, 0, 0, uvScale, 1.0, 1.0, 0);
		//std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur7, std3D_pFb->blur6.tex, 0, 0, uvScale, 1.0, 1.0, 0);
		//std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur8, std3D_pFb->blur7.tex, 0, 0, uvScale, 1.0, 1.0, 0);

		// upscale + blend
		//uvScale = 4.0f; // source tex is 4x smaller

		float blendLerp = 0.6f;
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		//glBlendFunc(GL_ONE, GL_ONE);
		//std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur7, std3D_pFb->blur8.tex, 0, 0, uvScale, blendLerp, 1.0, 0);
		//std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur6, std3D_pFb->blur7.tex, 0, 0, uvScale, blendLerp, 1.0, 0);
		//std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur5, std3D_pFb->blur6.tex, 0, 0, uvScale, blendLerp, 1.0, 0);
		//std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur4, std3D_pFb->blur5.tex, 0, 0, uvScale, blendLerp, 1.0, 0);
		std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur3, std3D_pFb->blur4.tex, 0, 0, uvScale, blendLerp, 1.0, 0);
		std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur2, std3D_pFb->blur3.tex, 0, 0, uvScale, blendLerp, 1.0, 0);
		std3D_DrawSimpleTex(&std3D_bloomStage, &std3D_pFb->blur1, std3D_pFb->blur2.tex, 0, 0, uvScale, blendLerp, 1.0, 0);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);
		std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->postfx, std3D_pFb->blur1.tex, 0, 0, 1.0f, bloom_intensity, 1.5, 0);

		std3D_PopDebugGroup();

	#else
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        std3D_DrawSimpleTex(&std3D_blurStage, &std3D_pFb->blur1, std3D_pFb->tex1, 0, 0, 16.0, 3.0, 2.0 * rad_scale, 1);
        std3D_DrawSimpleTex(&std3D_blurStage, &std3D_pFb->blur2, std3D_pFb->blur1.tex, 0, 0, 16.0, 3.0, 2.0 * rad_scale, 1);
        std3D_DrawSimpleTex(&std3D_blurStage, &std3D_pFb->blur3, std3D_pFb->blur2.tex, 0, 0, 16.0, 3.0, 2.0 * rad_scale, 1);
        std3D_DrawSimpleTex(&std3D_blurStage, &std3D_pFb->blur4, std3D_pFb->blur3.tex, 0, 0, 16.0, 3.0, 2.0 * rad_scale, 1);

        float bloom_gamma = 1.0;
        glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
        /*std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->blurBlend, std3D_pFb->tex1, 0, 0, 1.0, bloom_intensity * 1.0, bloom_gamma, 0);
        std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->blurBlend, std3D_pFb->blur1.tex, 0, 0, 1.0, bloom_intensity * 1.0, bloom_gamma, 0);
        std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->blurBlend, std3D_pFb->blur2.tex, 0, 0, 1.0, bloom_intensity * 1.2, bloom_gamma, 0);
        std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->blurBlend, std3D_pFb->blur3.tex, 0, 0, 1.0, bloom_intensity * 1.0, bloom_gamma, 0);
        std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->blurBlend, std3D_pFb->blur4.tex, 0, 0, 1.0, bloom_intensity * 1.2, bloom_gamma, 0);
        */

        glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
        //std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->postfx, std3D_pFb->blurBlend.tex, 0, 0, 1.0, 1.0, 1.0, 0);
        std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->postfx, std3D_pFb->tex1, 0, 0, 1.0, 1.5, 1.0, 0);
        std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->postfx, std3D_pFb->blur1.tex, 0, 0, 1.0, bloom_intensity * 1.5, 1.0, 0);
        std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->postfx, std3D_pFb->blur2.tex, 0, 0, 1.0, bloom_intensity * 1.0, 1.0, 0);
        std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->postfx, std3D_pFb->blur3.tex, 0, 0, 1.0, bloom_intensity * 1.0, 1.0, 0);
        std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->postfx, std3D_pFb->blur4.tex, 0, 0, 1.0, bloom_intensity * 0.8, 1.0, 0);
	#endif
    }

	glDisable(GL_BLEND);
	std3D_DrawSimpleTex(&std3D_postfxStage, &std3D_pFb->window, std3D_pFb->postfx.tex, 0, 0, (rdCamera_pCurCamera->flags & 0x1) ? sithTime_curSeconds : -1.0, !jkPlayer_enable32Bit, jkPlayer_gamma, 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	std3D_PopDebugGroup();
}

void std3D_DoTex(rdDDrawSurface* tex, rdTri* tri, int tris_left)
{
    if (!tex)
	{
        glActiveTexture(GL_TEXTURE0 + 3);
        glBindTexture(GL_TEXTURE_2D, blank_tex); // emissive
        glActiveTexture(GL_TEXTURE0 + 4);
        glBindTexture(GL_TEXTURE_2D, blank_tex); // displace

        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, blank_tex_white);
        glUniform1i(uniform_tex_mode, TEX_MODE_TEST);
        glUniform1i(uniform_blend_mode, 2);
        return;
    }
    int tex_id = tex->texture_id;
    glActiveTexture(GL_TEXTURE0 + 0);
    if (tex_id == 0)
        glBindTexture(GL_TEXTURE_2D, blank_tex_white);
    else
        glBindTexture(GL_TEXTURE_2D, tex_id);

    int emiss_tex_id = tex->emissive_texture_id;
    glActiveTexture(GL_TEXTURE0 + 3);
    if (emiss_tex_id == 0) {
        glBindTexture(GL_TEXTURE_2D, blank_tex);
    }
    else {
        //printf("emissive tex id %x\n", emiss_tex_id);
        glBindTexture(GL_TEXTURE_2D, emiss_tex_id);

        // HACK
        if (tri[0].flags & 0x600) {
            //glUniform1i(uniform_blend_mode, 6);
            //last_flags |= 0x200;
        }

        
        for (int i = 0; i < tris_left; i++) {
            if (tri[i].texture != tex) break;
            if (tri[i].flags & 0x600) {
                //tri[i].flags |= 0x200;
            }
        }
    }

    int displace_tex_id = tex->displacement_texture_id;
    glActiveTexture(GL_TEXTURE0 + 4);
    if (displace_tex_id == 0) {
        glBindTexture(GL_TEXTURE_2D, blank_tex);
    }
    else {
        glBindTexture(GL_TEXTURE_2D, displace_tex_id);
    }
    //if (tex->emissive_factor[0] != 0.0 || tex->emissive_factor[1] != 0.0 || tex->emissive_factor[2] != 0.0)
    //    stdPlatform_Printf("%f %f %f\n", tex->emissive_factor[0], tex->emissive_factor[1], tex->emissive_factor[2]);
    float emissive_mult = (jkPlayer_enableBloom ? 1.0 : 5.0);
    glUniform3f(uniform_emissiveFactor, tex->emissive_factor[0] * emissive_mult, tex->emissive_factor[1] * emissive_mult, tex->emissive_factor[2] * emissive_mult);
    glUniform4f(uniform_albedoFactor, tex->albedo_factor[0], tex->albedo_factor[1], tex->albedo_factor[2], tex->albedo_factor[3]);
    if (tex->displacement_factor) {
        //printf("%f\n", tex->displacement_factor);
        //tex->displacement_factor = -0.4;
    }
    glUniform1f(uniform_displacement_factor, tex->displacement_factor);
    glActiveTexture(GL_TEXTURE0 + 0);

    if (!jkPlayer_enableTextureFilter)
        glUniform1i(uniform_tex_mode, tex->is_16bit ? TEX_MODE_16BPP : TEX_MODE_WORLDPAL);
    else
        glUniform1i(uniform_tex_mode, tex->is_16bit ? TEX_MODE_BILINEAR_16BPP : TEX_MODE_BILINEAR);
    
     glActiveTexture(GL_TEXTURE0 + 0);

    if (tex_id == 0)
        glUniform1i(uniform_tex_mode, TEX_MODE_TEST);
}

void std3D_DrawRenderList()
{
    if (Main_bHeadless) return;

    //printf("Draw render list\n");
    glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->fbo);
	std3D_useProgram(programDefault);

    GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3
#ifdef VIEW_SPACE_GBUFFER
	, GL_COLOR_ATTACHMENT4
#endif
	};
    glDrawBuffers(ARRAYSIZE(bufs), bufs);
    
    last_tex = NULL;

    // Generate vertices list
    D3DVERTEX* vertexes = GL_tmpVertices;

    float maxX, maxY, scaleX, scaleY, width, height;

    float internalWidth = Video_menuBuffer.format.width;
    float internalHeight = Video_menuBuffer.format.height;

    if (jkGuiBuildMulti_bRendering) {
        internalWidth = 640.0;
        internalHeight = 480.0;
    }

    maxX = 1.0;
    maxY = 1.0;
    scaleX = 1.0/((double)internalWidth / 2.0);
    scaleY = 1.0/((double)internalHeight / 2.0);
    width = std3D_pFb->w;
    height = std3D_pFb->h;

    if (jkGuiBuildMulti_bRendering) {
        width = 640;
        height = 480;
    }

    // JKDF2's vertical FOV is fixed with their projection, for whatever reason. 
    // This ends up resulting in the view looking squished vertically at wide/ultrawide aspect ratios.
    // To compensate, we zoom the y axis here.
    // I also went ahead and fixed vertical displays in the same way because it seems to look better.
    float zoom_yaspect = (width/height);
    float zoom_xaspect = (height/width);

    if (height > width)
    {
        zoom_yaspect = 1.0;
    }

    if (width > height)
    {
        zoom_xaspect = 1.0;
    }

    // We no longer need all the weird squishing
    if (!jkGuiBuildMulti_bRendering) {
        zoom_yaspect = 1.0;
        zoom_xaspect = 1.0;
    }
    
    float shift_add_x = 0;
    float shift_add_y = 0;

    if (jkGuiBuildMulti_bRendering) {
        float menu_w, menu_h, menu_x;
        menu_w = (double)std3D_pFb->w;
        menu_h = (double)std3D_pFb->h;

        // Keep 4:3 aspect
        menu_x = (menu_w - (menu_h * (640.0 / 480.0))) / 2.0;

        width = std3D_pFb->w;
        height = std3D_pFb->h;

        zoom_xaspect = (height/width);

        shift_add_x = (((1.0 - ((menu_x * zoom_xaspect) / std3D_pFb->w)) + 0.15) * zoom_xaspect);
        shift_add_y = -0.5;
        zoom_yaspect = 1.0;
    }

	glBindVertexArray(world_vao);
	glBindBuffer(GL_ARRAY_BUFFER, world_vbo_all);
	glBufferData(GL_ARRAY_BUFFER, GL_tmpVerticesAmt * sizeof(D3DVERTEX), vertexes, GL_STREAM_DRAW);
    
    glUniform1i(uniform_tex_mode, TEX_MODE_TEST);
    glUniform1i(uniform_blend_mode, 2);
    glActiveTexture(GL_TEXTURE0 + 4);
    glBindTexture(GL_TEXTURE_2D, blank_tex);
    glActiveTexture(GL_TEXTURE0 + 3);
    glBindTexture(GL_TEXTURE_2D, blank_tex);
    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, worldpal_lights_texture);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, worldpal_texture);
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, blank_tex_white);
    
    glUniform1i(uniform_tex, 0);
    glUniform1i(uniform_worldPalette, 1);
    glUniform1i(uniform_worldPaletteLights, 2);
    glUniform1i(uniform_texEmiss, 3);
    glUniform1i(uniform_displacement_map, 4);
    
    {
    
    float d3dmat[16] = {
       (float)(maxX*scaleX*zoom_xaspect),      0,                                          0,      0, // right
       0,                                       (float)(-maxY*scaleY*zoom_yaspect),               0,      0, // up
       0,                                       0,                                          1,     0, // forward
       (float)(-(internalWidth/2)*scaleX*zoom_xaspect + shift_add_x),  (float)((internalHeight/2)*scaleY*zoom_yaspect + shift_add_y),     (float)((!rdCamera_pCurCamera || rdCamera_pCurCamera->projectType == rdCameraProjectType_Perspective) ? -1 : 1),      1  // pos
    };
    
    glUniformMatrix4fv(uniform_mvp, 1, GL_FALSE, d3dmat);
    glViewport(0, 0, width, height);
    
    }

    glUniform2f(uniform_iResolution, width, height);

	glUniform1i(uniform_enableDither, 0);

    //rdroid_curColorEffects.tint.x = 0.0;
    //rdroid_curColorEffects.tint.y = 0.5;
    //rdroid_curColorEffects.tint.z = 0.5;

#if 0
    //if (rdroid_curColorEffects.filter.x || rdroid_curColorEffects.filter.y || rdroid_curColorEffects.filter.z)
    //if (rdroid_curColorEffects.tint.x || rdroid_curColorEffects.tint.y || rdroid_curColorEffects.tint.z)
    if (rdroid_curColorEffects.add.x || rdroid_curColorEffects.add.y || rdroid_curColorEffects.add.z)
    {
        stdPlatform_Printf("a %f %f %f ", rdroid_curColorEffects.tint.x, rdroid_curColorEffects.tint.y, rdroid_curColorEffects.tint.z);
        stdPlatform_Printf("%d %d %d ", rdroid_curColorEffects.filter.x, rdroid_curColorEffects.filter.y, rdroid_curColorEffects.filter.z);
        stdPlatform_Printf("%d %d %d ", rdroid_curColorEffects.add.x, rdroid_curColorEffects.add.y, rdroid_curColorEffects.add.z);
        stdPlatform_Printf("%f\n", rdroid_curColorEffects.fade);
    }
#endif

    glUniform3f(uniform_tint, rdroid_curColorEffects.tint.x, rdroid_curColorEffects.tint.y, rdroid_curColorEffects.tint.z);
    if (rdroid_curColorEffects.filter.x || rdroid_curColorEffects.filter.y || rdroid_curColorEffects.filter.z)
        glUniform3f(uniform_filter, rdroid_curColorEffects.filter.x ? 1.0 : 0.25, rdroid_curColorEffects.filter.y ? 1.0 : 0.25, rdroid_curColorEffects.filter.z ? 1.0 : 0.25);
    else
        glUniform3f(uniform_filter, 1.0, 1.0, 1.0);
    glUniform1f(uniform_fade, rdroid_curColorEffects.fade);
    glUniform3f(uniform_add, (float)rdroid_curColorEffects.add.x / 255.0f, (float)rdroid_curColorEffects.add.y / 255.0f, (float)rdroid_curColorEffects.add.z / 255.0f);
    glUniform3f(uniform_emissiveFactor, 0.0, 0.0, 0.0);
    glUniform4f(uniform_albedoFactor, 1.0, 1.0, 1.0, 1.0);
    glUniform1f(uniform_light_mult, jkGuiBuildMulti_bRendering ? 0.85 : (jkPlayer_enableBloom ? 0.9 : 0.85));
    glUniform1f(uniform_displacement_factor, 1.0);

#ifdef FOG
	extern int rdroid_curFogEnabled;
	extern rdVector4 rdroid_curFogColor;
	extern float rdroid_curFogStartDepth;
	extern float rdroid_curFogEndDepth;
	glUniform1i(uniform_fog, rdroid_curFogEnabled);
	glUniform4f(uniform_fog_color, rdroid_curFogColor.x, rdroid_curFogColor.y, rdroid_curFogColor.z, rdroid_curFogColor.w);
	glUniform1f(uniform_fog_start, rdroid_curFogStartDepth);
	glUniform1f(uniform_fog_end, rdroid_curFogEndDepth);
#endif

    rdTri* tris = GL_tmpTris;
    rdLine* lines = GL_tmpLines;
    
    //glEnableVertexAttribArray(attribute_v_norm);

    int last_tex_idx = 0;
    //GLushort* world_data_elements = malloc(sizeof(GLushort) * 3 * GL_tmpTrisAmt);
    for (int j = 0; j < GL_tmpTrisAmt; j++)
    {
        world_data_elements[(j*3)+0] = tris[j].v1;
        world_data_elements[(j*3)+1] = tris[j].v2;
        world_data_elements[(j*3)+2] = tris[j].v3;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, world_ibo_triangle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GL_tmpTrisAmt * 3 * sizeof(GLushort), world_data_elements, GL_STREAM_DRAW);
    
    int do_batch = 0;
    
    //glDepthFunc(GL_LESS);
    //glDepthMask(GL_TRUE);
    //glCullFace(GL_FRONT);

    if (last_tex) {
        std3D_DoTex(last_tex, &tris[0], GL_tmpTrisAmt);
    }

    last_flags = tris[0].flags;

    if (last_flags & 0x800) {
        glDepthFunc(GL_LESS);
        //glClear(GL_DEPTH_BUFFER_BIT);
    }
    else {
        glDepthFunc(GL_ALWAYS);
    }

	int lastBlendEnable = 0;
#ifdef ADDITIVE_BLEND
	if (last_flags & 0x180000)
	{
		if (last_flags & 0x100000)
		{
			glUniform1i(uniform_blend_mode, D3DBLEND_SRCALPHA);
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		}
		else
		{
			glUniform1i(uniform_blend_mode, D3DBLEND_SRCALPHA);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		}
		glEnable(GL_BLEND);
		lastBlendEnable = 1;
	}
	else
#endif
    if (last_flags & 0x600) {
        
        if (last_flags & 0x200) {
            glUniform1i(uniform_blend_mode, D3DBLEND_INVSRCALPHA);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
        else {
            glUniform1i(uniform_blend_mode, D3DBLEND_SRCALPHA);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
		glEnable(GL_BLEND);
		lastBlendEnable = 1;
	}
    else {
        glUniform1i(uniform_blend_mode, D3DBLEND_ONE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);
		lastBlendEnable = 0;
    }

	// this is probably not ideal to do repeatedly mid drawing but the alpha-reject approach to
	// not writing the gbuffer targets is pretty freaking busted sometimes
	// todo: cache the state along with the other states, sort draw calls by state
	if(lastBlendEnable) // only write to color and emissive when blending
	{
		GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(ARRAYSIZE(bufs), bufs);
	}
	else // otherwise draw them all
	{
		GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3
	#ifdef VIEW_SPACE_GBUFFER
		, GL_COLOR_ATTACHMENT4
	#endif
		};
		glDrawBuffers(ARRAYSIZE(bufs), bufs);
	}

    if (last_flags & 0x1000)
    {
        glDepthMask(GL_TRUE);
    }
    else
    {
        glDepthMask(GL_FALSE);
    }

    if (last_flags & 0x10000) {
        glCullFace(GL_BACK);
    }
    else
    {
        glCullFace(GL_FRONT);
    }

#ifdef STENCIL_BUFFER
	if (last_flags & 0x200000)
	{
		glEnable(GL_STENCIL_TEST);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilFunc(GL_ALWAYS, 1, 0xFF);
	}
	else
	{
		glDisable(GL_STENCIL_TEST);
	}
#endif
    
    for (int j = 0; j < GL_tmpTrisAmt; j++)
    {
        if (tris[j].texture != last_tex || tris[j].flags != last_flags)
        {
            do_batch = 1;
        }
        
        if (do_batch)
        {
            int num_tris_batch = j - last_tex_idx;
            rdDDrawSurface* tex = tris[j].texture;


            
            test_idk = tex;

            if (num_tris_batch)
            {
                //printf("batch %u~%u\n", last_tex_idx, j);
                glDrawElements(GL_TRIANGLES, num_tris_batch * 3, GL_UNSIGNED_SHORT, (GLvoid*)((intptr_t)&world_data_elements[last_tex_idx * 3] - (intptr_t)&world_data_elements[0]));
            }

            std3D_DoTex(tex, &tris[j], GL_tmpTrisAmt-j);
            
            int changed_flags = (last_flags ^ tris[j].flags);
			
#ifdef ADDITIVE_BLEND
			if (changed_flags & 0x180600)
#else
            if (changed_flags & 0x600)
#endif
			{
			#ifdef ADDITIVE_BLEND
				if (tris[j].flags & 0x180000)
				{
					if (tris[j].flags & 0x100000)
					{
						glUniform1i(uniform_blend_mode, D3DBLEND_SRCALPHA);
						glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
					}
					else
					{
						glUniform1i(uniform_blend_mode, D3DBLEND_SRCALPHA);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE);
					}
					glEnable(GL_BLEND);
				}
				else
			#endif
                if (tris[j].flags & 0x600) {
                    
                    if (tris[j].flags & 0x200) {
                        glUniform1i(uniform_blend_mode, D3DBLEND_INVSRCALPHA);
                        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    }
                    else {
                        //printf ("flags %x\n", tris[j].flags);
                        glUniform1i(uniform_blend_mode, D3DBLEND_SRCALPHA);
                        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    }
					glEnable(GL_BLEND);
				}
                else {
                    glUniform1i(uniform_blend_mode, D3DBLEND_ONE);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					glDisable(GL_BLEND);
				}
            }

			int blendEnable = 0;
#ifdef ADDITIVE_BLEND
			if (tris[j].flags & 0x180000)
				blendEnable = 1;
			else
#endif
			if (tris[j].flags & 0x600)

				blendEnable = 1;
			else
				blendEnable = 0;

			if (blendEnable != lastBlendEnable)
			{
				if (blendEnable) // only write to color and emissive when blending
				{
					GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
					glDrawBuffers(ARRAYSIZE(bufs), bufs);
				}
				else // otherwise draw them all
				{
					GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3
				#ifdef VIEW_SPACE_GBUFFER
					, GL_COLOR_ATTACHMENT4
				#endif
					};
					glDrawBuffers(ARRAYSIZE(bufs), bufs);
				}
			}
            
            if (changed_flags & 0x1800)
            {
                if (tris[j].flags & 0x800)
                {
                    glDepthFunc(GL_LESS);
                }
                else
                {
                    glDepthFunc(GL_ALWAYS);
                    //glClear(GL_DEPTH_BUFFER_BIT);
                }
                
                if (tris[j].flags & 0x1000)
                {
                    glDepthMask(GL_TRUE);
                }
                else
                {
                    glDepthMask(GL_FALSE);
                }
            }

            if (changed_flags & 0x10000)
            {
                if (tris[j].flags & 0x10000) {
                    glCullFace(GL_BACK);
                }
                else
                {
                    glCullFace(GL_FRONT);
                }
            }

#ifdef STENCIL_BUFFER
			if (changed_flags & 0x200000)
			{
				if (tris[j].flags & 0x200000)
				{
					glEnable(GL_STENCIL_TEST);
					glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
					glStencilFunc(GL_ALWAYS, 1, 0xFF);
				}
				else
				{
					glDisable(GL_STENCIL_TEST);
				}
			}
#endif
            
            last_tex = tris[j].texture;
            last_flags = tris[j].flags;
			lastBlendEnable = blendEnable;
            last_tex_idx = j;

            do_batch = 0;
        }
        //printf("tri %u: %u,%u,%u, flags %x\n", j, tris[j].v1, tris[j].v2, tris[j].v3, tris[j].flags);
        
        
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
    
    int remaining_batch = GL_tmpTrisAmt - last_tex_idx;

    if (remaining_batch)
    {
        glDrawElements(GL_TRIANGLES, remaining_batch * 3, GL_UNSIGNED_SHORT, (GLvoid*)((intptr_t)&world_data_elements[last_tex_idx * 3] - (intptr_t)&world_data_elements[0]));
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindVertexArray(vao);

#if 0
    // Draw all lines
    world_data_elements = malloc(sizeof(GLushort) * 2 * GL_tmpLinesAmt);
    for (int j = 0; j < GL_tmpLinesAmt; j++)
    {
        world_data_elements[(j*2)+0] = lines[j].v1;
        world_data_elements[(j*2)+1] = lines[j].v2;
    }
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, world_ibo_triangle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GL_tmpLinesAmt * 2 * sizeof(GLushort), world_data_elements, GL_STREAM_DRAW);

    int lines_size;
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &lines_size);
    glDrawElements(GL_LINES, lines_size / sizeof(GLushort), GL_UNSIGNED_SHORT, 0);
#endif

#ifdef DECAL_RENDERING
	lightBufferDirty = 1;
#endif
        
    // Done drawing    
    glBindTexture(GL_TEXTURE_2D, worldpal_texture);
    glCullFace(GL_FRONT);
   
#ifdef STENCIL_BUFFER
	glDisable(GL_STENCIL_TEST);
#endif
    std3D_ResetRenderList();
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
    glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->fbo);
    glClear(GL_DEPTH_BUFFER_BIT);
#ifdef DEFERRED_FRAMEWORK
	canUseDepthStencil = 0; // depth-stencil invald, can't use for deferred optimizations
#endif
    return 1;
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
    
    GLuint image_texture;
    glGenTextures(1, &image_texture);

    uint8_t* image_8bpp = (uint8_t*)(*vbuf)->sdlSurface->pixels;
    uint16_t* image_16bpp = (uint16_t*)(*vbuf)->sdlSurface->pixels;
    uint8_t* pal = (uint8_t*)(*vbuf)->palette;
    
    uint32_t width, height;
    width = (*vbuf)->format.width;
    height = (*vbuf)->format.height;

    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    if (jkPlayer_enableTextureFilter && texture->is_16bit)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    if ((*vbuf)->format.format.colorMode)
    {
        texture->is_16bit = 1;
#if 1
        if (!is_alpha_tex)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0,  GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, image_8bpp);
        else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,  GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, image_8bpp);
#endif

#ifdef __NOTDEF_FORMAT_CONVERSION
        void* image_data = malloc(width*height*4);
    
        for (int j = 0; j < height; j++)
        {
            for (int i = 0; i < width; i++)
            {
                uint32_t index = (i*height) + j;
                uint32_t val_rgba = 0x00000000;
                
                uint16_t val = image_16bpp[index];
                if (!is_alpha_tex) // RGB565
                {
                    uint8_t val_a1 = 1;
                    uint8_t val_r5 = (val >> 11) & 0x1F;
                    uint8_t val_g6 = (val >> 5) & 0x3F;
                    uint8_t val_b5 = (val >> 0) & 0x1F;

                    uint8_t val_a8 = val_a1 ? 0xFF : 0x0;
                    uint8_t val_r8 = ( val_r5 * 527 + 23 ) >> 6;
                    uint8_t val_g8 = ( val_g6 * 259 + 33 ) >> 6;
                    uint8_t val_b8 = ( val_b5 * 527 + 23 ) >> 6;

#ifdef __NOTDEF_TRANSPARENT_BLACK
                    uint8_t transparent_r8 = (vbuf->transparent_color >> 16) & 0xFF;
                    uint8_t transparent_g8 = (vbuf->transparent_color >> 8) & 0xFF;
                    uint8_t transparent_b8 = (vbuf->transparent_color >> 0) & 0xFF;

                    if (val_r8 == transparent_r8 && val_g8 == transparent_g8 && val_b8 == transparent_b8) {
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
                    uint8_t val_r8 = ( val_r5 * 527 + 23 ) >> 6;
                    uint8_t val_g8 = ( val_g5 * 527 + 23 ) >> 6;
                    uint8_t val_b8 = ( val_b5 * 527 + 23 ) >> 6;

                    val_rgba |= (val_a8 << 24);
                    val_rgba |= (val_b8 << 16);
                    val_rgba |= (val_g8 << 8);
                    val_rgba |= (val_r8 << 0);
                }
                    
                *(uint32_t*)(image_data + index*4) = val_rgba;
            }
        }
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, image_data);

        texture->pDataDepthConverted = image_data;
#endif // __NOTDEF_FORMAT_CONVERSION
    }
    else {
#if 0
        void* image_data = malloc(width*height*4);
    
        for (int j = 0; j < height; j++)
        {
            for (int i = 0; i < width; i++)
            {
                uint32_t index = (i*height) + j;
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
                    rdColor24* pal_master = (rdColor24*)sithWorld_pCurrentWorld->colormaps->colors;//stdDisplay_gammaPalette;
                    rdColor24* color = &pal_master[val];
                    val_rgba |= (color->r << 16);
                    val_rgba |= (color->g << 8);
                    val_rgba |= (color->b << 0);
                }
                
                *(uint32_t*)(image_data + index*4) = val_rgba;
            }
        }
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

        texture->pDataDepthConverted = image_data;
#endif
        texture->is_16bit = 0;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, image_8bpp);

        texture->pDataDepthConverted = NULL;
    }

    
    std3D_aLoadedSurfaces[std3D_loadedTexturesAmt] = texture;
    std3D_aLoadedTextures[std3D_loadedTexturesAmt++] = image_texture;
    /*ext->surfacebuf = image_data;
    ext->surfacetex = image_texture;
    ext->surfacepaltex = pal_texture;*/
    
    texture->texture_id = image_texture;
    texture->emissive_texture_id = 0;
    texture->displacement_texture_id = 0;
    texture->texture_loaded = 1;
    texture->emissive_factor[0] = 0.0;
    texture->emissive_factor[1] = 0.0;
    texture->emissive_factor[2] = 0.0;
    texture->albedo_factor[0] = 1.0;
    texture->albedo_factor[1] = 1.0;
    texture->albedo_factor[2] = 1.0;
    texture->albedo_factor[3] = 1.0;
    texture->displacement_factor = 0.0;
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
    uint8_t* image_8bpp = (uint8_t*)vbuf->sdlSurface->pixels;
    uint16_t* image_16bpp = (uint16_t*)vbuf->sdlSurface->pixels;
    uint8_t* pal = (uint8_t*)vbuf->palette;
    
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

void std3D_UpdateFrameCount(rdDDrawSurface *pTexture) {
    //pTexture->frameNum = std3D_frameCount; // lol LEC bug
    std3D_RemoveTextureFromCacheList(pTexture);
    std3D_AddTextureToCacheList(pTexture);
    pTexture->frameNum = std3D_frameCount;
}
void std3D_RemoveTextureFromCacheList(rdDDrawSurface *surface) {
}
void std3D_AddTextureToCacheList(rdDDrawSurface *surface) {
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
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
        else
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
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
    }

#if 0
    for (int i = 0; i < STD3D_MAX_TEXTURES; i++)
    {
        if (!std3D_aUITextures[i]) continue;
        glBindTexture(GL_TEXTURE_2D, std3D_aUITextures[i]);

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
#endif

    glBindTexture(GL_TEXTURE_2D, blank_tex);
}

// Added
void std3D_Screenshot(const char* pFpath)
{
#ifdef TARGET_CAN_JKGM
    if (!std3D_pFb) return;

    uint8_t* data = (uint8_t*)malloc(std3D_pFb->window.w * std3D_pFb->window.h * 3 * sizeof(uint8_t));
    glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->window.fbo);
    glReadPixels(0, 0, std3D_pFb->window.w, std3D_pFb->window.h, GL_RGB, GL_UNSIGNED_BYTE, data);
    jkgm_write_png(pFpath, std3D_pFb->window.w, std3D_pFb->window.h, data);

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

// From https://github.com/smlu/OpenJones3D/blob/main/Libs/std/Win95/std3D.c
int std3D_PurgeTextureCache(size_t size)
{
    size_t purgedBytes = 0;
    for ( rdDDrawSurface* pCacheTexture = std3D_pFirstTexCache; pCacheTexture && pCacheTexture->frameNum != std3D_frameCount; pCacheTexture = pCacheTexture->pNextCachedTexture )
    {
        if ( pCacheTexture->textureSize == size )
        {
            //IDirect3DTexture2_Release(pCacheTexture->pD3DCachedTex);
            std3D_PurgeSurfaceRefs(pCacheTexture);
            //pCacheTexture->pD3DCachedTex = NULL;
            std3D_RemoveTextureFromCacheList(pCacheTexture);
            return 1;
        }
    }

    rdDDrawSurface* pNextCachedTexture = NULL;
    for ( rdDDrawSurface* pCacheTexture = std3D_pFirstTexCache; pCacheTexture && purgedBytes < size; pCacheTexture = pNextCachedTexture )
    {
        pNextCachedTexture = pCacheTexture->pNextCachedTexture;
        if ( pCacheTexture->frameNum != std3D_frameCount )
        {
            //if ( pCacheTexture->pD3DCachedTex ) { // Added: Added check for null pointer
                //IDirect3DTexture2_Release(pCacheTexture->pD3DCachedTex);
                std3D_PurgeSurfaceRefs(pCacheTexture);
            //}
            //pCacheTexture->pD3DCachedTex = NULL;
            purgedBytes += pCacheTexture->textureSize;
            std3D_RemoveTextureFromCacheList(pCacheTexture);
        }
    }

    return purgedBytes != 0;
}

void std3D_PurgeEntireTextureCache()
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
}

void std3D_InitializeViewport(rdRect *viewRect)
{
    signed int v1; // ebx
    signed int height; // ebp

    float viewXMax_2; // [esp+14h] [ebp+4h]
    float viewRectYMax; // [esp+14h] [ebp+4h]

    std3D_rectViewIdk.x = viewRect->x;
    v1 = viewRect->width;
    std3D_rectViewIdk.y = viewRect->y;
    std3D_rectViewIdk.width = v1;
    height = viewRect->height;
    memset(std3D_aViewIdk, 0, sizeof(std3D_aViewIdk));
    std3D_aViewIdk[0] = (float)std3D_rectViewIdk.x;
    std3D_aViewIdk[1] = (float)std3D_rectViewIdk.y;
    std3D_rectViewIdk.height = height;
    std3D_aViewTris[0].v1 = 0;
    std3D_aViewTris[0].v2 = 1;
    viewXMax_2 = (float)(v1 + std3D_rectViewIdk.x);
    std3D_aViewIdk[8] = viewXMax_2;
    std3D_aViewIdk[9] = std3D_aViewIdk[1];
    std3D_aViewIdk[16] = viewXMax_2;
    viewRectYMax = (float)(height + std3D_rectViewIdk.y);
    std3D_aViewTris[0].texture = 0;
    std3D_aViewIdk[17] = viewRectYMax;
    std3D_aViewIdk[25] = viewRectYMax;
    std3D_aViewIdk[24] = std3D_aViewIdk[0];
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

#ifdef DEFERRED_FRAMEWORK
// fixme: blending state currently set outside due to different formats and behavior
void std3D_DrawDeferredStage(std3D_deferredStage* pStage, rdVector3* verts, rdDDrawSurface* texture, uint32_t flags, rdVector3* position, float radius, rdVector3* color, rdMatrix34* matrix)
{
	if (Main_bHeadless) return;

	// never write depth
	glDepthMask(GL_FALSE);

	std3D_useProgram(pStage->program);

	glUniform1i(pStage->uniform_texDepth, 0);
	glUniform1i(pStage->uniform_texLight, 1);
	glUniform1i(pStage->uniform_texNormal, 2);
	glUniform1i(pStage->uniform_texDiffuse, 3);
	glUniform1i(pStage->uniform_worldPalette, 4);
	glUniform1i(pStage->uniform_tex, 5);

	{
		float maxX, maxY, scaleX, scaleY, width, height;

		float internalWidth = Video_menuBuffer.format.width;
		float internalHeight = Video_menuBuffer.format.height;

		if (jkGuiBuildMulti_bRendering)
		{
			internalWidth = 640.0;
			internalHeight = 480.0;
		}

		maxX = 1.0;
		maxY = 1.0;
		scaleX = 1.0 / ((double)internalWidth / 2.0);
		scaleY = 1.0 / ((double)internalHeight / 2.0);
		width = std3D_pFb->w;
		height = std3D_pFb->h;

		if (jkGuiBuildMulti_bRendering)
		{
			width = 640;
			height = 480;
		}

		// JKDF2's vertical FOV is fixed with their projection, for whatever reason. 
		// This ends up resulting in the view looking squished vertically at wide/ultrawide aspect ratios.
		// To compensate, we zoom the y axis here.
		// I also went ahead and fixed vertical displays in the same way because it seems to look better.
		float zoom_yaspect = (width / height);
		float zoom_xaspect = (height / width);

		if (height > width)
		{
			zoom_yaspect = 1.0;
		}

		if (width > height)
		{
			zoom_xaspect = 1.0;
		}

		// We no longer need all the weird squishing
		if (!jkGuiBuildMulti_bRendering)
		{
			zoom_yaspect = 1.0;
			zoom_xaspect = 1.0;
		}

		float shift_add_x = 0;
		float shift_add_y = 0;

		if (jkGuiBuildMulti_bRendering)
		{
			float menu_w, menu_h, menu_x;
			menu_w = (double)std3D_pFb->w;
			menu_h = (double)std3D_pFb->h;

			// Keep 4:3 aspect
			menu_x = (menu_w - (menu_h * (640.0 / 480.0))) / 2.0;

			width = std3D_pFb->w;
			height = std3D_pFb->h;

			zoom_xaspect = (height / width);

			shift_add_x = (((1.0 - ((menu_x * zoom_xaspect) / std3D_pFb->w)) + 0.15) * zoom_xaspect);
			shift_add_y = -0.5;
			zoom_yaspect = 1.0;
		}

		float d3dmat[16] = {
				maxX * scaleX * zoom_xaspect,      0,                                          0,      0, // right
				0,                                       -maxY * scaleY * zoom_yaspect,               0,      0, // up
				0,                                       0,                                          1,     0, // forward
				-(internalWidth / 2) * scaleX * zoom_xaspect + shift_add_x,  (internalHeight / 2) * scaleY * zoom_yaspect + shift_add_y,     (!rdCamera_pCurCamera || rdCamera_pCurCamera->projectType == rdCameraProjectType_Perspective) ? -1 : 1,      1  // pos
		};

		glUniformMatrix4fv(pStage->uniform_mvp, 1, GL_FALSE, d3dmat);
		glViewport(0, 0, width, height);
		glUniform2f(pStage->uniform_iResolution, width, height);
	}

	glUniform3fv(pStage->uniform_rt, 1, (float*)&rdCamera_pCurCamera->pClipFrustum->rt);
	glUniform3fv(pStage->uniform_lt, 1, (float*)&rdCamera_pCurCamera->pClipFrustum->lt);
	glUniform3fv(pStage->uniform_rb, 1, (float*)&rdCamera_pCurCamera->pClipFrustum->rb);
	glUniform3fv(pStage->uniform_lb, 1, (float*)&rdCamera_pCurCamera->pClipFrustum->lb);

	glBindVertexArray(pStage->vao);
	glBindBuffer(GL_ARRAY_BUFFER, deferred_vbo);

	// luckily we can precompute the indices
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, deferred_ibo);

	// vertices
	GL_tmpVerticesAmt = 8;
	for (int v = 0; v < GL_tmpVerticesAmt; ++v)
	{
		GL_tmpVertices[v].x = verts[v].x;
		GL_tmpVertices[v].y = verts[v].y;
		GL_tmpVertices[v].z = verts[v].z;
		GL_tmpVertices[v].tu = 0;
		GL_tmpVertices[v].tv = 0;
		*(uint32_t*)&GL_tmpVertices[v].nx = 0;
		GL_tmpVertices[v].color = 0xFFFFFFFF;
		*(uint32_t*)&GL_tmpVertices[v].nz = 0;
	}

	glBufferData(GL_ARRAY_BUFFER, GL_tmpVerticesAmt * sizeof(D3DVERTEX), GL_tmpVertices, GL_STREAM_DRAW);

	glActiveTexture(GL_TEXTURE0 + 0);
	glBindTexture(GL_TEXTURE_2D, std3D_pFb->tex2);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, std3D_pFb->decalLight.tex);
	glActiveTexture(GL_TEXTURE0 + 2);
	glBindTexture(GL_TEXTURE_2D, std3D_pFb->tex3);
	glActiveTexture(GL_TEXTURE0 + 3);
	glBindTexture(GL_TEXTURE_2D, std3D_pFb->tex4);
	glActiveTexture(GL_TEXTURE0 + 4);
	glBindTexture(GL_TEXTURE_2D, displaypal_texture);
	glActiveTexture(GL_TEXTURE0 + 5);
	glBindTexture(GL_TEXTURE_2D, texture ? texture->texture_id : blank_tex_white);

	if (!jkPlayer_enableTextureFilter)
		glUniform1i(pStage->uniform_texmode, texture && texture->is_16bit ? TEX_MODE_16BPP : TEX_MODE_WORLDPAL);
	else
		glUniform1i(pStage->uniform_texmode, texture && texture->is_16bit ? TEX_MODE_BILINEAR_16BPP : TEX_MODE_BILINEAR);

	glUniform3f(pStage->uniform_tint, rdroid_curColorEffects.tint.x, rdroid_curColorEffects.tint.y, rdroid_curColorEffects.tint.z);
	if (rdroid_curColorEffects.filter.x || rdroid_curColorEffects.filter.y || rdroid_curColorEffects.filter.z)
		glUniform3f(pStage->uniform_filter, rdroid_curColorEffects.filter.x ? 1.0 : 0.25, rdroid_curColorEffects.filter.y ? 1.0 : 0.25, rdroid_curColorEffects.filter.z ? 1.0 : 0.25);
	else
		glUniform3f(pStage->uniform_filter, 1.0, 1.0, 1.0);
	glUniform1f(pStage->uniform_fade, rdroid_curColorEffects.fade);
	glUniform3f(pStage->uniform_add, (float)rdroid_curColorEffects.add.x / 255.0f, (float)rdroid_curColorEffects.add.y / 255.0f, (float)rdroid_curColorEffects.add.z / 255.0f);

	glUniform1ui(pStage->uniform_flags, flags);
	glUniform3f(pStage->uniform_position, position->x, position->y, position->z);
	glUniform1f(pStage->uniform_radius, radius);
	glUniform3f(pStage->uniform_color, color->x, color->y, color->z);

	rdVector4 mat[4];
	rdVector_Set4(&mat[0], matrix->rvec.x, matrix->rvec.y, matrix->rvec.z, 0.0f);
	rdVector_Set4(&mat[1], matrix->lvec.x, matrix->lvec.y, matrix->lvec.z, 0.0f);
	rdVector_Set4(&mat[2], matrix->uvec.x, matrix->uvec.y, matrix->uvec.z, 0.0f);
	rdVector_Set4(&mat[3], matrix->scale.x, matrix->scale.y, matrix->scale.z, 1.0f);
	glUniformMatrix4fv(pStage->uniform_objectMatrix, 1, GL_FALSE, &mat[0].x);

	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);

	glBindVertexArray(vao);

	glDepthMask(GL_TRUE);
}
#endif

#ifdef DECAL_RENDERING
void std3D_DrawDecal(stdVBuffer* vbuf, rdDDrawSurface* texture, rdVector3* verts, rdMatrix44* decalMatrix, rdVector3* color, uint32_t flags, float angleFade)
{
	if (Main_bHeadless) return;

	// we need a copy of the main buffer for lighting, so copy one...
	// todo: get rid of this... maybe just read the ambient SH for decals and call it a day instead of using the underlying lighting
	if(lightBufferDirty)
	{
		std3D_DrawSimpleTex(&std3D_texFboStage, &std3D_pFb->decalLight, std3D_pFb->tex0, 0, 0, 1.0, 1.0, 1.0, 0);
		lightBufferDirty = 0;
	}

	// todo: track this so we don't redo it every decal draw
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
#ifdef STENCIL_BUFFER
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_EQUAL, 0, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
#endif

	if (flags & RD_DECAL_ADD)
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	else
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (flags & RD_DECAL_INSIDE)
		glCullFace(GL_FRONT);
	else
		glCullFace(GL_BACK);

	// bind to main fbo for drawing
	glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->main.fbo);

	// we only need to write color + emissivie
	GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, bufs);

	std3D_DrawDeferredStage(&std3D_decalStage, verts, texture, flags, &rdroid_zeroVector3, angleFade, color, decalMatrix);

	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LESS);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glCullFace(GL_FRONT);
#ifdef STENCIL_BUFFER
	glDisable(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS, 0, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
#endif
}
#endif

#ifdef PARTICLE_LIGHTS
void std3D_DrawLight(rdLight* light, rdVector3* position, rdVector3* verts)
{
	if (Main_bHeadless) return;

	glBlendFunc(GL_ONE, GL_ONE);
	glEnable(GL_BLEND);
	glEnable(GL_CULL_FACE);
#ifdef STENCIL_BUFFER
	glDisable(GL_STENCIL_TEST);
	//glStencilFunc(GL_EQUAL, 0, 0xFF);
	//glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
#endif
	glDepthFunc(GL_GEQUAL);
	glCullFace(GL_FRONT);

	rdVector3 lightColor;
	rdVector_Scale3(&lightColor, &light->color, light->intensity);

	// bind to main fbo for drawing
	glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->main.fbo);

	// we only need to write color + emissivie
	GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, bufs);

	std3D_DrawDeferredStage(&std3D_lightStage, verts, NULL, 0, position, light->falloffMin, &lightColor, &rdroid_identMatrix34);

	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LESS);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);
	glCullFace(GL_FRONT);
#ifdef STENCIL_BUFFER
	//glDisable(GL_STENCIL_TEST);
	//glStencilFunc(GL_ALWAYS, 0, 0xFF);
	//glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
#endif
}
#endif

#ifdef SPHERE_AO

void std3D_DrawOccluder(rdVector3* position, float radius, rdVector3* verts)
{
	if (Main_bHeadless) return;

	// bind to main fbo for drawing
	glBindFramebuffer(GL_FRAMEBUFFER, std3D_pFb->main.fbo);

	canUseDepthStencil = 0; // disable for now, somethings wrong in renderdroid2
	if (!canUseDepthStencil)
	{
		glDisable(GL_DEPTH_TEST);
#ifdef STENCIL_BUFFER
		glDisable(GL_STENCIL_TEST);
#endif
	}
	else
	{
		glEnable(GL_DEPTH_TEST);
#ifdef STENCIL_BUFFER
		glDisable(GL_CULL_FACE);
		glDrawBuffer(GL_NONE);
		glEnable(GL_STENCIL_TEST);
		glClear(GL_STENCIL_BUFFER_BIT);
		glStencilFunc(GL_ALWAYS, 0, 0xFF);
		glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
		glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
		std3D_DrawDeferredStage(&std3D_stencilStage, verts, NULL, 0, position, radius, &rdroid_zeroVector3, &rdroid_identMatrix34);
		glDisable(GL_DEPTH_TEST);
#endif
	}

	// we only need to write color 
	GLenum bufs[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, bufs);

	glEnable(GL_CULL_FACE);
#ifdef STENCIL_BUFFER
	if (canUseDepthStencil)
	{
		glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
		glDisable(GL_DEPTH_TEST);

		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
	}
	else
#endif
	{
		glCullFace(GL_FRONT);
		glDepthFunc(GL_GEQUAL);
	}

	// multiply blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_ZERO);

	std3D_DrawDeferredStage(&std3D_occluderStage[!jkPlayer_enable32Bit], verts, NULL, 0, position, radius, &rdroid_zeroVector3, &rdroid_identMatrix34);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LESS);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glBlendEquation(GL_FUNC_ADD);
	glCullFace(GL_FRONT);
#ifdef STENCIL_BUFFER
	//glDisable(GL_STENCIL_TEST);
	//glStencilFunc(GL_ALWAYS, 0, 0xFF);
	//glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
#endif
}

#endif

#endif // !RENDER_DROID2
