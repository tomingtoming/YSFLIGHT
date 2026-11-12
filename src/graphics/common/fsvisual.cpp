#include <ysclass.h>
#include <ysport.h>

#include <ysshelldnmident.h>
#include <ysshellext_orientationutil.h>

#include "fsvisual.h"


FsVisualSrf::FsVisualSrf(const FsVisualSrf &incoming)
{
	YsShellExt::CopyFrom(incoming);
}
FsVisualSrf &FsVisualSrf::operator=(const FsVisualSrf &incoming)
{
	vboSet.CleanUp();
	YsShellExt::CopyFrom(incoming);
	return *this;
}

YSRESULT FsVisualSrf::Load(const wchar_t fn[])
{
	YsFileIO::File fp(fn,"r");
	if(nullptr!=fp.Fp())
	{
		auto inStream=fp.InStream();
		return YsVisualSrf::Load(inStream);
	}
	return YSERR;
}


////////////////////////////////////////////////////////////


FsVisualDnm::FsVisualDnm()
{
	specialRenderingRequirment=RENDER_NORMAL;
	papiAngle=0.0;
}
FsVisualDnm::FsVisualDnm(std::nullptr_t)
{
	specialRenderingRequirment=RENDER_NORMAL;
	papiAngle=0.0;
}
FsVisualDnm::~FsVisualDnm()
{
}
YSRESULT FsVisualDnm::GetBoundingSphere(YsVec3 &cen,double &rad,const YsVec3 &pos,const YsAtt3 &att) const
{
	if(dnmPtr)
	{
		// Bounding box in the model coordinate.  The transformation to the
		// world coordinate is rigid, therefore the radius is unchanged.
		YsBoundingBoxMaker3 mkBbx;
		for(auto nodePtr : dnmPtr->GetRootNodeArray())
		{
			AddNodeBoundingBox(mkBbx,YsIdentity4x4(),nodePtr);
		}

		YsVec3 bbx[2];
		mkBbx.Get(bbx[0],bbx[1]);

		YsMatrix4x4 modelTfm;
		modelTfm.Translate(pos);
		modelTfm.RotateXZ(att.h());
		modelTfm.RotateZY(att.p());
		modelTfm.RotateXY(att.b());

		cen=modelTfm*((bbx[0]+bbx[1])/2.0);
		rad=(bbx[1]-bbx[0]).GetLength()/2.0;
		return YSOK;
	}
	return YSERR;
}

/* static */ YSBOOL FsVisualDnm::IsSphereOutsideOfViewVolume(
    const YsVec3 &cen,const double &rad,const YsMatrix4x4 &viewMat,const YsMatrix4x4 &projMat)
{
	YsVec3 pos=viewMat*cen;
	if(YSLEFT_ZPLUS_YPLUS==YsCoordSysModel)
	{
		pos.SetZ(-pos.z());
	}

	const double inflatedRad=rad*1.001+0.001;
	const double row[4][4]=
	{
		{projMat.v11(),projMat.v12(),projMat.v13(),projMat.v14()},
		{projMat.v21(),projMat.v22(),projMat.v23(),projMat.v24()},
		{projMat.v31(),projMat.v32(),projMat.v33(),projMat.v34()},
		{projMat.v41(),projMat.v42(),projMat.v43(),projMat.v44()}
	};
	for(int axis=0; axis<3; ++axis)
	{
		for(int sign=-1; sign<=1; sign+=2)
		{
			// Same plane equations as YsIsBoundingBoxVisible:  +-row[axis]+row[3]
			const double a=(double)sign*row[axis][0]+row[3][0];
			const double b=(double)sign*row[axis][1]+row[3][1];
			const double c=(double)sign*row[axis][2]+row[3][2];
			const double d=(double)sign*row[axis][3]+row[3][3];
			if(a*pos.x()+b*pos.y()+c*pos.z()+d<-inflatedRad*sqrt(a*a+b*b+c*c))
			{
				return YSTRUE;
			}
		}
	}
	return YSFALSE;
}

void FsVisualDnm::AddNodeBoundingBox(YsBoundingBoxMaker3 &mkBbx,const YsMatrix4x4 &tfm,Dnm::Node *nodePtr) const
{
	auto &nodeState=dnmState.GetState(nodePtr);
	const YsMatrix4x4 newTfm=tfm*nodeState.tfmCache;

	YsVec3 bbx[2];
	nodePtr->GetBoundingBox(bbx);
	for(int i=0; i<8; ++i)
	{
		const YsVec3 pos(bbx[i&1].x(),bbx[(i/2)&1].y(),bbx[(i/4)&1].z());
		mkBbx.Add(newTfm*pos);
	}

	for(auto childPtr : nodePtr->children)
	{
		AddNodeBoundingBox(mkBbx,newTfm,childPtr);
	}
}

YSRESULT FsVisualDnm::Load(const wchar_t fn[])
{
	YsString fnUtf8;
	fnUtf8.EncodeUTF8(fn);

	YsFileIO::File fp(fn,"r");
	if(nullptr!=fp.Fp())
	{
		auto inStream=fp.InStream();
		return YsVisualDnm::Load(inStream,fnUtf8);
	}
	return YSERR;
}

// See fsvisual.h's doc comment.  Same node walk / VBO-prep / two-pass shape
// as YsVisualDnm::Draw(modelView,drawFlag) in ysvisual.cpp, with the
// RenderingOption built from TurnOffAll so the polygonEdge default (YSTRUE,
// meant for the modeler GUI) stays off in play draws.
void FsVisualDnm::DrawSolidNoEdge(const YsMatrix4x4 &modelView) const
{
	if(dnmPtr)
	{
		for(auto nodePtr : dnmPtr->GetRootNodeArray())
		{
			DrawSolidNoEdgeNode(modelView,nodePtr);
		}
	}
}

void FsVisualDnm::DrawSolidNoEdgeNode(const YsMatrix4x4 &tfm,Dnm::Node *nodePtr) const
{
	auto &nodeState=dnmState.GetState(nodePtr);
	const YsMatrix4x4 newTfm=tfm*nodeState.tfmCache;

	if(YSTRUE==nodeState.GetShow())
	{
		if(YSTRUE!=nodePtr->IsPolygonVboPrepared())
		{
			nodePtr->GetDrawingBuffer().RemakePolygonBuffer(*nodePtr,0.8);
			nodePtr->RemakePolygonVbo(nodePtr->GetVboSet(),nodePtr->GetDrawingBuffer());
		}

		YsHasShellExtVboSet::RenderingOption opt;
		opt.TurnOffAll();
		opt.solidPolygon=YSTRUE;
		nodePtr->Render(newTfm,opt);

		opt.TurnOffAll();
		opt.transparentPolygon=YSTRUE;
		opt.light=YSTRUE;
		nodePtr->Render(newTfm,opt);
	}

	for(auto childPtr : nodePtr->children)
	{
		DrawSolidNoEdgeNode(newTfm,childPtr);
	}
}
