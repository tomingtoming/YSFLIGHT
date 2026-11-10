#include "fsvr.h"

#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <ctime>
#endif

// Deliberately dependency-free: this is plain shared state between a VR
// runtime (the writer) and the graphics back-end / simulation core (the
// readers).  See fsvr.h for the layout.

#define FSVR_EYEDATA_INIT \
	{ \
		1.0f,1.0f,1.0f,1.0f, \
		1.0f,0.0f,0.0f,0.0f, \
		0.0f,1.0f,0.0f,0.0f, \
		0.0f,0.0f,1.0f,0.0f, \
		0.0f,0.0f,0.0f,1.0f, \
		0.0f,0.0f,0.0f,0.0f \
	}

static int fsVrActive=0;
static float fsVrEyeData[FsVrNumEye][24]=
{
	FSVR_EYEDATA_INIT,
	FSVR_EYEDATA_INIT
};
static int fsVrSimDrawnFrame=0;
static float fsVrExternalView[8]={0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};

extern "C" float *FsVrExternalViewDataPointer(void)
{
	return fsVrExternalView;
}

static float fsVrPlayerPose[24]={0.0f};

extern "C" float *FsVrPlayerPoseDataPointer(void)
{
	return fsVrPlayerPose;
}

static float fsVrPlayerVisual[24]={0.0f};
extern "C" float *FsVrPlayerVisualDataPointer(void)
{
	return fsVrPlayerVisual;
}

static double fsVrFrameTimeMs=-1.0;
extern "C" void FsVrSetFrameTimeMs(double ms)
{
	fsVrFrameTimeMs=ms;
}
extern "C" double FsVrFrameTimeMs(void)
{
	return fsVrFrameTimeMs;
}

extern "C" int FsVrAbiVersion(void)
{
	return FSVR_ABI_VERSION;
}

extern "C" int FsVrIsActive(void)
{
	return fsVrActive;
}

extern "C" void FsVrSetActive(int active)
{
	fsVrActive=(0!=active ? 1 : 0);
}

extern "C" float *FsVrEyeDataPointer(int eye)
{
	if(eye<0 || FsVrNumEye<=eye)
	{
		eye=0;
	}
	return fsVrEyeData[eye];
}

static int fsVrMultiview=0;

extern "C" int FsVrIsMultiview(void)
{
	return fsVrMultiview;
}

extern "C" void FsVrSetMultiview(int multiview)
{
	fsVrMultiview=(0!=multiview ? 1 : 0);
}

extern "C" int FsVrConsumeSimDrawnFrames(void)
{
	const int n=fsVrSimDrawnFrame;
	fsVrSimDrawnFrame=0;
	return n;
}

#define FSVR_CTLDATA_INIT \
	{ \
		0.0f,0.0f,0.0f,0.0f, \
		0.0f,0.0f,0.0f,0.0f, \
		0.0f,0.0f,0.0f,0.0f, \
		0.0f,0.0f,0.0f,0.0f \
	}

static float fsVrCtlData[16]=FSVR_CTLDATA_INIT;

extern "C" float *FsVrControlDataPointer(void)
{
	return fsVrCtlData;
}

// See fsvr.h's FsVrHandPoseDataPointer doc comment for the slot layout.
// Identity quaternion (w=1) in both hands' slots by default so a reader
// that runs before the first write never sees a degenerate all-zero
// quaternion.
static float fsVrHandPose[16]=
{
	0.0f,0.0f,0.0f, 0.0f,0.0f,0.0f,1.0f, 0.0f,
	0.0f,0.0f,0.0f, 0.0f,0.0f,0.0f,1.0f, 0.0f
};

extern "C" float *FsVrHandPoseDataPointer(void)
{
	return fsVrHandPose;
}

static float fsVrHudData[8]={0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};

extern "C" float *FsVrHudDataPointer(void)
{
	return fsVrHudData;
}

// See fsvr.h's FsVrShadowFboDataPointer doc comment for the slot layout.
static float fsVrShadowFboData[8]={0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};

extern "C" float *FsVrShadowFboDataPointer(void)
{
	return fsVrShadowFboData;
}

static int fsVrHudRenderActive=0;
static int fsVrHudRenderW=0;
static int fsVrHudRenderH=0;

extern "C" void FsVrSetHudRenderTarget(int active,int w,int h)
{
	fsVrHudRenderActive=(0!=active ? 1 : 0);
	fsVrHudRenderW=w;
	fsVrHudRenderH=h;
}

extern "C" int FsVrHudRenderTargetActive(void)
{
	return fsVrHudRenderActive;
}

extern "C" void FsVrGetHudRenderSize(int *w,int *h)
{
	if(nullptr!=w)
	{
		*w=fsVrHudRenderW;
	}
	if(nullptr!=h)
	{
		*h=fsVrHudRenderH;
	}
}

static float fsVrAircraftState[8]={0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};

extern "C" float *FsVrAircraftStateDataPointer(void)
{
	return fsVrAircraftState;
}

// See fsvr.h's FsVrPerfDataPointer doc comment for the slot layout.
static float fsVrPerfData[16]=
{
	0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,
	0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f
};

extern "C" float *FsVrPerfDataPointer(void)
{
	return fsVrPerfData;
}

double FsVrPerfNow(void)
{
#ifdef __EMSCRIPTEN__
	return emscripten_get_now();
#else
	// Coarse (millisecond-resolution) fallback -- non-Emscripten builds
	// never read fsVrPerfData for anything real, this just keeps the
	// instrumentation in fssimulation.cpp / fslazywindow_emscripten.cpp
	// compiling on every platform.
	return 1000.0*(double)clock()/(double)CLOCKS_PER_SEC;
#endif
}

void FsVrPerfAccumulate(int slot,double ms)
{
	if(slot<0 || 16<=slot)
	{
		return;
	}
	fsVrPerfData[slot]=(0.0f==fsVrPerfData[slot] ? (float)ms : fsVrPerfData[slot]*0.95f+(float)ms*0.05f);
}

static float fsVrGuiData[8]={0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};

extern "C" float *FsVrGuiDataPointer(void)
{
	return fsVrGuiData;
}

static float fsVrMenuData[8]={0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};

extern "C" float *FsVrMenuDataPointer(void)
{
	return fsVrMenuData;
}

static int fsVrMenuPassActive=0;

extern "C" void FsVrSetMenuPassActive(int active)
{
	fsVrMenuPassActive=(0!=active ? 1 : 0);
}

extern "C" int FsVrIsMenuPassActive(void)
{
	return fsVrMenuPassActive;
}

#define FSVR_GUIMENU_CAP 4096

static char fsVrGuiMenu[FSVR_GUIMENU_CAP];
static int fsVrGuiMenuLen=0;
static int fsVrGuiMenuVersion=0;

extern "C" const char *FsVrGuiMenuPointer(void)
{
	return fsVrGuiMenu;
}

extern "C" int FsVrGuiMenuLength(void)
{
	return fsVrGuiMenuLen;
}

extern "C" int FsVrGuiMenuVersion(void)
{
	return fsVrGuiMenuVersion;
}

void FsVrSetGuiMenu(const char *utf8,int len)
{
	if(NULL==utf8 || len<0)
	{
		len=0;
	}
	if(FSVR_GUIMENU_CAP<len)
	{
		len=FSVR_GUIMENU_CAP;
	}
	if(len!=fsVrGuiMenuLen || (0<len && 0!=memcmp(fsVrGuiMenu,utf8,len)))
	{
		if(0<len)
		{
			memcpy(fsVrGuiMenu,utf8,len);
		}
		fsVrGuiMenuLen=len;
		++fsVrGuiMenuVersion;
	}
}

static float fsVrBlackoutOverride[5]={0.0f,0.0f,0.0f,0.0f,0.0f};

extern "C" float *FsVrBlackoutOverridePointer(void)
{
	return fsVrBlackoutOverride;
}

extern "C" void FsVrSetBlackoutOverride(int active,float r,float g,float b,float alpha)
{
	fsVrBlackoutOverride[0]=(0!=active ? 1.0f : 0.0f);
	fsVrBlackoutOverride[1]=r;
	fsVrBlackoutOverride[2]=g;
	fsVrBlackoutOverride[3]=b;
	fsVrBlackoutOverride[4]=alpha;
}

void FsVrMarkSimDrawn(void)
{
	if(fsVrSimDrawnFrame<0x40000000)
	{
		++fsVrSimDrawnFrame;
	}
}

void FsVrGetEyeFrustum(
    int eye,const double &nearz,const double &farz,
    double &lft,double &rit,double &btm,double &top)
{
	const float *eyeData=FsVrEyeDataPointer(eye);
	lft=-(double)eyeData[0]*nearz;
	rit= (double)eyeData[1]*nearz;
	top= (double)eyeData[2]*nearz;
	btm=-(double)eyeData[3]*nearz;
	(void)farz;
}

const float *FsVrEyeViewMatrix(int eye)
{
	return FsVrEyeDataPointer(eye)+4;
}

void FsVrGetEyeViewport(int eye,int &x0,int &y0,int &wid,int &hei)
{
	const float *eyeData=FsVrEyeDataPointer(eye);
	x0 =(int)eyeData[20];
	y0 =(int)eyeData[21];
	wid=(int)eyeData[22];
	hei=(int)eyeData[23];
}

// ---- Network-lobby automation (ysflight-xrift; see fsvr.h) -----------------

static char fsVrNetJoinAir[256]="";
static char fsVrNetJoinStp[256]="";
static int fsVrNetJoinIff=-1;
static int fsVrNetJoinPending=0;
static int fsVrNetLeavePending=0;

extern "C" void FsVrNetRequestJoin(const char air[],const char stp[],int iff)
{
	strncpy(fsVrNetJoinAir,(NULL!=air ? air : ""),sizeof(fsVrNetJoinAir)-1);
	fsVrNetJoinAir[sizeof(fsVrNetJoinAir)-1]=0;
	strncpy(fsVrNetJoinStp,(NULL!=stp ? stp : ""),sizeof(fsVrNetJoinStp)-1);
	fsVrNetJoinStp[sizeof(fsVrNetJoinStp)-1]=0;
	fsVrNetJoinIff=iff;
	fsVrNetJoinPending=1;
	fsVrNetLeavePending=0;
}

extern "C" void FsVrNetRequestLeave(void)
{
	fsVrNetLeavePending=1;
	fsVrNetJoinPending=0;
}

extern "C" int FsVrNetTakeJoinRequest(char air[],int airLen,char stp[],int stpLen,int *iff)
{
	if(0==fsVrNetJoinPending)
	{
		return 0;
	}
	strncpy(air,fsVrNetJoinAir,airLen-1);
	air[airLen-1]=0;
	strncpy(stp,fsVrNetJoinStp,stpLen-1);
	stp[stpLen-1]=0;
	if(NULL!=iff)
	{
		*iff=fsVrNetJoinIff;
	}
	fsVrNetJoinPending=0;
	return 1;
}

extern "C" int FsVrNetTakeLeaveRequest(void)
{
	const int r=fsVrNetLeavePending;
	fsVrNetLeavePending=0;
	return r;
}

static float fsVrPreview[8]={0.0f};
static char fsVrPreviewAir[256]="";

extern "C" float *FsVrPreviewDataPointer(void)
{
	return fsVrPreview;
}

extern "C" void FsVrPreviewSetAircraft(const char air[])
{
	strncpy(fsVrPreviewAir,(NULL!=air ? air : ""),sizeof(fsVrPreviewAir)-1);
	fsVrPreviewAir[sizeof(fsVrPreviewAir)-1]=0;
	fsVrPreview[5]=0.0f;
}

extern "C" const char *FsVrPreviewAircraft(void)
{
	return fsVrPreviewAir;
}

static int fsVrNetCmd[8]={0};
static int fsVrNetCmdN=0;

extern "C" void FsVrNetRequestCommand(int cmd)
{
	if(0<cmd && fsVrNetCmdN<8)
	{
		fsVrNetCmd[fsVrNetCmdN++]=cmd;
	}
}

extern "C" int FsVrNetTakeCommand(void)
{
	if(0==fsVrNetCmdN)
	{
		return 0;
	}
	const int cmd=fsVrNetCmd[0];
	for(int i=1; i<fsVrNetCmdN; ++i)
	{
		fsVrNetCmd[i-1]=fsVrNetCmd[i];
	}
	--fsVrNetCmdN;
	return cmd;
}

#define FSVR_CONSOLE_LINES 64
static char fsVrConsole[FSVR_CONSOLE_LINES][200]={{0}};
static int fsVrConsoleSeq=0;
static int fsVrConsoleMuted=0;

extern "C" void FsVrConsoleMute(int mute)
{
	fsVrConsoleMuted=mute;
}

extern "C" void FsVrConsoleAppend(const char utf8[])
{
	if(0!=fsVrConsoleMuted || NULL==utf8 || 0==utf8[0])
	{
		return;
	}
	char *dst=fsVrConsole[fsVrConsoleSeq%FSVR_CONSOLE_LINES];
	strncpy(dst,utf8,199);
	dst[199]=0;
	// Do not leave half of a UTF-8 sequence at the cut.
	int n=(int)strlen(dst);
	if(199==n)
	{
		while(0<n && 0x80==(((unsigned char)dst[n-1])&0xC0))
		{
			--n;
		}
		if(0<n && 0!=(((unsigned char)dst[n-1])&0x80))
		{
			--n;
		}
		dst[n]=0;
	}
	++fsVrConsoleSeq;
}

extern "C" int FsVrConsoleSeq(void)
{
	return fsVrConsoleSeq;
}

extern "C" const char *FsVrConsoleLine(int back)
{
	if(back<0 || FSVR_CONSOLE_LINES<=back || fsVrConsoleSeq<=back)
	{
		return "";
	}
	return fsVrConsole[(fsVrConsoleSeq-1-back)%FSVR_CONSOLE_LINES];
}

static float fsVrNetState[16]={0.0f};

extern "C" float *FsVrNetStateDataPointer(void)
{
	return fsVrNetState;
}

static float fsVrAirList[1+FsVrAirListMax*24]={0.0f};
static char fsVrAirListId[FsVrAirListMax][64]={{0}};

extern "C" float *FsVrAirListDataPointer(void)
{
	return fsVrAirList;
}

extern "C" const char *FsVrAirListIdentifier(int i)
{
	if(i<0 || FsVrAirListMax<=i)
	{
		return "";
	}
	return fsVrAirListId[i];
}

extern "C" char *FsVrAirListIdentifierBuf(int i)
{
	if(i<0 || FsVrAirListMax<=i)
	{
		i=0;
	}
	return fsVrAirListId[i];
}

static char fsVrAirListName[FsVrAirListMax][64]={{0}};

extern "C" const char *FsVrAirListName(int i)
{
	if(i<0 || FsVrAirListMax<=i)
	{
		return "";
	}
	return fsVrAirListName[i];
}

extern "C" char *FsVrAirListNameBuf(int i)
{
	if(i<0 || FsVrAirListMax<=i)
	{
		i=0;
	}
	return fsVrAirListName[i];
}

static char fsVrLobbyAir[FsVrLobbyAirMax][64]={{0}};
static char fsVrLobbyStp[FsVrLobbyStpMax][64]={{0}};
static int fsVrLobbyNumAir=0,fsVrLobbyNumStp=0;

extern "C" int FsVrLobbyAirCount(void)
{
	return fsVrLobbyNumAir;
}
extern "C" const char *FsVrLobbyAirName(int i)
{
	return (0<=i && i<fsVrLobbyNumAir) ? fsVrLobbyAir[i] : "";
}
extern "C" int FsVrLobbyStpCount(void)
{
	return fsVrLobbyNumStp;
}
extern "C" const char *FsVrLobbyStpName(int i)
{
	return (0<=i && i<fsVrLobbyNumStp) ? fsVrLobbyStp[i] : "";
}
extern "C" void FsVrLobbySetAir(int i,const char name[])
{
	if(0<=i && i<FsVrLobbyAirMax)
	{
		strncpy(fsVrLobbyAir[i],(NULL!=name ? name : ""),63);
		fsVrLobbyAir[i][63]=0;
	}
}
extern "C" void FsVrLobbySetStp(int i,const char name[])
{
	if(0<=i && i<FsVrLobbyStpMax)
	{
		strncpy(fsVrLobbyStp[i],(NULL!=name ? name : ""),63);
		fsVrLobbyStp[i][63]=0;
	}
}
static float fsVrLobbyStpPos[FsVrLobbyStpMax][3]={{0.0f}};
extern "C" float *FsVrLobbyStpPos(int i)
{
	static float none[3]={0.0f,0.0f,0.0f};
	return (0<=i && i<FsVrLobbyStpMax) ? fsVrLobbyStpPos[i] : none;
}
extern "C" void FsVrLobbySetStpPos(int i,float x,float y,float z)
{
	if(0<=i && i<FsVrLobbyStpMax)
	{
		fsVrLobbyStpPos[i][0]=x;
		fsVrLobbyStpPos[i][1]=y;
		fsVrLobbyStpPos[i][2]=z;
	}
}
static float fsVrLobbyStpHeading[FsVrLobbyStpMax]={0.0f};
extern "C" float FsVrLobbyStpHeading(int i)
{
	return (0<=i && i<FsVrLobbyStpMax) ? fsVrLobbyStpHeading[i] : 0.0f;
}
extern "C" void FsVrLobbySetStpHeading(int i,float h)
{
	if(0<=i && i<FsVrLobbyStpMax)
	{
		fsVrLobbyStpHeading[i]=h;
	}
}
static char fsVrLobbyField[FsVrLobbyFieldMax][64]={{0}};
static int fsVrLobbyNumField=0;
extern "C" int FsVrLobbyFieldCount(void)
{
	return fsVrLobbyNumField;
}
extern "C" const char *FsVrLobbyFieldName(int i)
{
	return (0<=i && i<fsVrLobbyNumField) ? fsVrLobbyField[i] : "";
}
extern "C" void FsVrLobbySetField(int i,const char name[])
{
	if(0<=i && i<FsVrLobbyFieldMax)
	{
		strncpy(fsVrLobbyField[i],(NULL!=name ? name : ""),63);
		fsVrLobbyField[i][63]=0;
	}
}
extern "C" void FsVrLobbySetFieldCount(int n)
{
	fsVrLobbyNumField=(n<0 ? 0 : (FsVrLobbyFieldMax<n ? FsVrLobbyFieldMax : n));
}
extern "C" void FsVrLobbySetCounts(int nAir,int nStp)
{
	fsVrLobbyNumAir=(nAir<0 ? 0 : (FsVrLobbyAirMax<nAir ? FsVrLobbyAirMax : nAir));
	fsVrLobbyNumStp=(nStp<0 ? 0 : (FsVrLobbyStpMax<nStp ? FsVrLobbyStpMax : nStp));
}

static float fsVrGroundProbe[8]={0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
extern "C" float *FsVrGroundProbeDataPointer(void)
{
	return fsVrGroundProbe;
}

static float fsVrFlightData[16]={0.0f};

extern "C" float *FsVrFlightDataPointer(void)
{
	return fsVrFlightData;
}
