#ifndef _STDPROFILER_H
#define _STDPROFILER_H

#include "types.h"
#include "globals.h"

extern int stdProfiler_enable;

void stdProfiler_Startup();
void stdProfiler_Shutdown();

void stdProfiler_Tick();

void* stdProfiler_BeginLabel(const char* name);
void stdProfiler_EndLabel(void* v);

void stdProfiler_Enumerate(void(*func)(const char*, int64_t, uint32_t));

#define STD_BEGIN_PROFILER_LABEL() void* __label = stdProfiler_BeginLabel(__FUNCTION__);
#define STD_END_PROFILER_LABEL() stdProfiler_EndLabel(__label);

#endif // _STDPROFILER_H
