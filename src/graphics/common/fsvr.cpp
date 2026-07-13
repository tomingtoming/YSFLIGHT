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

static float fsVrHudData[8]={0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};

extern "C" float *FsVrHudDataPointer(void)
{
	return fsVrHudData;
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
