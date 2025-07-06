#ifndef _SITH_CONSTRAINT_H
#define _SITH_CONSTRAINT_H

#include "types.h"
#include "globals.h"

void sithConstraint_TickConstraints(sithThing* pThing, float deltaSeconds);

void sithConstraint_AddBallSocketConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pTargetAnchor, const rdVector3* pConstrainedAnchor, flex_t distance);
void sithConstraint_AddConeConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pAxis, flex_t angle, const rdVector3* pJointAxis, flex_t twistAngle);
void sithConstraint_AddHingeConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pTargetAxis, const rdVector3* pJointAxis, flex_t minAngle, flex_t maxAngle);
void sithConstraint_AddTwistConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pTargetAxis, const rdVector3* pJointAxis, flex_t minAngle, flex_t maxAngle);
void sithConstraint_RemoveConstraint(sithConstraint* pConstraint);

void sithConstraint_Draw(sithConstraint* pConstraint);
void sithConstraint_DebugDrawConstraints(sithThing* pThing);

#endif
