#ifndef _OPENJKDF2_ENGINE_CONFIG_H
#define _OPENJKDF2_ENGINE_CONFIG_H

#include <float.h>
#include "types_enums.h"

// Experimental features
#ifdef QOL_IMPROVEMENTS

//#define RENDER_DROID2        // new gpu path, skips rdCache and uses a fixed-function like API to pass information to the GPU for HW transform, clipping and lighting

//#define HW_VBUFFER // temp stuff, trying to move vbuffer to GPU and use fbos in there so we can render to them

#define JOB_SYSTEM           // multithreaded job system

#ifndef RENDER_DROID2
#define TILE_SW_RASTER
#endif

// Gameplay features
#define FP_LEGS              // Draws legs in first person
#define DYNAMIC_POV          // POV enhancements, such as the weapon following the autoaim target, muzzle flashes, sway improvements
#define LIGHTSABER_TRAILS    // visual enhancement for lightsabers, draws extra tris to simulate lightsaber trails/motion blur
#define LIGHTSABER_MARKS     // visual enhancement for lightsabers, draws impact decals with saber collision, requires DECAL_RENDERING
//#define LIGHTSABER_GLOW    // visual enhancement for lightsabers, draws a sprite-based glow similar to Jedi Outcast/Academy
#define REGIONAL_DAMAGE      // dismemberment enhancements and regional damage support
#define GAMEPLAY_COGS        // adds some gameplay system cogs (like MasterCog and ActionCog) that can be used for specific shared game logic
#define PUPPET_PHYSICS       // physics-based animations (ex. ragdolls) using puppet joints
#define STATIC_JKL_EXT       // load extra .jkl files from resources on startup along side static.jkl

// Old render pipeline features
//#define STENCIL_BUFFER       // mark the stencil buffer with dynamic/transparent stuff, so we can effectively cull things like decals
//#define CLASSIC_EMISSIVE     // compute emissives using the same approach as stock JK with max(color, emissive), rather than adding it on top as bloom
//#define NEW_SSAO             // disk-to-disk based SSAO with performance improvements
//#define NEW_BLOOM            // optimized downsample based bloom with better performance
//#define RGB_THING_LIGHTS     // RGB thing lighting with lightcolor template param
//#define RGB_AMBIENT          // RGB sector ambient with spherical harmonics for gouraud models
//#define PARTICLE_LIGHTS      // very simple lights for particle elements
//#define HALF_LAMBERT         // use half lambert gouraud for dynamic lights
//#define SPECULAR_LIGHTING    // RD_LIGHTMODE_SPECULAR
//#define DECAL_RENDERING      // .dcal support and decal rendering path (for blast marks etc)
//#define SPHERE_AO            // deferred ambient occlusion via sphere volumes
//#define ADDITIVE_BLEND       // additive/screen blending support with a new face flags
//#define POLYLINE_EXT         // .pln support and polyline template param, allows using polylines as rendering primitives for things (ex. blaster bolts)
//#define VERTEX_COLORS        // add vertex color face flag to indicate using the rdProcEntry's color field as the vertex color
//#define FOG                  // infernal machine style fog rendering

#define RD_AMBIENT_LOBES 8

#ifdef RENDER_DROID2

#define MENU_16BIT           // 16 bit menu bitmaps

// these are implicit in RENDER_DROID2, the defines are for the old pipeline
#ifdef SPHERE_AO
#undef SPHERE_AO
#endif
#ifdef PARTICLE_LIGHTS
#undef PARTICLE_LIGHTS
#endif
#ifdef DECAL_RENDERING
#undef DECAL_RENDERING
#endif
#ifdef NEW_BLOOM
#undef NEW_BLOOM
#endif
#ifdef NEW_SSAO
#undef NEW_SSAO
#endif

// these are always active for RENDER_DROID2
#ifndef RGB_AMBIENT
#define RGB_AMBIENT
#endif
#ifndef RGB_THING_LIGHTS
#define RGB_THING_LIGHTS
#endif
#ifndef SPECULAR_LIGHTING
#define SPECULAR_LIGHTING
#endif
#ifndef ADDITIVE_BLEND
#define ADDITIVE_BLEND
#endif
#ifndef POLYLINE_EXT
#define POLYLINE_EXT
#endif
#ifndef VERTEX_COLORS
#define VERTEX_COLORS
#endif
#ifndef FOG
#define FOG
#endif

//#define MOTION_BLUR

#define RD_CACHE_MAX_DRAW_CALLS				16384
#define RD_CACHE_MAX_DRAW_CALL_VERTS		(RD_CACHE_MAX_DRAW_CALLS * 24)
#define RD_CACHE_MAX_DRAW_CALL_INDICES		(RD_CACHE_MAX_DRAW_CALLS * 66)

#endif

#define STD3D_CLUSTER_MAX_LIGHTS          256 // match RDCAMERA_MAX_LIGHTS/SITHREND_NUM_LIGHTS
#define STD3D_CLUSTER_MAX_OCCLUDERS       128
#define STD3D_CLUSTER_MAX_DECALS          256
#define STD3D_CLUSTER_MAX_ITEMS           (STD3D_CLUSTER_MAX_LIGHTS + STD3D_CLUSTER_MAX_OCCLUDERS + STD3D_CLUSTER_MAX_DECALS)
#define STD3D_CLUSTER_BUCKETS_PER_CLUSTER (STD3D_CLUSTER_MAX_ITEMS >> 5)
#define STD3D_CLUSTER_GRID_SIZE_X         16
#define STD3D_CLUSTER_GRID_SIZE_Y         8
#define STD3D_CLUSTER_GRID_SIZE_Z         24

#define STD3D_CLUSTER_GRID_SIZE_XYZ (STD3D_CLUSTER_GRID_SIZE_X * STD3D_CLUSTER_GRID_SIZE_Y * STD3D_CLUSTER_GRID_SIZE_Z)
#define STD3D_CLUSTER_GRID_TOTAL_SIZE (STD3D_CLUSTER_GRID_SIZE_X * STD3D_CLUSTER_GRID_SIZE_Y * STD3D_CLUSTER_GRID_SIZE_Z * STD3D_CLUSTER_BUCKETS_PER_CLUSTER)

// helper to avoid redundantly checking this constantly
#ifdef SPECULAR_LIGHTING
#define USES_VERTEX_LIGHTING(LIGHT_MODE) (((LIGHT_MODE) == 3) || ((LIGHT_MODE) == 4)|| ((LIGHT_MODE) == 5))
#else
#define USES_VERTEX_LIGHTING(LIGHT_MODE) ((LIGHT_MODE) == 3)
#endif

#if defined(DECAL_RENDERING) || defined(PARTICLE_LIGHTS) || defined(SPHERE_AO)
#define VIEW_SPACE_GBUFFER // store view space data in the gbuffer instead of clip space, important for deferred effects like decals
#define DEFERRED_FRAMEWORK // helper stuff for deferred passes
#endif

#endif // QOL_IMPROVEMENTS

#ifdef PUPPET_PHYSICS

#ifndef REGIONAL_DAMAGE
#define REGIONAL_DAMAGE // make sure this on
#endif

#ifndef ANIMCLASS_NAMES
#define ANIMCLASS_NAMES // make sure we use the named system
#endif

#endif // QOL_IMPROVEMENTS

// If I ever do demo recording, add it here
#define NEEDS_STEPPED_PHYS 0//(!jkPlayer_bJankyPhysics || sithNet_isMulti)

// Settings for stepped physics
#ifdef TARGET_TWL
#define TARGET_PHYSTICK_FPS (20.0)
#define DELTA_PHYSTICK_FPS (1.0/TARGET_PHYSTICK_FPS)
#else
#define TARGET_PHYSTICK_FPS (sithNet_isMulti ? (sithNet_tickrate < 100 ? 150.0 : 50.0) : 150.0)
#define DELTA_PHYSTICK_FPS (1.0/TARGET_PHYSTICK_FPS)
#endif

// Settings for the old stepped physics
#define OLDSTEP_TARGET_FPS (sithNet_isMulti ? (sithNet_tickrate < 100 ? 150.0 : 50.0) : 50.0)
#define OLDSTEP_DELTA_50FPS (1.0/OLDSTEP_TARGET_FPS)

// Default setting for canonical COG tickrate minimum.
// In the original game, sleeps < 0.02s will always round up to 0.02s
// in practice, because every COG execute tick was always 0.02s apart from
// the frame limiter.
// For consistency on some sector thrusts (Lv18's air shafts for example)
// we have to round up.
#define CANONICAL_COG_TICKRATE (1.0 / 50.0)

// Default setting for canonical physics tickrate minimum.
// In the original game, slopes drag the player downwards
// faster depending on the framerate.
// At high FPS, this causes the player to get stuck climbing
// up slopes, because the player falls more than they were
// able to climb, creating a barrier.
//
// This default is based on the boxes in Training.jkl in Droidworks
#define CANONICAL_PHYS_TICKRATE (1.0 / 25.0)

// Use microsecond timing to calculate sithTime_deltaSecs/etc
#if defined(PLATFORM_POSIX) && !defined(TARGET_TWL)
#define MICROSECOND_TIME
#endif

// Original game will speed up if framerate is over 100?
#ifndef QOL_IMPROVEMENTS
#define SITHTIME_MINDELTA (10)
#define SITHTIME_MAXDELTA (500)
#define SITHTIME_MINDELTA_US (10*1000)
#define SITHTIME_MAXDELTA_US (500*1000)
#else
#define SITHTIME_MINDELTA (1)
#define SITHTIME_MAXDELTA (500)
#define SITHTIME_MINDELTA_US (1)
#define SITHTIME_MAXDELTA_US (500*1000)
#endif

#ifdef QOL_IMPROVEMENTS
#define MULTI_NEXTLEVEL_DELAY_MS (2000)
#define MULTI_LEAVEJOIN_DELAY_MS (1000)
#define MULTI_TIMEOUT_MS (30000)
#define MULTI_SCORE_UPDATE_INTERVAL_MS (5000)
#define MULTI_BIG_UPDATE_INTERVAL_MS (30)
#else // !QOL_IMPROVEMENTS
#define MULTI_NEXTLEVEL_DELAY_MS (10000)
#define MULTI_LEAVEJOIN_DELAY_MS (5000)
#define MULTI_TIMEOUT_MS (45000)
#define MULTI_SCORE_UPDATE_INTERVAL_MS (10000)
#define MULTI_BIG_UPDATE_INTERVAL_MS (60)
#endif

// World limits
#if !defined(QOL_IMPROVEMENTS)
#define SITH_MAX_THINGS (641)
#define SITH_MAX_VISIBLE_SECTORS (0x80)
#define SITH_MAX_VISIBLE_SECTORS_2 (0xA0)
#define SITH_MAX_VISIBLE_ALPHA_SURFACES (32)
#define SITH_MAX_SURFACE_CLIP_ITERS (25) // not real
#elif defined(TARGET_TWL)
#define SITH_MAX_THINGS (641)
#define SITH_MAX_VISIBLE_SECTORS (256)
#define SITH_MAX_VISIBLE_SECTORS_2 (256+32)
#define SITH_MAX_VISIBLE_ALPHA_SURFACES (32)
#define SITH_MAX_SURFACE_CLIP_ITERS (25)
#else // QOL_IMPROVEMENTS
#define SITH_MAX_THINGS (32000)
#define SITH_MAX_VISIBLE_SECTORS (1024)
#define SITH_MAX_VISIBLE_SECTORS_2 (1280)
#define SITH_MAX_VISIBLE_ALPHA_SURFACES (1024)
#define SITH_MAX_SURFACE_CLIP_ITERS (50)
#endif // QOL_IMPROVEMENTS

#if !defined(QOL_IMPROVEMENTS) || defined(TARGET_TWL)
#define SITHREND_NUM_LIGHTS (32) // why is this different than the max in rdCamera?
#else
#ifdef RENDER_DROID2
#define SITHREND_NUM_LIGHTS (256)
#else
#define SITHREND_NUM_LIGHTS (64)
#endif
#endif

// COG resource limits
#if defined(QOL_IMPROVEMENTS) && !defined(TARGET_TWL)
#define SITHCOGVM_MAX_STACKSIZE (0x10000)
#define SITHCOG_SYMBOL_LIMIT (2048) // JK was 512, MoTS/DW are 1024
#define SITHCOG_LINKED_SYMBOL_LIMIT (2048)
#define SITHCOG_MAX_LINKS (2048)
#define SITHCOG_NODE_STACKDEPTH (0x800) // JK was 0x200, MoTS is 0x400
#elif defined(TARGET_TWL)

#define SITHCOGVM_MAX_STACKSIZE (64)
#define SITHCOG_SYMBOL_LIMIT (1024) // JK was 512, MoTS/DW are 1024
#define SITHCOG_LINKED_SYMBOL_LIMIT (256)
#define SITHCOG_MAX_LINKS (512)
#define SITHCOG_NODE_STACKDEPTH (0x400) // JK was 0x200, MoTS is 0x400

#else // !QOL_IMPROVEMENTS
#define SITHCOGVM_MAX_STACKSIZE (64)
#define SITHCOG_SYMBOL_LIMIT (1024) // JK was 512, MoTS/DW are 1024
#define SITHCOG_LINKED_SYMBOL_LIMIT (256)
#define SITHCOG_MAX_LINKS (512)
#define SITHCOG_NODE_STACKDEPTH (0x400) // JK was 0x200, MoTS is 0x400
#endif // QOL_IMPROVEMENTS

// Weapon-related limits
#define MAX_DEFLECTION_BOUNCES (6)

#if defined(TARGET_TWL)
#define RDCACHE_MAX_TRIS (0x800) // theoretical max 0x800?
#define RDCACHE_MAX_VERTICES (0x1800)

#define STD3D_MAX_TEXTURES (512)
#define STD3D_MAX_UI_TRIS (0x100)
#define STD3D_MAX_UI_VERTICES (0x100)
#elif defined(TILE_SW_RASTER)
#define RDCACHE_MAX_TRIS (0x1000)
#define RDCACHE_MAX_VERTICES (0x8000)

#define STD3D_MAX_TEXTURES (4096)
#define STD3D_MAX_UI_TRIS (0x8000)
#define STD3D_MAX_UI_VERTICES (0x8000)
#else
#define RDCACHE_MAX_TRIS (0x400)
#define RDCACHE_MAX_VERTICES (0x8000)

#define STD3D_MAX_TEXTURES (4096)
#define STD3D_MAX_UI_TRIS (0x8000)
#define STD3D_MAX_UI_VERTICES (0x8000)
#endif

#ifdef TILE_SW_RASTER
#define RDCACHE_MAX_TILE_TRIS        (RDCACHE_MAX_TRIS * 22)
#define RDCACHE_FINE_TILE_SIZE       32
#define RDCACHE_TILE_DOWNSAMPLE      8
#define RDCACHE_COARSE_TILE_SIZE     (RDCACHE_TILE_DOWNSAMPLE * RDCACHE_FINE_TILE_SIZE)
#define RDCACHE_TILE_BINNING_STRIDE  ((RDCACHE_MAX_TRIS+63) / 64)
//#define RDCACHE_TILE_BINNING_STRIDE  ((RDCACHE_MAX_TILE_TRIS+63) / 64)
#define RDCACHE_FINE_PER_COARSE      RDCACHE_TILE_DOWNSAMPLE
#endif

#ifdef QOL_IMPROVEMENTS
#define RDSURFACE_MAX 4096
#else
#define RDSURFACE_MAX 256
#endif

#define SITHCONTROL_NUM_HANDLERS (9)

// jkGuiMultiTally delay between maps
#define SCORE_DELAY_MS ((jkGuiNetHost_bIsDedicated && stdComm_bIsServer) ? 0 : 30000)

// UI tickrate limits
#ifdef QOL_IMPROVEMENTS
#define TICKRATE_MIN (1)
#define TICKRATE_MAX (1000)
#else
#define TICKRATE_MIN (100)
#define TICKRATE_MAX (300)
#endif

// Run game physics at a fixed timestep
// todo: disabled for now, causes a lot of stuttering esp with high framerates, need to add some control interpolation or something
#define FIXED_TIMESTEP_PHYS

// Backport MOTS RGB lighting and bone changes
#ifdef QOL_IMPROVEMENTS
#define JKM_LIGHTING
#define JKM_BONES
#define JKM_PARAMS
#define JKM_AI
#define JKM_SABER
#define JKM_DSS
#define JKM_CAMERA
#endif

// Backport Droidworks misc
#ifdef QOL_IMPROVEMENTS
#define DW_CAMERA
#endif

#ifdef JKM_DSS
#define NUM_JKPLAYER_THINGS (64)
#define SITHINVENTORY_NUM_POWERKEYBINDS (32+1)
#else
#define NUM_JKPLAYER_THINGS (16)
#define SITHINVENTORY_NUM_POWERKEYBINDS (20+1)
#endif

#define SITHBIN_NUMBINS (200)

// Droidworks cameras
#ifdef DW_CAMERA
#define SITHCAMERA_NUMCAMERAS (8)
#else
#define SITHCAMERA_NUMCAMERAS (7)
#endif

#define SITHCAMERA_FOV (90.0)
#define SITHCAMERA_ASPECT (1.0)
#define SITHCAMERA_ATTENUATION_MIN (0.4)
#define SITHCAMERA_ATTENUATION_MAX (0.8)

#if defined(DYNAMIC_POV) && !defined(TILE_SW_RASTER)
#define SITHCAMERA_ZNEAR_FIRSTPERSON (1.0 / 512.0f)
#define SITHCAMERA_ZNEAR (1.0 / 64.0)
#define SITHCAMERA_ZFAR (256.0)
#else
#if defined(SDL2_RENDER) && !defined(TILE_SW_RASTER)
#define SITHCAMERA_ZNEAR_FIRSTPERSON (1.0 / 128.0)
#define SITHCAMERA_ZNEAR (1.0 / 64.0)
#define SITHCAMERA_ZFAR (128.0)
#elif defined(TARGET_TWL)
#define SITHCAMERA_ZNEAR_FIRSTPERSON (1.0 / 64.0)
#define SITHCAMERA_ZNEAR (1.0 / 64.0)
#define SITHCAMERA_ZFAR (64.0)
#else
#define SITHCAMERA_ZNEAR_FIRSTPERSON (1.0 / 64.0)
#define SITHCAMERA_ZNEAR (1.0 / 64.0)
#define SITHCAMERA_ZFAR (64.0)
#endif
#endif

#define SITHPARTICLE_MAX_PARTICLES (64)

#if !defined(QOL_IMPROVEMENTS) || defined(TARGET_TWL)
#define RDCAMERA_MAX_LIGHTS 64
#else
#ifdef RENDER_DROID2
#define RDCAMERA_MAX_LIGHTS 256
#else
#define RDCAMERA_MAX_LIGHTS 128

#endif
#endif

#ifdef SDL2_RENDER
#define JOYSTICK_MAX_STRS (6)
#else
#define JOYSTICK_MAX_STRS (3)
#endif

//
// Resource configuration
//
#define JKRES_GOB_EXT (Main_bMotsCompat ? "goo" : "gob")

#define JKRES_DF2_MAGIC_0 0x69973284
#define JKRES_DF2_MAGIC_1 0x699232C4
#define JKRES_DF2_MAGIC_2 0x69923384
#define JKRES_DF2_MAGIC_3 0x69923284

#define JKRES_MOTS_MAGIC_0 0x3B426929
#define JKRES_MOTS_MAGIC_1 0x3B426929
#define JKRES_MOTS_MAGIC_2 0x3B426929
#define JKRES_MOTS_MAGIC_3 0x3B426929

#define JKRES_MAGIC_0 (Main_bMotsCompat ? JKRES_MOTS_MAGIC_0 : JKRES_DF2_MAGIC_0)
#define JKRES_MAGIC_1 (Main_bMotsCompat ? JKRES_MOTS_MAGIC_1 : JKRES_DF2_MAGIC_1)
#define JKRES_MAGIC_2 (Main_bMotsCompat ? JKRES_MOTS_MAGIC_2 : JKRES_DF2_MAGIC_2)
#define JKRES_MAGIC_3 (Main_bMotsCompat ? JKRES_MOTS_MAGIC_3 : JKRES_DF2_MAGIC_3)

#define JKRES_IS_MOTS_MAGIC(kval) ((kval == JKRES_MOTS_MAGIC_0) || (kval == JKRES_MOTS_MAGIC_1) || (kval == JKRES_MOTS_MAGIC_2) || (kval == JKRES_MOTS_MAGIC_3))
#define JKRES_IS_DF2_MAGIC(kval) ((kval == JKRES_DF2_MAGIC_0) || (kval == JKRES_DF2_MAGIC_1) || (kval == JKRES_DF2_MAGIC_2) || (kval == JKRES_DF2_MAGIC_3))

#ifndef JKM_PARAMS
#define STDCONF_LINEBUFFER_LEN (1024)
#else
#define STDCONF_LINEBUFFER_LEN (2048)
#endif

#define SITHAI_MAX_ACTORS (256)

#if defined(QOL_IMPROVEMENTS) && !defined(TARGET_TWL)
#define SITH_MIXER_NUMPLAYINGSOUNDS (256)
#else
#define SITH_MIXER_NUMPLAYINGSOUNDS (32)
#endif

#ifdef PLATFORM_STEAM

#define VOICE_OUTPUT_SAMPLE_RATE		11025
#define VOICE_OUTPUT_BYTES_PER_SAMPLE		2

#endif

//
// Misc optimizations/features
//
#if defined(TARGET_TWL)
#define STDHASHTABLE_CRC32_KEYS
#endif

// Deferred loading and LRU unloading
#if defined(QOL_IMPROVEMENTS)
#define RDMATERIAL_LRU_LOAD_UNLOAD
#endif

#ifdef QOL_IMPROVEMENTS
#define JKDEV_NUM_CHEATS (64)
#else
#define JKDEV_NUM_CHEATS (32)
#endif

#define SITHCVAR_MAX_CVARS (512)
#define SITHCVAR_MAX_STRLEN (256)
#define SITHCVAR_MAX_NAME_STRLEN (64)

#ifdef ARCH_WASM
#define SITHCVAR_FNAME ("persist/openjkdf2_cvars.json")
#define SITHBINDS_FNAME ("persist/openjkdf2_binds.json")
#define REGISTRY_FNAME ("persist/registry.json")
#else
#define SITHCVAR_FNAME ("openjkdf2_cvars.json")
#define SITHBINDS_FNAME ("openjkdf2_binds.json")
#define REGISTRY_FNAME ("registry.json")
#endif

#define STDUPDATER_DEFAULT_URL ("https://api.github.com/repos/ladyeebs/OpenJKDF2/releases?per_page=1")
#define STDUPDATER_DEFAULT_WIN64_FILENAME ("win64-debug.zip")
#define STDUPDATER_DEFAULT_MACOS_FILENAME ("macos-debug.tar.gz")

#define DF2_ONLY_COND(cond) ( Main_bMotsCompat || (!Main_bMotsCompat && (cond)) )
#define MOTS_ONLY_COND(cond) ( !Main_bMotsCompat || (Main_bMotsCompat && (cond)) )
#define MOTS_ONLY_FLAG(_flag) (Main_bMotsCompat ? (_flag) : (0))

#define MOTS_SAVE_VERSION 0x7D6
#define JK_SAVE_VERSION   0x6

#define OPENJKDF2_SAVE_VERSION 0x7FE

#ifdef STATIC_JKL_EXT // requires higher precision indices, so the savegame version must be updated
#define COMPAT_SAVE_VERSION OPENJKDF2_SAVE_VERSION
#else
#define COMPAT_SAVE_VERSION (Main_bMotsCompat ? MOTS_SAVE_VERSION : JK_SAVE_VERSION)
#endif
#define JKSAVE_FORMATSTR (Main_bMotsCompat ? "msav%04d.jks" : "save%04d.jks")

extern int Window_isHiDpi;
#ifdef WIN64_STANDALONE
#ifdef TILE_SW_RASTER
	#define WINDOW_DEFAULT_WIDTH  (640)
	#define WINDOW_DEFAULT_HEIGHT (480)
	#define WINDOW_DEFAULT_WIDTH_4K  WINDOW_DEFAULT_WIDTH
	#define WINDOW_DEFAULT_HEIGHT_4K WINDOW_DEFAULT_HEIGHT
#elif defined(RENDER_DROID2)
#	define WINDOW_DEFAULT_WIDTH  (1280)
#	define WINDOW_DEFAULT_HEIGHT (720)
#	define WINDOW_DEFAULT_WIDTH_4K  (WINDOW_DEFAULT_WIDTH*2)
#	define WINDOW_DEFAULT_HEIGHT_4K (WINDOW_DEFAULT_HEIGHT*2)
#else
#	define WINDOW_DEFAULT_WIDTH  (640*2)
#	define WINDOW_DEFAULT_HEIGHT (480*2)
#	define WINDOW_DEFAULT_WIDTH_4K  (640*3)
#	define WINDOW_DEFAULT_HEIGHT_4K (480*3)
#endif
#else // WIN64_STANDALONE
#define WINDOW_DEFAULT_WIDTH  (640)
#define WINDOW_DEFAULT_HEIGHT (480)

#define WINDOW_DEFAULT_WIDTH_4K  WINDOW_DEFAULT_WIDTH
#define WINDOW_DEFAULT_HEIGHT_4K WINDOW_DEFAULT_HEIGHT
#endif // WIN64_STANDALONE

// The type to use for flex_t:
// - float for original game behavior
// - double to verify flex_t vs flex32_t vs cog_flex_t is working
// - TODO: fixed point support?
typedef float flex_t_type; // _Float16
typedef double flex_d_t_type;

// Fixed point experiment
#ifdef EXPERIMENTAL_FIXED_POINT
#define FIXED_POINT_DECIMAL_BITS (16)
#define FIXED_POINT_WHOLE_BITS   (32-FIXED_POINT_DECIMAL_BITS)
#endif

#define FLEX(n) ((flex_t)n)

// Disable warnings for Vegetable Studio
#if 1 && defined _MSC_VER
#pragma warning(disable: 4003) // not enough arguments for function-like macro invocation
#pragma warning(disable: 4005) // 'blahblah': macro redefinition
#pragma warning(disable: 4022) // pointer mismatch for actual parameter
#pragma warning(disable: 4024) // different types for formal and actual parameter
#pragma warning(disable: 4047) // 'blahblah *' differs in levels of indirection from 'blahblah (*)[blah]'
#pragma warning(disable: 4090) // different 'const' qualifiers
#pragma warning(disable: 4098) // 'void' function returning a value
#pragma warning(disable: 4113) // 'FARPROC' differs in parameter lists from 'blahblah'
#pragma warning(disable: 4133) // 'function': incompatible types - from 'blahblah [blah]' to 'const blah *'
#pragma warning(disable: 4190) // 'blahblah' has C-linkage specified, but returns UDT 'blahblahblahhhhh' which is incompatible with C
#pragma warning(disable: 4229) // anachronism used: modifiers on data are ignored
#pragma warning(disable: 4311) // 'type cast': pointer truncation from 'blah *' to 'blah'
#pragma warning(disable: 4312) // 'type cast': conversion from 'blah' to 'blah *' of greater size
#pragma warning(disable: 4700) // uninitialized local variable 'blahblah' used
#pragma warning(disable: 4715) // not all control paths return a value
#pragma warning(disable: 4716) // 'blahblah': must return a value
#pragma warning(disable: 5105) // macro expansion producing 'defined' has undefined behavior
#endif

// Optimize for math operations, depending on the architecture
// TODO: Have a TARGET_ARMvIDK or something
#ifdef TARGET_TWL
#define MATH_FUNC __attribute__((target("arm")))
#else
#define MATH_FUNC
#endif

#endif // _OPENJKDF2_ENGINE_CONFIG_H