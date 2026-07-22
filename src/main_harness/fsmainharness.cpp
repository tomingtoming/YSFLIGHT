/* Characterization harness for YSFLIGHT.

   A headless (null graphics + nownd platform) entry point that drives the
   simulation with a FIXED timestep and a FIXED RNG seed, then dumps the player
   aircraft's trajectory (position + attitude) to CSV every step.  The point is
   to produce a deterministic "golden" trajectory that a future refactor (or a
   Rust/TS reimplementation) can be checked against.

   This deliberately bypasses:
     - FsRunLoop::RunOneStep()  -> uses wall-clock PassedTime() (non-deterministic dt)
     - the console-server dispatch that refuses interactive flight
   and instead calls FsWorld::SimulateOneStep(dt, ...) directly, mirroring the
   free-flight setup recipe from main/fsmain.cpp (executionMode==3).

   Usage:
     ysflight64_harness [field] [airplane] [start] [nSteps] [out.csv] [seed]
   Any omitted argument falls back to a sensible default (first registered
   field / first registered airplane / first start position / 600 steps /
   harness_trajectory.csv / 12345).  Pass "-" for [start] to keep the default.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ysclass.h>
#include <ysport.h>
#include <fsgui.h>

#include "fsconfig.h"
#include "fsoption.h"
#include "fsapplyoption.h"
#include "fsnetconfig.h"

#include "fs.h"
#include "fsdef.h"
#include "fsworld.h"
#include "fsexistence.h"
#include "fssimulation.h"
#include "fssimextension.h"
#include "platform/common/fswindow.h"
#include "graphics/common/fsopengl.h"
#include "graphics/common/fsfontrenderer.h"
#include "fsfilename.h"
#include "fspluginmgr.h"
#include "fstextresource.h"
#include "fsrunloop.h"
#include "fscmdparaminfo.h"

#ifndef _WIN32
#include <unistd.h>
#endif

// Same link-order workaround as fsmainsvr.cpp: force ysglcpp_nownd to link.
#include <ysglbuffermanager.h>
void ForceLink(void)
{
	auto bufMan=YsGLBufferManager::GetSharedBufferManager();
}

const wchar_t *FsProgramName=L"YSFLIGHT";
const char *FsProgramTitle="YS FLIGHT SIMULATOR - Characterization Harness";

FsWorld *world=NULL;

static char FsWorkingDirectory[512];
static void FsCaptureWorkingDirectory(void)
{
	getcwd(FsWorkingDirectory,511);
	long long int l=strlen(FsWorkingDirectory);
	if(l>0 && (FsWorkingDirectory[l-1]=='/' || FsWorkingDirectory[l-1]=='\\'))
	{
		FsWorkingDirectory[l-1]=0;
	}
}

void Initialize(void)
{
	world=NULL;
	FsWeaponHolder::LoadMissilePattern();
}

extern FsScreenMessage fsConsole;

// ---------------------------------------------------------------------------
// Sample "experience" authored entirely OUTSIDE the engine core.
//
// This proves the FsSimExtensionBase seam lets you add gameplay/behaviors
// without modifying YSFLIGHT itself: the engine knows nothing about this class;
// the host (this harness) instantiates it and attaches it with
// FsSimulation::RegisterExtension().  "Altitude Watch" tracks the player's
// altitude band over a fixed sim-time budget and then ends the run, exercising
// StartSimulation / OnInterval / MustTerminate / EndSimulation.
// ---------------------------------------------------------------------------
class HarnessAltitudeChallenge : public FsSimExtensionBase
{
public:
	double timeLimit=20.0;
	long long intervalCount=0;
	double maxAlt=-1.0e30,minAlt=1.0e30;
	double startTime=0.0;

	const char *GetIdent(void) const override { return "HARNESSALTWATCH"; }

	YsArray <YsString> Serialize(const FsSimulation *) override
	{
		YsArray <YsString> a;
		a.Append("EXTENSIO");
		a.Append(GetIdent());
		return a;
	}
	YSRESULT ProcessCommand(FsSimulation *,const YsConstArrayMask <YsString> &) override
	{
		return YSOK;
	}

	void StartSimulation(FsSimulation *sim) override
	{
		startTime=sim->CurrentTime();
		printf("[experience] StartSimulation: Altitude-Watch begins (budget %.1fs)\n",timeLimit);
	}
	void OnInterval(FsSimulation *sim,double dt) override
	{
		++intervalCount;
		const FsAirplane *p=sim->GetPlayerAirplane();
		if(NULL!=p)
		{
			double y=p->GetPosition().y();
			if(y>maxAlt){maxAlt=y;}
			if(y<minAlt){minAlt=y;}
		}
	}
	YSBOOL MustTerminate(const FsSimulation *sim) const override
	{
		return (sim->CurrentTime()-startTime>=timeLimit ? YSTRUE : YSFALSE);
	}
	void EndSimulation(FsSimulation *sim) override
	{
		printf("[experience] EndSimulation: OnInterval fired %lld times, altitude band=[%.1f, %.1f] m\n",
		       intervalCount,minAlt,maxAlt);
	}
};

// ---------------------------------------------------------------------------
// Generic callback-driven extension: closes the seam-2 gap that experiences
// must be authored as C++ subclasses.  One concrete class delegates every hook
// to host-supplied callbacks, so an experience becomes a set of functions handed
// in at runtime -- no subclass, no edit to RegisterKnownExtension, no engine
// rebuild for the *shape* of a new experience.
//
// This is exactly the architecture of a JS/TS bridge: in the Emscripten build
// these std::function slots forward to JS functions (emscripten::val / EM_ASM),
// so a web developer authors the experience in TypeScript while this C++ class
// is the single, fixed forwarding shim.  Here, native lambdas stand in for the
// JS callbacks so the pattern is verifiable headlessly.
// ---------------------------------------------------------------------------
class FsSimExtension_Callback : public FsSimExtensionBase
{
public:
	YsString ident;
	std::function <void(FsSimulation *)> onStart;
	std::function <void(FsSimulation *,double)> onInterval;
	std::function <bool(const FsSimulation *)> mustTerminate;
	std::function <void(FsSimulation *)> onEnd;

	const char *GetIdent(void) const override { return ident.Txt(); }
	YsArray <YsString> Serialize(const FsSimulation *) override
	{
		YsArray <YsString> a; a.Append("EXTENSIO"); a.Append(ident); return a;
	}
	YSRESULT ProcessCommand(FsSimulation *,const YsConstArrayMask <YsString> &) override { return YSOK; }

	void StartSimulation(FsSimulation *sim) override { if(onStart){ onStart(sim); } }
	void OnInterval(FsSimulation *sim,double dt) override { if(onInterval){ onInterval(sim,dt); } }
	YSBOOL MustTerminate(const FsSimulation *sim) const override
	{
		return (mustTerminate && mustTerminate(sim)) ? YSTRUE : YSFALSE;
	}
	void EndSimulation(FsSimulation *sim) override { if(onEnd){ onEnd(sim); } }
};

// Mutable state shared between an experience's callbacks (stands in for the
// closure/state a TS experience would keep).
struct ScriptedState
{
	double startTime=0.0;
	double maxAlt=-1.0e30;
	long long intervalCount=0;
};

// ---------------------------------------------------------------------------
// Render snapshot: a backend-agnostic, GL-free description of what to draw this
// frame for one aircraft -- model identifier, world transform (position +
// attitude), and the animation scalars that FsAirplaneProperty::SetupVisual
// feeds into the DNM (gear, flap, spoiler, control surfaces, throttle, ...).
//
// This is exactly the data a retained-mode renderer (Three.js, a Rust/wgpu
// backend, ...) needs: load the model once by id, then per frame set the node
// transform and joint states from this snapshot.  It is produced from public
// getters only -- the engine is not modified, and because it is pure data the
// characterization harness can dump it and verify determinism.
// ---------------------------------------------------------------------------
static void WriteRenderSnapshot(FILE *fp,int step,double t,const FsAirplane *p)
{
	const YsVec3 &pos=p->GetPosition();
	const YsAtt3 &att=p->GetAttitude();
	const FsAirplaneProperty &pr=p->Prop();
	fprintf(fp,
	    "{\"step\":%d,\"t\":%.3f,\"id\":\"%s\","
	    "\"pos\":[%.3f,%.3f,%.3f],\"att\":[%.6f,%.6f,%.6f],"
	    "\"gear\":%.3f,\"flap\":%.3f,\"spoiler\":%.3f,\"vgw\":%.3f,"
	    "\"elevator\":%.4f,\"aileron\":%.4f,\"rudder\":%.4f,"
	    "\"throttle\":%.4f,\"thrustRev\":%.4f,\"brake\":%d}\n",
	    step,t,p->GetIdentifier(),
	    pos.x(),pos.y(),pos.z(),att.h(),att.p(),att.b(),
	    pr.GetLandingGear(),pr.GetFlap(),pr.GetSpoiler(),pr.GetControlVgw(),
	    pr.GetElevator(),pr.GetAileron(),pr.GetRudder(),
	    pr.GetThrottle(),pr.GetThrustReverser(),(YSTRUE==pr.GetBrake()?1:0));
}

int main(int ac,char *av[])
{
	ForceLink();

	printf("YSFLIGHT CHARACTERIZATION HARNESS\n");
	printf("VERSION %d / YFSVERSION %d\n",YSFLIGHT_VERSION,YSFLIGHT_YFSVERSION);

#ifdef __APPLE__
	{
		YsWString argv0;
		argv0.SetUTF8String(av[0]);
		auto realArgv0=YsFileIO::GetRealPath(argv0);
		YsWString dir,fil;
		realArgv0.SeparatePathFile(dir,fil);
		YsFileIO::ChDir(dir);
	}
#else
	FsChangeToProgramDir();
#endif

	if(YsFileExist("misc/aim9.srf")!=YSTRUE)
	{
		char fil[512],pth[512];
		YsSeparatePathFile(pth,fil,av[0]);
		if(pth[0]!=0 && (pth[strlen(pth)-1]=='/' || pth[strlen(pth)-1]=='\\'))
		{
			pth[strlen(pth)-1]=0;
		}
		chdir(pth);
		printf("Move CWD to %s\n",pth);
	}

	// --- Runtime initialization (mirrors fsmainsvr.cpp) ----------------------
	FsOption opt;
	opt.Load(FsGetOptionFile());

	FsFlightConfig cfg;
	cfg.Load(FsGetConfigFile());

	FsCaptureWorkingDirectory();

	FsBeforeOpenWindow(opt,cfg);
	auto owo=FsGetOpenWindowOption(opt,cfg,FsGetWindowSizeFile(),FsMainWindowTitle());
	FsOpenWindow(owo);
	FsAfterOpenWindow(opt,cfg);

	FsSoundInitialize();
	FsClearScreenAndZBuffer(YsBlack());

	FsGuiObject::defAsciiRenderer=&fsAsciiRenderer;
	FsGuiObject::defUnicodeRenderer=&fsUnicodeRenderer;

	FsSetFont(opt.fontName,opt.fontHeight);

	FsLoadPlugIn();
	FsApplyNonScreenOption(opt);

	// DETERMINISM: fixed seed instead of time(NULL).
	unsigned int seed=(6<ac ? (unsigned int)atoi(av[6]) : 12345u);
	srand(seed);
	printf("RNG seed = %u\n",seed);

	FsAirplaneAllocator.SetAllocUnit(16);
	FsGroundAllocator.SetAllocUnit(64);

	Initialize();

	FsRunLoop fsRunLoop;
	fsRunLoop.ChangeRunMode(FsRunLoop::YSRUNMODE_MENU);

	world=fsRunLoop.GetWorld();
	if(NULL==world)
	{
		printf("ERROR: world is null after FsRunLoop construction.\n");
		return 1;
	}

	FsPollDevice();
	FsSoundSetMasterSwitch(YSFALSE);

	// --- Pick scenario -------------------------------------------------------
	YsString fldName,airName;
	if(1<ac) fldName.Set(av[1]); else fldName.Set(world->GetFieldTemplateName(0));
	if(2<ac) airName.Set(av[2]); else airName.Set(world->GetAirplaneTemplateName(0));
	int nSteps=(4<ac ? atoi(av[4]) : 600);
	const char *csvPath=(5<ac ? av[5] : "harness_trajectory.csv");

	if(0==fldName.Strlen() || 0==airName.Strlen())
	{
		printf("ERROR: no field/airplane templates loaded (fld='%s' air='%s').\n",
		       fldName.Txt(),airName.Txt());
		return 1;
	}

	YsString startPos;
	if(3<ac && 0!=strcmp(av[3],"-"))
	{
		startPos.Set(av[3]);
	}
	else if(world->GetFieldStartPositionName(startPos,fldName,0)!=YSOK)
	{
		startPos.Set("");
	}

	printf("Scenario: field='%s' airplane='%s' start='%s' steps=%d -> %s\n",
	       fldName.Txt(),airName.Txt(),startPos.Txt(),nSteps,csvPath);

	// --- Free-flight setup (mirrors main/fsmain.cpp executionMode==3) --------
	world->TerminateSimulation();
	world->PrepareSimulation();

	YsVec3 vec(0.0,0.0,0.0);
	YsAtt3 att(0.0,0.0,0.0);
	if(NULL==world->AddField(NULL,fldName,vec,att))
	{
		printf("ERROR: AddField('%s') failed.\n",fldName.Txt());
		return 1;
	}

	FsAirplane *air=world->AddAirplane(airName,YSTRUE);
	if(NULL==air)
	{
		printf("ERROR: AddAirplane('%s') failed.\n",airName.Txt());
		return 1;
	}
	air->iff=FS_IFF0;
	world->SettleAirplane(*air,startPos.Txt());

	// --- Optional: dump a backend-agnostic render snapshot (YSF_RENDER_SNAPSHOT)
	// Proves the "sim -> render snapshot -> (any) renderer" seam: GL-free draw
	// data extracted from public getters, the input a Three.js/TS renderer would
	// consume.  Output is JSON-lines; deterministic, so the harness can verify it.
	if(NULL!=getenv("YSF_RENDER_SNAPSHOT"))
	{
		FILE *fp=fopen(csvPath,"w");
		if(NULL==fp){ printf("ERROR: cannot open '%s'.\n",csvPath); return 1; }

		const double dt=0.025;
		double t=0.0;
		for(int i=0; i<nSteps; i++)
		{
			world->SimulateOneStep(dt,YSFALSE,YSTRUE,YSFALSE,YSFALSE,FSUSC_ENABLE,YSFALSE);
			t+=dt;
			FsAirplane *p=world->GetPlayerAirplane();
			if(NULL!=p)
			{
				WriteRenderSnapshot(fp,i,t,p);
			}
		}
		fclose(fp);
		printf("Render snapshot: wrote %d frames to %s\n",nSteps,csvPath);

		world->TerminateSimulation();
		FsFreePlugIn();
		FsCloseWindow();
		return 0;
	}

	// --- Optional: experience defined purely as runtime callbacks (no subclass)
	// Closes the seam-2 gap: the same generic FsSimExtension_Callback is
	// configured with lambdas handed in at runtime -- the native stand-in for a
	// TS-authored experience.  Budget is read from YSF_BUDGET to show the
	// experience is parameterized at run time, not compiled in.
	if(NULL!=getenv("YSF_SCRIPTED_EXPERIENCE"))
	{
		FsSimulation *sim=world->GetSimulation();
		double budget=(NULL!=getenv("YSF_BUDGET") ? atof(getenv("YSF_BUDGET")) : 15.0);
		auto st=std::make_shared<ScriptedState>();
		auto ext=std::make_shared<FsSimExtension_Callback>();
		ext->ident.Set("TS_SCRIPTED");
		ext->onStart=[st,budget](FsSimulation *s)
		{
			st->startTime=s->CurrentTime();
			printf("[scripted] start (budget %.1fs) -- experience defined by callbacks, no C++ subclass\n",budget);
		};
		ext->onInterval=[st](FsSimulation *s,double)
		{
			++st->intervalCount;
			const FsAirplane *p=s->GetPlayerAirplane();
			if(NULL!=p){ double y=p->GetPosition().y(); if(y>st->maxAlt){ st->maxAlt=y; } }
		};
		ext->mustTerminate=[st,budget](const FsSimulation *s)
		{
			return s->CurrentTime()-st->startTime>=budget;
		};
		ext->onEnd=[st](FsSimulation *)
		{
			printf("[scripted] end: intervals=%lld, max altitude=%.1f m\n",st->intervalCount,st->maxAlt);
		};

		sim->RegisterExtension(ext);
		ext->StartSimulation(sim);

		FILE *fp=fopen(csvPath,"w");
		if(NULL==fp){ printf("ERROR: cannot open '%s'.\n",csvPath); return 1; }
		fprintf(fp,"step,t,x,y,z,heading,pitch,bank\n");
		const double dt=0.025;
		double t=0.0; int i=0;
		for(; i<nSteps; i++)
		{
			world->SimulateOneStep(dt,YSFALSE,YSTRUE,YSFALSE,YSFALSE,FSUSC_ENABLE,YSFALSE);
			t+=dt;
			FsAirplane *p=world->GetPlayerAirplane();
			if(NULL!=p)
			{
				const YsVec3 &pos=p->GetPosition();
				const YsAtt3 &a=p->GetAttitude();
				fprintf(fp,"%d,%.6f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n",
				        i,t,pos.x(),pos.y(),pos.z(),a.h(),a.p(),a.b());
			}
			if(ext->MustTerminate(sim)==YSTRUE){ printf("[scripted] terminate at step %d (t=%.3fs)\n",i,t); ++i; break; }
		}
		ext->EndSimulation(sim);
		fclose(fp);
		printf("Scripted experience: wrote %d steps to %s\n",i,csvPath);

		world->TerminateSimulation();
		FsFreePlugIn();
		FsCloseWindow();
		return 0;
	}

	// --- Optional: run with a host-supplied "experience" (YSF_EXPERIENCE) ----
	// Demonstrates adding gameplay via FsSimExtensionBase without touching the
	// engine.  Same fixed-dt loop; OnInterval fires automatically from inside
	// SimulateOneStep for every registered extension, while the host drives the
	// StartSimulation / MustTerminate / EndSimulation lifecycle.
	if(NULL!=getenv("YSF_EXPERIENCE"))
	{
		FsSimulation *sim=world->GetSimulation();
		auto exp=std::make_shared<HarnessAltitudeChallenge>();
		sim->RegisterExtension(exp);
		exp->StartSimulation(sim);

		FILE *fp=fopen(csvPath,"w");
		if(NULL==fp){ printf("ERROR: cannot open '%s'.\n",csvPath); return 1; }
		fprintf(fp,"step,t,x,y,z,heading,pitch,bank\n");

		const double dt=0.025;
		double t=0.0;
		int i=0;
		for(; i<nSteps; i++)
		{
			world->SimulateOneStep(dt,YSFALSE,YSTRUE,YSFALSE,YSFALSE,FSUSC_ENABLE,YSFALSE);
			t+=dt;
			FsAirplane *p=world->GetPlayerAirplane();
			if(NULL!=p)
			{
				const YsVec3 &pos=p->GetPosition();
				const YsAtt3 &a=p->GetAttitude();
				fprintf(fp,"%d,%.6f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n",
				        i,t,pos.x(),pos.y(),pos.z(),a.h(),a.p(),a.b());
			}
			if(exp->MustTerminate(sim)==YSTRUE)
			{
				printf("[experience] MustTerminate -> ending flight at step %d (t=%.3fs)\n",i,t);
				++i;
				break;
			}
		}
		exp->EndSimulation(sim);
		fclose(fp);
		printf("Experience run: wrote %d steps to %s\n",i,csvPath);

		world->TerminateSimulation();
		FsFreePlugIn();
		FsCloseWindow();
		return 0;
	}

	// --- Deterministic fixed-step loop --------------------------------------
	FILE *fp=fopen(csvPath,"w");
	if(NULL==fp)
	{
		printf("ERROR: cannot open output '%s'.\n",csvPath);
		return 1;
	}
	fprintf(fp,"step,t,x,y,z,heading,pitch,bank\n");

	const double dt=0.025; // 40 Hz fixed step (YSFLIGHT's accurate-time substep)
	double t=0.0;
	for(int i=0; i<nSteps; i++)
	{
		world->SimulateOneStep(dt,YSFALSE,YSTRUE,YSFALSE,YSFALSE,FSUSC_ENABLE,YSFALSE);
		t+=dt;

		FsAirplane *p=world->GetPlayerAirplane();
		if(NULL!=p)
		{
			const YsVec3 &pos=p->GetPosition();
			const YsAtt3 &a=p->GetAttitude();
			fprintf(fp,"%d,%.6f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n",
			        i,t,pos.x(),pos.y(),pos.z(),a.h(),a.p(),a.b());
		}
	}
	fclose(fp);
	printf("Wrote %d steps to %s\n",nSteps,csvPath);

	world->TerminateSimulation();
	FsFreePlugIn();
	FsCloseWindow();
	return 0;
}
