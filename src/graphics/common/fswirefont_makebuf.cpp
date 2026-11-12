#include "fswirefont.h"

extern int *FsFontSet[];
extern int FsWireFontWidth;
extern int FsWireFontHeight;

void FsMakeWireFontVertexBuffer(
    YsGLVertexBuffer &lineBuf,YsGLColorBuffer &lineColBuf,
    YsGLVertexBuffer &triBuf,YsGLColorBuffer &triColBuf,
    float x,float y,float z,const char str[],YsColor col,float fontWid,float fontHei)
{
	lineBuf.CleanUp();
	triBuf.CleanUp();
	FsAddWireFontVertexBuffer(lineBuf,lineColBuf,triBuf,triColBuf,x,y,z,str,col,fontWid,fontHei);
}

void FsAddWireFontVertexBuffer(
    YsGLVertexBuffer &lineBuf,YsGLColorBuffer &lineColBuf,
    YsGLVertexBuffer &triBuf,YsGLColorBuffer &triColBuf,
    float x,float y,float z,const char str[],YsColor col,float fontWid,float fontHei)
{
	const auto x0=x,y0=y;
	for(int i=0; 0<str[i]; i++)
	{
		if(str[i]=='\n' || str[i]=='\a' || str[i]=='\r')
		{
			x=x0;
			y+=fontHei;
			continue;
		}

		auto ptr=FsFontSet[str[i]];
		if(nullptr!=ptr)
		{
			ptr++;  // Skip charactor code
			while(ptr[0]!=-1)
			{
				switch(ptr[0])
				{
				default:
				case 0:
					if(3<=ptr[1])
					{
						float tx0=x+fontWid*(float)ptr[2]/((float)FsWireFontWidth*1.1F);
						float ty0=y+fontHei*(float)ptr[3]/((float)FsWireFontHeight*1.1F);
						for(int j=1; j<ptr[1]-1; j++)
						{
							triBuf.Add(tx0,ty0,z);
							triBuf.Add(
								(x+fontWid*(float)ptr[2+j*2]/((float)FsWireFontWidth*1.1F)),
								(y+fontHei*(float)ptr[3+j*2]/((float)FsWireFontHeight*1.1F)),
								z);
							triBuf.Add(
								(x+fontWid*(float)ptr[4+j*2]/((float)FsWireFontWidth*1.1F)),
								(y+fontHei*(float)ptr[5+j*2]/((float)FsWireFontHeight*1.1F)),
								z);
							triColBuf.Add(col);
							triColBuf.Add(col);
							triColBuf.Add(col);
						}
					}
					break;
				case 1:
					{
						for(int j=0; j<ptr[1]-1; j++)
						{
							lineBuf.Add(
								(x+fontWid*(float)ptr[2+j*2]/((float)FsWireFontWidth*1.1F)),
								(y+fontHei*(float)ptr[3+j*2]/((float)FsWireFontHeight*1.1F)),
								z);
							lineBuf.Add(
								(x+fontWid*(float)ptr[4+j*2]/((float)FsWireFontWidth*1.1F)),
								(y+fontHei*(float)ptr[5+j*2]/((float)FsWireFontHeight*1.1F)),
								z);
							lineColBuf.Add(col);
							lineColBuf.Add(col);
						}
					}
					break;
				case 2:
					{
						for(int j=0; j<=ptr[1]-2; j+=2)
						{
							lineBuf.Add(
								(x+fontWid*(float)ptr[2+j*2]/((float)FsWireFontWidth*1.1F)),
								(y+fontHei*(float)ptr[3+j*2]/((float)FsWireFontHeight*1.1F)),
								z);
							lineBuf.Add(
								(x+fontWid*(float)ptr[4+j*2]/((float)FsWireFontWidth*1.1F)),
								(y+fontHei*(float)ptr[5+j*2]/((float)FsWireFontHeight*1.1F)),
								z);
							lineColBuf.Add(col);
							lineColBuf.Add(col);
						}
					}
					break;
				}
				ptr=ptr+2+ptr[1]*2;
			}
		}
		x+=fontWid;
	}
}

////////////////////////////////////////////////////////////

void FsMakeWireFontVertexBuffer(
    YsGLVertexBuffer &lineBuf,YsGLColorBuffer &lineColBuf,
    YsGLVertexBuffer &triBuf,YsGLColorBuffer &triColBuf,
    const YsMatrix4x4 &tfm,const char str[],YsColor col)
{
	lineBuf.CleanUp();
	triBuf.CleanUp();
	FsAddWireFontVertexBuffer(lineBuf,lineColBuf,triBuf,triColBuf,tfm,str,col);
}

void FsAddWireFontVertexBuffer(
    YsGLVertexBuffer &lineBuf,YsGLColorBuffer &lineColBuf,
    YsGLVertexBuffer &triBuf,YsGLColorBuffer &triColBuf,
    const YsMatrix4x4 &_tfm,const char str[],YsColor col)
{
	auto tfm=_tfm;
	int lineNum=0;
	for(int i=0; 0<str[i]; i++)
	{
		if(str[i]=='\n' || str[i]=='\a' || str[i]=='\r')
		{
			++lineNum;
			tfm=_tfm;
			for(int j=0; j<lineNum; ++j)
			{
				tfm.Translate(0,1,0);
			}
			continue;
		}

		auto ptr=FsFontSet[str[i]];
		if(nullptr!=ptr)
		{
			ptr++;  // Skip charactor code
			while(ptr[0]!=-1)
			{
				switch(ptr[0])
				{
				default:
				case 0:
					if(3<=ptr[1])
					{
						const YsVec3 vtx0=tfm*YsVec3((float)ptr[2]/((float)FsWireFontWidth*1.1F),(float)ptr[3]/((float)FsWireFontHeight*1.1F),0);
						for(int j=1; j<ptr[1]-1; j++)
						{
							const YsVec3 triVtPos[2]=
							{
								tfm*YsVec3((float)ptr[2+j*2]/((float)FsWireFontWidth*1.1F),(float)ptr[3+j*2]/((float)FsWireFontHeight*1.1F),0),
								tfm*YsVec3((float)ptr[4+j*2]/((float)FsWireFontWidth*1.1F),(float)ptr[5+j*2]/((float)FsWireFontHeight*1.1F),0)
							};
							triBuf.Add(vtx0);
							triBuf.Add(triVtPos[0]);
							triBuf.Add(triVtPos[1]);
							triColBuf.Add(col);
							triColBuf.Add(col);
							triColBuf.Add(col);
						}
					}
					break;
				case 1:
					{
						for(int j=0; j<ptr[1]-1; j++)
						{
							const YsVec3 lineVtPos[2]=
							{
								tfm*YsVec3((float)ptr[2+j*2]/((float)FsWireFontWidth*1.1F),(float)ptr[3+j*2]/((float)FsWireFontHeight*1.1F),0),
								tfm*YsVec3((float)ptr[4+j*2]/((float)FsWireFontWidth*1.1F),(float)ptr[5+j*2]/((float)FsWireFontHeight*1.1F),0)
							};
							lineBuf.Add(lineVtPos[0]);
							lineBuf.Add(lineVtPos[1]);
							lineColBuf.Add(col);
							lineColBuf.Add(col);
						}
					}
					break;
				case 2:
					{
						for(int j=0; j<=ptr[1]-2; j+=2)
						{
							const YsVec3 lineVtPos[2]=
							{
								tfm*YsVec3((float)ptr[2+j*2]/((float)FsWireFontWidth*1.1F),(float)ptr[3+j*2]/((float)FsWireFontHeight*1.1F),0),
								tfm*YsVec3((float)ptr[4+j*2]/((float)FsWireFontWidth*1.1F),(float)ptr[5+j*2]/((float)FsWireFontHeight*1.1F),0)
							};
							lineBuf.Add(lineVtPos[0]);
							lineBuf.Add(lineVtPos[1]);
							lineColBuf.Add(col);
							lineColBuf.Add(col);
						}
					}
					break;
				}
				ptr=ptr+2+ptr[1]*2;
			}
		}
		tfm.Translate(1.0,0.0,0.0);
	}
}

////////////////////////////////////////////////////////////

void FsMakeWireFontVertexBuffer(
    YsGLVertexBuffer &lineBuf,YsGLColorBuffer &lineColBuf,
    YsGLVertexBuffer &triBuf,YsGLColorBuffer &triColBuf,
    float x,float y,float z,const char str[],YsColor col,float fontWid,float fontHei,
    YSSIDE sideToDraw,YsConstArrayMask <YsVec2> trimPlg,YsRect2 trimPlgBbx)
{
	lineBuf.CleanUp();
	lineColBuf.CleanUp();
	triBuf.CleanUp();
	triColBuf.CleanUp();
	FsAddWireFontVertexBuffer(
	    lineBuf,lineColBuf,
	    triBuf,triColBuf,
	    x,y,z,str,col,fontWid,fontHei,
	    sideToDraw,trimPlg,trimPlgBbx);
}

// Result of classifying one whole glyph against the trimming polygon.
//   FSWIREFONT_TRIM_PER_SEGMENT  The glyph may straddle the polygon boundary.  Clip segment by segment.
//   FSWIREFONT_TRIM_KEEP_ALL     Every segment of the glyph is entirely on the side to draw.
//   FSWIREFONT_TRIM_DROP_ALL     Every segment of the glyph is entirely on the other side.
enum FSWIREFONT_TRIM_MODE
{
	FSWIREFONT_TRIM_PER_SEGMENT,
	FSWIREFONT_TRIM_KEEP_ALL,
	FSWIREFONT_TRIM_DROP_ALL
};

// Decides, once per glyph, what YsClipLineByPolygon would answer for every
// segment of the glyph.  A HUD string is typically either entirely inside or
// entirely outside the trimming polygon, in which case clipping each segment
// against each polygon edge (and running a point-in-polygon test per segment)
// only reproduces the same answer many times.
//
// The classification is conservative.  It returns KEEP_ALL or DROP_ALL only
// when the answer of the per-segment clipping is known to be the same for
// every segment that fits in the glyph's bounding box:
//   (1) The box is strictly outside the bounding box of the polygon.  This is
//       the same condition as the early-out of YsClipLineByPolygon.
//   (2) No polygon edge comes near the box (the box is inflated by a margin
//       that is far greater than the tolerance of the intersection test),
//       so no segment in the box can intersect the polygon, and the side of
//       the box centre is the side of every point in the box.
// Otherwise it returns PER_SEGMENT and the caller falls back to the exact
// per-segment clipping, so the generated vertices are unchanged.
static FSWIREFONT_TRIM_MODE FsClassifyWireFontGlyphForTrim(
    float minx,float miny,float maxx,float maxy,
    YSSIDE sideToDraw,YsConstArrayMask <YsVec2> trimPlg,const YsRect2 &trimPlgBbx)
{
	if(YSINSIDE!=sideToDraw && YSOUTSIDE!=sideToDraw)
	{
		return FSWIREFONT_TRIM_PER_SEGMENT;
	}

	if(maxx<trimPlgBbx.Min().x() || maxy<trimPlgBbx.Min().y() ||
	   minx>trimPlgBbx.Max().x() || miny>trimPlgBbx.Max().y())
	{
		return (YSOUTSIDE==sideToDraw ? FSWIREFONT_TRIM_KEEP_ALL : FSWIREFONT_TRIM_DROP_ALL);
	}

	const double margin=0.05*(double)YsGreater(maxx-minx,maxy-miny)+1e-6;
	const double bx0=(double)minx-margin,by0=(double)miny-margin;
	const double bx1=(double)maxx+margin,by1=(double)maxy+margin;
	for(auto idx : trimPlg.AllIndex())
	{
		auto &q0=trimPlg[idx];
		auto &q1=trimPlg.GetCyclic(idx+1);
		if((q0.x()<bx0 && q1.x()<bx0) || (q0.x()>bx1 && q1.x()>bx1) ||
		   (q0.y()<by0 && q1.y()<by0) || (q0.y()>by1 && q1.y()>by1))
		{
			continue; // The edge is apart from the box along X or Y.
		}

		// If the four corners of the box are strictly on one side of the
		// line of the edge, the edge is apart from the box.
		const double nx=q1.y()-q0.y(),ny=q0.x()-q1.x();
		const double d00=(bx0-q0.x())*nx+(by0-q0.y())*ny;
		const double d10=(bx1-q0.x())*nx+(by0-q0.y())*ny;
		const double d01=(bx0-q0.x())*nx+(by1-q0.y())*ny;
		const double d11=(bx1-q0.x())*nx+(by1-q0.y())*ny;
		if((0.0<d00 && 0.0<d10 && 0.0<d01 && 0.0<d11) ||
		   (0.0>d00 && 0.0>d10 && 0.0>d01 && 0.0>d11))
		{
			continue;
		}
		return FSWIREFONT_TRIM_PER_SEGMENT;
	}

	const YsVec2 cen(((double)minx+(double)maxx)/2.0,((double)miny+(double)maxy)/2.0);
	const auto side=YsCheckInsidePolygon2(cen,trimPlg.GetN(),trimPlg);
	if(YSINSIDE==side)
	{
		return (YSINSIDE==sideToDraw ? FSWIREFONT_TRIM_KEEP_ALL : FSWIREFONT_TRIM_DROP_ALL);
	}
	else if(YSOUTSIDE==side)
	{
		return (YSOUTSIDE==sideToDraw ? FSWIREFONT_TRIM_KEEP_ALL : FSWIREFONT_TRIM_DROP_ALL);
	}
	return FSWIREFONT_TRIM_PER_SEGMENT;
}

// Bounding box of every point that the glyph refers to, in the same
// coordinates as the vertices generated by FsAddWireFontVertexBuffer.
static void FsGetWireFontGlyphBoundingBox(
    float &minx,float &miny,float &maxx,float &maxy,
    const int *ptr,float x,float y,float fontWid,float fontHei)
{
	minx=x;
	miny=y;
	maxx=x;
	maxy=y;
	while(ptr[0]!=-1)
	{
		for(int j=0; j<ptr[1]; j++)
		{
			float xx=(x+fontWid*(float)ptr[2+j*2]/((float)FsWireFontWidth*1.1F));
			float yy=(y+fontHei*(float)ptr[3+j*2]/((float)FsWireFontHeight*1.1F));
			minx=YsSmaller(minx,xx);
			miny=YsSmaller(miny,yy);
			maxx=YsGreater(maxx,xx);
			maxy=YsGreater(maxy,yy);
		}
		ptr=ptr+2+ptr[1]*2;
	}
}

static void FsAddWireFontVertexBufferLineTrim(
    YsGLVertexBuffer &lineBuf,YsGLColorBuffer &lineColBuf,
    float x0,float y0,float z0,float x1,float y1,float z1,YsColor col,
    YSSIDE sideToDraw,YsConstArrayMask <YsVec2> trimPlg,YsRect2 trimPlgBbx,
    FSWIREFONT_TRIM_MODE trimMode)
{
	if(FSWIREFONT_TRIM_DROP_ALL==trimMode)
	{
		return;
	}
	if(FSWIREFONT_TRIM_KEEP_ALL==trimMode)
	{
		// Same vertices as the per-segment clipping emits for a segment
		// that is entirely on the side to draw.
		lineBuf.Add<double>((double)x0,(double)y0,z0);
		lineBuf.Add<double>((double)x1,(double)y1,z1);
		lineColBuf.Add(col);
		lineColBuf.Add(col);
		return;
	}

	YsArray <YsVec2,2> inside,outside,toDraw;
	YsClipLineByPolygon(inside,outside,YsVec2(x0,y0),YsVec2(x1,y1),trimPlg,trimPlgBbx);
	if(YSINSIDE==sideToDraw)
	{
		toDraw.MoveFrom(inside);
	}
	else if(YSOUTSIDE==sideToDraw)
	{
		toDraw.MoveFrom(outside);
	}
	for(int i=0; i<toDraw.GetN()-1; i+=2)
	{
		lineBuf.Add<double>(toDraw[i  ].x(),toDraw[i  ].y(),z0);
		lineBuf.Add<double>(toDraw[i+1].x(),toDraw[i+1].y(),z1);
		lineColBuf.Add(col);
		lineColBuf.Add(col);
	}
}

void FsAddWireFontVertexBuffer(
    YsGLVertexBuffer &lineBuf,YsGLColorBuffer &lineColBuf,
    YsGLVertexBuffer &triBuf,YsGLColorBuffer &triColBuf,
    float x,float y,float z,const char str[],YsColor col,float fontWid,float fontHei,
    YSSIDE sideToDraw,YsConstArrayMask <YsVec2> trimPlg,YsRect2 trimPlgBbx)
{
	const auto x0=x,y0=y;
	for(int i=0; 0<str[i]; i++)
	{
		if(str[i]=='\n' || str[i]=='\a' || str[i]=='\r')
		{
			x=x0;
			y+=fontHei;
			continue;
		}

		auto ptr=FsFontSet[str[i]];
		if(nullptr!=ptr)
		{
			ptr++;  // Skip charactor code

			float minx,miny,maxx,maxy;
			FsGetWireFontGlyphBoundingBox(minx,miny,maxx,maxy,ptr,x,y,fontWid,fontHei);
			const auto trimMode=FsClassifyWireFontGlyphForTrim(minx,miny,maxx,maxy,sideToDraw,trimPlg,trimPlgBbx);

			while(ptr[0]!=-1)
			{
				switch(ptr[0])
				{
				default:
				case 0:
					if(3<=ptr[1])
					{
						float tx0,ty0,lastX,lastY;
						for(int j=0; j<ptr[1]; j++)
						{
							float xx=(x+fontWid*(float)ptr[2+j*2]/((float)FsWireFontWidth*1.1F));
							float yy=(y+fontHei*(float)ptr[3+j*2]/((float)FsWireFontHeight*1.1F));
							if(0==j)
							{
								tx0=xx;
								ty0=yy;
							}
							else
							{
								FsAddWireFontVertexBufferLineTrim(
								    lineBuf,lineColBuf,
								    lastX,lastY,z,xx,yy,z,col,
								    sideToDraw,trimPlg,trimPlgBbx,trimMode);
							}
							lastX=xx;
							lastY=yy;
						}
						FsAddWireFontVertexBufferLineTrim(
						    lineBuf,lineColBuf,
						    lastX,lastY,z,tx0,ty0,z,col,
						    sideToDraw,trimPlg,trimPlgBbx,trimMode);
					}
					break;
				case 1:
					{
						for(int j=0; j<ptr[1]-1; j++)
						{
							float x0=(x+fontWid*(float)ptr[2+j*2]/((float)FsWireFontWidth*1.1F));
							float y0=(y+fontHei*(float)ptr[3+j*2]/((float)FsWireFontHeight*1.1F));
							float x1=(x+fontWid*(float)ptr[4+j*2]/((float)FsWireFontWidth*1.1F));
							float y1=(y+fontHei*(float)ptr[5+j*2]/((float)FsWireFontHeight*1.1F));

							FsAddWireFontVertexBufferLineTrim(
							    lineBuf,lineColBuf,
							    x0,y0,z,x1,y1,z,col,
							    sideToDraw,trimPlg,trimPlgBbx,trimMode);
						}
					}
					break;
				case 2:
					{
						for(int j=0; j<=ptr[1]-2; j+=2)
						{
							float x0=(x+fontWid*(float)ptr[2+j*2]/((float)FsWireFontWidth*1.1F));
							float y0=(y+fontHei*(float)ptr[3+j*2]/((float)FsWireFontHeight*1.1F));
							float x1=(x+fontWid*(float)ptr[4+j*2]/((float)FsWireFontWidth*1.1F));
							float y1=(y+fontHei*(float)ptr[5+j*2]/((float)FsWireFontHeight*1.1F));

							FsAddWireFontVertexBufferLineTrim(
							    lineBuf,lineColBuf,
							    x0,y0,z,x1,y1,z,col,
							    sideToDraw,trimPlg,trimPlgBbx,trimMode);
						}
					}
					break;
				}
				ptr=ptr+2+ptr[1]*2;
			}
		}
		x+=fontWid;
	}
}

////////////////////////////////////////////////////////////

void FsMakeWireFontVertexBuffer2D(
    YsGLVertexBuffer2D &lineBuf,YsGLColorBuffer &lineColBuf,
    YsGLVertexBuffer2D &triBuf,YsGLColorBuffer &triColBuf,
    float x,float y,float z,const char str[],YsColor col,float fontWid,float fontHei)
{
	lineBuf.CleanUp();
	triBuf.CleanUp();
	FsAddWireFontVertexBuffer2D(lineBuf,lineColBuf,triBuf,triColBuf,x,y,z,str,col,fontWid,fontHei);
}

void FsAddWireFontVertexBuffer2D(
    YsGLVertexBuffer2D &lineBuf,YsGLColorBuffer &lineColBuf,
    YsGLVertexBuffer2D &triBuf,YsGLColorBuffer &triColBuf,
    float x,float y,float z,const char str[],YsColor col,float fontWid,float fontHei)
{
	const auto x0=x,y0=y;
	for(int i=0; 0<str[i]; i++)
	{
		if(str[i]=='\n' || str[i]=='\a' || str[i]=='\r')
		{
			x=x0;
			y+=fontHei;
			continue;
		}

		auto ptr=FsFontSet[str[i]];
		if(nullptr!=ptr)
		{
			ptr++;  // Skip charactor code
			while(ptr[0]!=-1)
			{
				switch(ptr[0])
				{
				default:
				case 0:
					if(3<=ptr[1])
					{
						float tx0=x+fontWid*(float)ptr[2]/((float)FsWireFontWidth*1.1F);
						float ty0=y+fontHei*(float)ptr[3]/((float)FsWireFontHeight*1.1F);
						for(int j=1; j<ptr[1]-1; j++)
						{
							triBuf.Add(tx0,ty0);
							triBuf.Add(
								(x+fontWid*(float)ptr[2+j*2]/((float)FsWireFontWidth*1.1F)),
								(y+fontHei*(float)ptr[3+j*2]/((float)FsWireFontHeight*1.1F))
								);
							triBuf.Add(
								(x+fontWid*(float)ptr[4+j*2]/((float)FsWireFontWidth*1.1F)),
								(y+fontHei*(float)ptr[5+j*2]/((float)FsWireFontHeight*1.1F))
								);
							triColBuf.Add(col);
							triColBuf.Add(col);
							triColBuf.Add(col);
						}
					}
					break;
				case 1:
					{
						for(int j=0; j<ptr[1]-1; j++)
						{
							lineBuf.Add(
								(x+fontWid*(float)ptr[2+j*2]/((float)FsWireFontWidth*1.1F)),
								(y+fontHei*(float)ptr[3+j*2]/((float)FsWireFontHeight*1.1F))
								);
							lineBuf.Add(
								(x+fontWid*(float)ptr[4+j*2]/((float)FsWireFontWidth*1.1F)),
								(y+fontHei*(float)ptr[5+j*2]/((float)FsWireFontHeight*1.1F))
								);
							lineColBuf.Add(col);
							lineColBuf.Add(col);
						}
					}
					break;
				case 2:
					{
						for(int j=0; j<=ptr[1]-2; j+=2)
						{
							lineBuf.Add(
								(x+fontWid*(float)ptr[2+j*2]/((float)FsWireFontWidth*1.1F)),
								(y+fontHei*(float)ptr[3+j*2]/((float)FsWireFontHeight*1.1F))
								);
							lineBuf.Add(
								(x+fontWid*(float)ptr[4+j*2]/((float)FsWireFontWidth*1.1F)),
								(y+fontHei*(float)ptr[5+j*2]/((float)FsWireFontHeight*1.1F))
								);
							lineColBuf.Add(col);
							lineColBuf.Add(col);
						}
					}
					break;
				}
				ptr=ptr+2+ptr[1]*2;
			}
		}
		x+=fontWid;
	}
}
