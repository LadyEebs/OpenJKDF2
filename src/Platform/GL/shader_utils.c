/**
 * From the OpenGL Programming wikibook: http://en.wikibooks.org/wiki/OpenGL_Programming
 * This file is in the public domain.
 * Contributors: Sylvain Beucler
 */

#ifdef SDL2_RENDER

#include "shader_utils.h"
#include "globals.h"

#include "SDL2_helper.h"
#include <stdio.h>
#include <string.h>

#include "stdPlatform.h"
#include "General/stdString.h"
#include "General/stdFnames.h"

#ifdef LINUX
#include "external/fcaseopen/fcaseopen.h"
#endif

#include "Platform/Common/stdEmbeddedRes.h"

/**
 * Display compilation errors from the OpenGL shader compiler
 */
void print_log(GLuint object) {
	GLint log_length = 0;
	if (glIsShader(object)) {
		glGetShaderiv(object, GL_INFO_LOG_LENGTH, &log_length);
	} else if (glIsProgram(object)) {
		glGetProgramiv(object, GL_INFO_LOG_LENGTH, &log_length);
	} else {
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR,
					   "printlog: Not a shader or a program");
		return;
	}

	char* log = (char*)malloc(log_length);
	
	if (glIsShader(object))
		glGetShaderInfoLog(object, log_length, NULL, log);
	else if (glIsProgram(object))
		glGetProgramInfoLog(object, log_length, NULL, log);
	
	SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, "%s\n", log);
	
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", log, NULL);
	
	free(log);
}

typedef struct 
{
	char  name[32];
	char* source;
} std3D_shaderImport;
std3D_shaderImport std3D_loadedImports[32];
int std3D_numLoadedImports = 0;

int std3D_Imported(const char* name)
{
	for (int i = 0; i < std3D_numLoadedImports; ++i)
	{
		if (strnicmp(name, std3D_loadedImports[i].name, 32) == 0)
			return 1;
	}
	return 0;
}

char* load_source(const char* filepath)
{
	char* shader_contents = stdEmbeddedRes_Load(filepath, NULL);
	if(!shader_contents)
		return NULL;

	// scan the shader line by line for imports
	// we do it by line instead of at first sight of an import because it might be commented out
	char* curLine = shader_contents;
	while (curLine)
	{
		// grab the next line
		char* nextLine = strchr(curLine, '\n');
		if (nextLine)
			*nextLine = '\0';

		// comments are not supported on the same line for simplicity
		char* comment = strstr(curLine, "//");
		if (!comment)
		{
			// check for an import
			char* import = strstr(curLine, "import");
			if (import)
			{
				// extract the name
				char included_file[32];
				stdString_GetQuotedStringContents(import, included_file, 32);
				if (!std3D_Imported(included_file))
				{
					if (std3D_numLoadedImports >= 32)
					{
						char errtmp[256];
						snprintf(errtmp, 256, "std3D: Too many imports in shader file `%s`\n", filepath);
						SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", errtmp, NULL);
						free(shader_contents);
						return NULL;
					}

					// all imports are in the same folder for simplicity
					char resolved_path[128];
					snprintf(resolved_path, 128, "shaders/includes/%s", included_file);
					char* source = load_source(resolved_path);
					if (source)
					{
						std3D_shaderImport* include = &std3D_loadedImports[std3D_numLoadedImports++];
						stdString_SafeStrCopy(include->name, included_file, 32);
						include->source = source;
					}
				}

				// wipe the entire line
				char* cur = curLine;
				while (cur != nextLine)
					*(cur++) = ' ';
			}
		}

		if (nextLine)
			*nextLine = '\n';

		curLine = nextLine ? (nextLine + 1) : NULL;
	}

	return shader_contents;
}

GLuint load_shader_file(const char* filepath, GLenum type, const char* userDefines)
{
	std3D_numLoadedImports = 0;

    char* shader_contents = load_source(filepath);// stdEmbeddedRes_Load(filepath, NULL);

    if (!shader_contents)
    {
    	char errtmp[256];
        snprintf(errtmp, 256, "std3D: Failed to load shader file `%s` [%s]!\n", filepath, userDefines);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", errtmp, NULL);
        return -1;
    }
    
    stdPlatform_Printf("std3D: Loading shader `%s` [%s]\n", filepath, userDefines);
    
    GLuint ret = create_shader(shader_contents, type, userDefines);
    free(shader_contents);
    
	for (int i = 0; i < std3D_numLoadedImports; ++i)
	{
		free(std3D_loadedImports[i].source);
		std3D_loadedImports[i].source = 0;
	}

	std3D_numLoadedImports = 0;

    return ret;
}

/**
 * Compile the shader from file 'filename', with error handling
 */
GLuint create_shader(const char* shader, GLenum type, const char* userDefines)
{
	const GLchar* source = (const GLchar*)shader;
	GLuint res = glCreateShader(type);

	// GLSL version
	const char* version = "";
	int profile;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);


	const char* type_name = (type == GL_VERTEX_SHADER) ? "#define VERTEX_SHADER\n" : "#define FRAGMENT_SHADER\n";
	const char* extensions = "\n";
	const char* defines = "\n";
	//if (profile == SDL_GL_CONTEXT_PROFILE_ES)
	//	version = "#version 100\n";  // OpenGL ES 2.0
	//else
    //version = "#version 330 core\n";  // OpenGL 3.3
#ifdef MACOS
	version = "#version 330\n";
	extensions = "#extension GL_ARB_texture_gather : enable\n";
	defines = "#define CAN_BILINEAR_FILTER\n#define HAS_MIPS\n";
#else
    version = "#version 330\n";  // OpenGL ES 2.0
    extensions = "#extension GL_ARB_texture_gather : enable\n";
    defines = "#define CAN_BILINEAR_FILTER\n#define HAS_MIPS\n";
#endif

#if defined(WIN64_STANDALONE)
	extern int Window_GL4;
    version = Window_GL4 ? "#version 460\n" : "#version 330\n";
	// note: if this changes, make sure to update the list in std3D_Startup for logging purposes
    extensions =	"#if __VERSION__ < 400\n"
					// ARB
					"#	extension GL_ARB_shading_language_packing	: require\n"	// unpackUnorm4x8, core in 4.0
					"#	extension GL_ARB_texture_gather				: require\n"	// textureGather, core in 4.0
					"#	extension GL_ARB_texture_query_lod			: require\n"	// textureQueryLod, core in 4.0
					"#endif\n"
					"#if __VERSION__ < 420\n"
					"#	extension GL_ARB_shading_language_420pack	: require\n"	// layout(binding=n), line continuation in macros '\'
					"#endif\n"
					"#if __VERSION__ < 430\n"
					"#	extension GL_ARB_explicit_uniform_location	: require\n"	// layout(location=n), core in 4.3
					"#	extension GL_ARB_texture_query_levels		: require\n"	// textureQueryLevels, core in 4.3
					"#endif\n"
					"#extension GL_ARB_gpu_shader5					: require\n"	// findLSB, required for sampler/ubo dynamic indexing
					// KHR
					"#extension GL_KHR_shader_subgroup_ballot		: enable\n"		// subgroupBroadcastFirst
					"#extension GL_KHR_shader_subgroup_arithmetic	: enable\n"		// subgroupOr
					"#extension GL_KHR_shader_subgroup_vote			: enable\n"		// subgroupAll
					// EXT
					"#extension GL_EXT_shader_explicit_arithmetic_types_int8	: enable\n"	// 8 bit int
					"#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable\n"	// 16 bit float
					"#extension GL_EXT_shader_explicit_arithmetic_types_int16	: enable\n"	// 16 bit int
					"#extension GL_EXT_shader_explicit_arithmetic_types_int64	: enable\n"	// 64 bit int
					"#extension GL_EXT_shader_subgroup_extended_types_float16   : enable\n"
					// AMD
					"#extension GL_AMD_shader_trinary_minmax		: enable\n"		// min3, max3
					"#extension GL_AMD_gpu_shader_half_float_fetch	: enable\n"		// 16 bit samplers
					//"#extension GL_KHR_shader_subgroup_shuffle			: enable\n"
					//"#extension GL_KHR_shader_subgroup_shuffle_relative	: enable\n"
					//"#extension GL_KHR_shader_subgroup_clustered			: enable\n"
					//"#extension GL_KHR_shader_subgroup_quad				: enable\n"
					"#if __VERSION__ > 430\n"
					"#	extension GL_AMD_gcn_shader : enable\n" // cubeFaceIndexAMD
					"#endif\n"
					;
    defines = "#define CAN_BILINEAR_FILTER\n#define HAS_MIPS\n";
#endif

#if defined(ARCH_WASM)
    version = "#version 320 es\n";
    extensions = "\n";
    defines = "#define CAN_BILINEAR_FILTER\n";
#endif

#if defined(TARGET_ANDROID)
    version = "#version 320 es\n";
    extensions = "\n";
    defines = "#define CAN_BILINEAR_FILTER\n";
#endif

	static const char* featureDefines = "\n"
#ifdef CLASSIC_EMISSIVE
	"#define CLASSIC_EMISSIVE\n"
#endif
#ifdef NEW_BLOOM
	"#define NEW_BLOOM\n"
#endif
#ifdef NEW_SSAO
	"#define NEW_SSAO\n"
#endif
#ifdef VIEW_SPACE_GBUFFER
	"#define VIEW_SPACE_GBUFFER\n"
#endif
#ifdef RENDER_DROID2
	"#define RENDER_DROID2\n"
	"#define NEW_SSAO\n"
	"#define NEW_BLOOM\n"
#endif
#ifdef FOG
	"#define FOG\n"
#endif
#ifdef MOTION_BLUR
	"#define MOTION_BLUR\n"
#endif
#ifdef HW_VBUFFER
	"#define HW_VBUFFER\n"
#endif
	;

	char userDefs[1024] = { '\0' };
	if(userDefines && userDefines[0] != '\0')
	{
		char tmp[32];
		char tmp2[32+9];
		char* defs = userDefines;
		while (defs)
		{
			defs = stdString_CopyBetweenDelimiter(defs, tmp, 32, ";");
			if (tmp[0])
			{
				stdString_snprintf(tmp2, 32 + 9, "#define %s\n", tmp);
				strcat_s(userDefs, 1024, tmp2);
			}
		}
	}

	// GLES2 precision specifiers
	static const char* precision =
		"#ifdef GL_ES                        \n"
		"#  ifdef GL_FRAGMENT_PRECISION_HIGH \n"
		"     precision highp float;         \n"
		"#  else                             \n"
		"     precision mediump float;       \n"
		"#  endif                            \n"
		"#else                               \n"
		// Ignore unsupported precision specifiers
		"#  define lowp                      \n"
		"#  define mediump                   \n"
		"#  define highp                     \n"
		"#endif                              \n"
		// fp16, fallback to fp32 if not present
		// tricky to use correctly, lots of conversions if not careful
		"#ifdef GL_EXT_shader_explicit_arithmetic_types_float16\n"
		"#	define flex		float16_t\n"
		"#	define flex2	f16vec2\n"
		"#	define flex3	f16vec3\n"
		"#	define flex4	f16vec4\n"
		"#  define flex3x3	f16mat3\n"
		"#else\n"
		"#	define flex		float\n"
		"#	define flex2	vec2\n"
		"#	define flex3	vec3\n"
		"#	define flex4	vec4\n"
		"#  define flex3x3	mat3\n"
		"#endif\n"
		"#ifdef GL_AMD_gpu_shader_half_float_fetch\n"
		"#	define flexSampler2D	f16sampler2D\n"
		"#	define flexSampler2DMS	f16sampler2DMS\n"
		"#else\n"
		"#	define flexSampler2D	sampler2D\n"
		"#	define flexSampler2DMS	sampler2DMS\n"
		"#endif\n"
		;

	// custom intrinsics
	static const char* intrinsics =
		"#define M_PI 3.14159265358979323846\n"
		"#define M_2PI (M_PI * 2.0)\n"
		"#define M_INV_2PI (1.0 / M_2PI)\n"
		// min3 and max3 fallbacks
		"#ifndef GL_AMD_shader_trinary_minmax\n"
		"#define max3(x, y, z) max( (x), max( (y), (z) ) )\n"
		"#define min3(x, y, z) min( (x), min( (y), (z) ) )\n"
		"#endif\n"
		// saturate helpers
		"float saturate(float x)\n"
		"{\n"
		"	return clamp(x, 0.0, 1.0);\n"
		"}\n"
		"vec2 saturate(vec2 x)\n"
		"{\n"
		"	return clamp(x, vec2(0.0), vec2(1.0));\n"
		"}\n"
		"vec3 saturate(vec3 x)\n"
		"{\n"
		"	return clamp(x, vec3(0.0), vec3(1.0));\n"
		"}\n"
		"vec4 saturate(vec4 x)\n"
		"{\n"
		"	return clamp(x, vec4(0.0), vec4(1.0));\n"
		"}\n"
		"#ifdef GL_EXT_shader_explicit_arithmetic_types_float16\n"
		"float16_t saturate(float16_t x)\n"
		"{\n"
		"	return clamp(x, float16_t(0.0), float16_t(1.0));\n"
		"}\n"
		"f16vec2 saturate(f16vec2 x)\n"
		"{\n"
		"	return clamp(x, f16vec2(0.0), f16vec2(1.0));\n"
		"}\n"
		"f16vec3 saturate(f16vec3 x)\n"
		"{\n"
		"	return clamp(x, f16vec3(0.0), f16vec3(1.0));\n"
		"}\n"
		"f16vec4 saturate(f16vec4 x)\n"
		"{\n"
		"	return clamp(x, f16vec4(0.0), f16vec4(1.0));\n"
		"}\n"
		"#endif\n"
		// rcp
		"float rcp(float x)\n"
		"{\n"
		"	return 1.0 / x;\n"
		"}\n"
		"vec2 rcp(vec2 x)\n"
		"{\n"
		"	return 1.0 / x;\n"
		"}\n"
		"vec3 rcp(vec3 x)\n"
		"{\n"
		"	return 1.0 / x;\n"
		"}\n"
		"vec4 rcp(vec4 x)\n"
		"{\n"
		"	return 1.0 / x;\n"
		"}\n"
		"#ifdef GL_EXT_shader_explicit_arithmetic_types_float16\n"
		"float16_t rcp(float16_t x)\n"
		"{\n"
		"	return float16_t(1.0) / x;\n"
		"}\n"
		"f16vec2 rcp(f16vec2 x)\n"
		"{\n"
		"	return float16_t(1.0) / x;\n"
		"}\n"
		"f16vec3 rcp(f16vec3 x)\n"
		"{\n"
		"	return float16_t(1.0) / x;\n"
		"}\n"
		"f16vec4 rcp(f16vec4 x)\n"
		"{\n"
		"	return float16_t(1.0) / x;\n"
		"}\n"
		"#endif\n"
		"float rcpSafe(float x)\n"
		"{\n"
		"	return (abs(x) < 1e-5) ? 0.0 : 1.0 / x;\n"
		"}\n"
		"vec2 rcpSafe(vec2 x)\n"
		"{\n"
		"	return vec2(rcpSafe(x.x), rcpSafe(x.y));\n"
		"}\n"
		"vec3 rcpSafe(vec3 x)\n"
		"{\n"
		"	return vec3(rcpSafe(x.x), rcpSafe(x.y), rcpSafe(x.z));\n"
		"}\n"
		"vec4 rcpSafe(vec4 x)\n"
		"{\n"
		"	return vec4(rcpSafe(x.x), rcpSafe(x.y), rcpSafe(x.z), rcpSafe(x.w));\n"
		"}\n"
		// cubeFaceIndex
		"#ifdef GL_AMD_gcn_shader\n"
		"	float cubeFaceIndex(vec3 p)\n"
		"	{\n"
		"		return cubeFaceIndexAMD(p);\n"
		"	}\n"
		"#else\n"
		"	float cubeFaceIndex(vec3 v)\n"
		"	{\n"
		"		float faceID;\n"
		"		if (abs(v.z) >= abs(v.x) && abs(v.z) >= abs(v.y))\n"
		"			faceID = (v.z < 0.0) ? 5.0 : 4.0;\n"
		"		else if (abs(v.y) >= abs(v.x))\n"
		"			faceID = (v.y < 0.0) ? 3.0 : 2.0;\n"
		"		else\n"
		"			faceID = (v.x < 0.0) ? 1.0 : 0.0;\n"
		"		return faceID;\n"
		"	}\n"
		"#endif\n"
		// flex packing and unpacking
		"#ifdef GL_EXT_shader_explicit_arithmetic_types_float16\n"
		"uint packFlex2x16(flex2 v)\n"
		"{\n"
		"	return packFloat2x16(v);\n"
		"}\n"
		"flex2 unpackFlex2x16(uint v)\n"
		"{\n"
		"	return unpackFloat2x16(v);\n"
		"}\n"
		"#else\n"
		"uint packFlex2x16(vec2 v)\n"
		"{\n"
		"	return packHalf2x16(v);\n"
		"}\n"
		"vec2 unpackFlex2x16(uint v)\n"
		"{\n"
		"	return unpackHalf2x16(v);\n"
		"}\n"
		"#endif\n"
		// r11_g11_b10f packing and unpacking
		"uint packF2x11_1x10(vec3 rgb)\n"
		"{\n"
		"	uint r = (packHalf2x16(flex2(rgb.x)) << 17) & 0xFFE00000;\n"
		"	uint g = (packHalf2x16(flex2(rgb.y)) <<  6) & 0x001FFC00;\n"
		"	uint b = (packHalf2x16(flex2(rgb.z)) >>  5) & 0x000003FF;\n"
		"	return uint(r | g | b);\n"
		"}\n"
		"vec3 unpackF2x11_1x10(uint rgb)\n"
		"{\n"
		"	float r = unpackHalf2x16((rgb >> 17) & 0x7FF0).x;\n"
		"	float g = unpackHalf2x16((rgb >> 6) & 0x7FF0).x;\n"
		"	float b = unpackHalf2x16((rgb << 5) & 0x7FE0).x;\n"
		"	return vec3(r, g, b); \n"
		"}\n"
		// rgb10a2 packing and unpacking
		"uint packUnorm4x10_2(vec4 unpackedInput)\n"
		"{\n"
		"	uvec4 v = uvec4(round( clamp(unpackedInput, vec4(0.0), vec4(1.0)) * vec4(vec3(1023), 3) ));\n"
		"	return (v.x) | (v.y << 10) | (v.z << 20) | (v.w << 30); \n"
		"}\n"
		"vec4 unpackUnorm4x10_2(uint packedInput)\n"
		"{\n"
		"	uvec4 p = uvec4(packedInput & 0x3FF, (packedInput >> 10) & 0x3FF, (packedInput >> 20) & 0x3FF, (packedInput >> 30) & 0x3); \n"
		"	return clamp(vec4(p) / vec4(vec3(1023), 3), vec4(0.0), vec4(1.0)); \n"
		"}\n"

		"vec4 unpackUnorm4x4(uint packedInput)\n"
		"{\n"
		"	uvec4 p = uvec4(packedInput & 0x1F, (packedInput >> 4) & 0x1F, (packedInput >> 8) & 0x1F, (packedInput >> 12) & 0x1F); \n"
		"	return clamp(vec4(p) / 31.0, vec4(0.0), vec4(1.0)); \n"
		"}\n"
		;

	char limits[512]; // make sure this is big enough when adding new defines
	char* buff = limits;
	buff += sprintf(buff, "#define AMBIENT_LOBES %d\n", RD_AMBIENT_LOBES);
	buff += sprintf(buff, "#define CLUSTER_MAX_LIGHTS %d\n", STD3D_CLUSTER_MAX_LIGHTS);
	buff += sprintf(buff, "#define CLUSTER_MAX_OCCLUDERS %d\n", STD3D_CLUSTER_MAX_OCCLUDERS);
	buff += sprintf(buff, "#define CLUSTER_MAX_DECALS %d\n", STD3D_CLUSTER_MAX_DECALS);
	buff += sprintf(buff, "#define CLUSTER_MAX_ITEMS %d\n", STD3D_CLUSTER_MAX_ITEMS);
	buff += sprintf(buff, "#define CLUSTER_BUCKETS_PER_CLUSTER %d\n", STD3D_CLUSTER_BUCKETS_PER_CLUSTER);
	buff += sprintf(buff, "#define CLUSTER_GRID_SIZE_X %d\n", STD3D_CLUSTER_GRID_SIZE_X);
	buff += sprintf(buff, "#define CLUSTER_GRID_SIZE_Y %d\n", STD3D_CLUSTER_GRID_SIZE_Y);
	buff += sprintf(buff, "#define CLUSTER_GRID_SIZE_Z %d\n", STD3D_CLUSTER_GRID_SIZE_Z);
	buff += sprintf(buff, "#define CLUSTER_GRID_SIZE_Z %d\n", STD3D_CLUSTER_GRID_SIZE_Z);
	buff += sprintf(buff, "#define CLUSTER_GRID_SIZE_XYZ %d\n", STD3D_CLUSTER_GRID_SIZE_XYZ);
	buff += sprintf(buff, "#define CLUSTER_GRID_TOTAL_SIZE %d\n", STD3D_CLUSTER_GRID_TOTAL_SIZE);
	buff = "\0"; // just in case

	uint32_t source_count = 0;
	GLchar** sources = malloc(sizeof(GLchar*) * (std3D_numLoadedImports + 10));
	if (!sources)
	{
		glDeleteShader(res);
		return 0;
	}
	sources[source_count++] = version;
	sources[source_count++] = type_name;
	sources[source_count++] = extensions;
	sources[source_count++] = defines;
	sources[source_count++] = featureDefines;
	sources[source_count++] = limits;
	sources[source_count++] = userDefs;
	sources[source_count++] = precision;
	sources[source_count++] = intrinsics;

	for (int i = 0; i < std3D_numLoadedImports; ++i)
		sources[source_count++] = std3D_loadedImports[i].source;
	sources[source_count++] = source;

	glShaderSource(res, source_count, sources, NULL);
	
	glCompileShader(res);
	GLint compile_ok = GL_FALSE;
	glGetShaderiv(res, GL_COMPILE_STATUS, &compile_ok);
	if (compile_ok == GL_FALSE) {
		print_log(res);
		glDeleteShader(res);
		return 0;
	}
	
	return res;
}
#endif // LINUX
