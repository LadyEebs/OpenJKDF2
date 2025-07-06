#ifndef _DSS_SITHDSSTHING_H
#define _DSS_SITHDSSTHING_H

#include "types.h"
#include "globals.h"

#define sithDSSThing_SendPos_ADDR (0x004F3120)
#define sithDSSThing_ProcessPos_ADDR (0x004F3270)
#define sithDSSThing_SendSyncThing_ADDR (0x004F3420)
#define sithDSSThing_ProcessSyncThing_ADDR (0x004F35E0)
#define sithDSSThing_SendPlaySound_ADDR (0x004F37B0)
#define sithDSSThing_ProcessPlaySound_ADDR (0x004F3870)
#define sithDSSThing_SendPlaySoundMode_ADDR (0x004F3960)
#define sithDSSThing_ProcessPlaySoundMode_ADDR (0x004F39C0)
#define sithDSSThing_SendPlayKey_ADDR (0x004F3A30)
#define sithDSSThing_ProcessPlayKey_ADDR (0x004F3AA0)
#define sithDSSThing_SendPlayKeyMode_ADDR (0x004F3B30)
#define sithDSSThing_ProcessPlayKeyMode_ADDR (0x004F3B90)
#define sithDSSThing_SendSetThingModel_ADDR (0x004F3C00)
#define sithDSSThing_ProcessSetThingModel_ADDR (0x004F3C80)
#define sithDSSThing_SendStopKey_ADDR (0x004F3CF0)
#define sithDSSThing_ProcessStopKey_ADDR (0x004F3D50)
#define sithDSSThing_SendStopSound_ADDR (0x004F3DC0)
#define sithDSSThing_ProcessStopSound_ADDR (0x004F3E10)
#define sithDSSThing_SendFireProjectile_ADDR (0x004F3E70)
#define sithDSSThing_ProcessFireProjectile_ADDR (0x004F3F60)
#define sithDSSThing_SendDeath_ADDR (0x004F4040)
#define sithDSSThing_ProcessDeath_ADDR (0x004F40B0)
#define sithDSSThing_SendDamage_ADDR (0x004F4120)
#define sithDSSThing_ProcessDamage_ADDR (0x004F41A0)
#define sithDSSThing_SendFullDesc_ADDR (0x004F4210)
#define sithDSSThing_ProcessFullDesc_ADDR (0x004F46F0)
#define sithDSSThing_SendPathMove_ADDR (0x004F4C60)
#define sithDSSThing_ProcessPathMove_ADDR (0x004F4D60)
#define sithDSSThing_SendSyncThingAttachment_ADDR (0x004F4E80)
#define sithDSSThing_ProcessSyncThingAttachment_ADDR (0x004F4F50)
#define sithDSSThing_SendTakeItem_ADDR (0x004F5040)
#define sithDSSThing_ProcessTakeItem_ADDR (0x004F5150)
#define sithDSSThing_SendCreateThing_ADDR (0x004F5220)
#define sithDSSThing_ProcessCreateThing_ADDR (0x004F52E0)
#define sithDSSThing_SendDestroyThing_ADDR (0x004F53D0)
#define sithDSSThing_ProcessDestroyThing_ADDR (0x004F5410)
#define sithSector_TransitionMovingThing_ADDR (0x004F5440)

void sithDSSThing_SendPos(sithThing *pThing, DPID sendto_id, int bSync);
int sithDSSThing_ProcessPos(sithCogMsg *msg);

void sithDSSThing_SendSyncThing(sithThing *pThing, DPID sendto_id, int mpFlags);
int sithDSSThing_ProcessSyncThing(sithCogMsg *msg);

void sithDSSThing_SendPlaySound(sithThing *followThing, rdVector3 *pos, sithSound *sound, flex32_t volume, flex32_t a5, int flags, int refid, DPID sendto_id, int mpFlags);
int sithDSSThing_ProcessPlaySound(sithCogMsg *msg);

void sithDSSThing_SendPlaySoundMode(sithThing *pThing, int16_t a2, int a3, flex32_t a4);
int sithDSSThing_ProcessPlaySoundMode(sithCogMsg *msg);

void sithDSSThing_SendPlayKey(sithThing *pThing, rdKeyframe *pRdKeyframe, int a3, int16_t a4, int a5, DPID a6, int a7);
int sithDSSThing_ProcessPlayKey(sithCogMsg *msg);

void sithDSSThing_SendPlayKeyMode(sithThing *pThing, int16_t idx1, int idx2, DPID sendtoId, int mpFlags);
int sithDSSThing_ProcessPlayKeyMode(sithCogMsg *msg);

void sithDSSThing_SendSetThingModel(sithThing *pThing, DPID sendtoId);
int sithDSSThing_ProcessSetThingModel(sithCogMsg *msg);

void sithDSSThing_SendStopKey(sithThing *pThing, int a2, flex32_t a3, DPID sendtoId, int mpFlags);
int sithDSSThing_ProcessStopKey(sithCogMsg *msg);

void sithDSSThing_SendStopSound(sithPlayingSound *pSound, flex32_t a2, DPID a3, int a4);
int sithDSSThing_ProcessStopSound(sithCogMsg *msg);

void sithDSSThing_SendFireProjectile(sithThing *pWeapon, sithThing *pProjectile, rdVector3 *pFireOffset, rdVector3 *pAimError, sithSound *pFireSound, int16_t anim, flex32_t scale, int16_t scaleFlags, flex32_t a9, int thingId, DPID sendtoId, int mpFlags, int idk);

int sithDSSThing_ProcessFireProjectile(sithCogMsg *msg);
int sithDSSThing_ProcessMOTSNew2(sithCogMsg *msg);

void sithDSSThing_SendDeath(sithThing *sender, sithThing *receiver, char cause, DPID sendto_id, int mpFlags);
int sithDSSThing_ProcessDeath(sithCogMsg *msg);

void sithDSSThing_SendDamage(sithThing *pDamagedThing, sithThing *pDamagedBy, flex32_t amt, int16_t a4, DPID sendtoId, int mpFlags);
int sithDSSThing_ProcessDamage(sithCogMsg *msg);

void sithDSSThing_SendFullDesc(sithThing *thing, DPID sendto_id, int mpFlags);
int sithDSSThing_ProcessFullDesc(sithCogMsg *msg);

void sithDSSThing_SendPathMove(sithThing *pThing, int16_t a2, flex32_t a3, int a4, DPID sendtoId, int mpFlags);
int sithDSSThing_ProcessPathMove(sithCogMsg *msg);

void sithDSSThing_SendSyncThingAttachment(sithThing *thing, DPID sendto_id, int mpFlags, int a4);
int sithDSSThing_ProcessSyncThingAttachment(sithCogMsg *msg);

void sithDSSThing_SendTakeItem(sithThing *pItemThing, sithThing *pActor, int mpFlags);
int sithDSSThing_ProcessTakeItem(sithCogMsg *msg);

void sithDSSThing_SendCreateThing(sithThing *pTemplate, sithThing *pThing, sithThing *pThing2, sithSector *pSector, rdVector3 *pPos, rdVector3 *pRot, int mpFlags, int bSync);
int sithDSSThing_ProcessCreateThing(sithCogMsg *msg);

void sithDSSThing_SendDestroyThing(int idx, DPID sendtoId);
int sithDSSThing_ProcessDestroyThing(sithCogMsg *msg);

void sithDSSThing_TransitionMovingThing(sithThing *pThing, rdVector3 *pPos, sithSector *pSector);

int sithDSSThing_ProcessMOTSNew1(sithCogMsg *msg);
void sithDSSThing_SendMOTSNew1(sithThing* pThing1, sithThing* pThing2, sithThing* pThing3, sithSector* pSector, 
    rdVector3* pVec1, rdVector3* pVec2, int mpFlags, int param_8);

//static void (*sithDSSThing_SendPlayKey)(sithThing *a1, rdKeyframe *a2, int a3, wchar_t a4, int a5, int a6, int a7) = (void*)sithDSSThing_SendPlayKey_ADDR;
//static void (*sithDSSThing_SendStopKey)(sithThing *a1, int a2, flex32_t a3, int a4, int a5) = (void*)sithDSSThing_SendStopKey_ADDR;
//static void (*sithDSSThing_SendSetThingModel)(sithThing *a1, int a2) = (void*)sithDSSThing_SendSetThingModel_ADDR;
//static int (*sithDSSThing_SendStopSound)(sithPlayingSound *a1, flex32_t a2, int a3, int a4) = (void*)sithDSSThing_SendStopSound_ADDR;
//static int (*sithDSSThing_PlaySoundMode)(sithThing *a1, int16_t a2, int a3, flex32_t a4) = (void*)sithDSSThing_PlaySoundMode_ADDR;
//static int (*sithDSSThing_SendFireProjectile)(sithThing *weapon, sithThing *projectile, rdVector3 *fireOffset, rdVector3 *aimError, sithSound *fireSound, __int16 anim, flex32_t scale, __int16 scaleFlags, flex32_t a9, int thingId, int a11, int a12) = (void*)sithDSSThing_SendFireProjectile_ADDR;
//static void (*sithDSSThing_SendPathMove)(sithThing *a1, __int16 a2, flex32_t a3, int a4, int a5, int a6) = (void*)sithDSSThing_SendPathMove_ADDR;
//static void (*sithDSSThing_SendPlayKeyMode)(sithThing *a1, __int16 a2, int a3, int a4, int a5) = (void*)sithDSSThing_SendPlayKeyMode_ADDR;
//static void (*sithDSSThing_SendDestroyThing)(int a1, int a2) = (void*)sithDSSThing_SendDestroyThing_ADDR;
//static int (*sithDSSThing_SendCreateThing)(sithThing *a1, sithThing *a2, sithThing *a3, sithSector *a4, int *a5, int *a6, int a7, int a8) = (void*)sithDSSThing_SendCreateThing_ADDR;
//static void (*sithDSSThing_SendDamage)(sithThing *a1, sithThing *a2, flex32_t a3, __int16 a4, int a5, int a6) = (void*)sithDSSThing_SendDamage_ADDR;
//static void (*sithDSSThing_SendSyncThing)(sithThing *a1, int a2, int a3) = (void*)sithDSSThing_SendSyncThing_ADDR;
//static void (*sithDSSThing_SendTakeItem)(sithThing *a1, sithThing *a2, int a3) = (void*)sithDSSThing_SendTakeItem_ADDR;


#endif // _DSS_SITHDSSTHING_H