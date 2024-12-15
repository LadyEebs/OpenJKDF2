#include "sithConstraint.h"
#include "Engine/sithCollision.h"
#include "Engine/sithPhysics.h"

#include "General/stdMath.h"

void sithConstraint_AddDistanceConstraint(sithThing* pThing, sithThing* pThingA, sithThing* pThingB, const rdVector3* pAnchor)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;

	constraint->type = SITH_CONSTRAINT_DISTANCE;
	constraint->thingA = pThingA;
	constraint->thingB = pThingB;
	constraint->distanceParams.constraintAnchor = *pAnchor;
	constraint->distanceParams.constraintDistance = 0.0f;//rdVector_Len3(&constraint->distanceParams.constraintAnchor);

	constraint->next = pThing->constraints;
	pThing->constraints = constraint;
}


void sithConstraint_AddConeConstraint(sithThing* pThing, sithThing* pThingA, sithThing* pThingB, const rdVector3* pMinAngles, const rdVector3* pMaxAngles)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;

	constraint->type = SITH_CONSTRAINT_ANGLES;
	constraint->thingA = pThingA;
	constraint->thingB = pThingB;

	constraint->angleParams.minAngles = *pMinAngles;
	constraint->angleParams.maxAngles = *pMaxAngles;

	constraint->next = pThing->constraints;
	pThing->constraints = constraint;

}

void sithConstraint_AddLookConstraint(sithThing* pThing, sithThing* pThingA, sithThing* pThingB, const rdMatrix34* pRefMat, int flipUp)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;

	constraint->type = SITH_CONSTRAINT_LOOK;
	constraint->thingA = pThingA;
	constraint->thingB = pThingB;
	rdMatrix_Copy34(&constraint->lookParams.referenceMat, pRefMat);
	constraint->lookParams.flipUp = flipUp;

	constraint->next = pThing->constraints;
	pThing->constraints = constraint;
}

void sithConstraint_SolveDistanceConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
	rdVector_Copy3(&pConstraint->thingB->lookOrientation.scale, &pConstraint->thingB->position);

	rdVector3 anchor;
	rdMatrix_TransformPoint34(&anchor, &pConstraint->distanceParams.constraintAnchor, &pConstraint->thingB->lookOrientation);

	rdVector_Zero3(&pConstraint->thingB->lookOrientation.scale);

	rdVector3 relativePos;
	rdVector_Sub3(&relativePos, &anchor, &pConstraint->thingA->position);

	float currentDistance = rdVector_Len3(&relativePos);
	float offset = pConstraint->distanceParams.constraintDistance - currentDistance;
	if (stdMath_Fabs(offset) <= 0.0001f)
		return;

	rdVector3 offsetDir;
	rdVector_Normalize3(&offsetDir, &relativePos);

	float invMassA = 1.0f / pConstraint->thingB->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->thingA->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
		return;

	rdVector3 relativeVelocity;
	rdVector_Sub3(&relativeVelocity, &pConstraint->thingB->physicsParams.vel, &pConstraint->thingA->physicsParams.vel);

	// how much of their relative force is affecting the constraint
	float velocityDot = rdVector_Dot3(&relativeVelocity, &offsetDir);

	const float biasFactor = 0.03f;
	float bias = -(biasFactor / deltaSeconds) * offset;

	float lambda = -(velocityDot + bias) / constraintMass;

	const float dampingFactor = 0.2f;

	rdVector3 aImpulse;
	rdVector_Scale3(&aImpulse, &offsetDir, lambda);

	rdVector3 bImpulse;
	rdVector_Scale3(&bImpulse, &offsetDir, -lambda);

	//rdVector3 dampingForce;
	//rdVector_Scale3(&dampingForce, &aImpulse, dampingFactor);

	//rdVector_Sub3Acc(&aImpulse, &dampingForce);
	//rdVector_Sub3Acc(&bImpulse, &dampingForce);

	rdVector_MultAcc3(&pConstraint->thingB->physicsParams.vel, &aImpulse, invMassB);
	rdVector_MultAcc3(&pConstraint->thingA->physicsParams.vel, &bImpulse, invMassA);

	//sithPhysics_ThingApplyForce(pConstraint->thingB, &aImpulse);
	//sithPhysics_ThingApplyForce(pConstraint->thingA, &bImpulse);
}

void sithConstraint_SolveAngleConstrain(sithConstraint* pConstraint, float deltaSeconds)
{
	rdMatrix34 parentRotTranspose, relativeRotation;
	rdMatrix_InvertOrtho34(&parentRotTranspose, &pConstraint->thingB->lookOrientation);
	rdMatrix_Multiply34(&relativeRotation, &parentRotTranspose, &pConstraint->thingA->lookOrientation);

	rdVector3 angles;
	rdMatrix_ExtractAngles34(&relativeRotation, &angles);

	rdVector3 constrainedAngles;
	constrainedAngles.x = stdMath_Clamp(angles.x, pConstraint->angleParams.minAngles.x, pConstraint->angleParams.maxAngles.x);
	constrainedAngles.y = stdMath_Clamp(angles.y, pConstraint->angleParams.minAngles.y, pConstraint->angleParams.maxAngles.y);
	constrainedAngles.z = stdMath_Clamp(angles.z, pConstraint->angleParams.minAngles.z, pConstraint->angleParams.maxAngles.z);

	rdVector3 angleDifferences;
	rdVector_Sub3(&angleDifferences, &constrainedAngles, &angles);
	//rdVector_MultAcc3(&pConstraint->thingA->physicsParams.angVel, &angleDifferences, 1.0f / deltaSeconds);
	rdVector_Add3Acc(&pConstraint->thingA->physicsParams.angVel, &angleDifferences);

//	rdMatrix34 constrainedRotation;
//	rdMatrix_BuildRotate34(&constrainedRotation, &constrainedAngles);
//	rdMatrix_Multiply34(&pConstraint->thingA->lookOrientation, &pConstraint->thingB->lookOrientation, &constrainedRotation);
}

void sithConstraint_SolveLookConstraint(sithConstraint* pConstraint)
{
	rdMatrix34* pMat = &pConstraint->thingA->lookOrientation;
	rdVector_Sub3(&pMat->uvec, &pConstraint->thingB->position, &pConstraint->thingA->position);
	rdVector_Normalize3Acc(&pMat->uvec);

	if (pConstraint->lookParams.flipUp)
		rdVector_Neg3Acc(&pMat->uvec);

	rdVector_Cross3(&pMat->rvec, &pConstraint->lookParams.referenceMat.lvec, &pMat->uvec);
	rdVector_Normalize3Acc(&pMat->rvec);

	rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);
	rdVector_Normalize3Acc(&pMat->lvec);
}

void sithConstraint_SolveConstraints(sithThing* pThing, float deltaSeconds)
{
	if (pThing->constraints)
	{
		rdVector3 oldPos = pThing->position;

		for (int k = 0; k < 10; ++k)
		{
			sithConstraint* constraint = pThing->constraints;
			while (constraint)
			{
				switch (constraint->type)
				{
				case SITH_CONSTRAINT_DISTANCE:
					sithConstraint_SolveDistanceConstraint(constraint, sithTime_deltaSeconds);
					break;
				case SITH_CONSTRAINT_ANGLES:
					sithConstraint_SolveAngleConstrain(constraint, sithTime_deltaSeconds);
					break;
				case SITH_CONSTRAINT_LOOK:
					sithConstraint_SolveLookConstraint(constraint);
					break;
				default:
					break;
				}
				constraint = constraint->next;
			}
		}

		//uint64_t jointBits = pThing->animclass->physicsJointBits;
		//while (jointBits != 0)
		//{
		//	int jointIdx = stdMath_FindLSB64(jointBits);
		//	jointBits ^= 1ull << jointIdx;
		//
		//	sithPuppetJoint* pJoint = &pThing->puppet->physics->joints[jointIdx];
		//	
		//	rdVector_Zero3(&pJoint->thing.physicsParams.velocityMaybe);
		//	rdVector_Zero3(&pJoint->thing.physicsParams.addedVelocity);
		//
		//	// would it make sense to split this so we're not diving head first into collision code?
		//	sithPhysics_ThingTick(&pJoint->thing, deltaSeconds);
		//	sithThing_TickPhysics(&pJoint->thing, deltaSeconds);
		//
		//	rdVector_Zero3(&pJoint->thing.lookOrientation.scale);
		//}

	//rdVector3 delta;
	//rdVector_Sub3(&delta, &pThing->position, &oldPos);
	//float deltaLen = rdVector_Normalize3Acc(&delta);
	//if (deltaLen > 0.0)
	//	sithCollision_UpdateThingCollision(pThing, &delta, deltaLen, 0);
	}
}