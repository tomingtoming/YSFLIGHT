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
