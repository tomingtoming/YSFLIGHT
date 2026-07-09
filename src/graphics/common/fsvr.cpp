#include "fsvr.h"

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
