#ifndef _SITHCAMERA_H
#define _SITHCAMERA_H

#include "types.h"
#include "globals.h"
#include "Engine/rdCamera.h"

#ifdef __cplusplus
extern "C" {
#endif

#define sithCamera_Startup_ADDR (0x004C4DE0)
#define sithCamera_Shutdown_ADDR (0x004C4EF0)
#define sithCamera_Open_ADDR (0x004C4F20)
#define sithCamera_Close_ADDR (0x004C5130)
#define sithCamera_SetsFocus_ADDR (0x004C5150)
#define sithCamera_New_ADDR (0x004C5260)
#define sithCamera_NewEntry_ADDR (0x004C52B0)
#define sithCamera_FreeEntry_ADDR (0x004C5370)
#define sithCamera_Free_ADDR (0x004C53A0)
#define sithCamera_SetCanvas_ADDR (0x004C53C0)
#define sithCamera_SetCurrentCamera_ADDR (0x004C5420)
#define sithCamera_CycleCamera_ADDR (0x004C54D0)
#define sithCamera_DoIdleAnimation_ADDR (0x004C5590)
#define sithCamera_IdkChecksDword4_ADDR (0x004C5640)
#define sithCamera_SetCameraFocus_ADDR (0x004C5670)
#define sithCamera_GetPrimaryFocus_ADDR (0x004C5690)
#define sithCamera_GetSecondaryFocus_ADDR (0x004C56A0)
#define sithCamera_FollowFocus_ADDR (0x004C56B0)
#define sithCamera_SetRdCameraAndRenderidk_ADDR (0x004C5FD0)
#define sithCamera_SetPovShake_ADDR (0x004C6000)
#define sithCamera_create_unk_struct_ADDR (0x004C6050)
#define sithCamera_SetState_ADDR (0x004C6160)
#define sithCamera_GetState_ADDR (0x004C6170)

int sithCamera_Startup();
void sithCamera_Shutdown();
int sithCamera_Open(rdCanvas *canvas, flex_t aspect);
void sithCamera_Close();
void sithCamera_SetsFocus();
int sithCamera_NewEntry(sithCamera *camera, uint32_t a2, uint32_t a3, flex_t fov, flex_t aspectRatio, rdCanvas *canvas, sithThing *focus_far, sithThing *focus_near);

MATH_FUNC void sithCamera_FollowFocus(sithCamera *cam);
void sithCamera_SetRdCameraAndRenderidk();
void sithCamera_DoIdleAnimation();
int sithCamera_SetCurrentCamera(sithCamera *camera);
void sithCamera_SetCameraFocus(sithCamera *camera, sithThing *primary, sithThing *secondary);
sithSector* sithCamera_create_unk_struct(sithThing* a3, sithSector* a2, rdVector3* a4, rdVector3* a6, flex_t a7, int flags);
void sithCamera_SetPovShake(rdVector3 *a1, rdVector3 *a2, flex_t a3, flex_t a4);
#ifdef DYNAMIC_POV
void sithCamera_SetPovWaggle(rdVector3* waggleVec, flex_t waggleSpeed, flex_t waggleSmooth);
#endif
sithThing* sithCamera_GetPrimaryFocus(sithCamera *pCamera);
sithThing* sithCamera_GetSecondaryFocus(sithCamera *pCamera);
int sithCamera_SetState(int a1);
int sithCamera_GetState();
void sithCamera_CycleCamera();
MATH_FUNC void sithCamera_SetZoom(sithCamera *pCamera, flex_t zoomScale, flex_t zoom_2); // MOTS added
MATH_FUNC void sithCamera_UpdateZoom(sithCamera *pCamera);

#ifndef __cplusplus
//static void (*sithCamera_Shutdown)() = (void*)sithCamera_Shutdown_ADDR;
static int (*sithCamera_NewEntry_)(sithCamera *camera, int a2, int a3, flex_t fov, flex_t a5, rdCanvas* a6, sithThing *focus_far, sithThing *focus_near) = (void*)sithCamera_NewEntry_ADDR;
//static void (*sithCamera_SetCameraFocus)(sithCamera *a1, sithThing *primary, sithThing *secondary) = (void*)sithCamera_SetCameraFocus_ADDR;
//static sithThing* (*sithCamera_GetPrimaryFocus)(sithCamera *cam) = (void*)sithCamera_GetPrimaryFocus_ADDR;
//static sithThing* (*sithCamera_GetSecondaryFocus)(sithCamera *cam) = (void*)sithCamera_GetSecondaryFocus_ADDR;
//static void (*sithCamera_CycleCamera)(void) = (void*)sithCamera_CycleCamera_ADDR;
//static void (*sithCamera_SetPovShake)(rdVector3 *a1, rdVector3 *a2, flex_t a3, flex_t a4) = (void*)sithCamera_SetPovShake_ADDR;
//static int (*sithCamera_SetCurrentCamera)(sithCamera *a1) = (void*)sithCamera_SetCurrentCamera_ADDR;
//static int (*sithCamera_GetState)(void) = (void*)sithCamera_GetState_ADDR;
//static void (*sithCamera_SetState)(int) = (void*)sithCamera_SetState_ADDR;
//static void (*sithCamera_Close)() = (void*)sithCamera_Close_ADDR;
//static void (*sithCamera_FollowFocus)(sithCamera *cam) = (void*)sithCamera_FollowFocus_ADDR;
//static void (*sithCamera_SetRdCameraAndRenderidk)() = (void*)sithCamera_SetRdCameraAndRenderidk_ADDR;
//static sithSector* (*sithCamera_create_unk_struct)(sithThing *a3, sithSector *a2, rdVector3 *a4, rdVector3 *a6, flex_t a7, int arg14) = (void*)sithCamera_create_unk_struct_ADDR;
//static void (*sithCamera_SetsFocus)() = (void*)sithCamera_SetsFocus_ADDR;
#endif

#ifdef __cplusplus
}
#endif

#endif // _SITHCAMERA_H
