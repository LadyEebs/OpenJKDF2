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

#define MAX_NAME_LENGTH 32
#define MAX_PATH_LENGTH 128
#define MAX_LINE_LENGTH 2048
#define MAX_SOURCE_LENGTH 16777216//65536 // todo: how big should this actually be?

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

int starts_with(const char* str, const char* prefix)
{
	return strnicmp(str, prefix, strlen(prefix)) == 0;
}

const char loadedImports[64][128];
int numLoadedImports = 0;

int already_imported(const char* name)
{
	for (int i = 0; i < numLoadedImports; ++i)
	{
		if(strnicmp(name, loadedImports[i], 128) == 0)
			return 1;
	}
	return 0;
}

char* load_source(const char* filepath)
{
	char* shader_contents = stdEmbeddedRes_Load(filepath, NULL);
	if(!shader_contents)
		return NULL;

	char* full_source_code = (char*)malloc(MAX_SOURCE_LENGTH);
	if (!full_source_code)
	{
		char errtmp[256];
		snprintf(errtmp, 256, "std3D: Failed to preprocess shader file `%s`\n", filepath);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", errtmp, NULL);
		free(shader_contents);
		return NULL;
	}
	full_source_code[0] = '\0'; // Start with an empty string

	char line_buffer[MAX_LINE_LENGTH];
	const char* current = shader_contents;

	// Read the file line by line
	while (*current)
	{
		// Extract the next line
		char* next_newline = strchr(current, '\n');
		size_t line_length = next_newline ? (size_t)(next_newline - current) : strlen(current);
		if (line_length >= MAX_LINE_LENGTH)
		{
			char errtmp[256];
			snprintf(errtmp, 256, "std3D: Line too long in file  `%s`\n", filepath);
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", errtmp, NULL);
			free(shader_contents);
			free(full_source_code);
			return NULL;
		}

		strncpy(line_buffer, current, line_length);
		line_buffer[line_length] = '\0';
		current += line_length + (next_newline ? 1 : 0);

		// Process import directives
		if (starts_with(line_buffer, "import "))
		{
			char included_file[MAX_NAME_LENGTH];
			strncpy(included_file, line_buffer + 8, MAX_NAME_LENGTH - 1); // remove "import_
			included_file[strlen(included_file) - 1] = '\0'; // Remove trailing "

			if(!already_imported(included_file))
			{
				strcpy_s(loadedImports[numLoadedImports++], 128, included_file);

				// For simplicity all includes are in the same folder
				char resolved_path[MAX_PATH_LENGTH];
				snprintf(resolved_path, MAX_PATH_LENGTH, "shaders/includes/%s", included_file);

				// Recursively load the included file
				char* included_source = load_source(resolved_path);
				if (included_source)
				{
					// todo: instead of concatenating, it might be better to append the contents
					// to a buffer of strings and let glShaderSource handle it
					strcat(full_source_code, included_source);
					free(included_source);
				}
			}
			continue; // Skip adding #include lines to the source
		}

		// Add the line to the full source code
		strcat(full_source_code, line_buffer);
		strcat(full_source_code, "\n");
	}
	return full_source_code;
}

GLuint load_shader_file(const char* filepath, GLenum type, const char* userDefines)
{
	numLoadedImports = 0;

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

	const char* featureDefines = "\n"
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
	const char* precision;
	precision =
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
		"#else\n"
		"#	define flexSampler2D	sampler2D\n"
		"#endif\n"
		;

	// custom intrinsics
	const char* intrinsics =
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

	const GLchar* sources[] = {
		version,
		type_name,
		extensions,
		defines,
		featureDefines,
		limits,
		userDefs,
		precision,
		intrinsics,
		source
	};
	glShaderSource(res, 10, sources, NULL);
	
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
