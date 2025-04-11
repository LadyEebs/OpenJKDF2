#include "rdCluster.h"

#ifdef RENDER_DROID2

#include "General/stdMath.h"

#include "Engine/rdClip.h"
#include "Engine/rdCamera.h"
#include "Primitives/rdMath.h"

#include "Modules/std/std3D.h"
#include "Modules/std/stdJob.h"
#include "Modules/std/stdProfiler.h"

#ifdef JOB_SYSTEM
#define USE_JOBS
#endif

#ifdef USE_JOBS
#include "SDL_atomic.h"
#endif

// todo: split XY and Z clusters so we can do XY | Z in the shader and reduce cluster build cost

typedef enum
{
	CLUSTER_ITEM_SPHERE,
	CLUSTER_ITEM_CONE,
	CLUSTER_ITEM_BOX
} rdCluster_ItemType;

typedef struct
{
	float     angle;
	rdVector3 apex;
	rdVector3 direction;
} rdCluster_ConeParams;

typedef struct
{
	rdMatrix44 matrix;
} rdCluster_BoxParams;

typedef struct
{
	int       itemIndex;
	int       type;
	float     radius;
	rdVector3 position;
	union
	{
		rdCluster_ConeParams   coneParams;
		rdCluster_BoxParams    boxParams;
	};
} rdCluster_Item;

static int         rdroid_clustersDirty;								// clusters need rebuilding/refilling
static int         rdroid_clusterFrustumFrame;							// current frame for clusters, any cluster not matching will have its bounds updated
static int         rdroid_lastClusterFrustumFrame;						// last frame for clusters, will be updated to clusterFrustumFrame after building
static rdCluster   rdroid_clusters[STD3D_CLUSTER_GRID_TOTAL_SIZE];		// per cluster data (bounds and frame count)
static uint32_t    rdroid_clusterBits[STD3D_CLUSTER_GRID_TOTAL_SIZE];	// cluster bit sets

static rdMatrix44* rdroid_clusterJobProjectionMat;

static uint32_t          rdroid_clusterLightOffset;
static uint32_t          rdroid_numClusterLights;
static rdClusterLight    rdroid_clusterLights[STD3D_CLUSTER_MAX_LIGHTS];

static uint32_t          rdroid_clusterOccluderOffset;
static uint32_t          rdroid_numClusterOccluders;
static rdClusterOccluder rdroid_clusterOccluders[STD3D_CLUSTER_MAX_OCCLUDERS];

static uint32_t          rdroid_clusterDecalOffset;
static uint32_t          rdroid_numClusterDecals;
static rdClusterDecal    rdroid_clusterDecals[STD3D_CLUSTER_MAX_DECALS];

void rdCluster_ClearLights()
{
	rdroid_clusterLightOffset = 0;
	rdroid_numClusterLights = 0;
	rdroid_clustersDirty = 1;
}

void rdCluster_ClearOccluders()
{
	rdroid_clusterOccluderOffset = 0;
	rdroid_numClusterOccluders = 0;
	rdroid_clustersDirty = 1;
}

void rdCluster_ClearDecals()
{
	rdroid_clusterDecalOffset = 0;
	rdroid_numClusterDecals = 0;
	rdroid_clustersDirty = 1;
	std3D_PurgeDecals();
}

void rdCluster_Clear()
{
	rdCluster_ClearLights();
	rdCluster_ClearOccluders();
	rdCluster_ClearDecals();
}

float rdCluster_BuildAreaLightMatrix(rdClusterLight* pLight, rdVector3* position, rdMatrix44* mat, float* radius)
{
	const float width  = pLight->right.w + *radius * 2.0;
	const float height = pLight->up.w + *radius * 2.0;
		
	rdVector_Scale3((rdVector3*)&mat->vA, (rdVector3*)&pLight->right, width);
	mat->vA.w = 0.0f;

	rdVector_Scale3((rdVector3*)&mat->vB, (rdVector3*)&pLight->direction_intensity, *radius * 2.0);
	mat->vB.w = 0.0f;

	rdVector_Scale3((rdVector3*)&mat->vC, (rdVector3*)&pLight->up, height);
	mat->vC.w = 0.0f;

	rdVector3 offset;
	rdVector_Scale3(&offset, (rdVector3*)&pLight->direction_intensity, *radius * 0.5f);
	rdVector_Add3((rdVector3*)&mat->vD, (rdVector3*)&pLight->position, (rdVector3*)&offset);
	mat->vD.w = 1.0f;

	rdVector_Copy3(position, (rdVector3*)&mat->vD);

	float scaleX = rdVector_Dot3((rdVector3*)&mat->vA, (rdVector3*)&mat->vA);
	float scaleY = rdVector_Dot3((rdVector3*)&mat->vB, (rdVector3*)&mat->vB);
	float scaleZ = rdVector_Dot3((rdVector3*)&mat->vC, (rdVector3*)&mat->vC);

	//*radius = fmax(width, fmax(height, *radius * 2.0));
	float diagonal = stdMath_Sqrt(scaleX + scaleY + scaleZ);
	*radius = diagonal / 2.0f;
}

// https://bartwronski.com/2017/04/13/cull-that-cone/
void rdCluster_BoundingSphere(rdVector3* outPos, float* outRadius, rdVector3* origin, rdVector3* forward, float size, float angle)
{
	float s, c;
	stdMath_SinCos(angle, &s, &c);
	if (angle > 45.0)
	{
		float offset = c * size;
		outPos->x = origin->x + offset * forward->x;
		outPos->y = origin->y + offset * forward->y;
		outPos->z = origin->z + offset * forward->z;
		*outRadius = s * size;
	}
	else
	{
		float offset = size / (2.0f * c);
		outPos->x = origin->x + offset * forward->x;
		outPos->y = origin->y + offset * forward->y;
		outPos->z = origin->z + offset * forward->z;
		*outRadius = offset;
	}
}

// clipping and clustering

static void rdCluster_UpdateClipRegionRoot(float nc, float lc, float lz, float Radius, float CameraScale, float* ClipMin, float* ClipMax)
{
	float nz = (Radius - nc * lc) / lz;
	float pz = (lc * lc + lz * lz - Radius * Radius) / (lz - (nz / nc) * lc);
	if (pz > 0.0f)
	{
		float c = -nz * CameraScale / nc;
		if (nc > 0.0f)
			*ClipMin = fmax(*ClipMin, c);
		else
			*ClipMax = fmin(*ClipMax, c);
	}
}

static void rdCluster_UpdateClipRegion(float lc, float lz, float Radius, float CameraScale, float* ClipMin, float* ClipMax)
{
	float rSq = Radius * Radius;
	float lcSqPluslzSq = lc * lc + lz * lz;
	float d = rSq * lc * lc - lcSqPluslzSq * (rSq - lz * lz);
	if (d > 0.0f)
	{
		float a = Radius * lc;
		float b = stdMath_Sqrt(d);
		float nx0 = (a + b) / lcSqPluslzSq;
		float nx1 = (a - b) / lcSqPluslzSq;
		rdCluster_UpdateClipRegionRoot(nx0, lc, lz, Radius, CameraScale, ClipMin, ClipMax);
		rdCluster_UpdateClipRegionRoot(nx1, lc, lz, Radius, CameraScale, ClipMin, ClipMax);
	}
}

static int rdCluster_ComputeClipRegion(const rdVector3* Center, float Radius, rdMatrix44* pProjection, float Near, rdVector4* ClipRegion)
{
	rdVector_Set4(ClipRegion, 1.0f, 1.0f, 0.0f, 0.0f);
	if ((Center->y + Radius) >= Near)
	{
		rdVector2 ClipMin = { -1.0f, -1.0f };
		rdVector2 ClipMax = { +1.0f, +1.0f };
		rdCluster_UpdateClipRegion(Center->x, Center->y, Radius, pProjection->vA.x, &ClipMin.x, &ClipMax.x);
		rdCluster_UpdateClipRegion(-Center->z, Center->y, Radius, pProjection->vC.y, &ClipMin.y, &ClipMax.y);
		rdVector_Set4(ClipRegion, ClipMin.x, ClipMin.y, ClipMax.x, ClipMax.y);
		return 1;
	}
	return 0;
}

static int rdCluster_ComputeBoundingBox(const rdVector3* Center, float Radius, rdMatrix44* pProjection, float Near, rdVector4* Bounds)
{
	rdVector4 bounds; 
	int clipped = rdCluster_ComputeClipRegion(Center, Radius, pProjection, Near, &bounds);

	Bounds->x = 0.5f *  bounds.x + 0.5f;
	Bounds->y = 0.5f * -bounds.w + 0.5f;
	Bounds->z = 0.5f *  bounds.z + 0.5f;
	Bounds->w = 0.5f * -bounds.y + 0.5f;

	return clipped;
}

static void rdCluster_BuildCluster(rdCluster* pCluster, int x, int y, int z, float znear, float zfar)
{
	float z0 = (float)(z + 0) / STD3D_CLUSTER_GRID_SIZE_Z;
	float z1 = (float)(z + 1) / STD3D_CLUSTER_GRID_SIZE_Z;
	z0 = znear * powf(zfar / znear, z0) / zfar; // linear 0-1
	z1 = znear * powf(zfar / znear, z1) / zfar; // linear 0-1

	float v0 = (float)(y + 0) / STD3D_CLUSTER_GRID_SIZE_Y;
	float v1 = (float)(y + 1) / STD3D_CLUSTER_GRID_SIZE_Y;

	float u0 = (float)(x + 0) / STD3D_CLUSTER_GRID_SIZE_X;
	float u1 = (float)(x + 1) / STD3D_CLUSTER_GRID_SIZE_X;

	// calculate the corners of the cluster
	rdVector3 corners[8];
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[0], u0, v0, z0);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[1], u1, v0, z0);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[2], u0, v1, z0);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[3], u0, v0, z1);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[4], u1, v1, z0);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[5], u0, v1, z1);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[6], u1, v0, z1);
	rdCamera_GetFrustumRay(rdCamera_pCurCamera, &corners[7], u1, v1, z1);

	// calculate the AABB of the cluster
	rdVector_Set3(&pCluster->minb, 1e+16f, 1e+16f, 1e+16f);
	rdVector_Set3(&pCluster->maxb, -1e+16f, -1e+16f, -1e+16f);
	for (int c = 0; c < 8; ++c)
	{
		pCluster->minb.x = fmin(pCluster->minb.x, corners[c].x);
		pCluster->minb.y = fmin(pCluster->minb.y, corners[c].y);
		pCluster->minb.z = fmin(pCluster->minb.z, corners[c].z);
		pCluster->maxb.x = fmax(pCluster->maxb.x, corners[c].x);
		pCluster->maxb.y = fmax(pCluster->maxb.y, corners[c].y);
		pCluster->maxb.z = fmax(pCluster->maxb.z, corners[c].z);
	}
}

static int rdCluster_IntersectsCluster(rdCluster_Item* pItem, rdCluster* pCluster)
{
	int intersects;
	switch (pItem->type)
	{
	case CLUSTER_ITEM_BOX:
		return rdMath_IntersectAABB_OBB(&pCluster->minb, &pCluster->maxb, &pItem->boxParams.matrix);
	case CLUSTER_ITEM_CONE:
		return rdMath_IntersectAABB_Cone(&pCluster->minb, &pCluster->maxb, &pItem->coneParams.apex, &pItem->coneParams.direction, pItem->coneParams.angle, pItem->radius);
	case CLUSTER_ITEM_SPHERE:
	default:
		return rdMath_IntersectAABB_Sphere(&pCluster->minb, &pCluster->maxb, &pItem->position,  pItem->radius);
	}
}

static void rdCluster_AssignItemToClusters(rdCluster_Item* pItem, rdMatrix44* pProjection, float znear, float zfar)
{
	// scale and bias factor for non-linear cluster distribution
	float sliceScalingFactor = (float)STD3D_CLUSTER_GRID_SIZE_Z / logf(zfar / znear);
	float sliceBiasFactor = -((float)STD3D_CLUSTER_GRID_SIZE_Z * logf(znear) / logf(zfar / znear));

	// use a tight screen space bounding rect to determine which tiles the item needs to be assigned to
	rdVector4 rect;
	int clipped = rdCluster_ComputeBoundingBox(&pItem->position, pItem->radius, pProjection, znear, &rect); // todo: this seems to be a bit expensive, would it be better to use a naive box?
	if (rect.x < rect.z && rect.y < rect.w)
	{
		// linear depth for near and far edges of the light
		float zMin = fmax(0.0f, (pItem->position.y - pItem->radius));
		float zMax = fmax(0.0f, (pItem->position.y + pItem->radius));

		// non linear depth distribution
		int zStartIndex = (int)floorf(fmax(0.0f, logf(zMin) * sliceScalingFactor + sliceBiasFactor));
		int zEndIndex = (int)ceilf(fmax(0.0f, logf(zMax) * sliceScalingFactor + sliceBiasFactor));

		int yStartIndex = (int)floorf(rect.y * (float)STD3D_CLUSTER_GRID_SIZE_Y);
		int yEndIndex = (int)ceilf(rect.w * (float)STD3D_CLUSTER_GRID_SIZE_Y);

		int xStartIndex = (int)floorf(rect.x * (float)STD3D_CLUSTER_GRID_SIZE_X);
		int xEndIndex = (int)ceilf(rect.z * (float)STD3D_CLUSTER_GRID_SIZE_X);

		if ((zStartIndex < 0 && zEndIndex < 0) || (zStartIndex >= (int)STD3D_CLUSTER_GRID_SIZE_Z && zEndIndex >= (int)STD3D_CLUSTER_GRID_SIZE_Z))
			return;

		if ((yStartIndex < 0 && yEndIndex < 0) || (yStartIndex >= (int)STD3D_CLUSTER_GRID_SIZE_Y && yEndIndex >= (int)STD3D_CLUSTER_GRID_SIZE_Y))
			return;

		if ((xStartIndex < 0 && xEndIndex < 0) || (xStartIndex >= (int)STD3D_CLUSTER_GRID_SIZE_X && xEndIndex >= (int)STD3D_CLUSTER_GRID_SIZE_X))
			return;

		zStartIndex = stdMath_ClampInt(zStartIndex, 0, STD3D_CLUSTER_GRID_SIZE_Z - 1);
		zEndIndex = stdMath_ClampInt(zEndIndex, 0, STD3D_CLUSTER_GRID_SIZE_Z - 1);

		yStartIndex = stdMath_ClampInt(yStartIndex, 0, STD3D_CLUSTER_GRID_SIZE_Y - 1);
		yEndIndex = stdMath_ClampInt(yEndIndex, 0, STD3D_CLUSTER_GRID_SIZE_Y - 1);

		xStartIndex = stdMath_ClampInt(xStartIndex, 0, STD3D_CLUSTER_GRID_SIZE_X - 1);
		xEndIndex = stdMath_ClampInt(xEndIndex, 0, STD3D_CLUSTER_GRID_SIZE_X - 1);

		for (uint32_t z = zStartIndex; z <= zEndIndex; ++z)
		{
			for (uint32_t y = yStartIndex; y <= yEndIndex; ++y)
			{
				for (uint32_t x = xStartIndex; x <= xEndIndex; ++x)
				{
					uint32_t clusterID = x + y * STD3D_CLUSTER_GRID_SIZE_X + z * STD3D_CLUSTER_GRID_SIZE_X * STD3D_CLUSTER_GRID_SIZE_Y;
					uint32_t tile_bucket_index = clusterID * STD3D_CLUSTER_BUCKETS_PER_CLUSTER;

					// note: updating the cluster bounds is by far the most expensive part of this entire thing, avoid doing it!
#ifndef USE_JOBS
					if (rdroid_lastClusterFrustumFrame != rdroid_clusterFrustumFrame)
					{
						rdCluster_BuildCluster(&rdroid_clusters[clusterID], x, y, z, znear, zfar);
						rdroid_clusters[clusterID].lastUpdateFrame = rdroid_clusterFrustumFrame;
					}
#endif
					int intersects = rdCluster_IntersectsCluster(pItem, &rdroid_clusters[clusterID]);
					if (intersects)
					{
						const uint32_t bucket_index = pItem->itemIndex / 32;
						const uint32_t bucket_place = pItem->itemIndex % 32;

						// SDL doesn't have proper atomic bit operations so fucking do it ourselves I guess
#ifdef USE_JOBS
#ifdef _WIN32
						_InterlockedOr((long*)&rdroid_clusterBits[tile_bucket_index + bucket_index], (1 << bucket_place));
#else // hopefully this is ok for Linux?
						__sync_or_and_fetch(&rdroid_clusterBits[tile_bucket_index + bucket_index], (1 << bucket_place));
#endif
#else
						rdroid_clusterBits[tile_bucket_index + bucket_index] |= (1 << bucket_place);
#endif
					}
				}
			}
		}
	}
}

static void rdCluster_BuildClustersJob(uint32_t jobIndex, uint32_t groupIndex)
{
	int z = jobIndex / (STD3D_CLUSTER_GRID_SIZE_X * STD3D_CLUSTER_GRID_SIZE_Y);
	int y = (jobIndex % (STD3D_CLUSTER_GRID_SIZE_X * STD3D_CLUSTER_GRID_SIZE_Y)) / STD3D_CLUSTER_GRID_SIZE_X;
	int x = jobIndex % STD3D_CLUSTER_GRID_SIZE_X;

	float znear = -rdroid_clusterJobProjectionMat->vD.z / (rdroid_clusterJobProjectionMat->vB.z + 1.0f);
	float zfar = -rdroid_clusterJobProjectionMat->vD.z / (rdroid_clusterJobProjectionMat->vB.z - 1.0f);

	if (rdroid_clusters[jobIndex].lastUpdateFrame != rdroid_clusterFrustumFrame)
	{
		rdCluster_BuildCluster(&rdroid_clusters[jobIndex], x, y, z, znear, zfar);
		rdroid_clusters[jobIndex].lastUpdateFrame = rdroid_clusterFrustumFrame;
	}
}

static void rdCluster_AssignLightsToClustersJob(uint32_t jobIndex, uint32_t groudIndex)
{
	if (jobIndex >= rdroid_numClusterLights)
		return;
		 
	float znear = -rdroid_clusterJobProjectionMat->vD.z / (rdroid_clusterJobProjectionMat->vB.z + 1.0f);
	float zfar = -rdroid_clusterJobProjectionMat->vD.z / (rdroid_clusterJobProjectionMat->vB.z - 1.0f);

	rdCluster_Item item;
	item.itemIndex = rdroid_clusterLightOffset + jobIndex;
	item.radius    = rdroid_clusterLights[jobIndex].falloffMin;
	rdVector_Copy4To3(&item.position, &rdroid_clusterLights[jobIndex].position);

	if (rdroid_clusterLights[jobIndex].type == RD_LIGHT_SPOTLIGHT)
	{ 
		item.type = CLUSTER_ITEM_CONE;
		item.coneParams.angle = rdroid_clusterLights[jobIndex].angleY;
		item.coneParams.apex = item.position;
		rdVector_Copy4To3(&item.coneParams.direction, &rdroid_clusterLights[jobIndex].direction_intensity);

		rdCluster_BoundingSphere(&item.position, &item.radius, &item.coneParams.apex, &item.coneParams.direction, item.radius, item.coneParams.angle);
	}
	else if(rdroid_clusterLights[jobIndex].type == RD_LIGHT_RECTANGLE)
	{
		item.type = CLUSTER_ITEM_BOX;
		rdCluster_BuildAreaLightMatrix(&rdroid_clusterLights[jobIndex], &item.position, &item.boxParams.matrix, &item.radius);
	}
	else
	{
		item.type = CLUSTER_ITEM_SPHERE;
	}

	rdCluster_AssignItemToClusters(&item, rdroid_clusterJobProjectionMat, znear, zfar);
}

static void rdCluster_AssignOccludersToClustersJob(uint32_t jobIndex, uint32_t groupIndex)
{
	if (jobIndex >= rdroid_numClusterOccluders)
		return;

	float znear = -rdroid_clusterJobProjectionMat->vD.z / (rdroid_clusterJobProjectionMat->vB.z + 1.0f);
	float zfar = -rdroid_clusterJobProjectionMat->vD.z / (rdroid_clusterJobProjectionMat->vB.z - 1.0f);

	rdCluster_Item item;
	item.type = CLUSTER_ITEM_SPHERE;
	item.itemIndex = rdroid_clusterOccluderOffset + jobIndex;
	item.radius = rdroid_clusterOccluders[jobIndex].position.w;
	rdVector_Copy4To3(&item.position, &rdroid_clusterOccluders[jobIndex].position);

	rdCluster_AssignItemToClusters(&item, rdroid_clusterJobProjectionMat, znear, zfar);
}

void rdCluster_AssignDecalsToClustersJob(uint32_t jobIndex, uint32_t groupIndex)
{
	if (jobIndex >= rdroid_numClusterDecals)
		return;

	float znear = -rdroid_clusterJobProjectionMat->vD.z / (rdroid_clusterJobProjectionMat->vB.z + 1.0f);
	float zfar = -rdroid_clusterJobProjectionMat->vD.z / (rdroid_clusterJobProjectionMat->vB.z - 1.0f);

	rdCluster_Item item;
	item.type = CLUSTER_ITEM_BOX;
	item.itemIndex = rdroid_clusterDecalOffset + jobIndex;
	item.radius = rdroid_clusterDecals[jobIndex].posRad.w;
	rdVector_Copy4To3(&item.position, &rdroid_clusterDecals[jobIndex].posRad);
	item.boxParams.matrix = rdroid_clusterDecals[jobIndex].decalMatrix;

	rdCluster_AssignItemToClusters(&item, rdroid_clusterJobProjectionMat, znear, zfar);
}

int rdCluster_AddLight(rdLight* light, rdVector3* position, rdVector3* direction, rdVector3* right, rdVector3* up, float width, float height, float intensity)
{
	if (rdroid_numClusterLights >= STD3D_CLUSTER_MAX_LIGHTS || !light->active)
		return 0;

	rdroid_clustersDirty = 1;

	rdClusterLight* clusterLight = &rdroid_clusterLights[rdroid_numClusterLights++];
	clusterLight->type = light->type;
	rdVector_Copy3To4(&clusterLight->position, position);

	rdVector_Copy3To4(&clusterLight->right, right);
	clusterLight->right.w = width;

	rdVector_Copy3To4(&clusterLight->up, up);
	clusterLight->up.w = height;

	rdVector_Copy3To4(&clusterLight->direction_intensity, direction);
	clusterLight->direction_intensity.w = light->intensity;

	clusterLight->color.x = light->color.x * intensity;
	clusterLight->color.y = light->color.y * intensity;
	clusterLight->color.z = light->color.z * intensity;
	clusterLight->color.w = fmin(light->color.x, fmin(light->color.y, light->color.z));

#ifdef JKM_LIGHTING
	clusterLight->angleX = light->angleX;
	clusterLight->cosAngleX = light->cosAngleX;
	clusterLight->angleY = light->angleY;
	clusterLight->cosAngleY = light->cosAngleY;
	clusterLight->lux = light->lux;
#endif
	clusterLight->radiusSqr = light->falloffMin * light->falloffMin;
	clusterLight->invFalloff = 1.0f / light->falloffMin;
	clusterLight->falloffMin = light->falloffMin;
	clusterLight->falloffMax = light->falloffMax;
	clusterLight->falloffType = light->falloffModel;

	return 1;
}

int rdCluster_AddPointLight(rdLight* light, rdVector3* position, float intensity)
{
	return rdCluster_AddLight(light, position, &rdroid_zeroVector3, &rdroid_zeroVector3, &rdroid_zeroVector3, 0.0f, 0.0f, intensity);
}

int rdCluster_AddSpotLight(rdLight* light, rdVector3* position, rdVector3* direction, float intensity)
{
	return rdCluster_AddLight(light, position, direction, &rdroid_zeroVector3, &rdroid_zeroVector3, 0.0f, 0.0f, intensity);
}

int rdCluster_AddRectangleLight(rdLight* light, rdVector3* position, rdVector3* direction, rdVector3* right, rdVector3* up, float width, float height, float intensity)
{
	return rdCluster_AddLight(light, position, direction, right, up, width, height, intensity);
}

int rdCluster_AddOccluder(rdVector3* position, float radius, rdVector3* verts)
{
	if (rdroid_numClusterOccluders >= STD3D_CLUSTER_MAX_OCCLUDERS)
		return 0;

	rdroid_clustersDirty = 1;

	rdClusterOccluder* occ = &rdroid_clusterOccluders[rdroid_numClusterOccluders++];
	rdVector_Copy3To4(&occ->position, position);
	occ->position.w = radius;
	occ->invRadius = 1.0f / radius;

	return 1;
}

int rdCluster_AddDecal(stdVBuffer* vbuf, rdDDrawSurface* texture, rdVector3* verts, rdMatrix44* decalMatrix, rdVector3* color, uint32_t flags, float angleFade)
{
	if (rdroid_numClusterDecals >= STD3D_CLUSTER_MAX_DECALS)
		return 0;

	rdroid_clustersDirty = 1;

	rdRectf uvScaleBias;
	if (!std3D_UploadDecalTexture(&uvScaleBias, vbuf, texture))
		return 0;

	rdClusterDecal* decal = &rdroid_clusterDecals[rdroid_numClusterDecals++];
	decal->uvScaleBias.x = uvScaleBias.x;
	decal->uvScaleBias.y = uvScaleBias.y;
	decal->uvScaleBias.z = uvScaleBias.width;
	decal->uvScaleBias.w = uvScaleBias.height;
	rdVector_Copy3((rdVector3*)&decal->posRad, (rdVector3*)&decalMatrix->vD);

	//rdVector3 diag;
	//diag.x = decalMatrix->vA.x;
	//diag.y = decalMatrix->vB.y;
	//diag.z = decalMatrix->vC.z;
	//decal->posRad.w = rdVector_Len3(&diag);
	decal->posRad.w = rdVector_Len3(verts) * 0.5f;

	rdMatrix_Copy44(&decal->decalMatrix, decalMatrix);
	rdMatrix_Invert44(&decal->invDecalMatrix, decalMatrix);

	rdVector_Copy3((rdVector3*)&decal->color, color);
	decal->color.w = 1.0f;
	decal->flags = flags;
	decal->angleFade = angleFade;

	return 1;
}


void rdCluster_Build(rdMatrix44* pProjection, uint32_t width, uint32_t height)
{
	STD_BEGIN_PROFILER_LABEL();

	// todo: right now this forces all clusters to update every frame
	// that kind sucks and it would be icer if we could just update it if the FOV changes
	// but right now the FP view and main view share clusters while I handle the refactor
	rdroid_clusterFrustumFrame++;

	// pull the near/far from the projection matrix
	// note: common sources list this as [3][2] and [2][2] but we have a rotated projection matrix, so we use [1][2]
	float znear = -pProjection->vD.z / (pProjection->vB.z + 1.0f);
	float zfar = -pProjection->vD.z / (pProjection->vB.z - 1.0f);

	// scale and bias factor for non-linear cluster distribution
	float sliceScalingFactor = (float)STD3D_CLUSTER_GRID_SIZE_Z / logf(zfar / znear);
	float sliceBiasFactor = -((float)STD3D_CLUSTER_GRID_SIZE_Z * logf(znear) / logf(zfar / znear));

	// ratio of tile to pixel
	uint32_t tileSizeX = (uint32_t)ceilf((float)width / (float)STD3D_CLUSTER_GRID_SIZE_X);
	uint32_t tileSizeY = (uint32_t)ceilf((float)height / (float)STD3D_CLUSTER_GRID_SIZE_Y);

	// nothing to build
	if (!rdroid_numClusterLights && !rdroid_numClusterOccluders && !rdroid_numClusterDecals)
	{
		memset(rdroid_clusterBits, 0, sizeof(rdroid_clusterBits));
		std3D_SendClusterBitsToHardware(rdroid_clusterBits, znear, zfar, tileSizeX, tileSizeY);
		return;
	}

	rdroid_clusterJobProjectionMat = pProjection;

	rdroid_clustersDirty = 0;

#ifdef USE_JOBS

	rdroid_clusterLightOffset    = 0;
	rdroid_clusterOccluderOffset = rdroid_clusterLightOffset + rdroid_numClusterLights;
	rdroid_clusterDecalOffset    = rdroid_clusterOccluderOffset + rdroid_numClusterOccluders;

	// todo: do we need to atomically clear this?
	// maybe this should also be in a dispatch
	//for (int i = 0; i < ARRAY_SIZE(std3D_clusterBits); ++i)
		//SDL_AtomicSet(&std3D_clusterBits[i], 0);
	memset(rdroid_clusterBits, 0, sizeof(rdroid_clusterBits));

	// build all clusters in parallel
	if (rdroid_lastClusterFrustumFrame != rdroid_clusterFrustumFrame)
	{
		stdJob_Dispatch(STD3D_CLUSTER_GRID_SIZE_XYZ, 64, rdCluster_BuildClustersJob);

		// wait for clusters to finish building
		stdJob_Wait();

		rdroid_lastClusterFrustumFrame = rdroid_clusterFrustumFrame;
	}

	// now assign items to clusters in parallel
	if (rdroid_numClusterLights)
		stdJob_Dispatch(rdroid_numClusterLights, 8, rdCluster_AssignLightsToClustersJob);

	if (rdroid_numClusterOccluders)
		stdJob_Dispatch(rdroid_numClusterOccluders, 8, rdCluster_AssignOccludersToClustersJob);

	if (rdroid_numClusterDecals)
		stdJob_Dispatch(rdroid_numClusterDecals, 8, rdCluster_AssignDecalsToClustersJob);

	// wait on the result before uploading to GPU
	stdJob_Wait();
#else
	// clean slate
	memset(rdroid_clusterBits, 0, sizeof(rdroid_clusterBits));

	// assign lights
	rdroid_clusterLightOffset = 0;
	for (int i = 0; i < rdroid_numClusterLights; ++i)
		rdCluster_AssignLightsToClustersJob(i, 0);

	// assign occluders
	rdroid_clusterOccluderOffset = rdroid_clusterLightOffset + rdroid_numClusterLights;
	for (int i = 0; i < rdroid_numClusterOccluders; ++i)
		rdCluster_AssignOccludersToClustersJob(i, 0);

	// assign decals
	rdroid_clusterDecalOffset = rdroid_clusterOccluderOffset + rdroid_numClusterLights;
	for (int i = 0; i < rdroid_numClusterDecals; ++i)
		rdCluster_AssignDecalsToClustersJob(i, 0);
#endif

	std3D_SendLightsToHardware(rdroid_clusterLights, rdroid_clusterLightOffset, rdroid_numClusterLights);
	std3D_SendOccludersToHardware(rdroid_clusterOccluders, rdroid_clusterOccluderOffset, rdroid_numClusterOccluders);
	std3D_SendDecalsToHardware(rdroid_clusterDecals, rdroid_clusterDecalOffset, rdroid_numClusterDecals);
	std3D_SendClusterBitsToHardware(rdroid_clusterBits, znear, zfar, tileSizeX, tileSizeY);

	STD_END_PROFILER_LABEL();
}


#endif
