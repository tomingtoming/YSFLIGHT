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
