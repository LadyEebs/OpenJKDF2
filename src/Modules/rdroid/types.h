#pragma once

#ifdef RENDER_DROID2

#define RD_TRUE  1
#define RD_FALSE 0

#define RD_COLOR0 0
#define RD_COLOR1 1

#define RD_NUM_COLORS 2

#define RD_TEXTURE0 0
#define RD_TEXTURE1 1
#define RD_TEXTURE2 2
#define RD_TEXTURE3 3

#define RD_NUM_TEXTURES 4

#define RD_TEXCOORD0 0
#define RD_TEXCOORD1 1
#define RD_TEXCOORD2 2
#define RD_TEXCOORD3 3

#define RD_NUM_TEXCOORDS 4

// render state

typedef uint8_t rdBlend_t;
typedef enum RD_BLEND
{
	RD_BLEND_ZERO        = 0,
	RD_BLEND_ONE         = 1,
	RD_BLEND_DSTCOLOR    = 2,
	RD_BLEND_INVDSTCOLOR = 3,
	RD_BLEND_SRCALPHA    = 4,
	RD_BLEND_INVSRCALPHA = 5,
	RD_BLEND_SRCCOLOR    = 6,
	RD_BLEND_INVSRCCOLOR = 7
	//RD_BLEND_DSTALPHA    = 6,
	//RD_BLEND_INVDSTALPHA = 7,
} RD_BLEND;

typedef uint8_t rdCompare_t;
typedef enum RD_COMPARE
{
	RD_COMPARE_ALWAYS        = 0,
	RD_COMPARE_LESS          = 1,
	RD_COMPARE_LESS_EQUAL    = 2,
	RD_COMPARE_GREATER       = 3,
	RD_COMPARE_GREATER_EQUAL = 4,
	RD_COMPARE_EQUAL         = 5,
	RD_COMPARE_NOT_EQUAL     = 6,
	RD_COMPARE_NEVER         = 7
} RD_COMPARE;

typedef uint8_t rdCullMode_t;
typedef enum RD_CULL_MODES
{
	RD_CULL_MODE_NONE,
	RD_CULL_MODE_BACK,
	RD_CULL_MODE_FRONT
} RD_CULL_MODES;

typedef uint8_t rdScissorMode_t;
typedef enum RD_SCISSOR_MODES
{
	RD_SCISSOR_DISABLED,
	RD_SCISSOR_ENABLED
} RD_SCISSOR_MODES;

typedef uint8_t rdChromaKeyMode_t;
typedef enum RD_CHROMA_KEY_MODE
{
	RD_CHROMA_KEY_DISABLED,
	RD_CHROMA_KEY_ENABLED,
} RD_CHROMA_KEY_MODE;

typedef uint8_t rdPrimitiveType_t;
typedef enum RD_PRIMITIVE_TYPE
{
	RD_PRIMITIVE_NONE,
	RD_PRIMITIVE_TRIANGLES,
	RD_PRIMITIVE_TRIANGLE_FAN,
	RD_PRIMITIVE_POLYGON,
} RD_PRIMITIVE_TYPE;

typedef uint8_t rdMatrixMode_t;
typedef enum RD_MATRIX_MODE
{
	RD_MATRIX_MODEL,
	RD_MATRIX_VIEW,
	RD_MATRIX_PROJECTION,

	RD_MATRIX_TYPES
} RD_MATRIX_MODE;

typedef uint8_t rdTexGen_t;
enum RD_TEXGEN
{
	RD_TEXGEN_NONE    = 0,
	RD_TEXGEN_HORIZON = 1,
	RD_TEXGEN_CEILING = 2,
	RD_TEXGEN_WATER   = 3
};

typedef uint8_t rdTexFilter_t;
enum RD_TEXFILTER
{
	RD_TEXFILTER_NEAREST  = 0,
	RD_TEXFILTER_BILINEAR = 1,
};

typedef uint8_t rdDitherMode_t;
enum RD_DITHER_MODE
{
	RD_DITHER_NONE = 0,
	RD_DITHER_4x4  = 1,
};

typedef uint8_t rdAmbientFlags_t;
enum RD_AMBIENT_FLAGS
{
	RD_AMBIENT_NONE         = 0x0,
	RD_AMBIENT_OCCLUDERS    = 0x1,
	RD_AMBIENT_SCREEN_SPACE = 0x2,
	RD_AMBIENT_CAUSTICS     = 0x4, // todo: remove this, caustics should be a texture effect but we don't have TMUs yet
};

typedef uint8_t rdDecalMode_t;
enum RD_DECAL_MODE
{
	RD_DECALS_DISABLED = 0,
	RD_DECALS_ENABLED  = 1
};

typedef uint8_t rdFogMode_t;
enum RD_FOG_MODE
{
	RD_FOG_DISABLED = 0,
	RD_FOG_ENABLED  = 1
};

typedef uint8_t rdDecalMode_t;

#endif
