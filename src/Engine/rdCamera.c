#include "rdCamera.h"

#include "Engine/rdLight.h"
#include "jk.h"
#include "Engine/rdroid.h"
#include "General/stdMath.h"
#include "Win95/stdDisplay.h"
#include "Platform/std3D.h"
#include "Engine/sithRender.h"
#include "World/jkPlayer.h"

static rdVector3 rdCamera_camRotation;
static flex_t rdCamera_mipmapScalar = 1.0; // MOTS added

rdCamera* rdCamera_New(flex_t fov, flex_t x, flex_t y, flex_t z, flex_t aspectRatio)
{
    rdCamera* out = (rdCamera *)rdroid_pHS->alloc(sizeof(rdCamera));
    if ( !out ) {
        return 0;
    }
    
    // Added: zero out alloc
    memset(out, 0, sizeof(*out));

    rdCamera_NewEntry(out, fov, x, y, z, aspectRatio);    
    
    return out;
}

int rdCamera_NewEntry(rdCamera *camera, flex_t fov, BOOL bClipFar, flex_t zNear, flex_t zFar, flex_t aspectRatio)
{
    if (!camera)
        return 0;

    // Added: Don't double-alloc
    if (!camera->pClipFrustum)
    {
        camera->pClipFrustum = (rdClipFrustum *)rdroid_pHS->alloc(sizeof(rdClipFrustum));
    }

    if ( camera->pClipFrustum )
    {
        camera->canvas = 0;
        rdCamera_SetFOV(camera, fov);
        rdCamera_SetOrthoScale(camera, 1.0);

        camera->pClipFrustum->bClipFar = bClipFar;
        camera->pClipFrustum->zNear = zNear;
        camera->pClipFrustum->zFar = zFar;
        camera->screenAspectRatio = aspectRatio;
	#ifdef RGB_AMBIENT
		rdVector_Zero3(&camera->ambientLight);
		rdAmbient_Zero(&camera->ambientSH);
	#else
        camera->ambientLight = 0.0;
	#endif
        camera->numLights = 0;
        camera->attenuationMin = 0.2;
        camera->attenuationMax = 0.1;
        
        rdCamera_SetProjectType(camera, rdCameraProjectType_Perspective);

        return 1;
    }
    return 0;
}

void rdCamera_Free(rdCamera *camera)
{
    if (camera)
    {
        rdCamera_FreeEntry(camera);
        rdroid_pHS->free(camera);
    }
}

void rdCamera_FreeEntry(rdCamera *camera)
{
    if ( camera->pClipFrustum ) {
        rdroid_pHS->free(camera->pClipFrustum);
        camera->pClipFrustum = NULL; // Added: no UAF
    }
}

int rdCamera_SetCanvas(rdCamera *camera, rdCanvas *canvas)
{
    camera->canvas = canvas;
    rdCamera_BuildFOV(camera);
    return 1;
}

int rdCamera_SetCurrent(rdCamera *camera)
{
    if ( rdCamera_pCurCamera != camera )
        rdCamera_pCurCamera = camera;
    return 1;
}

extern int jkGuiBuildMulti_bRendering;
int rdCamera_SetFOV(rdCamera *camera, flex_t fovVal)
{
    if ( fovVal < 5.0 )
    {
        fovVal = 5.0;
    }
    else if ( fovVal > 179.0 )
    {
        fovVal = 179.0;
    }

#ifdef QOL_IMPROVEMENTS
    if (!jkGuiBuildMulti_bRendering && jkPlayer_fovIsVertical && camera->screenAspectRatio != 0.0) {
        camera->fov = stdMath_ArcTan3(1.0, stdMath_Tan(fovVal * 0.5) / camera->screenAspectRatio) * -2.0;
        
        if ( camera->fov < 5.0 )
        {
            camera->fov = 5.0;
        }
        else if ( camera->fov > 179.0 )
        {
            camera->fov = 179.0;
        }
    }
    else
#endif
    {
        camera->fov = fovVal;
    }     
    
    rdCamera_BuildFOV(camera);
    return 1;
}

int rdCamera_SetProjectType(rdCamera *camera, int type)
{
    camera->projectType = type;
    
    switch (type)
    {
        case rdCameraProjectType_Ortho:
        {
            if (camera->screenAspectRatio == 1.0 )
            {
                camera->fnProject = rdCamera_OrthoProjectSquare;
                camera->fnProjectLst = rdCamera_OrthoProjectSquareLst;
            }
            else
            {
                camera->fnProject = rdCamera_OrthoProject;
                camera->fnProjectLst = rdCamera_OrthoProjectLst;
            }
            break;
        }
        case rdCameraProjectType_Perspective:
        {
            if (camera->screenAspectRatio == 1.0)
            {
                camera->fnProject = rdCamera_PerspProjectSquare;
                camera->fnProjectLst = rdCamera_PerspProjectSquareLst;
            }
            else
            {
                camera->fnProject = rdCamera_PerspProject;
                camera->fnProjectLst = rdCamera_PerspProjectLst;
            }
            break;
        }
        
    }

    if ( camera->canvas )
        rdCamera_BuildFOV(camera);

    return 1;
}

int rdCamera_SetOrthoScale(rdCamera *camera, flex_t scale)
{
    camera->orthoScale = scale;
    rdCamera_BuildFOV(camera);
    return 1;
}

int rdCamera_SetAspectRatio(rdCamera *camera, flex_t ratio)
{
#ifdef QOL_IMPROVEMENTS
    if (jkPlayer_enableOrigAspect) ratio = 1.0;
#endif

    camera->screenAspectRatio = ratio;
    return rdCamera_SetProjectType(camera, camera->projectType);
}

int rdCamera_BuildFOV(rdCamera *camera)
{
    flex_d_t v10; // st3
    flex_d_t v15; // st4
    flex_t camerac; // [esp+1Ch] [ebp+4h]

    rdClipFrustum* clipFrustum = camera->pClipFrustum;
    rdCanvas* canvas = camera->canvas;
    if ( !canvas )
        return 0;

#ifdef RENDER_DROID2
	clipFrustum->x = (float)canvas->xStart / (float)(canvas->widthMinusOne + 1);
	clipFrustum->y = (float)canvas->yStart / (float)(canvas->heightMinusOne + 1);
	clipFrustum->width = 1.0f;
	clipFrustum->height = 1.0f;
#endif

    switch (camera->projectType)
    {
        case rdCameraProjectType_Ortho:
        {
            camera->fov_y = 0.0;
            camerac = ((flex_d_t)(canvas->heightMinusOne - canvas->yStart) * 0.5) / camera->orthoScale;
            v15 = ((flex_d_t)(canvas->widthMinusOne - canvas->xStart) * 0.5) / camera->orthoScale;
            clipFrustum->orthoLeft = -v15;
            clipFrustum->orthoTop = camerac / camera->screenAspectRatio;
            clipFrustum->orthoRight = v15;
            clipFrustum->orthoBottom = -camerac / camera->screenAspectRatio;
            clipFrustum->farTop = 0.0;
            clipFrustum->bottom = 0.0;
            clipFrustum->farLeft = 0.0;
            clipFrustum->right = 0.0;
            return 1;
        }
        
        case rdCameraProjectType_Perspective:
        {
#ifdef TARGET_TWL
            flex_t overdraw = 0.0;
#elif defined(QOL_IMPROVEMENTS)
            flex_t overdraw = 1.0; // Added: HACK for 1px off on the bottom of the screen
#else
            flex_t overdraw = 0.0;
#endif
            flex_t width = canvas->xStart;
            flex_t height = canvas->yStart;
            flex_t project_width_half = overdraw + (canvas->widthMinusOne - (flex_d_t)width) * 0.5;
            flex_t project_height_half = overdraw + (canvas->heightMinusOne - (flex_d_t)height) * 0.5;
            
            flex_t project_width_half_2 = project_width_half;
            flex_t project_height_half_2 = project_height_half;
            
            camera->fov_y = project_width_half / stdMath_Tan(camera->fov * 0.5);

            flex_t fov_calc = camera->fov_y;
            flex_t fov_calc_height = camera->fov_y;

#ifdef QOL_IMPROVEMENTS
            if (jkPlayer_enableOrigAspect)
                fov_calc_height = camera->fov_y * camera->screenAspectRatio;
#endif

            // UBSAN fixes
            if (fov_calc_height == 0) {
                fov_calc_height = 0.000001;
            }
            if (fov_calc == 0) {
                fov_calc = 0.000001;
            }

            clipFrustum->farTop = project_height_half / fov_calc_height; // far top
            clipFrustum->farLeft = -project_width_half / fov_calc; // far left
            clipFrustum->bottom = -project_height_half_2 / fov_calc_height; // bottom
            clipFrustum->right = project_width_half_2 / fov_calc; // right
            clipFrustum->nearTop = (project_height_half - -1.0) / fov_calc_height; // near top
            clipFrustum->nearLeft = -(project_width_half - -1.0) / fov_calc; // near left
            return 1;
        }
    }

    return 1;
}

int rdCamera_BuildClipFrustum(rdCamera *camera, rdClipFrustum *outClip, signed int minX, signed int minY, signed int maxX, signed int maxY)
{   
    //jk_printf("%u %u %u %u\n", height, width, height2, width2);

    rdClipFrustum* cameraClip = camera->pClipFrustum;
    rdCanvas* canvas = camera->canvas;
    if ( !canvas )
        return 0;

#ifdef RENDER_DROID2
	outClip->x = (float)minX / (float)(canvas->widthMinusOne + 1);
	outClip->y = (float)minY / (float)(canvas->heightMinusOne + 1);
	outClip->width = (float)(height2 - minX) / (float)(canvas->widthMinusOne + 1);
	outClip->height = (float)(minY - width) / (float)(canvas->heightMinusOne + 1);
#endif

#ifdef TARGET_TWL
    flex_t overdraw = 0.0;
#elif defined(QOL_IMPROVEMENTS)
    flex_t overdraw = 1.0; // Added: HACK for 1px off on the bottom of the screen
#else
    flex_t overdraw = 0.0;
#endif
    flex_t project_width_half = overdraw + canvas->half_screen_height - ((flex_d_t)minY - 0.5);
    flex_t project_height_half = overdraw + canvas->half_screen_width - ((flex_d_t)minX - 0.5);
    
    flex_t project_width_half_2 = -canvas->half_screen_height + ((flex_d_t)maxY - 0.5);
    flex_t project_height_half_2 = -canvas->half_screen_width + ((flex_d_t)maxX - 0.5);

    outClip->bClipFar = cameraClip->bClipFar;
    outClip->zNear = cameraClip->zNear;
    outClip->zFar = cameraClip->zFar;
    
    flex_t fov_calc = camera->fov_y;
    flex_t fov_calc_height = camera->fov_y;

#ifdef QOL_IMPROVEMENTS
    if (jkPlayer_enableOrigAspect)
        fov_calc_height = camera->fov_y * camera->screenAspectRatio;
#endif

    // UBSAN fixes
    if (fov_calc_height == 0) {
        fov_calc_height = 0.000001;
    }
    if (fov_calc == 0) {
        fov_calc = 0.000001;
    }

    outClip->farTop = project_width_half / fov_calc_height;
    outClip->farLeft = -project_height_half / fov_calc;
    outClip->bottom = -project_width_half_2 / fov_calc_height;
    outClip->right = project_height_half_2 / fov_calc;
    outClip->nearTop = (project_width_half - -1.0) / fov_calc_height;
    outClip->nearLeft = -(project_height_half - -1.0) / fov_calc;

#ifdef QOL_IMPROVEMENTS
    outClip->minX = minX;
    outClip->minY = minY;
    outClip->maxX = maxX;
    outClip->maxY = maxY;
#endif

    return 1;
}

void rdCamera_Update(rdMatrix34 *orthoProj)
{
#ifdef RENDER_DROID2
	rdMatrix_Copy34(&rdCamera_pCurCamera->prev_view_matrix, &rdCamera_pCurCamera->view_matrix);
#endif

    rdMatrix_InvertOrtho34(&rdCamera_pCurCamera->view_matrix, orthoProj);
    rdMatrix_Copy34(&rdCamera_camMatrix, orthoProj);
    rdMatrix_ExtractAngles34(&rdCamera_camMatrix, &rdCamera_camRotation);
#if defined(VIEW_SPACE_GBUFFER) || defined(RENDER_DROID2)
	rdCamera_GetFrustumCornerRays(rdCamera_pCurCamera, orthoProj, &rdCamera_pCurCamera->pClipFrustum->lt, &rdCamera_pCurCamera->pClipFrustum->rt, &rdCamera_pCurCamera->pClipFrustum->lb, &rdCamera_pCurCamera->pClipFrustum->rb);
#endif
#ifdef RENDER_DROID2
	rdMatrixMode(RD_MATRIX_VIEW);
	rdIdentity();

#ifdef MOTION_BLUR
	// init to cur frame, only pov model will use this
	rdMatrixMode(RD_MATRIX_VIEW_PREV);
	rdLoadMatrix34(&rdCamera_pCurCamera->view_matrix);
#endif
#endif
}

void rdCamera_OrthoProject(rdVector3* out, rdVector3* v)
{
    //rdCamera_pCurCamera->orthoScale = 200.0;

    out->x = rdCamera_pCurCamera->orthoScale * v->x + rdCamera_pCurCamera->canvas->half_screen_width;
    out->y = -(v->z * rdCamera_pCurCamera->orthoScale) * rdCamera_pCurCamera->screenAspectRatio + rdCamera_pCurCamera->canvas->half_screen_height;
    out->z = v->y * rdCamera_pCurCamera->orthoScale;

    //printf("%f %f %f -> %f %f %f\n", v->x, v->y, v->z, out->x, out->y, out->z);
}

void rdCamera_OrthoProjectLst(rdVector3 *vertices_out, rdVector3 *vertices_in, unsigned int num_vertices)
{
    for (int i = 0; i < num_vertices; i++)
    {
        rdCamera_OrthoProject(vertices_out, vertices_in);
        ++vertices_in;
        ++vertices_out;
    }
}

void rdCamera_OrthoProjectSquare(rdVector3 *out, rdVector3 *v)
{
    out->x = rdCamera_pCurCamera->orthoScale * v->x + rdCamera_pCurCamera->canvas->half_screen_width;
    out->y = rdCamera_pCurCamera->canvas->half_screen_height - v->z * rdCamera_pCurCamera->orthoScale;
    out->z = v->y;
}

void rdCamera_OrthoProjectSquareLst(rdVector3 *vertices_out, rdVector3 *vertices_in, unsigned int num_vertices)
{
    for (int i = 0; i < num_vertices; i++)
    {
        rdCamera_OrthoProjectSquare(vertices_out, vertices_in);
        ++vertices_in;
        ++vertices_out;
    }
}

void rdCamera_PerspProject(rdVector3 *out, rdVector3 *v)
{
    flex_t fov_y_calc = (rdCamera_pCurCamera->fov_y / v->y);
    out->x = rdCamera_pCurCamera->canvas->half_screen_width + (v->x * fov_y_calc);
    out->y = rdCamera_pCurCamera->canvas->half_screen_height - (jkPlayer_enableOrigAspect ? rdCamera_pCurCamera->screenAspectRatio : (flex_t)1.0) * fov_y_calc * v->z;
    out->z = v->y;

    //printf("%f %f %f -> %f %f %f\n", v->x, v->y, v->z, out->x, out->y, out->z);
}

void rdCamera_PerspProjectLst(rdVector3 *vertices_out, rdVector3 *vertices_in, unsigned int num_vertices)
{
    for (int i = 0; i < num_vertices; i++)
    {
        rdCamera_PerspProject(vertices_out, vertices_in);
        ++vertices_in;
        ++vertices_out;
    }
}

void rdCamera_PerspProjectSquare(rdVector3 *out, rdVector3 *v)
{
    flex_t fov_y_calc = (rdCamera_pCurCamera->fov_y / v->y);
    out->x = rdCamera_pCurCamera->canvas->half_screen_width + (v->x * fov_y_calc);
    out->y = rdCamera_pCurCamera->canvas->half_screen_height - (v->z * fov_y_calc);
    out->z = v->y;
}

void rdCamera_PerspProjectSquareLst(rdVector3 *vertices_out, rdVector3 *vertices_in, unsigned int num_vertices)
{
    for (int i = 0; i < num_vertices; i++)
    {
        rdCamera_PerspProjectSquare(vertices_out, vertices_in);
        ++vertices_in;
        ++vertices_out;
    }
}

#ifdef RGB_AMBIENT
void rdCamera_SetAmbientLight(rdCamera* camera, rdVector3* amt)
{
	rdVector_Copy3(&camera->ambientLight, amt);
}
void rdCamera_SetDirectionalAmbientLight(rdCamera* camera, rdAmbient* ambientCube)
{
	rdAmbient_Copy(&camera->ambientSH, ambientCube);
}
#else
void rdCamera_SetAmbientLight(rdCamera *camera, flex_t amt)
{
    camera->ambientLight = amt;
}
#endif

void rdCamera_SetAttenuation(rdCamera *camera, flex_t minVal, flex_t maxVal)
{
    int numLights; // edx
    rdLight **v4; // ecx
    rdLight *v5; // eax

    numLights = camera->numLights;
    camera->attenuationMax = maxVal;
    camera->attenuationMin = minVal;
    if ( numLights )
    {
        v4 = camera->lights;
        do
        {
            v5 = *v4++;
            --numLights;
            v5->falloffMin = v5->intensity / minVal;
            v5->falloffMax = v5->intensity / maxVal;
        }
        while ( numLights );
    }
}

#ifdef RENDER_DROID2

int rdCamera_AddLightExplicitRadius(rdCamera* camera, rdLight* light, float radius, rdVector3* lightPos)
{
	if (camera->numLights >= RDCAMERA_MAX_LIGHTS)
		return 0;

	camera->lights[camera->numLights] = light;
	light->type = RD_LIGHT_POINTLIGHT;
	light->falloffMin = radius;
	light->falloffMax = radius;
	light->falloffModel = RD_FALLOFF_QUADRATIC;

	light->id = camera->numLights;
	rdVector_Copy3(&camera->lightPositions[camera->numLights], lightPos);

	++camera->numLights;
	return 1;
}

#endif

int rdCamera_AddLight(rdCamera *camera, rdLight *light, rdVector3 *lightPos)
{
    //sithRender_RenderDebugLight(light->intensity * 10.0, lightPos);
    if ( camera->numLights >= RDCAMERA_MAX_LIGHTS)
        return 0;

    camera->lights[camera->numLights] = light;

	light->type = RD_LIGHT_POINTLIGHT;
#ifdef RENDER_DROID2
	light->falloffModel = RD_FALLOFF_DEFAULT;
#endif
    light->id = camera->numLights;
    rdVector_Copy3(&camera->lightPositions[camera->numLights], lightPos);
    light->falloffMin = light->intensity / camera->attenuationMin;
    light->falloffMax = light->intensity / camera->attenuationMax;

    ++camera->numLights;
    return 1;
}

int rdCamera_ClearLights(rdCamera *camera)
{
    camera->numLights = 0;
    return 1;
}

void rdCamera_AdvanceFrame()
{
    rdCanvas *v0; // eax
    rdRect a4; // [esp+0h] [ebp-10h] BYREF

    v0 = rdCamera_pCurCamera->canvas;
#ifdef TILE_SW_RASTER
	// eebs: fixme
	v0->bIdk |= 4;
	//stdDisplay_VBufferFill(v0->d3d_vbuf, 0xFFFFFFFF, 0);
#else
	if ( (rdroid_curRenderOptions & RD_CLEAR_BUFFERS) != 0 && (v0->bIdk & 2) != 0 )
    {
        if ( rdroid_curAcceleration <= 0 )
        {
            if ( (v0->bIdk & 1) != 0 )
            {
                a4.x = v0->xStart;
                a4.y = v0->yStart;
                a4.width = v0->widthMinusOne - v0->xStart + 1;
                a4.height = v0->heightMinusOne - v0->yStart + 1;
                stdDisplay_VBufferFill(v0->d3d_vbuf, 0, &a4);
            }
            else
            {
                stdDisplay_VBufferFill(v0->d3d_vbuf, 0, 0);
            }
        }
        else
        {
            std3D_ClearZBuffer();
        }
    }
#endif
}

// MOTS added
flex_t rdCamera_GetMipmapScalar()
{
    return rdCamera_mipmapScalar;
}

// MOTS added
void rdCamera_SetMipmapScalar(flex_t val)
{
    rdCamera_mipmapScalar = val;
}

#if defined(VIEW_SPACE_GBUFFER) || defined(RENDER_DROID2)
void rdCamera_GetFrustumCornerRays(rdCamera* camera, rdMatrix34* camMat, rdVector3* lt, rdVector3* rt, rdVector3* lb, rdVector3* rb)
{
	// todo: do all this math in view space to avoid the view -> world -> view transforms
	float fovRad = camera->fov * M_PI / 180.0f;
	float aspect = camera->screenAspectRatio;
	float wNear = 2.0f * tanf(fovRad / 2.0f) * camera->pClipFrustum->field_0.y;
	float hNear = wNear * aspect;
	float wFar = 2.0f * tanf(fovRad / 2.0f) * camera->pClipFrustum->field_0.z;
	float hFar = wFar * aspect;

	rdVector3 cNear;
	rdVector_Copy3(&cNear, &camMat->scale);
	rdVector_MultAcc3(&cNear, &camMat->lvec, camera->pClipFrustum->field_0.y);

	rdVector3 cFar;
	rdVector_Copy3(&cFar, &camMat->scale);
	rdVector_MultAcc3(&cFar, &camMat->lvec, camera->pClipFrustum->field_0.z);

	rdVector3 nearTopLeft;
	nearTopLeft.x = (cNear.x + (camMat->uvec.x * hNear / 2.0f)) - (camMat->rvec.x * wNear / 2.0f);
	nearTopLeft.y = (cNear.y + (camMat->uvec.y * hNear / 2.0f)) - (camMat->rvec.y * wNear / 2.0f);
	nearTopLeft.z = (cNear.z + (camMat->uvec.z * hNear / 2.0f)) - (camMat->rvec.z * wNear / 2.0f);

	rdVector3 nearTopRight;
	nearTopRight.x = (cNear.x + (camMat->uvec.x * hNear / 2.0f)) + (camMat->rvec.x * wNear / 2.0f);
	nearTopRight.y = (cNear.y + (camMat->uvec.y * hNear / 2.0f)) + (camMat->rvec.y * wNear / 2.0f);
	nearTopRight.z = (cNear.z + (camMat->uvec.z * hNear / 2.0f)) + (camMat->rvec.z * wNear / 2.0f);

	rdVector3 nearBottomLeft;
	nearBottomLeft.x = (cNear.x - (camMat->uvec.x * hNear / 2.0f)) - (camMat->rvec.x * wNear / 2.0f);
	nearBottomLeft.y = (cNear.y - (camMat->uvec.y * hNear / 2.0f)) - (camMat->rvec.y * wNear / 2.0f);
	nearBottomLeft.z = (cNear.z - (camMat->uvec.z * hNear / 2.0f)) - (camMat->rvec.z * wNear / 2.0f);

	rdVector3 nearBottomRight;
	nearBottomRight.x = (cNear.x - (camMat->uvec.x * hNear / 2.0f)) + (camMat->rvec.x * wNear / 2.0f);
	nearBottomRight.y = (cNear.y - (camMat->uvec.y * hNear / 2.0f)) + (camMat->rvec.y * wNear / 2.0f);
	nearBottomRight.z = (cNear.z - (camMat->uvec.z * hNear / 2.0f)) + (camMat->rvec.z * wNear / 2.0f);

	rdVector3 farTopLeft;
	farTopLeft.x = ((cFar.x + (camMat->uvec.x * hFar / 2.0f)) - (camMat->rvec.x * wFar / 2.0f));
	farTopLeft.y = ((cFar.y + (camMat->uvec.y * hFar / 2.0f)) - (camMat->rvec.y * wFar / 2.0f));
	farTopLeft.z = ((cFar.z + (camMat->uvec.z * hFar / 2.0f)) - (camMat->rvec.z * wFar / 2.0f));

	rdVector3 farTopRight;
	farTopRight.x = ((cFar.x + (camMat->uvec.x * hFar / 2.0f)) + (camMat->rvec.x * wFar / 2.0f));
	farTopRight.y = ((cFar.y + (camMat->uvec.y * hFar / 2.0f)) + (camMat->rvec.y * wFar / 2.0f));
	farTopRight.z = ((cFar.z + (camMat->uvec.z * hFar / 2.0f)) + (camMat->rvec.z * wFar / 2.0f));

	rdVector3 farBottomLeft;
	farBottomLeft.x = ((cFar.x - (camMat->uvec.x * hFar / 2.0f)) - (camMat->rvec.x * wFar / 2.0f));
	farBottomLeft.y = ((cFar.y - (camMat->uvec.y * hFar / 2.0f)) - (camMat->rvec.y * wFar / 2.0f));
	farBottomLeft.z = ((cFar.z - (camMat->uvec.z * hFar / 2.0f)) - (camMat->rvec.z * wFar / 2.0f));

	rdVector3 farBottomRight;
	farBottomRight.x = ((cFar.x - (camMat->uvec.x * hFar / 2.0f)) + (camMat->rvec.x * wFar / 2.0f));
	farBottomRight.y = ((cFar.y - (camMat->uvec.y * hFar / 2.0f)) + (camMat->rvec.y * wFar / 2.0f));
	farBottomRight.z = ((cFar.z - (camMat->uvec.z * hFar / 2.0f)) + (camMat->rvec.z * wFar / 2.0f));

	rdVector_Sub3(lt, &farTopLeft, &nearTopLeft);
	rdVector_Sub3(rt, &farTopRight, &nearTopRight);
	rdVector_Sub3(lb, &farBottomLeft, &nearBottomLeft);
	rdVector_Sub3(rb, &farBottomRight, &nearBottomRight);

	// rotate to view space
	rdMatrix_TransformVector34Acc(lt, &camera->view_matrix);
	rdMatrix_TransformVector34Acc(rt, &camera->view_matrix);
	rdMatrix_TransformVector34Acc(lb, &camera->view_matrix);
	rdMatrix_TransformVector34Acc(rb, &camera->view_matrix);
}

void rdCamera_GetFrustumRay(rdCamera* camera, rdVector3* result, float u, float v, float depth)
{
	result->x = ((1.0 - u - v) * camera->pClipFrustum->lb.x + (u * camera->pClipFrustum->rb.x + (v * camera->pClipFrustum->lt.x)));
	result->y = ((1.0 - u - v) * camera->pClipFrustum->lb.y + (u * camera->pClipFrustum->rb.y + (v * camera->pClipFrustum->lt.y)));
	result->z = ((1.0 - u - v) * camera->pClipFrustum->lb.z + (u * camera->pClipFrustum->rb.z + (v * camera->pClipFrustum->lt.z)));

	rdVector_Scale3Acc(result, depth);
}

#endif