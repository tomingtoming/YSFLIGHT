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
//   [7]  confirmTriggerPressed (0 or 1; either tracked hand's primary XR
//                               trigger is currently pressed). Used by the
//                               pre-flight CENTER JOYSTICK confirmation,
//                               whose normal joystick poll cannot see WebXR
//                               XRInputSource gamepads.
//   [8]  stickEverGrabbed     (0 or 1; once set to 1 by the writer, stays 1
//                              for the rest of the session -- the stick
//                              counterpart of [6].  While [0] is 0 and this
//                              is 1, the reader CENTERS aileron/elevator/
//                              rudder every frame, like a released
//                              spring-loaded stick.  Without it, releasing
//                              the grab left the last held deflection frozen
//                              in FsFlightControl -- the release-edge zero
//                              write and the [0]->0 flip land on the same
//                              frame, so the zeros were never consumed --
//                              and any stale mouse-as-joystick position
//                              could steer the plane after release.)
//   [9..15] reserved, always 0

// Hand-pose data layout (FsVrHandPoseDataPointer, 16 floats):
//   A VR controller runtime (WebXR in ysflight-web) writes each grabbed
//   hand's HOTAS-prop console pose here every frame, in VIEWER space (the
//   browser's own convention: x right, y up, -z forward, meters / unit
//   quaternion) -- i.e. relative to wherever the pilot's HEAD currently is,
//   NOT the engine's world frame.  FsSimulation reads it (SimDrawAllScreen's
//   multiview branch) to draw the engine's own stick/throttle DNM models
//   (misc/stick.dnm, misc/throttle.dnm -- the exact same models/draw calls
//   the calibration screen uses, see FsFlightControl::DrawJoystick/
//   DrawThrottle in fscontrol.cpp) at that pose, articulating from the LIVE
//   control deflections so the pilot sees the rod/lever move with their
//   wrist.  NOTE the pose is ANCHORED, not the live grip: the web layer
//   freezes the console at the grab-start point with a synthetic upright
//   facing-the-pilot orientation (fswebxr.cpp's updateHandPropAnchor --
//   streaming the live grip made the whole base plate wobble with the hand,
//   which read as broken on device) and only re-bases that frozen
//   reference-space pose onto each frame's head pose before writing here.
//   This works out cheaply because FsVisual::Draw's (pos,att) parameters
//   are ALREADY defined relative to the current eye ("the viewpoint is at
//   the origin looking straight ahead", see SimDrawJoystick's memo in
//   fssimulation.cpp) -- the exact same frame a viewer-space pose is
//   already given in, up to the WebXR-vs-engine handedness flip (negate z)
//   -- so no per-frame world-transform reconstruction is needed in the
//   engine, just that one flip.
//   Right hand: indices [0..7].  Left hand: indices [8..15].
//     [0..2]  console position (x,y,z), viewer space
//     [3..6]  console orientation quaternion (x,y,z,w), viewer space
//     [7]     grabbed (0/1; 1 iff the right/stick hand is CURRENTLY
//             effective-grabbed -- physical squeeze OR sticky latch, the
//             same condition that drives FsVrControlDataPointer[0]).
//             Informational/redundant with FsVrControlDataPointer[0]
//             (which is what the engine actually gates drawing on);
//             written here too so a hand-pose-only consumer is
//             self-contained.
//     [8..10] console position (x,y,z), viewer space, LEFT hand
//     [11..14] console orientation quaternion (x,y,z,w), viewer space, LEFT hand
//     [15]    reserved, always 0 (the engine gates the left/throttle hand
//             on FsVrControlDataPointer[4] instead -- see fsvr.h's control-
//             data doc comment above)

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

	/*! See the FsVrHandPoseDataPointer doc comment above this extern "C"
	    block for the 16-float layout. */
	float *FsVrHandPoseDataPointer(void);

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

	/*! VR multiview shadow-map render-target block (8 floats), written by the
	    WebXR layer when single-pass-stereo mode engages (setupShadowFbo in
	    fswebxr.cpp), consumed by the gl2.0 back-end's shadow-map path.
	    WHY it exists: while multiview is active EVERY shared-renderer program
	    is compiled with layout(num_views=2) (YsGLSLSetCompileNumViews /
	    FsReinitializeOpenGL), and OVR_multiview2 raises INVALID_OPERATION on
	    any draw whose framebuffer view count differs from the program's --
	    verified empirically on ANGLE: a num_views=2 program cannot draw into
	    the per-cascade single-layer 2048x2048 depth FBOs YsTextureManager
	    owns.  So the multiview shadow pass renders each cascade into THIS
	    shared two-layer depth-array FBO instead (both layers get the same
	    light-space projection -- see FsBeginRenderShadowMap), then blits
	    layer 0's depth into the cascade's ordinary 2D depth texture
	    (FsVrBlitShadowMapFromMultiview), which the scene pass samples exactly
	    like the flat path.  One shared array serves all cascades
	    sequentially -- render, blit, repeat.
	      [0] enable    (0/1, written by the web layer)
	      [1] mvFbo     (GL framebuffer name: two-layer multiview FBO, depth-
	                     only attachment, DEPTH_COMPONENT24 2048x2048x2)
	      [2] readFbo   (GL framebuffer name: layer 0 of that same depth array
	                     attached via framebufferTextureLayer -- the blit
	                     source)
	      [3] texWidth  (must equal the engine's shadow-map cascade size,
	                     2048 -- see FsCommonTexture::ReadyShadowMap; the
	                     depth blit requires equal rectangles)
	      [4] texHeight
	      [5..7] reserved (0) */
	float *FsVrShadowFboDataPointer(void);

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
	          open dialog is one of the small set of in-flight dialogs known
	          to accept the direct positional hotkeys Digit1..Digit9,Digit0,
	          Escape (see FsSimulation::SimComputeVrGuiState in
	          fssimulation.cpp for the exact member list -- the autopilot
	          family plus the radio-comm/ATC/approach menus that override
	          FsGuiInFlightDialog::ProcessRawKeyInput with a plain switch on
	          FSKEY_1.. -- see fsguiinfltdlg.cpp).  The field kept its
	          original "apMenu" name for ABI stability with existing web-layer
	          code; the autopilot menus were simply the first three dialogs
	          of this kind wired up.  Other in-flight dialogs (replay/continue
	          dialogs, the stationary/change-vehicle/chat dialogs, ...) are
	          mouse-only or have no ProcessRawKeyInput override, so the web
	          layer only offers the stick-sector-to-hotkey mapping when this
	          is set; otherwise it only offers a generic Escape-to-close
	          action (which itself may or may not be wired -- see
	          FsVrGuiMenuPointer's doc comment).
	      [7] reserved (0) */
	float *FsVrGuiDataPointer(void);

	/*! In-flight GUI-dialog MENU block: the ordered list of option labels the
	    CURRENTLY-OPEN modal in-flight dialog offers, so the web layer's
	    selection-guide (fswebxr.cpp's drawGuiDialGuide) can show the real
	    option text instead of hand-transcribed captions that silently go
	    stale whenever the engine-side dialog changes.  Written every VR
	    frame by FsSimulation::SimComputeVrGuiState -- independent of whether
	    the GUI quad composite is enabled (guiData[0] above), so the guide
	    stays truthful even when the quad itself is never rendered (the
	    default from ysfwVrOptions.guiPanel=false).

	    Each line is one dialog item's on-screen label text, UTF-8, in the
	    SAME order FsGuiDialog laid them out (FsGuiDialog::GetNumItem/
	    GetItem), one per '\n'.  Only FSGUI_BUTTON items that are currently
	    visible AND enabled are included (so a disabled Next/Prev page
	    button, or a hidden page of a paged menu, does not appear).  The
	    labels already carry their own human-facing hotkey prefix by strong
	    convention across every FsGuiInFlightDialog subclass (e.g. "1...Circle",
	    "0...Disengage Autopilot", "ESC:Cancel" -- see fsguiinfltdlg.cpp's
	    Make() functions) -- this is deliberate: several dialogs register
	    their buttons with fsKey==FSKEY_NULL and dispatch the real hotkey
	    positionally inside a hand-written ProcessRawKeyInput switch instead
	    (e.g. FsGuiRadioCommToFormationDialog, FsGuiRadioCommTargetDialog),
	    so the FsGuiDialogItem::fsKey field is NOT reliable across dialogs.
	    The label prefix is: it is the same text a keyboard-driven player
	    already reads off the flat 2D dialog to know what to press.  The web
	    layer parses that leading token back out for layout, rather than
	    trusting fsKey.

	    Empty (FsVrGuiMenuLength()==0) when no modal in-flight dialog is
	    open, or when the currently-open one is not itemList-based in a
	    useful way.

	      FsVrGuiMenuPointer()  -- pointer to a fixed-capacity static UTF-8
	          buffer (NOT null-terminated beyond FsVrGuiMenuLength() bytes;
	          content past capacity is silently truncated, never overflows
	          -- see FSVR_GUIMENU_CAP in fsvr.cpp).
	      FsVrGuiMenuLength()   -- number of valid bytes at
	          FsVrGuiMenuPointer().
	      FsVrGuiMenuVersion()  -- increments every time the content actually
	          changes (dialog opened/closed/paged/relabelled), so the web
	          layer only re-parses/re-draws the guide when this changes, not
	          every frame (mirrors FsVrAircraftStateDataPointer's
	          change-driven redraw idiom in fswebxr.cpp). */
	const char *FsVrGuiMenuPointer(void);
	int FsVrGuiMenuLength(void);
	int FsVrGuiMenuVersion(void);

	/*! Main-menu-in-VR state block (8 floats).
	    Written by the web layer (setupMenu) on session start; read by
	    fsrunloop.cpp::DrawMenu when VR is active.
	      [0] enable    (0/1; web layer sets to 1 when FBO is ready)
	      [1] menuFbo   (GL framebuffer name)
	      [2] menuTex   (GL texture name)
	      [3] texWidth
	      [4] texHeight
	      [5] menuDrawn (0/1; ENGINE writes 1 each frame it rendered the menu
	                     into the FBO; web layer resets to 0 after reading)
	      [6] textInput (0/1; ENGINE writes 1 when the menu frame it just
	                     rendered contains a keyboard-focused text box --
	                     latched via fsguilib's fsGuiTextBoxFocusDrawnHook in
	                     FsVrBegin/EndMenuRender, fsopengl2.0.cpp.  Read by
	                     the web layer's text-input bridge to summon the
	                     headset's system keyboard)
	      [7] reserved (0) */
	float *FsVrMenuDataPointer(void);

	/*! Raised by fsrunloop.cpp::DrawMenu while it is rendering into the
	    menu FBO; read by fsopengl2.0.cpp to redirect the viewport/scissor
	    machinery to the full menu-texture dimensions instead of the eye
	    viewport. Cleared by the same DrawMenu call after the pass ends. */
	void FsVrSetMenuPassActive(int active);
	int  FsVrIsMenuPassActive(void);

	/*! Begin/end the menu off-screen pass.  Called by fsrunloop.cpp::DrawMenu
	    when FsVrIsActive and the menu block is enabled.  Redirects GL
	    framebuffer + viewport machinery to the menu FBO for the duration of
	    the menu draw, then restores the scene (session) framebuffer. */
	void FsVrBeginMenuRender(void);
	void FsVrEndMenuRender(void);

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
	      [6] viewCycle (0..6 as float; where FsSimulation::mainWindowViewmode
	                     currently sits along the desktop F2/FSBTF_
	                     OUTSIDEPLAYERVIEW external-view chain -- see
	                     FsSimulation::ViewingControl's FSBTF_
	                     OUTSIDEPLAYERVIEW case: 0=FSCOCKPITVIEW,
	                     1=FSOUTSIDEPLAYERPLANE, 2=FSFIXEDPOINTPLAYERPLANE,
	                     3=FSVARIABLEPOINTPLAYERPLANE,
	                     4=FSFROMTOPOFPLAYERPLANE, 5=FSPLAYERPLANEFROMSIDE,
	                     6=anything else (additional-airplane/another-
	                     airplane/tower/carrier/...). Lets the web layer's
	                     left-Y view-cycle tap (fswebxr.cpp) decide whether
	                     its next tap should dispatch F2 (advance the chain,
	                     or enter it fresh from cockpit/other) or F1 (the
	                     chain's last stop, PLAYERPLANEFROMSIDE==5, wrapping
	                     back to cockpit) without duplicating
	                     ViewingControl's own cycle order.
	      [7] reserved (0) */
	float *FsVrAircraftStateDataPointer(void);

	/*! Phase-breakdown perf block (16 floats): an engine-side wall-clock EMA
	    of the VR multiview draw path (FsSimulation::SimDrawAllScreen's
	    single-pass-stereo branch), exposed so the web layer can print a
	    console phase breakdown of the tick on hardware with no devtools
	    profiler (Quest 3S standalone browser). Same exponential-moving-
	    average shape as fslazywindow_emscripten.cpp's YsfwGetTickMs
	    (alpha=0.05): each new sample o is folded in as
	    avg=(0==avg ? o : avg*0.95+o*0.05) -- see FsVrPerfAccumulate.
	      [0] interval/simulation ms -- SET BY THE PORT LAYER
	          (fslazywindow_emscripten.cpp's MainLoopTick), NOT this engine:
	          the appPtr->Interval() half of the tick. 0 until the port
	          layer's split lands a sample.
	      [1] draw ms -- SET BY THE PORT LAYER, the appPtr->Draw() half.
	      [2] scene draw ms -- the single SimDrawScreen call in the multiview
	          branch (the 3D world, one traversal serving both eyes).
	      [3] HUD ms -- SimDrawVrHud (inside its FsVrBeginHudRender/
	          FsVrEndHudRender off-screen-FBO redirect) plus FsVrDrawHudQuad.
	          0 while the HUD composite is disabled (hudData[0]==0).
	      [4] GUI ms -- SimComputeVrGuiState (always, includes
	          SimSerializeVrGuiMenu) plus, while a dialog is open/enabled,
	          SimDrawVrGui and FsVrDrawGuiQuad.
	      [5] reticle ms -- FsVrDrawReticle. 0 while the reticle is disabled
	          or there is no live HUD-equipped player plane.
	      [6..15] reserved, always 0.
	    Instruments ONLY the VR multiview branch; the flat-screen and per-eye
	    (non-multiview stereo) draw paths are untouched. Always-on: a handful
	    of FsVrPerfNow() calls per frame is negligible next to the ~20+ms
	    scene draw they are measuring, so there is no build flag to disable
	    it. */
	float *FsVrPerfDataPointer(void);

	/*! TEST-ONLY: forces the VR G-load blackout/redout full-field tint
	    (FsVrDrawFullScreenTint, called from FsSimulation::SimDrawAllScreen)
	    to a fixed colour/alpha regardless of the player's actual G, so a
	    headless test can exercise the tint without a real high-G manoeuvre.
	    Block layout (5 floats):
	      [0] active (0/1) -- while nonzero, SimDrawAllScreen uses [1..4]
	          verbatim instead of computing G-based blackout/redout.
	      [1..3] r,g,b (0..1 each)
	      [4] alpha (0..1)
	    See FsVrSetBlackoutOverride (the writer) and
	    FsVrBlackoutOverridePointer (the reader, read by SimDrawAllScreen). */
	float *FsVrBlackoutOverridePointer(void);
	void FsVrSetBlackoutOverride(int active,float r,float g,float b,float alpha);
}

/*! Wall-clock "now" in milliseconds for FsVrPerfDataPointer's EMA:
    sub-millisecond resolution under Emscripten (emscripten_get_now(), the
    same clock fslazywindow_emscripten.cpp's tick EMA already uses) and a
    coarse clock()-based fallback on every other platform (fsvr.{h,cpp} are
    deliberately dependency-free -- see the top of fsvr.cpp -- so this does
    not pull in fssimplewindow.h's FsSubSecondTimer just for a path that
    never reads this block for anything real). */
double FsVrPerfNow(void);

/*! Folds a new `ms` sample into perf slot `slot` (0..15, see
    FsVrPerfDataPointer's doc comment) with the same alpha=0.05 EMA as
    YsfwGetTickMs. Out-of-range slot is a silent no-op. */
void FsVrPerfAccumulate(int slot,double ms);

const int FsVrNumEye=2;

void FsVrMarkSimDrawn(void);

/*! Writer side of the FsVrGuiMenuPointer/Length/Version block above.  Called
    by FsSimulation::SimSerializeVrGuiMenu once per VR frame with the
    currently-open dialog's serialized option labels (utf8, NOT necessarily
    null-terminated, len bytes).  Truncates to the buffer's fixed capacity and
    only bumps FsVrGuiMenuVersion() when the content actually changed (a
    plain memcmp -- the buffer is tiny and this runs once per frame, so no
    need for anything smarter). Pass len==0 (utf8 may be NULL) to clear it. */
void FsVrSetGuiMenu(const char *utf8,int len);

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
