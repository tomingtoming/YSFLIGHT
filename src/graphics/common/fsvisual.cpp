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
