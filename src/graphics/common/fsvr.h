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

	/*! Transient override, set only while the HUD (or GUI-dialog, see
	    FsVrGuiDataPointer below) off-screen pass is running, so that the 2D
	    coordinate machinery (FsSetupViewport / FsGetWindowSize) believes the
	    "screen" is the off-screen texture (texW x texH) instead of the real
	    window / eye buffer.  Cleared immediately after each pass.  Shared by
	    both off-screen passes: they never run concurrently within a frame
	    (HUD pass fully brackets/ends before the GUI pass begins), so one
	    active/size pair is enough -- no need for a second, parallel set of
	    these functions just for the GUI pass. */
	void FsVrSetHudRenderTarget(int active,int w,int h);
	int FsVrHudRenderTargetActive(void);
	void FsVrGetHudRenderSize(int *w,int *h);

	/*! In-flight GUI-dialog composite state block (8 floats), mirroring
	    FsVrHudDataPointer above.  The VR runtime (the WebXR layer) writes
	    [0..4] when single-pass-stereo mode engages; the graphics back-end /
	    simulation core read/write the rest to render whatever modal in-flight
	    dialog is currently open (the autopilot menu, radio-comm menus, the
	    replay/continue dialogs, etc. -- see FsSimulation::SimDrawGuiDialog)
	    into an off-screen two-layer multiview framebuffer and composite it
	    onto a second, GUI-anchored quad, the same way FsVrHudDataPointer's
	    block drives the flying HUD.  In plain (non-VR) play these dialogs
	    already draw straight to the 2D screen (SimDrawGuiDialog, guarded by
	    0==FsVrIsActive()); in VR that whole call is skipped today, which
	    is why a dialog that pops up mid-flight (e.g. the autopilot menu,
	    Backspace/FSBTF_OPENAUTOPILOTMENU) is invisible and un-closeable --
	    this block is what makes it visible again.
	      [0] enable       (0/1, written by the web layer)
	      [1] guiFbo       (GL framebuffer name, two-layer multiview FBO)
	      [2] guiTexArray  (GL_TEXTURE_2D_ARRAY name, RGBA8, 2 layers)
	      [3] texWidth
	      [4] texHeight
	      [5] dialogVisible (0/1, WRITTEN BY THE ENGINE each frame: 1 while a
	          modal in-flight dialog / the replay dialog / the continue
	          dialog is being rendered into the GUI texture this frame, 0
	          otherwise.  The web layer reads this to know whether to draw
	          the GUI quad at all, and to route hand-controller input to the
	          dialog instead of its normal flight-control meaning.)
	      [6] apMenu        (0/1, WRITTEN BY THE ENGINE: 1 iff the currently
	          open dialog is one of the autopilot menus (airplane / VTOL /
	          helicopter), which are known to accept the direct hotkeys
	          Digit1..Digit5,Digit0,Escape (see FsGuiAutoPilotDialog /
	          FsGuiAutoPilotVTOLDialog / FsGuiAutoPilotHelicopterDialog's
	          ProcessRawKeyInput in fsguiinfltdlg.cpp).  Other in-flight
	          dialogs use per-instance hotkeys or mouse-only interaction, so
	          the web layer only offers the stick-sector-to-hotkey mapping
	          when this is set; otherwise it only offers a generic
	          Escape-to-close action.
	      [7] reserved (0) */
	float *FsVrGuiDataPointer(void);

	/*! Aircraft-state block (8 floats) for the VR radial function-dial's live
	    readouts (RIGHT_DIAL/LEFT_DIAL in fswebxr.cpp): the dial shows the
	    state a sector's key press WOULD change before the pilot presses it.
	    Filled once per sim frame, for the player's aircraft only, while VR is
	    active (FsSimulation::SimDrawAllScreen, right after
	    SimMakeUpCockpitIndicationSet -- the same FsCockpitIndicationSet /
	    FsAirplaneProperty accessors that already drive the flat HUD's
	    LDG/BRK/FLP/SPL readouts and DrawAmmo).
	      [0] valid     (0/1; 0 when there is no player airplane -- e.g. a
	                     ground-vehicle "player" or no active aircraft -- the
	                     rest of the block is then stale and must not be
	                     displayed)
	      [1] gear      (0.0=up .. 1.0=down; FsAirplaneProperty::
	                     GetLandingGear(), same source as the HUD's DrawGear.
	                     Transitional values are real: the gear takes time to
	                     travel, this is not just 0 or 1)
	      [2] brake     (0.0=off / 1.0=on; cockpitIndicationSet.inst.brake,
	                     i.e. GetBrake() thresholded at 0.5. KeyG's dial
	                     neighbor KeyB (FSBTF_SPOILERBRAKE) toggles ctlBrake
	                     AND ctlSpoiler together in one call
	                     (FsAirplaneProperty::ToggleBrake), so this is also
	                     the airbrake/spoiler state -- one button, one value)
	      [3] flap      (0.0..1.0; GetFlap(), same source as the HUD's
	                     DrawFlap)
	      [4] wpnType   (FSWEAPONTYPE as float; GetWeaponOfChoice(). See
	                     fsdef.h's FSWEAPON_* enum: 0=GUN, 1=AIM9, 2=AGM65,
	                     3=BOMB, 4=ROCKET, 5=FLARE, 6=AIM120, 7=BOMB250,
	                     8=SMOKE, 9=BOMB500HD, 10=AIM9X, 11=FLAREPOD,
	                     12=FUELTANK; FSWEAPON_NULL=127 if no weapon)
	      [5] wpnCount  (remaining count of the selected weapon;
	                     GetNumWeapon(woc), plus GetNumPilotControlledTurretBullet()
	                     for FSWEAPON_GUN -- the same computation the flat
	                     HUD's own ammo readout (DrawAmmo) uses for the gun)
	      [6..7] reserved (0) */
	float *FsVrAircraftStateDataPointer(void);
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
