#ifndef FSBLACKOUT_IS_INCLUDED
#define FSBLACKOUT_IS_INCLUDED
/* { */

#include <ysglcpp.h>
#include <ysglbuffer.h>

void FsMakeBlackOutPolygon(class YsGLVertexBuffer2D &vtxBuf,class YsGLColorBuffer &colBuf,const double G);

/*! Shared blackout(dark)/redout(red, negative-G) intensity+colour
    computation, factored out of FsMakeBlackOutPolygon's per-ring alpha math
    so the flat 2D vignette and the VR full-field tint
    (FsVrDrawFullScreenTint, called from FsSimulation::SimDrawAllScreen's
    multiview branch) can never drift apart on WHEN the effect kicks in or
    what colour it is -- only HOW it is presented (radial screen-space rings
    in flat play; a single flat world-space tint quad in VR, since VR has no
    one "screen centre" to radiate from) differs.
    Returns YSFALSE (r=g=b=alpha=0) when G is within
    [minusGLimit,plusGLimit] (the caller's early-out -- no effect active).
    Otherwise returns YSTRUE with r,g,b set (black for G>0, red for G<0,
    matching FsMakeBlackOutPolygon exactly) and alpha = the
    bounded-to-[0,1] linear "blackness" scalar the flat vignette's
    innermost ring also starts from (ring i=0: alpha=blackness*(1+0/5)^2 =
    blackness) -- the natural "how affected is even dead-centre vision"
    value for a UNIFORM full-field tint (clamping this to [0,1] before
    handing it to FsMakeBlackOutPolygon's own per-ring formula changes
    nothing there: every ring multiplier (1+i/5)^2, i=0..5, is >=1, and each
    ring's OWN alpha is independently bounded to [0,1] anyway, so a raw
    blackness>1 already saturated every ring to 1 either way). */
YSBOOL FsComputeBlackoutTint(const double G,double &r,double &g,double &b,double &alpha);

/* } */
#endif
