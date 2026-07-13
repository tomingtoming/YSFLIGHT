#ifndef FSOPENGL_IS_INCLUDED
#define FSOPENGL_IS_INCLUDED
/* { */

#include <ysviewcontrol.h>
#include "fsdef.h"

class FsProjection
{
public:
	YsProjectionTransformation::PROJMODE prjMode;

//   |    /
//   |FOV/
//   |  /
//   | /
//   |/
//   V
	double tanFov;      // Tangent of field of view (FOV)
	double prjPlnDist;  // =scrnwidth/(2*tanFov)
	double nearz,farz;
	YsVec2i viewportDim;

	int cx,cy;

protected:
	mutable YSBOOL matrixCached;
	mutable YsMatrix4x4 projMatCache;

public:
	FsProjection();
	void CacheMatrix(void) const;
	void UncacheMatrix(void) const;
	const YsMatrix4x4 &GetMatrix(void) const;
};

extern unsigned int FS_GL_FONT_BITMAP_BASE; // 256 lists
extern unsigned int FS_GL_WIRE_BITMAP_BASE; // 256 lists


const char *FsMainWindowTitle(void);


void FsInitializeOpenGL(void);
void FsReinitializeOpenGL(void);
void FsUninitializeOpenGL(void);

YSBOOL FsIsConsoleServer(void);

YSBOOL FsIsShadowMapAvailable(void);

/*! Returns YSTRUE if point sprite is available.
    As of 2018/04/06, only OpenGL 2.0 library returns YSTRUE.  D3D9, OpenGL 1.1 library return YSFALSE.
*/
YSBOOL FsIsPointSpriteAvailable(void);

void FsClearScreenAndZBuffer(const YsColor &clearColor);
void FsClearStencilBuffer(void);
void FsSetPerPixelShading(YSBOOL perPix);
void FsSetPointLight(const YsVec3 &cameraPosition,const YsVec3 &lightPosition,FSENVIRONMENT env);
void FsSetDirectionalLight(const YsVec3 &cameraPosition,const YsVec3 &lightDirection,FSENVIRONMENT env);
void FsFogOn(const YsColor &fogColor,const double &visibility);
void FsFogOff(void);

void FsBeginRenderShadowMap(const YsMatrix4x4 &projTfm,const YsMatrix4x4 &viewTfm,int texWid,int texHei);
void FsEndRenderShadowMap(void);
void FsEnableShadowMap(
    const YsMatrix4x4 &viewTfm,const YsMatrix4x4 &shadowProjTfm,const YsMatrix4x4 &shadowViewTfm,
    int samplerIdent,int shadowMapIdent);
void FsDisableShadowMap(int samplerIdent,int shadowMapIdent);

void FsSetSceneProjection(const class FsProjection &prj);
void FsSet2DDrawing(void);

// VR single-pass-stereo HUD composite (see fsopengl2.0.cpp).  FsVrBeginHudRender
// / FsVrEndHudRender bracket the off-screen 2D HUD pass into the two-layer
// multiview HUD framebuffer; FsVrDrawHudQuad composites that texture array onto
// a cockpit-anchored world-space quad (corner = 4 x vec3, BL,BR,TR,TL).
void FsVrBeginHudRender(void);
void FsVrEndHudRender(void);
void FsVrDrawHudQuad(const float corner[12]);

// VR single-pass-stereo collimated gunsight reticle (see fsopengl2.0.cpp).
// Draws a world-space cross (lineVtx = 8 x vec3, 4 GL_LINES segments) far along
// the boresight through each eye's own cached stereo projection, so it reads as
// collimated at optical infinity instead of a fixed-distance flat quad.  Drawn
// with the shared VariColor3D line renderer, blending on, depth test off, fog
// off; replaces the gun crosshair that used to be baked into the flat HUD glass.
void FsVrDrawReticle(const float lineVtx[24],const YsColor &col);

// VR single-pass-stereo hand-held HOTAS prop draw bracket (see
// fsopengl2.0.cpp).  Wraps a call to FsFlightControl::DrawJoystick/
// DrawThrottle (an ordinary FsVisualDnm::Draw, NOT the raw-vertex world-space
// path the reticle/tint above use) so the model cannot be swallowed by
// nearer cockpit geometry already in the depth buffer: re-establishes the
// scene's cached stereo projection + eye-0 viewport (same restore
// FsVrDrawReticle performs, in case the HUD/GUI off-screen passes left them
// pointed at a texture-sized ortho setup instead) and disables depth
// testing, saving the previous state.  Always pair Begin with End.
void FsVrBeginHandPropDraw(void);
void FsVrEndHandPropDraw(void);

// VR single-pass-stereo in-flight-GUI-dialog composite (see fsopengl2.0.cpp).
// Same bracket/composite shape as the HUD trio above, driven by
// FsVrGuiDataPointer (fsvr.h) instead of FsVrHudDataPointer: renders whatever
// modal in-flight dialog is currently open into its own off-screen two-layer
// multiview framebuffer and composites it onto a second, GUI-anchored quad.
// Reuses the same HUD-quad GL renderer (it takes the texture array as a
// parameter, so no second renderer instance is needed).
void FsVrBeginGuiRender(void);
void FsVrEndGuiRender(void);
void FsVrDrawGuiQuad(const float corner[12]);

// VR single-pass-stereo G-load blackout(dark)/redout(red) full-field tint
// (see fsopengl2.0.cpp).  Draws a huge, close, camera-facing solid-colour
// quad (corner = 4 x vec3, BL,BR,TR,TL, built by the caller from the same
// fwd/up/right cockpit basis the HUD quad/reticle use) through each eye's
// own cached stereo projection, covering the ENTIRE eye frustum regardless
// of headset FOV -- drawn AFTER the HUD quad/reticle so it darkens/reddens
// everything, matching the physiological effect. alpha<=0 is an early-out
// (no GPU cost, no state changes) when there is no G-load effect active.
void FsVrDrawFullScreenTint(const float corner[12],float r,float g,float b,float alpha);

void FsBeginDrawShadow(void);  // Set polygon offset -1,-1 and enable.
void FsEndDrawShadow(void);    // Disable polygon offset.

void FsSetCameraPosition(const YsVec3 &pos,const YsAtt3 &att,YSBOOL zClear);
void FsFlushScene(void);

void FsDrawString(int x,int y,const char str[],const YsColor &col);
void FsDrawString(int x,int y,const wchar_t str[],YsColor col);
void FsDrawLine(int x1,int y1,int x2,int y2,const YsColor &col);

void FsDrawRect(int x1,int y1,int x2,int y2,const YsColor &col,YSBOOL fill);
void FsDrawCircle(int x,int y,int rad,const YsColor &col,YSBOOL fill);
void FsDrawPolygon(int n,int plg[],const YsColor &col);
void FsDrawDiamond(int x,int y,int r,const YsColor &col,YSBOOL fill);
void FsDrawX(int x,int y,int r,const YsColor &col);
void FsDrawPoint(int x,int y,const YsColor &col);
void FsDrawPoint2Pix(int x,int y,const YsColor &col);

void FsDrawTitleBmp(const class YsBitmap &bmp,YSBOOL tile);
void FsDrawBmp(const class YsBitmap &bmp,int x,int y);

void FsGlDepthMask(YSBOOL sw);

void FsDrawLine3d(const YsVec3 &p1,const YsVec3 &p2,const YsColor &col);






extern int BiWorkSize;
extern char BiWork[];

extern double fsOpenGLVersion;

void FsGraphicsTest(int);



/* } */
#endif
