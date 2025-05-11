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

	uint64_t stdPlatform_GetAppID()
	{
		if (!SteamAPI_IsSteamRunning())
			return 0;
		return SteamUtils()->GetAppID();
	}

	uint64_t stdPlatform_GetJKAppID()
	{
		return 32380;
	}

	uint64_t stdPlatform_GetMotSAppID()
	{
		return 32390;
	}
}
