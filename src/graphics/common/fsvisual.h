#ifndef FSVISUAL_IS_INCLUDED
#define FSVISUAL_IS_INCLUDED
/* { */

#include <ysvisual.h>


const unsigned int FSVISUAL_DRAWOPAQUE=YsVisual::DRAWOPAQUE;
const unsigned int FSVISUAL_DRAWTRANSPARENT=YsVisual::DRAWTRANSPARENT;
const unsigned int FSVISUAL_DRAWALL=YsVisual::DRAWALL;


class FsVisualSrf : public YsVisualSrf
{
public:
	using YsShell::SetMatrix;
	using YsShell::ShootRayH;
	using YsShell::SetTrustPolygonNormal;

	inline FsVisualSrf(){}
	FsVisualSrf(const FsVisualSrf &incoming);
	FsVisualSrf &operator=(const FsVisualSrf &incoming);

	YSRESULT Load(const wchar_t fn[]);
};



class FsVisualDnm : public YsVisualDnm
{
public:
	FsVisualDnm();
	FsVisualDnm(std::nullptr_t);
	~FsVisualDnm();

	FsVisualDnm &operator=(std::nullptr_t)
	{
		dnmPtr=nullptr;
		return *this;
	}
	bool operator==(std::nullptr_t) const
	{
		return (nullptr==dnmPtr);
	}
	bool operator!=(std::nullptr_t) const
	{
		return (nullptr!=dnmPtr);
	}

	YSRESULT Load(const wchar_t fn[]);

	/*! Draws like YsVisualDnm::Draw(modelView,DRAWALL) but WITHOUT the
	    polygon-edge wireframe pass that Draw's default RenderingOption
	    drags in: on a ~4500-triangle controller shell the edges are pure
	    black fuzz plus per-frame GPU waste, and the upstream edge pass
	    additionally leaves the shared vari-color renderer's uniform color
	    at (0,0,0,0.2) for whatever draws next (it sets, never restores).
	    Used by the hand-held VR controller models (DrawVrHandController). */
	void DrawSolidNoEdge(const YsMatrix4x4 &modelView) const;

private:
	void DrawSolidNoEdgeNode(const YsMatrix4x4 &tfm,Dnm::Node *nodePtr) const;
public:
};

inline bool operator==(std::nullptr_t,const FsVisualDnm &vis)
{
	return (vis==nullptr);
}
inline bool operator!=(std::nullptr_t,const FsVisualDnm &vis)
{
	return (vis!=nullptr);
}

////////////////////////////////////////////////////////////

/* } */
#endif
