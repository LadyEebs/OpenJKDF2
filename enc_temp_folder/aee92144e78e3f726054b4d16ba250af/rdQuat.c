#include "rdQuat.h"

#include <math.h>
#include "rdMatrix.h"
#include "General/stdMath.h"
#include "Primitives/rdMath.h"

void rdQuat_Set(rdQuat* out, float x, float y, float z, float w)
{
	out->x = x;
	out->y = y;
	out->z = z;
	out->w = w;
}

void rdQuat_BuildFromAxisAngle(rdQuat* out, rdVector3* axis, float angle)
{
	float s, c;
	stdMath_SinCos(angle * 0.5f, &s, &c);
	out->w = c;
	out->x = axis->x * s;
	out->y = axis->y * s;
	out->z = axis->z * s;
}

void rdQuat_BuildFromVector(rdQuat* out, rdVector3* axis)
{
	rdQuat q0, q1, q2;
	rdQuat_BuildFromAxisAngle(&q0, &rdroid_zVector3, axis->y);
	rdQuat_BuildFromAxisAngle(&q1, &rdroid_xVector3, axis->x);
	rdQuat_BuildFromAxisAngle(&q2, &rdroid_yVector3, axis->z);
	rdQuat_Mul(out, &q0, &q1);
	rdQuat_MulAcc(out, &q2);
}

void rdQuat_BuildFrom34(rdQuat* out, const rdMatrix34* matrix)
{
	float trace = matrix->rvec.x + matrix->lvec.y + matrix->uvec.z;
	if (trace > 0.0f)
	{
		float s = 0.5f / sqrtf(trace + 1.0f);
		out->w = 0.25f / s;
		out->x = (matrix->uvec.y - matrix->lvec.z) * s;
		out->y = (matrix->rvec.z - matrix->uvec.x) * s;
		out->z = (matrix->lvec.x - matrix->rvec.y) * s;
	}
	else
	{
		if (matrix->rvec.x > matrix->lvec.y && matrix->rvec.x > matrix->uvec.z)
		{
			float s = 2.0f * sqrtf(1.0f + matrix->rvec.x - matrix->lvec.y - matrix->uvec.z);
			out->w = (matrix->uvec.y - matrix->lvec.z) / s;
			out->x = 0.25f * s;
			out->y = (matrix->rvec.y + matrix->lvec.x) / s;
			out->z = (matrix->rvec.z + matrix->uvec.x) / s;
		}
		else if (matrix->lvec.y > matrix->uvec.z)
		{
			float s = 2.0f * sqrtf(1.0f + matrix->lvec.y - matrix->rvec.x - matrix->uvec.z);
			out->w = (matrix->rvec.z - matrix->uvec.x) / s;
			out->x = (matrix->rvec.y + matrix->lvec.x) / s;
			out->y = 0.25f * s;
			out->z = (matrix->lvec.z + matrix->uvec.y) / s;
		}
		else
		{
			float s = 2.0f * sqrtf(1.0f + matrix->uvec.z - matrix->rvec.x - matrix->lvec.y);
			out->w = (matrix->lvec.x - matrix->rvec.y) / s;
			out->x = (matrix->rvec.z + matrix->uvec.x) / s;
			out->y = (matrix->lvec.z + matrix->uvec.y) / s;
			out->z = 0.25f * s;
		}
	}
}

void rdQuat_BuildFromVectors(rdQuat* out, const rdVector3* v1, const rdVector3* v2)
{
	rdVector3 cross;
	rdVector_Cross3(&cross, v2, v1);
	
	float dot = rdVector_Dot3(v1, v2);
	float s = sqrt((1.0f + dot) * 2.0f);
	
	out->w = s * 0.5f;
	out->x = cross.x / s;
	out->y = cross.y / s;
	out->z = cross.z / s;
}

void rdQuat_BuildFromAngles(rdQuat* q, const rdVector3* angles)
{
	float sp, cp;
	stdMath_SinCos(angles->x * 0.5f, &sp, &cp);

	float sy, cy;
	stdMath_SinCos(angles->y * 0.5f, &sy, &cy);

	float sr, cr;
	stdMath_SinCos(angles->z * 0.5f, &sr, &cr);

	q->w = cy * cr * cp + sy * sr * sp;
	q->x = cy * sr * cp - sy * cr * sp;
	q->y = cy * cr * sp + sy * sr * cp;
	q->z = sy * cr * cp - cy * sr * sp;
	
	return q;
}

void rdQuat_ExtractAxisAngle(rdQuat* q, rdVector3* axis, float* angle)
{
	*angle = 2.0f * (90.0f - stdMath_ArcSin3(q->w));

	float s, c;
	stdMath_SinCos(*angle * 0.5f, &s, &c);

	float omega = *angle / s;
	axis->x = q->x * omega;
	axis->y = q->y * omega;
	axis->z = q->z * omega;
}

void rdQuat_ExtractAngles(rdQuat* q, rdVector3* angles)
{
	// Pitch (x-axis rotation)
	float sinr_cosp = 2 * (q->w * q->x + q->y * q->z);
	float cosr_cosp = 1 - 2 * (q->x * q->x + q->y * q->y);
	
	angles->x = stdMath_ArcTan3(sinr_cosp, cosr_cosp);
	
	// Roll (y-axis rotation)
	float sinp = 2 * (q->w * q->y - q->z * q->x);
	if (fabs(sinp) >= 1)
		angles->z = copysign(M_PI / 2, sinp);
	// use 90 degrees if out of range
	else
		angles->z = stdMath_ArcSin3(sinp);
	
	// Yaw (z-axis rotation)
	float siny_cosp = 2 * (q->w * q->z + q->x * q->y);
	float cosy_cosp = 1 - 2 * (q->y * q->y + q->z * q->z);
	
	angles->y = stdMath_ArcTan3(siny_cosp, cosy_cosp);

//	angles->y = stdMath_ArcTan3(2.0f * (q->w * q->y + q->x * q->z), 1.0f - 2.0f * (q->y * q->y + q->x * q->x)); // Yaw
//	angles->x = stdMath_ArcSin3(2.0f * (q->w * q->x - q->z * q->y)); // Pitch
//	angles->z = stdMath_ArcTan3(2.0f * (q->w * q->z + q->x * q->y), 1.0f - 2.0f * (q->z * q->z + q->x * q->x)); // Roll
//
//	//angles->x = stdMath_ArcTan3(2.0f * (q->w * q->x + q->y * q->z), 1.0f - 2.0f * (q->x * q->x + q->y * q->y));
//	//angles->y = stdMath_ArcTan3(2.0f * (q->w * q->z + q->x * q->y), 1.0f - 2.0f * (q->y * q->y + q->z * q->z));
//	//angles->z = stdMath_ArcSin3(2.0f * (q->w * q->y - q->z * q->x));
}

void rdQuat_Mul(rdQuat* out, rdQuat* qa, rdQuat* qb)
{
	out->w = qa->w * qb->w - qa->x * qb->x - qa->y * qb->y - qa->z * qb->z;
	out->x = qa->w * qb->x + qa->x * qb->w + qa->y * qb->z - qa->z * qb->y;
	out->y = qa->w * qb->y - qa->x * qb->z + qa->y * qb->w + qa->z * qb->x;
	out->z = qa->w * qb->z + qa->x * qb->y - qa->y * qb->x + qa->z * qb->w;

//	rdVector3 v1;
//	v1.x = qa->x;
//	v1.y = qa->y;
//	v1.z = qa->z;
//
//	rdVector3 v2;
//	v2.x = qb->x;
//	v2.y = qb->y;
//	v2.z = qb->z;
//
//	out->w = qa->w * qb->w - rdVector_Dot3(&v1, &v2);
//
//	rdVector3 vp;
//	rdVector_Cross3(&vp, &v1, &v2);
//
//	out->x = v2.x * qa->w + v1.x * qa->w - vp.x;
//	out->y = v2.y * qa->w + v1.y * qa->w - vp.y;
//	out->z = v2.z * qa->w + v1.z * qa->w - vp.z;
}

void rdQuat_MulAcc(rdQuat* qa, rdQuat* qb)
{
	//rdQuat tmp = *qa;
	//rdQuat_Mul(qa, &tmp, qb);

	rdVector3 vp;
	vp.x = qb->x * qa->w + qa->x * qb->w - (qa->y * qb->z - qa->z * qb->y);
	vp.y = qb->y * qa->w + qa->y * qb->w - (qa->z * qb->x - qa->x * qb->z);
	vp.z = qb->z * qa->w + qa->z * qb->w - (qa->x * qb->y - qa->y * qb->x);
	qa->w = qa->w * qb->w - (qa->x * qb->x + qa->y * qb->y + qa->z * qb->z);
	qa->x = vp.x;
	qa->y = vp.y;
	qa->z = vp.z;
}

float rdQuat_LenSq(rdQuat* q)
{
	return q->x * q->x + q->y * q->y + q->z * q->z + q->w * q->w;
}

void rdQuat_Conjugate(rdQuat* out, const rdQuat* q)
{
	out->w =  q->w;
	out->x = -q->x;
	out->y = -q->y;
	out->z = -q->z;
}

void rdQuat_ConjugateAcc(rdQuat* q)
{
	q->x = -q->x;
	q->y = -q->y;
	q->z = -q->z;
}

void rdQuat_TransformVector(rdVector3* out, const rdQuat* q, const rdVector3* v)
{
	rdQuat vq;
	rdQuat_Set(&vq, v->x, v->y, v->z, 0.0f);

	rdQuat qh;
	rdQuat_Conjugate(&qh, q);

	rdQuat r;
	rdQuat_Mul(&r, q, &vq);
	rdQuat_MulAcc(&r, &qh);

	float len = rdQuat_LenSq(q);
	out->x = r.x / len;
	out->y = r.y / len;
	out->z = r.z / len;
}

void rdQuat_ToMatrix(rdMatrix34* out, const rdQuat* q)
{
	float sqw = q->w * q->w;
	float sqx = q->x * q->x;
	float sqy = q->y * q->y;
	float sqz = q->z * q->z;

	float invs = 1.0f / (sqx + sqy + sqz + sqw);

	float xy = q->x * q->y;
	float zw = q->z * q->w;

	float m00 = (sqx - sqy - sqz + sqw) * invs;
	float m11 = (-sqx + sqy - sqz + sqw) * invs;
	float m22 = (-sqx - sqy + sqz + sqw) * invs;

	float m10 = 2.0f * (xy + zw) * invs;
	float m01 = 2.0f * (xy - zw) * invs;

	float xz = q->x * q->z;
	float yw = q->y * q->w;

	float m20 = 2.0f * (xz - yw) * invs;
	float m02 = 2.0f * (xz + yw) * invs;

	float yz = q->y * q->z;
	float xw = q->x * q->w;

	float m21 = 2.0f * (yz + xw) * invs;
	float m12 = 2.0f * (yz - xw) * invs;

	out->rvec.x = m00;
	out->rvec.y = m01;
	out->rvec.z = m02;

	out->lvec.x = m10;
	out->lvec.y = m11;
	out->lvec.z = m12;

	out->uvec.x = m20;
	out->uvec.y = m21;
	out->uvec.z = m22;

	out->scale.x = out->scale.y = out->scale.z = 0.0f;
}

void rdQuat_Slerp(rdQuat* out, const rdQuat* qa, const rdQuat* qb, const float c)
{
	float dot = qa->x * qb->x + qa->y * qb->y + qa->z * qb->z + qa->w * qb->w;
	float theta = acosf(dot);
	float sinTheta = sinf(theta);
	if (sinTheta > 0.001f)
	{
		float invSinTheta = 1.0f / sinTheta;
		float scale1 = sinf((1.0f - c) * theta) * invSinTheta;
		float scale2 = sinf(c * theta) * invSinTheta;
		
		out->x = scale1 * qa->x + scale2 * qb->x;
		out->y = scale1 * qa->y + scale2 * qb->y;
		out->z = scale1 * qa->z + scale2 * qb->z;
		out->w = scale1 * qa->w + scale2 * qb->w;
	}
	else
	{
		// If the quaternions are very close, use linear interpolation
		out->x = qa->x * (1.0f - c) + qb->x * c;
		out->y = qa->y * (1.0f - c) + qb->y * c;
		out->z = qa->z * (1.0f - c) + qb->z * c;
		out->w = qa->w * (1.0f - c) + qb->w * c;
	}

//	float comega = qa->x * qb->x + qa->y * qb->y + qa->z * qb->z + qa->w * qb->w;
//
//	rdQuat q2;	
//	if (comega < 0.0f)
//	{
//		comega = -comega;
//		q2.x = -qb->x;
//		q2.y = -qb->y;
//		q2.z = -qb->z;
//		q2.w = -qb->w;
//	}
//	else
//	{
//		q2 = *qb;
//	}
//
//	float k1, k2;
//	if (1.0f - comega > 0.000001f)
//	{
//		// fixme: use stdMath stuff
//		float omega = acos(comega);
//		float sinfomega = sin(omega);
//		k1 = sin((1 - c) * omega) / sinfomega;
//		k2 = sin(c * omega) / sinfomega;
//	}
//	else
//	{
//		k1 = 1.0f - c;
//		k2 = c;
//	}
//
//	out->x = qa->x * k1 + q2.x * k2;
//	out->y = qa->y * k1 + q2.y * k2;
//	out->z = qa->z * k1 + q2.z * k2;
//	out->w = qa->w * k1 + q2.w * k2;
}

void rdQuat_NormalizeAcc(rdQuat* q)
{
	float length = sqrtf(q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z);
	q->w /= length;
	q->x /= length;
	q->y /= length;
	q->z /= length;
}

void rdQuat_Inverse(rdQuat* out, const rdQuat* q)
{
	float norm = q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z;
	out->w = q->w / norm;
	out->x = -q->x / norm;
	out->y = -q->y / norm;
	out->z = -q->z / norm;
}

int rdQuat_IsZero(const rdQuat* q)
{
	return (q->x == 0.0 && q->y == 0.0 && q->z == 0.0 && q->w == 0.0);
}