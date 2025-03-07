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
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

const char loadedIncludes[64][128];
int numLoadedIncludes = 0;

int already_included(const char* name)
{
	for (int i = 0; i < numLoadedIncludes; ++i)
	{
		if(strncmp(name, loadedIncludes[i], 128) == 0)
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

		// Process #include directives
		if (starts_with(line_buffer, "#include "))
		{
			char included_file[MAX_NAME_LENGTH];
			strncpy(included_file, line_buffer + 10, MAX_NAME_LENGTH - 1); // remove "#include_
			included_file[strlen(included_file) - 1] = '\0'; // Remove trailing "

			if(!already_included(included_file))
			{
				strcpy_s(loadedIncludes[numLoadedIncludes++], 128, included_file);

				// For simplicity all includes are in the same folder
				char resolved_path[MAX_PATH_LENGTH];
				snprintf(resolved_path, MAX_PATH_LENGTH, "shaders/includes/%s", included_file);

				// Recursively load the included file
				// todo: we could cache this by preloading includes
				// and also check if it was already included for this file
				// since atm the includes get repeatedly included and they bloat the code size
				char* included_source = load_source(resolved_path);
				if (included_source)
				{
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
	numLoadedIncludes = 0;

    char* shader_contents = load_source(filepath);// stdEmbeddedRes_Load(filepath, NULL);

    if (!shader_contents)
    {
    	char errtmp[256];
        snprintf(errtmp, 256, "std3D: Failed to load shader file `%s`!\n", filepath);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", errtmp, NULL);
        return -1;
    }
    
    stdPlatform_Printf("std3D: Parse shader `%s`\n", filepath);
    
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
    version = "#version 330\n";
    extensions = "#extension GL_ARB_texture_gather : enable\n#extension GL_ARB_gpu_shader5 : enable\n#extension GL_ARB_texture_query_lod : enable\n";
    defines = "#define CAN_BILINEAR_FILTER\n#define HAS_MIPS\n";
#endif

#if defined(ARCH_WASM)
    version = "#version 300 es\n";
    extensions = "\n";
    defines = "#define CAN_BILINEAR_FILTER\n";
#endif

#if defined(TARGET_ANDROID)
    version = "#version 300 es\n";
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
		"#endif                              \n";

	// custom intrinsics
	const char* intrinsics =
		"#ifndef VERTEX_SHADER\n"
		"#ifdef GL_ARB_texture_query_lod\n"
		"	float texQueryLod(sampler2D tex, vec2 uv)\n"
		"	{\n"
		"		return textureQueryLOD(tex, uv).x;\n"
		"	}\n"
		"#else\n"
		"	float texQueryLod(sampler2D tex, vec2 uv)\n"
		"	{\n"
		"		vec2 dims = textureSize(tex, 0);\n"
		"		vec2  texture_coordinate = uv * dims;\n"
		"		vec2  dx_vtc = dFdx(texture_coordinate);\n"
		"		vec2  dy_vtc = dFdy(texture_coordinate);\n"
		"		float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));\n"
		"		float mml = 0.5 * log2(delta_max_sqr);\n"
		"		return max(0, mml);\n"
		"	}\n"
		"#endif\n"
		"#endif\n"
		"#if !defined(GL_ARB_shading_language_packing)\n"
		"    uint f32tof16(float val)\n"
		"    {\n"
		"        uint f32 = floatBitsToUint(val);\n"
		"        uint f16 = 0u;\n"
		"        uint sign = (f32 >> 16) & 0x8000u;\n"
		"        int exponent = int((f32 >> 23) & 0xFFu) - 127;\n"
		"        uint mantissa = f32 & 0x007FFFFFu;\n"
		"        if (exponent == 128)\n"
		"        {\n"
		"            // Infinity or NaN\n"
		"            // NaN bits that are masked out by 0x3FF get discarded.\n"
		"            // This can turn some NaNs to infinity, but this is allowed by the spec.\n"
		"            f16 = sign | (0x1Fu << 10);\n"
		"            f16 |= (mantissa & 0x3FFu);\n"
		"        }\n"
		"        else if (exponent > 15)\n"
		"        {\n"
		"            // Overflow - flush to Infinity\n"
		"            f16 = sign | (0x1Fu << 10);\n"
		"        }\n"
		"        else if (exponent > -15)\n"
		"        {\n"
		"            // Representable value\n"
		"            exponent += 15;\n"
		"            mantissa >>= 13;\n"
		"            f16 = sign | uint(exponent << 10) | mantissa;\n"
		"        }\n"
		"        else\n"
		"        {\n"
		"            f16 = sign;\n"
		"        }\n"
		"        return f16;\n"
		"    }\n"
		"	uint packHalf2x16(vec2 v)\n"
		"	{\n"
		"        uint x = f32tof16(v.x);\n"
		"        uint y = f32tof16(v.y);\n"
		"        return (y << 16) | x;\n"
		"	}\n"
		"   float f16tof32(uint val)\n"
		"   {\n"
		"        uint sign = (val & 0x8000u) << 16;\n"
		"        int exponent = int((val & 0x7C00u) >> 10);\n"
		"        uint mantissa = val & 0x03FFu;\n"
		"        float f32 = 0.0;\n"
		"        if(exponent == 0)\n"
		"        {\n"
		"            if (mantissa != 0u)\n"
		"            {\n"
		"                const float scale = 1.0 / (1 << 24);\n"
		"                f32 = scale * mantissa;\n"
		"            }\n"
		"        }\n"
		"        else if (exponent == 31)\n"
		"        {\n"
		"            return uintBitsToFloat(sign | 0x7F800000u | mantissa);\n"
		"        }\n"
		"        else\n"
		"        {\n"
		"            exponent -= 15;\n"
		"            float scale;\n"
		"            if(exponent < 0)\n"
		"            {\n"
		"                // The negative unary operator is buggy on OSX.\n"
		"                // Work around this by using abs instead.\n"
		"                scale = 1.0 / (1 << abs(exponent));\n"
		"            }\n"
		"            else\n"
		"            {\n"
		"                scale = 1 << exponent;\n"
		"            }\n"
		"            float decimal = 1.0 + float(mantissa) / float(1 << 10);\n"
		"            f32 = scale * decimal;\n"
		"        }\n"
		"\n"
		"        if (sign != 0u)\n"
		"        {\n"
		"            f32 = -f32;\n"
		"        }\n"
		"\n"
		"        return f32;\n"
		"   }\n"
		"	vec2 unpackHalf2x16(uint u)\n"
		"	{\n"
		"        uint y = (u >> 16);\n"
		"        uint x = u & 0xFFFFu;\n"
		"        return vec2(f16tof32(x), f16tof32(y));\n"
		"	}\n"
		"	uint bitfieldExtract(uint src, uint off, uint bits)\n"
		"	{\n"
		"		uint mask = (1u << bits) - 1;\n"
		"		return (src >> off) & mask;\n"
		"	}\n"
		"	uint4 unpackUint4x8(uint p)\n"
		"	{\n"
		"		return uint4(\n"
		"			bitfieldExtract(p, 0, 8),\n"
		"			bitfieldExtract(p, 8, 8),\n"
		"			bitfieldExtract(p, 16, 8),\n"
		"			bitfieldExtract(p, 24, 8)\n"
		"		);\n"
		"	}\n"
		"	int2 unpackInt2x16(uint p)\n"
		"	{\n"
		"		return int2(\n"
		"			bitfieldExtract(p, 0, 16),\n"
		"			bitfieldExtract(p, 16, 16)\n"
		"		);\n"
		"	}\n"
		"	uint packUnorm1x16(float s)\n"
		"	{\n"
		"		return uint(round(saturate(s) * 65535.0));\n"
		"	}\n"
		"	float unpackUnorm1x16(uint p)\n"
		"	{\n"
		"		return float(p) * (1.0/65535.0);\n"
		"	}\n"
		"	uint packSnorm1x8(float v)\n"
		"	{\n"
		"		return uint(round(clamp(v, -1.0, 1.0) * 127.0) + 127.0);\n"
		"	}\n"
		"	float unpackSnorm1x8(uint p)\n"
		"	{\n"
		"		return clamp((float(p) - 127.0) / 127.0, -1.0, 1.0);\n"
		"	}\n"
		"	uint packUnorm1x8(float v)\n"
		"	{\n"
		"		return uint(round(clamp(v, 0.0, 1.0) * 255.0));\n"
		"	}\n"
		"	float unpackUnorm1x8(uint p)\n"
		"	{\n"
		"		return clamp((float(p)) / 255.0, 0.0, 1.0);\n"
		"	}\n"
		"	uint packSnorm1x16(float v)\n"
		"	{\n"
		"		return uint(round(clamp(v, -1.0, 1.0) * 32767.0) + 32767.0);\n"
		"	}\n"
		"	float unpackSnorm1x16(uint p)\n"
		"	{\n"
		"		return clamp((float(p) - 32767.0) * (1.0 / 32767.0), -1.0, 1.0);\n"
		"	}\n"
		"	uint packUnorm2x16(float2 v)\n"
		"	{\n"
		"		return packUnorm1x16(v.x) | packUnorm1x16(v.y) << uint(16);\n"
		"	}\n"
		"	float2 unpackUnorm2x16(uint p) {\n"
		"		return float2(unpackUnorm1x16(p & uint(0xffff)),\n"
		"					  unpackUnorm1x16(p >> uint(16)));\n"
		"	}\n"	
		"	uint packSnorm2x16(float2 v) {\n"
		"		return packSnorm1x16(v.x) | packSnorm1x16(v.y) << uint(16);\n"
		"	}\n"
		"	float2 unpackSnorm2x16(uint p) {\n"
		"		return float2(unpackSnorm1x16(p & uint(0xffff)),\n"
		"					  unpackSnorm1x16(p >> uint(16)));\n"
		"	}\n"
		"#endif\n";

	const GLchar* sources[] = {
		version,
		type_name,
		extensions,
		defines,
		featureDefines,
		userDefs,
		precision,
		intrinsics,
		source
	};
	glShaderSource(res, 9, sources, NULL);
	
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
