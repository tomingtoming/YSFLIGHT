#ifndef FSVR_IS_INCLUDED
#define FSVR_IS_INCLUDED
/* { */

// Virtual-reality (stereo, head-tracked) rendering state.
//
// A VR runtime (WebXR in ysflight-web; potentially OpenXR elsewhere) owns the
// session and writes per-eye data here every frame through the C API below.
// The graphics back-end reads the state to override the viewport, the
// projection, and the view matrix while VR is active.  The simulation core
// only checks FsVrIsActive to skip screen-space (2D) drawing, and reports
// each drawn simulation frame so that the VR runtime can tell whether the 3D
// scene is being presented (as opposed to the 2D menus, which are not
// meaningful in a head-mounted display).
//
// While VR is active, the simulation draws the main window once per eye
// (SimDrawAllScreen), selecting the eye through the active split-window
// index, and composes the whole viewpoint with the eye-view transform.
//
// Eye-data layout (FsVrEyeDataPointer, 24 floats per eye):
//   [ 0.. 3]  tangents of the half field of view: left, right, up, down
//             (all positive)
//   [ 4..19]  view matrix, OpenGL-compatible column-major order.  Maps the
//             base camera space (right-handed, -Z forward) to the eye space.
//   [20..23]  viewport in the VR framebuffer: x, y, width, height,
//             bottom-left origin (OpenGL convention)

// Control-data layout (FsVrControlDataPointer, 16 floats):
//   A VR controller runtime (WebXR in ysflight-web) writes hand-controller
//   state here every frame; FsFlightControl::ReadControl reads it (while
//   FsVrIsActive) to override the player airplane's aileron/elevator/rudder
//   and throttle controls.  This block's ranges and signs are the canonical,
//   engine-neutral convention -- the reader converts to whatever internal
//   range/sign FsFlightControl's own members happen to use.
//   [0]  stickGrabbed         (0 or 1; virtual control stick is held)
//   [1]  aileron              (-1..+1; positive = roll right)
//   [2]  elevator             (-1..+1; positive = nose up)
//   [3]  rudder               (-1..+1; positive = nose left)
//   [4]  throttleGrabbed      (0 or 1; virtual throttle lever is held)
//   [5]  throttle             (0..1)
//   [6]  throttleEverGrabbed  (0 or 1; once set to 1 by the writer, stays 1
//                              for the rest of the session -- tells the
//                              reader that [5] is a real, live-supplied
//                              throttle value, as opposed to never touched)
//   [7..15] reserved, always 0

extern "C"
{
	int FsVrIsActive(void);
	void FsVrSetActive(int active);
	float *FsVrEyeDataPointer(int eye);
	/*! Returns the number of simulation frames drawn since the last call. */
	int FsVrConsumeSimDrawnFrames(void);
	/*! Single-pass stereo (OVR_multiview2).  While active the scene is drawn
	    ONCE from the eye-0 pose into a two-layer framebuffer; the per-eye
	    difference is folded into a per-view projection array
	    (projection[i] = P_i * V_i * inverse(V_0)) by the graphics back-end.
	    The VR runtime must have the shared renderers compiled with
	    YsGLSLSetCompileNumViews(2) (see FsReinitializeOpenGL). */
	int FsVrIsMultiview(void);
	void FsVrSetMultiview(int multiview);
	float *FsVrControlDataPointer(void);

	/*! Head-up-display composite state block (8 floats).  The VR runtime (the
	    WebXR layer) writes it when single-pass-stereo mode engages; the
	    graphics back-end reads it to render the flat in-flight HUD into an
	    off-screen two-layer multiview framebuffer and composite it onto a
	    cockpit-anchored quad.
	      [0] enable    (0/1, written by the web layer)
	      [1] hudFbo    (GL framebuffer name, two-layer multiview FBO)
	      [2] hudTexArray (GL_TEXTURE_2D_ARRAY name, RGBA8, 2 layers)
	      [3] texWidth
	      [4] texHeight
	      [5..7] reserved (0)
	    The GL names are stored as floats because they are always small
	    non-negative integers that round-trip exactly through float32 (same
	    convention as the other fsvr shared blocks). */
	float *FsVrHudDataPointer(void);

	/*! Transient override, set only while the HUD off-screen pass is running,
	    so that the 2D coordinate machinery (FsSetupViewport / FsGetWindowSize)
	    believes the "screen" is the HUD texture (texW x texH) instead of the
	    real window / eye buffer.  Cleared immediately after the pass. */
	void FsVrSetHudRenderTarget(int active,int w,int h);
	int FsVrHudRenderTargetActive(void);
	void FsVrGetHudRenderSize(int *w,int *h);
}

const int FsVrNumEye=2;

void FsVrMarkSimDrawn(void);

/*! Frustum of the eye at the near/far planes chosen by the caller.
    Compatible with glFrustum/YsGLMakeFrustum parameters. */
void FsVrGetEyeFrustum(
    int eye,const double &nearz,const double &farz,
    double &lft,double &rit,double &btm,double &top);

/*! OpenGL-compatible column-major view matrix of the eye. */
const float *FsVrEyeViewMatrix(int eye);

void FsVrGetEyeViewport(int eye,int &x0,int &y0,int &wid,int &hei);

/* } */
#endif
