#include "sithConstraint.h"
#include "Engine/sithCollision.h"
#include "Engine/sithPhysics.h"

#include "General/stdMath.h"
#include "Primitives/rdMath.h"

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


void sithConstraint_AddTwistConstraint(sithThing* pThing, sithThing* pConstrainedThing, sithThing* pTargetThing, const rdVector3* pAxis, float angle)
{
	sithConstraint* constraint = (sithConstraint*)malloc(sizeof(sithConstraint));
	if (!constraint)
		return;

	constraint->type = SITH_CONSTRAINT_TWIST;
	constraint->constrainedThing = pConstrainedThing;
	constraint->targetThing = pTargetThing;

	rdVector_Zero3(&constraint->coneParams.prevImpulse);
	constraint->twistParams.twistAxis = *pAxis;
	constraint->twistParams.maxTwistAngle = angle;

	constraint->next = pThing->constraints;
	pThing->constraints = constraint;
}



static void sithConstraint_SolveDistanceConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
		return;

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

	rdVector3 relativeVelocity;
	rdVector_Sub3(&relativeVelocity, &pConstraint->targetThing->physicsParams.vel, &pConstraint->constrainedThing->physicsParams.vel);

	// how much of their relative force is affecting the constraint
	float velocityDot = rdVector_Dot3(&relativeVelocity, &offsetDir);
	velocityDot = stdMath_ClipPrecision(velocityDot);

	const float biasFactor = 0.05f;
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

	rdVector_MultAcc3(&pConstraint->targetThing->physicsParams.vel, &aImpulse, invMassB);
	rdVector_MultAcc3(&pConstraint->constrainedThing->physicsParams.vel, &bImpulse, invMassA);

	//sithPhysics_ThingApplyForce(pConstraint->targetThing, &aImpulse);
	//sithPhysics_ThingApplyForce(pConstraint->constrainedThing, &bImpulse);

	pConstraint->distanceParams.prevLambda = lambda;
}

static void sithPhysics_ThingApplyAngularImpulse(sithThing* pThing, const rdVector3* angularImpulse)
{
	// Calculate the change in angular velocity
	float inertia = (2.0f / 5.0f) * pThing->physicsParams.mass * pThing->collideSize * pThing->collideSize;

	rdVector3 axis;
	float angle = rdVector_Normalize3(&axis, angularImpulse);

	//delta time* (torque / moment of inertia)

	rdMatrix34 impulseMat;
	rdMatrix_BuildFromAxisAngle34(&impulseMat, &axis, angle * 180.0 / M_PI);

	rdVector3 angles;
	rdMatrix_ExtractAngles34(&impulseMat, &angles);

	//rdVector_Add3Acc(&pThing->physicsParams.angVel, &angles);
	rdVector_MultAcc3(&pThing->physicsParams.angVel, &angles, sithTime_deltaSeconds / inertia);
}

static void sithConstraint_SolveConeConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
		return;

	rdVector3 coneAxis;
	rdMatrix_TransformVector34(&coneAxis, &pConstraint->coneParams.coneAxis, &pConstraint->targetThing->lookOrientation);

	rdVector3 relativeVelocity;
	rdVector_Sub3(&relativeVelocity, &pConstraint->targetThing->physicsParams.vel, &pConstraint->constrainedThing->physicsParams.vel);

	rdVector3 normRelativeVelocity;
	rdVector_Normalize3(&normRelativeVelocity, &relativeVelocity);
	
	float cosTheta = rdVector_Dot3(&normRelativeVelocity, &coneAxis);
	float theta = acosf(cosTheta) * 180.0f / M_PI;
	
	if (theta > pConstraint->coneParams.coneAngle)
	{
		float scaleFactor = cosf(pConstraint->coneParams.coneAngle / (180.0f / M_PI)) / cosTheta;
		rdVector_Scale3Acc(&relativeVelocity, scaleFactor);
		
		rdVector3 correction;
		rdVector_Sub3(&correction, &relativeVelocity, &relativeVelocity);
		
		rdVector_MultAcc3(&pConstraint->targetThing, &correction, -invMassA / constraintMass);

		rdVector_MultAcc3(&pConstraint->constrainedThing, &correction, invMassB / constraintMass);

		
		//subtractVectors(velocityA, &correction, velocityA);
		//addVectors(velocityB, &correction, velocityB);
	}


#if 0
	float velocityDot = rdVector_Dot3(&relativeVelocity, &coneAxis);

	const float biasFactor = 0.1f;
	float bias = 0.0f;// -(biasFactor / deltaSeconds) * (cosf(pConstraint->coneParams.coneAngle/(180.0*M_PI)) - velocityDot);

	float lambda = -(velocityDot + bias) / constraintMass;

	rdVector3 impulseIncrement;
	rdVector_Scale3(&impulseIncrement, &coneAxis, lambda);

	//rdVector3 currentImpulse;
	//rdVector_Add3(&currentImpulse, &pConstraint->coneParams.prevImpulse, &impulseIncrement);

	rdVector_MultAcc3(&pConstraint->targetThing->physicsParams.vel, &impulseIncrement, invMassB);
	rdVector_MultAcc3(&pConstraint->constrainedThing->physicsParams.vel, &impulseIncrement, -invMassA);

	//sithPhysics_ThingApplyForce(pConstraint->targetThing, &impulseIncrement);
	// 
	//sithPhysics_ThingApplyForce(pConstraint->constrainedThing, &impulseIncrement);

//	rdVector_Copy3(&pConstraint->coneParams.prevImpulse, &currentImpulse);
#endif

#if 0
//	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
//	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
//	float constraintMass = invMassA + invMassB;
//	if (constraintMass <= 0.0f)
//		return;
//	
//	rdVector3 coneAxis;
//	rdMatrix_TransformVector34(&coneAxis, &pConstraint->coneParams.coneAxis, &pConstraint->targetThing->lookOrientation);
//	rdVector_Normalize3Acc(&coneAxis);
//
//	// Calculate the relative orientation
//	rdVector3 currentDir;
//	rdMatrix_TransformVector34(&currentDir, &coneAxis, &pConstraint->constrainedThing->lookOrientation);
//	rdVector_Normalize3Acc(&currentDir);
//	
//	// Calculate the angle between the current direction and cone axis
//	float dotProduct = rdVector_Dot3(&currentDir, &coneAxis);
//	float currentAngle = 90.0f - stdMath_ArcSin3(dotProduct);
//	
//	// Check if the current orientation is outside the cone angle
//	if (currentAngle > pConstraint->coneParams.coneAngle)
//	{
//		// Calculate the corrective impulse
//		float angleError = currentAngle - pConstraint->coneParams.coneAngle;
//
//		const float biasFactor = 0.05f;
//		float bias = -(biasFactor / deltaSeconds) * angleError;
//
//		float lambda = -(angleError + bias) / constraintMass;
//		
//		// Apply damping to the impulse to prevent high velocity
//		const float dampingFactor = 0.2f;
//		// Adjust as needed
//		//lambda *= (1.0f - dampingFactor);
//		
//		// Calculate the corrective axis (cross product)
//		rdVector3 correctiveAxis;
//		rdVector_Cross3(&correctiveAxis, &coneAxis, &currentDir);
//		rdVector_Normalize3Acc(&correctiveAxis);
//		
//		// Compute the impulse vector
//		rdVector3 impulse;
//		impulse.x = correctiveAxis.x * lambda;
//		impulse.y = correctiveAxis.y * lambda;
//		impulse.z = correctiveAxis.z * lambda;
//
//		rdVector3 currentImpulse;
//		rdVector_Add3(&currentImpulse, &pConstraint->coneParams.prevImpulse, &impulse);
//		//currentImpulse = impulse;
//
//		rdMatrix34 invOrient;
//		rdMatrix_InvertOrtho34(&invOrient, &pConstraint->targetThing->lookOrientation);
//		rdVector_Zero3(&invOrient.scale);
//
//		rdMatrix_TransformVector34Acc(&currentImpulse, &invOrient);
//
//		
//		// Apply the current angular impulse to the object being constrained (pThing)
//		sithPhysics_ThingApplyAngularImpulse(pConstraint->constrainedThing, &currentImpulse);
//		
//		// Optionally, apply the equal and opposite angular impulse to the base target (pTargetThing)
//		rdVector3 oppositeAngularImpulse;
//		rdVector_Scale3(&oppositeAngularImpulse, &currentImpulse, -1.0f);
//		sithPhysics_ThingApplyAngularImpulse(pConstraint->targetThing, &oppositeAngularImpulse);
//
//		// Combine the previous impulse with the current impulse
//		//rdVector3 currentImpulse;
//		//rdVector_Add3(&currentImpulse, &prevImpulse, &impulse);
//		
//		// Apply the current impulse to both objects
//		//sithPhysics_ThingApplyForce(pTargetThing, &currentImpulse);
//		//sithPhysics_ThingApplyForce(pThing, &currentImpulse);
//		
//		// Store the current impulse for the next frame
//		pConstraint->coneParams.prevImpulse = currentImpulse;
//	}
#endif
}

static void sithConstraint_SolveAngleConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
		return;

#if 1

	rdVector_Zero3(&pConstraint->targetThing->lookOrientation.scale);
	rdVector_Zero3(&pConstraint->constrainedThing->lookOrientation.scale);

	rdMatrix34 invOrientTarget;
	rdMatrix_InvertOrtho34(&invOrientTarget, &pConstraint->targetThing->lookOrientation);

	rdMatrix34 relativeOrientation;
	rdMatrix_Multiply34(&relativeOrientation, &invOrientTarget, &pConstraint->constrainedThing->lookOrientation);

	rdVector3 relativeAngles;
	rdMatrix_ExtractAngles34(&relativeOrientation, &relativeAngles);
	//relativeAngles.y = -relativeAngles.y; // yaw is inverted for whatever reason
	rdVector_NormalizeAngleAcute3(&relativeAngles);

	rdVector3 projectedAngles;
	projectedAngles.x = fmin(fmax(relativeAngles.x, pConstraint->angleParams.minAngles.x), pConstraint->angleParams.minAngles.x);
	projectedAngles.y = fmin(fmax(relativeAngles.y, pConstraint->angleParams.minAngles.y), pConstraint->angleParams.minAngles.y);
	projectedAngles.z = fmin(fmax(relativeAngles.z, pConstraint->angleParams.minAngles.z), pConstraint->angleParams.minAngles.z);

	//rdVector3 angularCorrection;
	//angularCorrection.x = stdMath_NormalizeDeltaAngle(projectedAngles.x, relativeAngles.x);
	//angularCorrection.y = stdMath_NormalizeDeltaAngle(projectedAngles.y, relativeAngles.y);
	//angularCorrection.z = stdMath_NormalizeDeltaAngle(projectedAngles.z, relativeAngles.z);

	rdVector3 angleError;
	angleError.x = stdMath_NormalizeDeltaAngle(projectedAngles.x, relativeAngles.x);
	angleError.y = stdMath_NormalizeDeltaAngle(projectedAngles.y, relativeAngles.y);
	angleError.z = stdMath_NormalizeDeltaAngle(projectedAngles.z, relativeAngles.z);

	float bias = 0.03f;
	rdVector3 biasTerm;
	rdVector_Scale3(&biasTerm, &angleError, bias / deltaSeconds);

	rdVector3 angularCorrection;
	angularCorrection.x = (angleError.x + biasTerm.x) * constraintMass;
	angularCorrection.y = (angleError.y + biasTerm.y) * constraintMass;
	angularCorrection.z = (angleError.z + biasTerm.z) * constraintMass;

	rdVector_NormalizeAngleAcute3(&angularCorrection);

	rdVector3 correctionParent;
	rdVector_Scale3(&correctionParent, &angularCorrection, invMassA / constraintMass);
	rdVector_Add3Acc(&pConstraint->targetThing->physicsParams.angVel, &correctionParent);

	rdVector3 correctionChild;
	rdVector_Scale3(&correctionChild, &angularCorrection, invMassB / constraintMass);
	rdVector_Sub3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &correctionChild);

	// Calculate the relative angular velocity
	//rdVector3 relativeAngVel;
	//relativeAngVel.x = stdMath_NormalizeDeltaAngle(pConstraint->targetThing->physicsParams.angVel.x, pConstraint->constrainedThing->physicsParams.angVel.x);
	//relativeAngVel.y = stdMath_NormalizeDeltaAngle(pConstraint->targetThing->physicsParams.angVel.y, pConstraint->constrainedThing->physicsParams.angVel.y);
	//relativeAngVel.z = stdMath_NormalizeDeltaAngle(pConstraint->targetThing->physicsParams.angVel.z, pConstraint->constrainedThing->physicsParams.angVel.z);

	// Apply the relative angular velocity correction to conserve angular momentum
	//rdVector3 velCorrection;
	//rdVector_Scale3(&velCorrection, &relativeAngVel, -1.0f);
	//
	//if (!isnan(velCorrection.x) && !isnan(velCorrection.y) && !isnan(velCorrection.z))
	//{
	//	rdVector3 velCorrectionParent = velCorrection;
	//	//rdVector_Scale3Acc(&velCorrectionParent, 1.0f / inertiaA);
	//	rdVector_Add3Acc(&pConstraint->targetThing->physicsParams.angVel, &velCorrectionParent);
	//
	//	rdVector3 velCorrectionChild = velCorrection;
	//	//rdVector_Scale3Acc(&velCorrectionChild, -1.0f / inertiaB);
	//	rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &velCorrectionChild);
	//}

#else
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
	//rdVector_MultAcc3(&pConstraint->constrainedThing->physicsParams.angVel, &angleDifferences, 1.0f / deltaSeconds);
	
	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
		return;
	
	rdVector3 diffA;
	rdVector_Scale3(&diffA, &angleDifferences, invMassB / constraintMass);
	rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &angleDifferences);
//	rdVector3 diffB;
//	rdVector_Scale3(&diffA, &angleDifferences, -invMassA / constraintMass);
//	rdVector_Add3Acc(&pConstraint->targetThing->physicsParams.angVel, &diffB);

//	rdMatrix34 constrainedRotation;
//	rdMatrix_BuildRotate34(&constrainedRotation, &constrainedAngles);
//	rdMatrix_Multiply34(&pConstraint->thingA->lookOrientation, &pConstraint->thingB->lookOrientation, &constrainedRotation);
#endif
}

static void sithConstraint_SolveLookConstraint(sithConstraint* pConstraint, float deltaSeconds)
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
void eulerToWorldCoordinates(rdVector3* worldAngularVelocity, const rdMatrix34* orientation)
{ 
	rdVector3 localX = {1.0f, 0.0f, 0.0f};
	rdVector3 localY = {0.0f, 1.0f, 0.0f};
	rdVector3 localZ = {0.0f, 0.0f, 1.0f};
	
	rdVector3 worldX, worldY, worldZ;
	rdMatrix_TransformVector34(&worldX, &localX, orientation);
	rdMatrix_TransformVector34(&worldY, &localY, orientation);
	rdMatrix_TransformVector34(&worldZ, &localZ, orientation);
	
	rdVector3 euler = *worldAngularVelocity;
	worldAngularVelocity->x = euler.x * worldX.x + euler.y * worldY.x + euler.z * worldZ.x;
	worldAngularVelocity->y = euler.x * worldX.y + euler.y * worldY.y + euler.z * worldZ.y;
	worldAngularVelocity->z = euler.x * worldX.z + euler.y * worldY.z + euler.z * worldZ.z;
}

void worldToEulerCoordinates(rdVector3* euler, const rdMatrix34* orientation)
{
	rdVector3 localX = {1.0f, 0.0f, 0.0f};
	rdVector3 localY = {0.0f, 1.0f, 0.0f};
	rdVector3 localZ = {0.0f, 0.0f, 1.0f};
	
	rdVector3 worldX, worldY, worldZ;
	rdMatrix_TransformVector34(&worldX, &localX, orientation);
	rdMatrix_TransformVector34(&worldY, &localY, orientation);
	rdMatrix_TransformVector34(&worldZ, &localZ, orientation);

	rdVector3 worldAngularVelocity = *euler;
	
	euler->x = rdVector_Dot3(&worldAngularVelocity, &worldX);
	euler->y = rdVector_Dot3(&worldAngularVelocity, &worldY); 
	euler->z = rdVector_Dot3(&worldAngularVelocity, &worldZ);
}

void sithConstraint_ApplyAngularCorrection(sithThing* parent, sithThing* child, const rdVector3* corrections, const rdVector3* relativeAngVel)
{
	// Calculate the inverse of the child's correction for the parent
	rdVector3 inverseCorrections = {-corrections->x, -corrections->y, -corrections->z};

	// Calculate the angular velocity correction to apply
	rdVector3 angularCorrection = *corrections;
	
	// Ensure angular correction is valid
	if (!isnan(angularCorrection.x) && !isnan(angularCorrection.y) && !isnan(angularCorrection.z))
	{
		// Calculate the inverse masses
		float invMassParent = 1.0f / parent->physicsParams.mass;
		float invMassChild = 1.0f / child->physicsParams.mass;
		float totalInvMass = invMassParent + invMassChild;
		
		// Apply corrections to angular velocities
		rdVector3 correctionParent = inverseCorrections;
		rdVector_Scale3Acc(&correctionParent, invMassParent / totalInvMass);
		//rdVector_Sub3Acc(&parent->physicsParams.angVel, &correctionParent);
		
		rdVector3 correctionChild = angularCorrection;
		rdVector_Scale3Acc(&correctionChild, invMassChild / totalInvMass);
		rdVector_Add3Acc(&child->physicsParams.angVel, &correctionChild);

		// Apply the relative angular velocity correction to both bodies
		rdVector3 velCorrection;
		rdVector_Scale3(&velCorrection, relativeAngVel, -1.0f);
		
		if (!isnan(velCorrection.x) && !isnan(velCorrection.y) && !isnan(velCorrection.z))
		{
			rdVector3 velCorrectionParent = velCorrection;
			rdVector_Scale3Acc(&velCorrectionParent, invMassParent / totalInvMass);
			//rdVector_Sub3Acc(&parent->physicsParams.angVel, &velCorrectionParent);
			
			rdVector3 velCorrectionChild = velCorrection;
			rdVector_Scale3Acc(&velCorrectionChild, invMassChild / totalInvMass);
			
			rdVector_Add3Acc(&child->physicsParams.angVel, &velCorrectionChild);
		}
	}

//	// Calculate the necessary angle correction to bring the twist angle within the limit
//	float correctionAngleDegrees = twistAngleDegrees - copysign(maxTwistAngle, twistAngleDegrees);
//
//	// Proportional correction factor to control severity of corrections
//	float correctionFactor = correctionAngleDegrees / twistAngleDegrees;
//
//	rdVector3 correction;
//	rdVector_Scale3(&correction, &twistAxis, correctionAngleDegrees * correctionFactor);
//
//	if (!isnan(correction.x) && !isnan(correction.y) && !isnan(correction.z))
//	{
//	// Step 4: Apply corrections to angular velocities
//		rdVector3 correctionA;
//		rdVector_Scale3(&correctionA, &correction, invMassA / constraintMass);
//		rdVector_Sub3Acc(&pConstraint->targetThing->physicsParams.angVel, &correctionA);
//
//		rdVector3 correctionB;
//		rdVector_Scale3(&correctionB, &correction, invMassB / constraintMass);
//		rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &correctionB);
//	}
//	else
//		printf("Fuck\n");
}

#include "Primitives/rdQuat.h"
void applyAngularDamping(sithThing* body, float dampingFactor) {
	rdVector3 dampedAngVel;
	rdVector_Scale3(&dampedAngVel, &body->physicsParams.angVel, dampingFactor);
	rdVector_Sub3Acc(&body->physicsParams.angVel, &dampedAngVel);
}

static void sithConstraint_SolveTwistConstraint(sithConstraint* pConstraint, float deltaSeconds)
{
	float invMassA = 1.0f / pConstraint->targetThing->physicsParams.mass;
	float invMassB = 1.0f / pConstraint->constrainedThing->physicsParams.mass;
	float constraintMass = invMassA + invMassB;
	if (constraintMass <= 0.0f)
		return;

	#if 1


#if 0 // quat
	// Convert current orientations to quaternions
	rdQuat parentQuat;
	rdQuat_BuildFrom34(&parentQuat, &pConstraint->targetThing->lookOrientation);

	rdQuat childQuat;
	rdQuat_BuildFrom34(&childQuat, &pConstraint->constrainedThing->lookOrientation);

	// Calculate relative quaternion
	rdQuat invParentQuat;
	rdQuat_Inverse(&invParentQuat, &parentQuat);
	
	rdQuat relativeQuat;
	rdQuat_Mul(&relativeQuat, &childQuat, &invParentQuat);

	// Extract Euler angles from relative quaternion
	rdVector3 relativeAngles;
	rdQuat_ExtractAngles(&relativeQuat, &relativeAngles);
	//if(pConstraint->twistParams.twistAxis.z > 0)
	//	relativeAngles.y -= 90.0f;
	//else
	//	relativeAngles.y += 90.0f;
#else // mat
	rdVector_Zero3(&pConstraint->targetThing->lookOrientation.scale);
	rdVector_Zero3(&pConstraint->constrainedThing->lookOrientation.scale);

	rdMatrix34 invOrientTarget;
	rdMatrix_InvertOrtho34(&invOrientTarget, &pConstraint->targetThing->lookOrientation);

	rdMatrix34 relativeOrientation;
	rdMatrix_Multiply34(&relativeOrientation, &invOrientTarget, &pConstraint->constrainedThing->lookOrientation);

	rdVector3 relativeAngles;
	rdMatrix_ExtractAngles34(&relativeOrientation, &relativeAngles);

	relativeAngles.y = -relativeAngles.y;
	//relativeAngles.y = stdMath_ArcTan3(relativeOrientation.rvec.z, relativeOrientation.uvec.z);
	//relativeAngles.x = stdMath_ArcTan3(-relativeOrientation.lvec.z, sqrt(relativeOrientation.rvec.z * relativeOrientation.rvec.z + relativeOrientation.uvec.z * relativeOrientation.uvec.z));
	//relativeAngles.z = stdMath_ArcTan3(relativeOrientation.lvec.x, relativeOrientation.lvec.y);
#endif

	//printf("relative angles (%f / %f / %f)\n", relativeAngles.x, relativeAngles.y, relativeAngles.z);

	rdVector_NormalizeAngleAcute3(&relativeAngles);

	float maxPitchAngleDegrees = 5.0f;
	float maxTwistAngleDegrees = 5.0f;
	float maxRollAngleDegrees = 5.0f;

	// Apply constraints
	rdVector3 projectedAngles;
	projectedAngles.x = fmin(fmax(relativeAngles.x, -maxPitchAngleDegrees), maxPitchAngleDegrees);
	projectedAngles.y = fmin(fmax(relativeAngles.y, -maxTwistAngleDegrees), maxTwistAngleDegrees);
	projectedAngles.z = fmin(fmax(relativeAngles.z, -maxRollAngleDegrees), maxRollAngleDegrees);

#if 0 // quat
	// Convert back to constrained quaternion
	rdQuat constrainedQuat;
	rdQuat_BuildFromAngles(&constrainedQuat, &relativeAngles);

	// Use Slerp to smoothly interpolate towards the constrained quaternion
	rdQuat newChildQuat;
	rdQuat_Slerp(&newChildQuat, &childQuat, &constrainedQuat, 0.95f);
	
	// Calculate corrections for angular velocities based on constrained quaternion
	rdQuat angularCorrectionQuat;
	rdQuat_Mul(&angularCorrectionQuat, &constrainedQuat, &invParentQuat);
	
	rdVector3 angularCorrection;
	rdQuat_ExtractAngles(&angularCorrectionQuat, &angularCorrection);
#else // mat
	//rdMatrix34 constrainedMat;
	//rdMatrix_BuildRotate34(&constrainedMat, &relativeAngles);
	//
	//rdMatrix34 angularCorrectionMat;
	//rdMatrix_Multiply34(&angularCorrectionMat, &invOrientTarget, &constrainedMat);//, &invOrientA);
	//
	//rdVector3 angularCorrection = relativeAngles;
	//rdMatrix_ExtractAngles34(&angularCorrectionMat, &angularCorrection);
	//angularCorrection.y = stdMath_ArcTan3(angularCorrectionMat.rvec.z, angularCorrectionMat.uvec.z);
	//angularCorrection.x = stdMath_ArcTan3(-angularCorrectionMat.lvec.z, sqrt(angularCorrectionMat.rvec.z * angularCorrectionMat.rvec.z + angularCorrectionMat.uvec.z * angularCorrectionMat.uvec.z));
	//angularCorrection.z = stdMath_ArcTan3(angularCorrectionMat.lvec.x, angularCorrectionMat.lvec.y);

	rdVector3 angularCorrection;
	angularCorrection.x = stdMath_NormalizeDeltaAngle(projectedAngles.x, relativeAngles.x);
	angularCorrection.y = stdMath_NormalizeDeltaAngle(projectedAngles.y, relativeAngles.y);
	angularCorrection.z = stdMath_NormalizeDeltaAngle(projectedAngles.z, relativeAngles.z);
#endif

	rdVector_NormalizeAngleAcute3(&angularCorrection);

	// Increase the correction strength
	//rdVector_Scale3Acc(&angularCorrection, 2.0f); // Scale the correction by 2 for stronger enforcement

	float inertiaA = (2.0f / 5.0f) * pConstraint->targetThing->physicsParams.mass * pConstraint->targetThing->collideSize * pConstraint->targetThing->collideSize;
	float inertiaB = (2.0f / 5.0f) * pConstraint->constrainedThing->physicsParams.mass * pConstraint->constrainedThing->collideSize * pConstraint->constrainedThing->collideSize;

	// Calculate the relative angular velocity
	rdVector3 relativeAngVel;
	//rdVector_Sub3(&relativeAngVel, &pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->targetThing->physicsParams.angVel);
	relativeAngVel.x = stdMath_NormalizeDeltaAngle(pConstraint->targetThing->physicsParams.angVel.x, pConstraint->constrainedThing->physicsParams.angVel.x);
	relativeAngVel.y = stdMath_NormalizeDeltaAngle(pConstraint->targetThing->physicsParams.angVel.y, pConstraint->constrainedThing->physicsParams.angVel.y);
	relativeAngVel.z = stdMath_NormalizeDeltaAngle(pConstraint->targetThing->physicsParams.angVel.z, pConstraint->constrainedThing->physicsParams.angVel.z);

	// Calculate the inverse of the child's correction for the parent
	rdVector3 inverseCorrection = { -angularCorrection.x, -angularCorrection.y, -angularCorrection.z };

	// Ensure corrections are valid
	if (!isnan(angularCorrection.x) && !isnan(angularCorrection.y) && !isnan(angularCorrection.z))
	{
		//rdVector3 correctionChild;
		//rdVector_Scale3(&correctionChild, &angularCorrection, 1.0f / inertiaB);
		//rdVector_Sub3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &correctionChild);

		// Apply corrections to both parent and child angular velocities
		rdVector3 correctionParent;
		rdVector_Scale3(&correctionParent, &inverseCorrection, invMassA / constraintMass);
		rdVector_Add3Acc(&pConstraint->targetThing->physicsParams.angVel, &correctionParent);
		
		rdVector3 correctionChild;
		rdVector_Scale3(&correctionChild, &angularCorrection, invMassB / constraintMass);
		rdVector_Sub3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &correctionChild);

		// Apply the relative angular velocity correction to conserve angular momentum
		//rdVector3 velCorrection;
		//rdVector_Scale3(&velCorrection, &relativeAngVel, -1.0f);
		//
		//if (!isnan(velCorrection.x) && !isnan(velCorrection.y) && !isnan(velCorrection.z))
		//{
		//	rdVector3 velCorrectionParent = velCorrection;
		//	rdVector_Scale3Acc(&velCorrectionParent, 1.0f / inertiaA);
		//	rdVector_Sub3Acc(&pConstraint->targetThing->physicsParams.field_1F8, &velCorrectionParent);
		//
		//	rdVector3 velCorrectionChild = velCorrection;
		//	rdVector_Scale3Acc(&velCorrectionChild, -1.0f / inertiaB);
		//	rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.field_1F8, &velCorrectionChild);
		//}
	}

//	applyAngularDamping(pConstraint->targetThing, 0.5f);
//	applyAngularDamping(pConstraint->constrainedThing, 0.5f);
	
	//rdMath_ClampVectorRange(&pConstraint->targetThing->physicsParams.angVel, -pConstraint->targetThing->physicsParams.maxRotVel, pConstraint->targetThing->physicsParams.maxRotVel);
	//rdMath_ClampVector(&pConstraint->targetThing->physicsParams.angVel, 0.00001);
	//
	//rdMath_ClampVectorRange(&pConstraint->constrainedThing->physicsParams.angVel, -pConstraint->constrainedThing->physicsParams.maxRotVel, pConstraint->constrainedThing->physicsParams.maxRotVel);
	//rdMath_ClampVector(&pConstraint->constrainedThing->physicsParams.angVel, 0.00001);

		//worldToEulerCoordinates(&pConstraint->targetThing->physicsParams.angVel, &pConstraint->targetThing->lookOrientation);
	//worldToEulerCoordinates(&pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->constrainedThing->lookOrientation);

	#elif 1
	
	rdQuat parentQuat;
	rdQuat_BuildFromAngles(&parentQuat , &pConstraint->targetThing->physicsParams.angVel);

	rdQuat childQuat;
	rdQuat_BuildFromAngles(&childQuat, &pConstraint->constrainedThing->physicsParams.angVel);

	// Calculate relative quaternion
	rdQuat invParentQuat = { -parentQuat.x, -parentQuat.y, -parentQuat.z, parentQuat.w };
	rdQuat relativeQuat;
	rdQuat_Mul(&relativeQuat, &childQuat, &invParentQuat);

	// Extract Euler angles from relative quaternion
	rdVector3 angles;
	rdQuat_ExtractAngles(&relativeQuat, &angles);
	
	rdVector3 maxAngles = { 5.0f, 5.0f, 5.0f};


	// Apply constraints
	angles.x = fmin(fmax(angles.x, -maxAngles.x), maxAngles.x);
	angles.y = fmin(fmax(angles.y, -maxAngles.y), maxAngles.y);
	angles.z = fmin(fmax(angles.z, -maxAngles.z), maxAngles.z);

	// Convert back to quaternions
	rdQuat constrainedQuat;
	rdQuat_BuildFromAngles(&constrainedQuat, &angles);

	// Calculate corrections
	rdQuat correctionQuat;
	rdQuat_Mul(&correctionQuat, &constrainedQuat, &invParentQuat);
	
	rdQuat_ExtractAngles(&correctionQuat, &pConstraint->constrainedThing->physicsParams.angVel);

	// Apply the corrections to the parent as well to conserve energy
	rdQuat inverseCorrectionQuat = { -correctionQuat.x, -correctionQuat.y, -correctionQuat.z, correctionQuat.w };
	rdQuat_ExtractAngles(&inverseCorrectionQuat, &pConstraint->targetThing->physicsParams.angVel);


	#else
	//eulerToWorldCoordinates(&pConstraint->targetThing->physicsParams.angVel, &pConstraint->targetThing->lookOrientation);
	//eulerToWorldCoordinates(&pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->constrainedThing->lookOrientation);

	rdMatrix34 invOrientA;
	rdMatrix_InvertOrtho34(&invOrientA, &pConstraint->targetThing->lookOrientation);
	
	rdMatrix34 relativeOrientation;
	rdMatrix_Multiply34(&relativeOrientation, &pConstraint->constrainedThing->lookOrientation, &invOrientA);
	
//	rdVector3 twistAxis;
//	rdMatrix_TransformVector34(&twistAxis, &pConstraint->twistParams.twistAxis, &pConstraint->targetThing->lookOrientation);
//
//	// Normalize the twist axis
//	rdVector3 normTwistAxis;
//	rdVector_Normalize3(&normTwistAxis, &twistAxis);
//	
//	rdMatrix34 twistMatrix;
//	rdMatrix_Copy34(&twistMatrix, &relativeOrientation);
//	
//	// Project the relative orientation onto the twist axis to extract the twist component
//	rdVector3 projectedUp;
//	rdMatrix_TransformVector34(&projectedUp, &twistMatrix.uvec, &twistMatrix);
//	
//	// Calculate the angle around the twist axis
//	float twistAngleRadians = atan2(projectedUp.y, projectedUp.x);
//	if (isnan(twistAngleRadians))
//		printf("Fuck\n");
//
//	float twistAngleDegrees = twistAngleRadians * (180.0 / M_PI);
//	
//	// Step 3: Check the twist constraint
//	float maxTwistAngle = pConstraint->twistParams.maxTwistAngle;// / (180.0f / M_PI);
//	if (fabs(twistAngleDegrees) > maxTwistAngle)
//	{
//		// Calculate the necessary angle correction to bring the twist angle within the limit
//		float correctionAngleDegrees = twistAngleDegrees - copysign(maxTwistAngle, twistAngleDegrees);
//		sithConstraint_ApplyAngularCorrection(pConstraint->targetThing, pConstraint->constrainedThing, &twistAxis, correctionAngleDegrees);
//	}

	// Extract pitch and roll angles
	rdVector3 relativeAngles;
	rdMatrix_ExtractAngles34(&relativeOrientation, &relativeAngles);
	
	float* anglesAsFloats = &relativeAngles.x;

	rdVector3 corrections;
	float* correctionsAsFloats = &corrections.x;

	rdVector3 maxAngles = { 5.0f, 10.0f, 5.0f };
	float* maxAnglesAsFloats = &maxAngles.x;

	for (int i = 0; i < 3; ++i)
	{
		if (fabs(anglesAsFloats[i]) > maxAnglesAsFloats[i])
		{
			correctionsAsFloats[i] = anglesAsFloats[i] - copysign(maxAnglesAsFloats[i], anglesAsFloats[i]);
		}
		else
		{
			correctionsAsFloats[i] = 0.0f;
		}
	}

	rdVector3 relativeAngVel;
	rdVector_Sub3(&relativeAngVel, &pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->targetThing->physicsParams.angVel);

	sithConstraint_ApplyAngularCorrection(pConstraint->targetThing, pConstraint->constrainedThing, &corrections, &relativeAngVel);

//	// Constrain yaw (twist)
//	float maxTwistAngleDegrees = 10.0f;
//	if (fabs(relativeAngles.y) > maxTwistAngleDegrees)
//	{
//		float correctionYawDegrees = relativeAngles.y - copysign(maxTwistAngleDegrees, relativeAngles.y);
//		
//		rdVector3 yawCorrection = {0.0f, correctionYawDegrees, 0.0f};
//		
//		// Apply corrections to angular velocities
//		sithConstraint_ApplyAngularCorrection(pConstraint->targetThing, pConstraint->constrainedThing, &yawCorrection);
//	}
//
//	// Check and correct the pitch constraint
//	float maxPitchAngleDegrees = 5.0f;
//	if (fabs(relativeAngles.x) > maxPitchAngleDegrees)
//	{
//		float correctionPitchDegrees = relativeAngles.x - copysign(maxPitchAngleDegrees, relativeAngles.x);
//		rdVector3 pitchCorrection = {correctionPitchDegrees, 0.0f, 0.0f};
//		sithConstraint_ApplyAngularCorrection(pConstraint->targetThing, pConstraint->constrainedThing, &pitchCorrection);
//	}
//	
//	// Check and correct the roll constraint
//	float maxRollAngleDegrees = 5.0f;
//	if (fabs(relativeAngles.z) > maxRollAngleDegrees)
//	{
//		float correctionRollDegrees = relativeAngles.z - copysign(maxRollAngleDegrees, relativeAngles.z);
//		rdVector3 rollCorrection = {0.0f, 0.0f, correctionRollDegrees};
//		sithConstraint_ApplyAngularCorrection(pConstraint->targetThing, pConstraint->constrainedThing, &rollCorrection);
//	}

//	// Calculate the relative angular velocity
//	rdVector3 relativeAngVel;
//	rdVector_Sub3(&relativeAngVel, &pConstraint->targetThing->physicsParams.angVel, &pConstraint->constrainedThing->physicsParams.angVel);
//	
//	// Project the relative angular velocity onto the world twist axis
//	float projAngVel = rdVector_Dot3(&relativeAngVel, &twistAxis);
//	if(isnan(projAngVel))
//		printf("Fuck\n");
//	
//	// Apply the velocity correction to both bodies
//	rdVector3 velCorrection;
//	rdVector_Scale3(&velCorrection, &twistAxis, projAngVel);
//	
//	if (!isnan(velCorrection.x) && !isnan(velCorrection.y) && !isnan(velCorrection.z))
//	{
//		rdVector3 velCorrectionParent = velCorrection;
//		rdVector_Scale3Acc(&velCorrectionParent, invMassA / constraintMass);
//		rdVector_Sub3Acc(&pConstraint->targetThing->physicsParams.angVel, &velCorrectionParent);
//	
//		rdVector3 velCorrectionChild = velCorrection;
//		rdVector_Scale3Acc(&velCorrectionChild, invMassB / constraintMass);
//		rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &velCorrectionChild);
//	}
//	else
//		printf("Fuck\n");

	//worldToEulerCoordinates(&pConstraint->targetThing->physicsParams.angVel, &pConstraint->targetThing->lookOrientation);
	//worldToEulerCoordinates(&pConstraint->constrainedThing->physicsParams.angVel, &pConstraint->constrainedThing->lookOrientation);
	#endif

#if 0
	rdVector3 crossLook;
	rdVector_Cross3(&crossLook, &pConstraint->targetThing->lookOrientation.lvec, &pConstraint->constrainedThing->lookOrientation.lvec);
	float dotLook = rdVector_Dot3(&pConstraint->targetThing->lookOrientation.lvec, &pConstraint->constrainedThing->lookOrientation.lvec);
	
	float angleChange = atan2f(rdVector_Len3(&crossLook), dotLook);
	
	// Calculate the relative angular velocity
	rdVector3 relativeAngVel;
	rdVector_Sub3(&relativeAngVel, &pConstraint->targetThing->physicsParams.angVel, &pConstraint->constrainedThing->physicsParams.angVel);
	
	// Normalize the twist axis
	rdVector3 normTwistAxis;
	rdMatrix_TransformVector34(&normTwistAxis, &pConstraint->twistParams.twistAxis, &pConstraint->targetThing->lookOrientation);
	
	// Project the relative angular velocity onto the twist axis
	float projAngVel = rdVector_Dot3(&relativeAngVel, &normTwistAxis);
	
	// Check if the twist exceeds the max allowed twist angle
	float twistAngleRadians = pConstraint->twistParams.maxTwistAngle / (180.0f / M_PI);
	if (fabs(projAngVel) > twistAngleRadians || angleChange > twistAngleRadians)
	{
		float correctionFactor = fminf(twistAngleRadians / fabs(projAngVel), twistAngleRadians / angleChange);

		// Correct the angular velocity to stay within the twist constraint
		//float correctionFactor = twistAngleRadians / fabs(projAngVel);
		//rdVector3 correction;
		//scaleVector(&relativeAngVel, correctionFactor);
		//ubtractVectors(&relativeAngVel, &correction, &correction);
		rdVector3 correction = {
			normTwistAxis.x * projAngVel * (1.0f - correctionFactor),
			normTwistAxis.y * projAngVel * (1.0f - correctionFactor),
			normTwistAxis.z * projAngVel * (1.0f - correctionFactor)
		};
		
		// Apply the correction to each body's angular velocity based on their masses
		rdVector3 correctionA;
		rdVector_Scale3(&correctionA, &correction, invMassA / constraintMass);
		rdVector_Sub3Acc(&pConstraint->targetThing->physicsParams.angVel, &correctionA);
		
		rdVector3 correctionB;
		rdVector_Scale3(&correctionB, &correction, invMassB / constraintMass);
		rdVector_Add3Acc(&pConstraint->constrainedThing->physicsParams.angVel, &correctionB);
	}
#endif
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
					sithConstraint_SolveAngleConstraint(constraint, deltaSeconds);
					break;
				case SITH_CONSTRAINT_LOOK:
					sithConstraint_SolveLookConstraint(constraint, deltaSeconds);
					break;
				case SITH_CONSTRAINT_TWIST:
					sithConstraint_SolveTwistConstraint(constraint, deltaSeconds);
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