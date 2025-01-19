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

	const GLchar* sources[] = {
		version,
		extensions,
		defines,
		featureDefines,
		userDefs,
		precision,
		source
	};
	glShaderSource(res, 7, sources, NULL);
	
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
