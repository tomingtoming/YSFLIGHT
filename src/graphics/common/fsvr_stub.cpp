#include <ysclass.h>
#include "fsopengl.h"
#include "fsvr.h"

// GL1, D3D9, and the console renderer do not support VR render targets.
// Each backend links these definitions instead of the GL2 implementation.
int FsVrShadowMapMultiviewReady(int,int)
{
	return 0;
}

void FsVrBindShadowMapMultiviewFbo(void)
{
}

void FsVrBlitShadowMapFromMultiview(int,int)
{
}

void FsVrBeginHudRender(void)
{
}

void FsVrEndHudRender(void)
{
}

void FsVrDrawHudQuad(const float[12])
{
}

void FsVrDrawReticle(const float[24],const YsColor &)
{
}

void FsVrBeginHandPropDraw(void)
{
}

void FsVrEndHandPropDraw(void)
{
}

void FsVrBeginHandCtlModelDraw(void)
{
}

void FsVrEndHandCtlModelDraw(void)
{
}

void FsVrBeginGuiRender(void)
{
}

void FsVrEndGuiRender(void)
{
}

void FsVrDrawGuiQuad(const float[12])
{
}

void FsVrBeginMenuRender(void)
{
}

void FsVrEndMenuRender(void)
{
}

void FsVrDrawFullScreenTint(const float[12],float,float,float,float)
{
}
