#include "sithConstraint.h"
#include "Engine/sithCollision.h"
#include "Engine/sithPhysics.h"

#include "General/stdMath.h"

void sithConstraint_AddDistanceConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pAnchor)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;

	constraint->type = SITH_CONSTRAINT_DISTANCE;
	constraint->constrainedThing = pConstrainedThing;
	constraint->targetThing = pTargetThing;
	constraint->distanceParams.constraintAnchor = *pAnchor;
	constraint->distanceParams.constraintDistance = 0.0f;//rdVector_Len3(&constraint->distanceParams.constraintAnchor);
	constraint->distanceParams.prevLambda = 0.0f;

	constraint->next = pThing->constraints;
	pThing->constraints = constraint;
}

void sithConstraint_AddConeConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pAxis, float angle)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;

	constraint->type = SITH_CONSTRAINT_CONE;
	constraint->constrainedThing = pConstrainedThing;
	constraint->targetThing = pTargetThing;

	rdVector_Zero3(&constraint->coneParams.prevImpulse);
	constraint->coneParams.coneAxis = *pAxis;
	constraint->coneParams.coneAngle = angle;

	constraint->next = pThing->constraints;
	pThing->constraints = constraint;
}


void sithConstraint_AddAngleConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pMinAngles, const rdVector3* pMaxAngles)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;

	constraint->type = SITH_CONSTRAINT_ANGLES;
	constraint->constrainedThing = pConstrainedThing;
	constraint->targetThing = pTargetThing;

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
	constraint->constrainedThing = pThingA;
	constraint->targetThing = pThingB;
	rdMatrix_Copy34(&constraint->lookParams.referenceMat, pRefMat);
	constraint->lookParams.flipUp = flipUp;

	constraint->next = pThing->constraints;
	pThing->constraints = constraint;
}

void sithConstraint_SolveDistanceConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
	//rdVector_Copy3(&pConstraint->targetThing->lookOrientation.scale, &pConstraint->targetThing->position);
	rdVector_Zero3(&pConstraint->targetThing->lookOrientation.scale);

	rdVector3 anchor;
	rdMatrix_TransformPoint34(&anchor, &pConstraint->distanceParams.constraintAnchor, &pConstraint->targetThing->lookOrientation);

	//rdVector_Zero3(&pConstraint->targetThing->lookOrientation.scale);
	rdVector_Add3Acc(&anchor, &pConstraint->targetThing->position);

	rdVector3 relativePos;
	rdVector_Sub3(&relativePos, &anchor, &pConstraint->constrainedThing->position);

	float currentDistance = rdVector_Len3(&relativePos);
	float offset = pConstraint->distanceParams.constraintDistance - currentDistance;
	offset = stdMath_ClipPrecision(offset);
	if (stdMath_Fabs(offset) <= 0.0001f)
		return;

	rdVector3 offsetDir;
	rdVector_Normalize3(&offsetDir, &relativePos);

	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
		return;

	rdVector3 relativeVelocity;
	rdVector_Sub3(&relativeVelocity, &pConstraint->targetThing->physicsParams.vel, &pConstraint->constrainedThing->physicsParams.vel);

	// how much of their relative force is affecting the constraint
	float velocityDot = rdVector_Dot3(&relativeVelocity, &offsetDir);
	velocityDot = stdMath_ClipPrecision(velocityDot);

	const float biasFactor = 0.01f;
	float bias = -(biasFactor / deltaSeconds) * offset;
	bias = stdMath_ClipPrecision(bias);

	float lambda = pConstraint->distanceParams.prevLambda;
	lambda = -(velocityDot + bias) / constraintMass;
	lambda = stdMath_ClipPrecision(lambda);

	const float dampingFactor = 0.2f;

	rdVector3 aImpulse;
	rdVector_Scale3(&aImpulse, &offsetDir, lambda);

	rdVector3 bImpulse;
	rdVector_Scale3(&bImpulse, &offsetDir, -lambda);

	//rdVector3 dampingForce;
	//rdVector_Scale3(&dampingForce, &aImpulse, dampingFactor);

	//rdVector_Sub3Acc(&aImpulse, &dampingForce);
	//rdVector_Sub3Acc(&bImpulse, &dampingForce);

	//rdVector_MultAcc3(&pConstraint->targetThing->physicsParams.vel, &aImpulse, invMassB);
	//rdVector_MultAcc3(&pConstraint->constrainedThing->physicsParams.vel, &bImpulse, invMassA);

	sithPhysics_ThingApplyForce(pConstraint->targetThing, &aImpulse);
	sithPhysics_ThingApplyForce(pConstraint->constrainedThing, &bImpulse);

	pConstraint->distanceParams.prevLambda = lambda;
}

void sithPhysics_ThingApplyAngularImpulse(sithThing* pThing, const rdVector3* angularImpulse)
{
	// Calculate the change in angular velocity
	float inertia = (2.0f / 5.0f) * pThing->physicsParams.mass * pThing->collideSize * pThing->collideSize;


	float angle = sqrtf(angularImpulse->x * angularImpulse->x + angularImpulse->y * angularImpulse->y + angularImpulse->z * angularImpulse->z);
	rdVector3 axis = { angularImpulse->x / angle, angularImpulse->y / angle, angularImpulse->z / angle };

	//delta time* (torque / moment of inertia)

	rdMatrix34 impulseMat;
	rdMatrix_BuildFromAxisAngle34(&impulseMat, &axis, angle * 180.0 / M_PI);

	rdMatrix34 invOrient;
	rdMatrix_InvertOrtho34(&invOrient, &pThing->lookOrientation);
	rdVector_Zero3(&invOrient.scale);

	rdMatrix_PostMultiply34(&impulseMat, &invOrient);

	rdVector3 angles;
	rdMatrix_ExtractAngles34(&impulseMat, &angles);

	//rdVector_Add3Acc(&pThing->physicsParams.angVel, &angles);
	rdVector_MultAcc3(&pThing->physicsParams.angVel, &angles, sithTime_deltaSeconds / inertia);
}

void sithConstraint_SolveConeConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
		return;
	
	rdVector3 coneAxis;
	rdMatrix_TransformVector34(&coneAxis, &pConstraint->coneParams.coneAxis, &pConstraint->targetThing->lookOrientation);
	rdVector_Normalize3Acc(&coneAxis);

	// Calculate the relative orientation
	rdVector3 currentDir;
	rdMatrix_TransformVector34(&currentDir, &coneAxis, &pConstraint->constrainedThing->lookOrientation);
	rdVector_Normalize3Acc(&currentDir);
	
	// Calculate the angle between the current direction and cone axis
	float dotProduct = rdVector_Dot3(&currentDir, &coneAxis);
	float currentAngle = 90.0f - stdMath_ArcSin3(dotProduct);
	
	// Check if the current orientation is outside the cone angle
	if (currentAngle > pConstraint->coneParams.coneAngle)
	{
		// Calculate the corrective impulse
		float angleError = currentAngle - pConstraint->coneParams.coneAngle;
		float lambda = -angleError / constraintMass;
		
		// Apply damping to the impulse to prevent high velocity
		const float dampingFactor = 0.2f;
		// Adjust as needed
		//lambda *= (1.0f - dampingFactor);
		
		// Calculate the corrective axis (cross product)
		rdVector3 correctiveAxis;
		rdVector_Cross3(&correctiveAxis, &coneAxis, &currentDir);
		rdVector_Normalize3Acc(&correctiveAxis);
		
		// Compute the impulse vector
		rdVector3 impulse;
		impulse.x = correctiveAxis.x * lambda;
		impulse.y = correctiveAxis.y * lambda;
		impulse.z = correctiveAxis.z * lambda;

		rdVector3 currentImpulse;
		rdVector_Add3(&currentImpulse, &pConstraint->coneParams.prevImpulse, &impulse);
		//currentImpulse = impulse;
		
		// Apply the current angular impulse to the object being constrained (pThing)
		sithPhysics_ThingApplyAngularImpulse(pConstraint->constrainedThing, &currentImpulse);
		
		// Optionally, apply the equal and opposite angular impulse to the base target (pTargetThing)
		rdVector3 oppositeAngularImpulse;
		rdVector_Scale3(&oppositeAngularImpulse, &currentImpulse, -1.0f);
	//	sithPhysics_ThingApplyAngularImpulse(pConstraint->targetThing, &oppositeAngularImpulse);

		// Combine the previous impulse with the current impulse
		//rdVector3 currentImpulse;
		//rdVector_Add3(&currentImpulse, &prevImpulse, &impulse);
		
		// Apply the current impulse to both objects
		//sithPhysics_ThingApplyForce(pTargetThing, &currentImpulse);
		//sithPhysics_ThingApplyForce(pThing, &currentImpulse);
		
		// Store the current impulse for the next frame
		pConstraint->coneParams.prevImpulse = currentImpulse;
	}
}

void sithConstraint_SolveAngleConstrain(sithConstraint* pConstraint, float deltaSeconds)
{
	rdMatrix34 parentRotTranspose, relativeRotation;
	rdMatrix_InvertOrtho34(&parentRotTranspose, &pConstraint->targetThing->lookOrientation);
	rdMatrix_Multiply34(&relativeRotation, &parentRotTranspose, &pConstraint->constrainedThing->lookOrientation);

	rdVector3 angles;
	rdMatrix_ExtractAngles34(&relativeRotation, &angles);

	rdVector3 constrainedAngles;
	constrainedAngles.x = stdMath_Clamp(angles.x, pConstraint->angleParams.minAngles.x, pConstraint->angleParams.maxAngles.x);
	constrainedAngles.y = stdMath_Clamp(angles.y, pConstraint->angleParams.minAngles.y, pConstraint->angleParams.maxAngles.y);
	constrainedAngles.z = stdMath_Clamp(angles.z, pConstraint->angleParams.minAngles.z, pConstraint->angleParams.maxAngles.z);
	rdVector_ClipPrecision3(&constrainedAngles);

	rdVector3 angleDifferences;
	rdVector_Sub3(&angleDifferences, &constrainedAngles, &angles);
	rdVector_MultAcc3(&pConstraint->constrainedThing->physicsParams.angVel, &angleDifferences, 1.0f / deltaSeconds);
	//rdVector_Add3Acc(&pConstraint->thingA->physicsParams.angVel, &angleDifferences);

//	rdMatrix34 constrainedRotation;
//	rdMatrix_BuildRotate34(&constrainedRotation, &constrainedAngles);
//	rdMatrix_Multiply34(&pConstraint->thingA->lookOrientation, &pConstraint->thingB->lookOrientation, &constrainedRotation);
}

void sithConstraint_SolveLookConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
	rdVector3 newUp;
	rdVector_Sub3(&newUp, &pConstraint->targetThing->position, &pConstraint->constrainedThing->position);
	rdVector_Normalize3Acc(&newUp);
	if(pConstraint->lookParams.flipUp)
		rdVector_Neg3Acc(&newUp);

	sithPhysics_ThingSetLook(pConstraint->constrainedThing, &newUp, 0.0f);

	return;







	// Calculate the target orientation
	rdMatrix34 targetOrientation = pConstraint->targetThing->lookOrientation;
	
	// Invert the current hip orientation
	rdMatrix34 invHipOrientation;
	rdMatrix_InvertOrtho34(&invHipOrientation, &pConstraint->constrainedThing->lookOrientation);
	
	// Calculate the relative orientation (target * invHip)
	rdMatrix34 relativeOrientation;
	rdMatrix_Multiply34(&relativeOrientation, &targetOrientation, &invHipOrientation);
	
	// Extract Euler angles from the relative orientation matrix
	rdVector3 relativeAngles;
	rdMatrix_ExtractAngles34(&relativeOrientation, &relativeAngles);
	
	// Calculate the necessary angular velocity
	rdVector3 angularVelocityCorrection;
	angularVelocityCorrection.x = relativeAngles.x;// / deltaSeconds;
	angularVelocityCorrection.y = relativeAngles.y;// / deltaSeconds;
	angularVelocityCorrection.z = relativeAngles.z;// / deltaSeconds;
	
	// Apply the angular velocity correction
//	rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &angularVelocityCorrection);

//	rdMatrix34* pMat = &pConstraint->thingA->lookOrientation;
//	rdVector_Sub3(&pMat->uvec, &pConstraint->thingB->position, &pConstraint->thingA->position);
//	rdVector_Normalize3Acc(&pMat->uvec);
//
//	if (pConstraint->lookParams.flipUp)
//		rdVector_Neg3Acc(&pMat->uvec);
//
//	rdVector_Cross3(&pMat->rvec, &pConstraint->lookParams.referenceMat.lvec, &pMat->uvec);
//	rdVector_Normalize3Acc(&pMat->rvec);
//
//	rdVector_Cross3(&pMat->lvec, &pMat->uvec, &pMat->rvec);
//	rdVector_Normalize3Acc(&pMat->lvec);
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
					sithConstraint_SolveDistanceConstraint(constraint, deltaSeconds);
					break;
				case SITH_CONSTRAINT_CONE:
					sithConstraint_SolveConeConstraint(constraint, deltaSeconds);
					break;
				case SITH_CONSTRAINT_ANGLES:
					sithConstraint_SolveAngleConstrain(constraint, deltaSeconds);
					break;
				case SITH_CONSTRAINT_LOOK:
					sithConstraint_SolveLookConstraint(constraint, deltaSeconds);
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