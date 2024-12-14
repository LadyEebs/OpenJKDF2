#include "sithConstraint.h"
#include "Engine/sithCollision.h"

#include "General/stdMath.h"

void sithConstraint_SolveConstraints(sithThing* pThing, float deltaSeconds)
{
	if (pThing->constraints)
	{
		for (int k = 0; k < 5; ++k)
		{
			sithConstraint* constraint = pThing->constraints;
			while (constraint)
			{
				switch (constraint->type)
				{
				case SITH_CONSTRAINT_DISTANCE:
					sithCollision_ApplyDistanceConstraint(pThing, constraint, sithTime_deltaSeconds);
					break;
				case SITH_CONSTRAINT_CONE:
					sithCollision_ConeConstrain(pThing, constraint, sithTime_deltaSeconds);
					break;
				case SITH_CONSTRAINT_LOOK:
					sithCollision_ApplyLookConstraint(pThing, constraint);
					break;
				default:
					break;
				}
				constraint = constraint->next;
			}
		}
	}
}

void sithConstraint_AddDistanceConstraint(sithThing* pThing, sithThing* pThingA, sithThing* pThingB, const rdVector3* pAnchor)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;

	constraint->type = SITH_CONSTRAINT_DISTANCE;
	constraint->thingA = pThingA;
	constraint->thingB = pThingB;
	constraint->distanceParams.constraintAnchor = *pAnchor;
	constraint->distanceParams.constraintDistance = rdVector_Len3(&constraint->distanceParams.constraintAnchor);

	constraint->next = pThing->constraints;
	pThing->constraints = constraint;
}


void sithConstraint_AddConeConstraint(sithThing* pThing, sithThing* pThingA, sithThing* pThingB, float maxSwingAngle, float maxTwistAngle)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;

	constraint->type = SITH_CONSTRAINT_CONE;
	constraint->thingA = pThingA;
	constraint->thingB = pThingB;

	constraint->coneParams.maxSwingAngle = maxSwingAngle;
	constraint->coneParams.maxTwistAngle = maxTwistAngle;

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
	float offset = -currentDistance;// pConstraint->distanceParams.constraintDistance - currentDistance;
	if (stdMath_Fabs(offset) <= 0.0001f)
		return;

	rdVector3 offsetDir;
	rdVector_Normalize3(&offsetDir, &relativePos);

	rdVector3 relativeVelocity;
	rdVector_Sub3(&relativeVelocity, &pConstraint->thingB->physicsParams.vel, &pConstraint->thingA->physicsParams.vel);

	float invMassA = 1.0f / pConstraint->thingB->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->thingA->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
		return;

	float diff = -offset / (constraintMass);
	rdVector_Neg3Acc(&offsetDir);

	sithCollision_UpdateThingCollision(pConstraint->thingB, &offsetDir, diff * invMassA, 0);
	//rdVector_MultAcc3(&pTargetThing->position, &offsetDir, diff * invMassA);

	rdVector_Neg3Acc(&offsetDir);
	sithCollision_UpdateThingCollision(pConstraint->thingA, &offsetDir, diff * invMassB, 0);
	//rdVector_MultAcc3(&pThing->position, &offsetDir, diff * invMassB);

//	// how much of their relative force is affecting the constraint
//	float velocityDot = rdVector_Dot3(&relativeVelocity, &offsetDir);
//	
//	const float biasFactor = 0.01f;
//	float bias = -(biasFactor / deltaSeconds) * offset;
//	
//	float lambda = -(velocityDot + bias) / constraintMass;
//	rdVector3 aImpulse;
//	rdVector_Scale3(&aImpulse, &offsetDir, lambda);
//	
//	rdVector3 bImpulse;
//	rdVector_Scale3(&bImpulse, &offsetDir, -lambda);
//	
//	sithPhysics_ThingApplyForce(pTargetThing, &aImpulse);
//	sithPhysics_ThingApplyForce(pConstraint->constraintThing, &bImpulse);
}

void sithConstraint_SolveConeConstrain(sithConstraint* pConstraint, float deltaSeconds)
{
	rdMatrix34 parentRotTranspose, relativeRotation;
	rdMatrix_InvertOrtho34(&parentRotTranspose, &pConstraint->thingB->lookOrientation);
	rdMatrix_Multiply34(&relativeRotation, &parentRotTranspose, &pConstraint->thingA->lookOrientation);

	rdVector3 angles;
	rdMatrix_ExtractAngles34(&relativeRotation, &angles);

	rdVector3 constrainedAngles = angles;
	if (stdMath_Fabs(constrainedAngles.x) > pConstraint->coneParams.maxSwingAngle)
		constrainedAngles.x = pConstraint->coneParams.maxSwingAngle * (constrainedAngles.x > 0 ? 1 : -1);

	if (stdMath_Fabs(constrainedAngles.y) > pConstraint->coneParams.maxTwistAngle)
		constrainedAngles.y = pConstraint->coneParams.maxTwistAngle * (constrainedAngles.y > 0 ? 1 : -1);

	if (stdMath_Fabs(constrainedAngles.z) > pConstraint->coneParams.maxSwingAngle)
		constrainedAngles.z = pConstraint->coneParams.maxSwingAngle * (constrainedAngles.z > 0 ? 1 : -1);

	rdMatrix34 constrainedRotation;
	rdMatrix_BuildRotate34(&constrainedRotation, &constrainedAngles);
	rdMatrix_Multiply34(&pConstraint->thingA->lookOrientation, &pConstraint->thingB->lookOrientation, &constrainedRotation);
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
