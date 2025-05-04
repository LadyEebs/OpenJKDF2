#ifndef _JKGUIMULTIFRIENDS_H
#define _JKGUIMULTIFRIENDS_H

#ifdef PLATFORM_STEAM

#include "types.h"

int jkGuiMultiFriends_Show(int a1);

void jkGuiMultiFriends_Startup();
void jkGuiMultiFriends_Shutdown();
void jkGuiMultiFriends_Refresh(jkGuiMenu* pMenu);

#endif

#endif // _JKGUIMULTIFRIENDS_H
