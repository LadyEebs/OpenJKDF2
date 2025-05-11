#include "Modules/std/stdVoice.h"

#include "stdPlatform.h"
#include "jk.h"

#include <steam_api.h>
#include "stdPlatform.h"

extern "C" {

	extern uint32_t timeAtStart;

	int stdPlatform_Startup()
	{
		timeAtStart = stdPlatform_GetTimeMsec();
		SteamAPI_Init();
		return 1;
	}

	void stdPlatform_Shutdown()
	{
		SteamAPI_Shutdown();
	}

	void stdPlatform_ShutdownBackendServices()
	{
		SteamAPI_Shutdown();
	}
}
