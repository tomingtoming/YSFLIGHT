#include <ysclass.h>
#include <yscompilerwarning.h>

#define FSSIMPLEWINDOW_DONT_INCLUDE_OPENGL_HEADERS
#include <fssimplewindow.h>

#include "fs.h"
#include "graphics/common/fsopengl.h"
#include "graphics/common/fsvr.h"
#include "platform/common/fswindow.h"

#include "graphics/common/fsfontrenderer.h"

#include <ysbitmap.h>

#include <ysgl.h>
#include <ysglcpp.h>
#include <ysglslcpp.h>
#include <ysglsldrawfontbitmap.h>
#include <ysglslhudquadrenderer.h>

#include "fsopengl2.0.h"

#include <time.h>


#ifdef _WIN32
#include <windows.h>
#endif


// #define YSOGLERRORCHECK

extern const char *FsProgramTitle;  // Defined in fsmain.cpp

const char *FsMainWindowTitle(void)
{
	// As of 2014/12/12  Main window title must include keyword "Main Window" so that FsWin32GetMainWindowHandle can find the handle.

	static YsString windowTitle;
	windowTitle.Set(FsProgramTitle);
	windowTitle.Append(" Main Window");
	windowTitle.Append(" (OpenGL 2.0 / ES 2.0)");
	return windowTitle;
}



#ifdef YSOGLERRORCHECK
void FsOpenGlShowError(const char tag[])
{
	int err;
	err=glGetError();
	if(err!=GL_NO_ERROR)
	{
		printf("%s ",tag);
		switch(err)
		{
		case GL_NO_ERROR:
			printf("GL_NO_ERROR (%d)\n",err);
			break;
		case GL_INVALID_ENUM:
			printf("GL_INVALID_ENUM (%d)\n",err);
			break;
		case GL_INVALID_VALUE:
			printf("GL_INVALID_VALUE (%d)\n",err);
			break;
		case GL_INVALID_OPERATION:
			printf("GL_INVALID_OPERATION (%d)\n",err);
			break;
		case GL_STACK_OVERFLOW:
			printf("GL_STACK_OVERFLOW (%d)\n",err);
			break;
		case GL_STACK_UNDERFLOW:
			printf("GL_STACK_UNDERFLOW (%d)\n",err);
			break;
		case GL_OUT_OF_MEMORY:
			printf("GL_OUT_OF_MEMORY (%d)\n",err);
			break;
//		case GL_TABLE_TOO_LARGE:
//			printf("GL_TABLE_TOO_LARGE\n");
//			break;
		default:
			printf("Uknown Error (%d)\n",err);
			break;
		}
	}
}
#endif



YSBOOL FsIsConsoleServer(void)
{
	return YSFALSE;
}

YSBOOL FsIsShadowMapAvailable(void)
{
	return YSTRUE;
}

YSBOOL FsIsPointSpriteAvailable(void)
{
	return YSTRUE;
}


#ifdef WIN32
static LARGE_INTEGER frmClock1,frmClock2;
#endif

extern GLuint ysScnGlRwLightTex;
extern GLuint ysScnGlMapTex;



const int fsMaxNumMaskTexture=10;

int fsNumExplosionTex=0;
GLuint fsExplosionTex[fsMaxNumMaskTexture];

int fsNumCloudParticleTex=0;
GLuint fsCloudParticleTex[fsMaxNumMaskTexture];
// 2012/01/02 Cloud Lighting is currently disabled due to performance issue. >>
// Nonetheless, cloudParticleTexSrc and cloudParticleTexBuf are set up anyway.
FsGL2Bitmap cloudParticleTexSrc[fsMaxNumMaskTexture];
FsGL2Bitmap cloudParticleTexBuf[fsMaxNumMaskTexture];
// 2012/01/02 Cloud Lighting is currently disabled due to performance issue. <<

int fsNumFlashTex=0;
GLuint fsFlashTex[fsMaxNumMaskTexture];

int fsNumCloudTex=0;
GLuint fsCloudTex[fsMaxNumMaskTexture];

GLuint fsParticleTexture=0;





double fsOpenGLVersion=1.0;

static void FsSetBmpTexture(GLuint texId,const YsBitmap &bmp,YSBOOL repeat)
{
	glBindTexture(GL_TEXTURE_2D,texId);

	// glPixelStorei(GL_UNPACK_ALIGNMENT,1);  Do I need it?
	if(YSTRUE==repeat)
	{
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	}
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

	glTexImage2D
	    (GL_TEXTURE_2D,
	     0,
	     GL_RGBA, // "4" is a GL1 relic; invalid in GLES2/WebGL.
	     bmp.GetWidth(),
	     bmp.GetHeight(),
	     0,
	     GL_RGBA,
	     GL_UNSIGNED_BYTE,
	     bmp.GetRGBABitmapPointer());
}

static void FsMakeAlphaMask(YsBitmap &bmp)
{
	const int nPix=bmp.GetWidth()*bmp.GetHeight();
	unsigned char *rgba=bmp.GetEditableRGBABitmapPointer();
	for(int i=0; i<nPix; ++i)
	{
		rgba[i*4+3]=rgba[i*4];

		rgba[i*4  ]=255;
		rgba[i*4+1]=255;
		rgba[i*4+2]=255;
	}
}

// VR single-pass-stereo HUD composite: the texture-array quad renderer.  Only
// non-NULL while the shared renderers are compiled stereo (multiview mode);
// YsGLSLCreateHudQuadRenderer returns NULL otherwise, so this stays NULL on the
// mono path and the composite is simply skipped.
static struct YsGLSLHudQuadRenderer *fsHudQuadRenderer=NULL;

void FsInitializeOpenGL(void)
{
	const char *verStr=(const char *)glGetString(GL_VERSION);
	if(NULL!=verStr)
	{
		fsOpenGLVersion=atof(verStr);
#ifdef _WIN32
		if(1.999999>fsOpenGLVersion)
		{
			YsString sysErrorMsg;
			sysErrorMsg.Printf(
			   "Newer OpenGL version required.\n"
			   "Required Version:     2.0\n"
			   "Your device supports: %.1lf\n"
			   "Please use YSFLIGHT on OpenGL 1.X instead.",fsOpenGLVersion);
			MessageBoxA(NULL,sysErrorMsg,"Newer OpenGL version required.",MB_OK);
			exit(1);
		}
#endif
	}

	YsBitmap bmp;

	ysScnGlRwLightTex=~(unsigned int)0;
	ysScnGlMapTex=~(unsigned int)0;

	if(bmp.LoadPng(FS_TEXTURE_RWLIGHT)==YSOK)
	{
		glGenTextures(1,&ysScnGlRwLightTex);
		FsSetBmpTexture(ysScnGlRwLightTex,bmp,YSFALSE);
	}

	if(bmp.LoadPng(FS_TEXTURE_GROUNDTILE)==YSOK)
	{
		glGenTextures(1,&ysScnGlMapTex);
		FsSetBmpTexture(ysScnGlMapTex,bmp,YSTRUE);
	}

	glGenTextures(fsMaxNumMaskTexture,fsExplosionTex);
	glGenTextures(fsMaxNumMaskTexture,fsCloudParticleTex);
	glGenTextures(fsMaxNumMaskTexture,fsFlashTex);
	glGenTextures(fsMaxNumMaskTexture,fsCloudTex);
	glGenTextures(1,&fsParticleTexture);

	fsNumExplosionTex=0;
	fsNumCloudParticleTex=0;

	for(int i=1; i<=fsMaxNumMaskTexture; ++i)
	{
		YsString fn;
		fn.Printf("misc/explosion%02d.png",i);
		if(bmp.LoadPng(fn)==YSOK)
		{
			FsMakeAlphaMask(bmp);
			FsSetBmpTexture(fsExplosionTex[fsNumExplosionTex++],bmp,YSFALSE);

			FsSetBmpTexture(fsCloudParticleTex[fsNumCloudParticleTex],bmp,YSFALSE);
			cloudParticleTexSrc[fsNumCloudParticleTex].MakeFromYsBitmap(bmp);
			fsNumCloudParticleTex++;
		}

		fn.Printf("misc/flash%02d.png",i);
		if(bmp.LoadPng(fn)==YSOK)
		{
			FsMakeAlphaMask(bmp);
			FsSetBmpTexture(fsFlashTex[fsNumFlashTex++],bmp,YSFALSE);
		}

		fn.Printf("misc/cloud%02d.png",i);
		if(bmp.LoadPng(fn)==YSOK)
		{
			FsMakeAlphaMask(bmp);
			FsSetBmpTexture(fsCloudTex[fsNumCloudTex++],bmp,YSTRUE);
		}
	}

	if(YSOK==bmp.LoadPng("misc/particle01.png"))
	{
		FsSetBmpTexture(fsParticleTexture,bmp,YSFALSE);
	}

	// FsGlMakeWireFontList();

	// 2010/03/02
	// Polygon offset no longer needed.  Lights are now drawn as textured polygon.  Not really apoint.
	// Therefore, polygon offset doesn't matter.
	// glEnable(GL_POLYGON_OFFSET_FILL);
	// glPolygonOffset(1,1);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	glStencilOp(GL_KEEP,GL_KEEP,GL_INCR);
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS,0,0);

	YsGLSLCreateSharedRenderer();
	YsGLSLSetShared3DRendererAlphaCutOff(0.02f);

	YsGLSLSetPerPixRendering(YSTRUE); 
	YsGLSLSetShared3DRendererSpecularExponent(600.0f);

	YsGLSLCreateSharedBitmapFontRenderer();

	// Stereo HUD-quad renderer: created only when the shared renderers are in
	// multiview compile mode (returns NULL otherwise).
	fsHudQuadRenderer=YsGLSLCreateHudQuadRenderer();
}

void FsReinitializeOpenGL(void)
{
	YsGLSLDeleteSharedRenderer();
	YsGLSLDeleteSharedBitmapFontRenderer();
	YsGLSLDeleteHudQuadRenderer(fsHudQuadRenderer);
	fsHudQuadRenderer=NULL;
	FsInitializeOpenGL();
}

void FsUninitializeOpenGL(void)
{
	glDeleteTextures(1,&ysScnGlRwLightTex);
	glDeleteTextures(1,&ysScnGlMapTex);

	glDeleteTextures(fsMaxNumMaskTexture,fsExplosionTex);
	glDeleteTextures(fsMaxNumMaskTexture,fsFlashTex);

	YsGLSLDeleteSharedBitmapFontRenderer();
	YsGLSLDeleteSharedRenderer();
	YsGLSLDeleteHudQuadRenderer(fsHudQuadRenderer);
	fsHudQuadRenderer=NULL;
}


void FsClearScreenAndZBuffer(const YsColor &clearColor)
{
	if(0!=FsVrIsActive())
	{
		int x0,y0,wid,hei;
		if(0!=FsVrHudRenderTargetActive())
		{
			// Off-screen pass (menu, HUD, or GUI): clear the full off-screen texture.
			FsVrGetHudRenderSize(&wid,&hei);
			x0=0; y0=0;
		}
		else
		{
			// Per-eye clear for the main scene.
			FsVrGetEyeViewport(FsGetActiveSplitWindow(),x0,y0,wid,hei);
		}
		glScissor(x0,y0,wid,hei);
		glEnable(GL_SCISSOR_TEST);
	}
	else if(YSTRUE!=FsIsMainWindowActive() || YSTRUE==FsIsMainWindowSplit())
	{
		int x0,y0,wid,hei;
		int mainWid,mainHei;
		FsGetWindowViewport(x0,y0,wid,hei); // x0,y0 is top-left corner.  OpenGL takes bottom-left corner.
		FsGetWindowSize(mainWid,mainHei);
		y0+=hei;  // Now y0 is bottom of the view port.

		glScissor(x0,mainHei-y0,wid,hei);
		glEnable(GL_SCISSOR_TEST);
	}

	glClearColor((float)clearColor.Rd(),(float)clearColor.Gd(),(float)clearColor.Bd(),1.0F);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

	// Why forced resetting is needed?
	//   List base is reset back to zero when OpenGL context is re-made.
	//   OpenGL context is re-made when the window is maximized, minimized or re-sized.

	if(0!=FsVrIsActive() || YSTRUE!=FsIsMainWindowActive() || YSTRUE==FsIsMainWindowSplit())
	{
		glDisable(GL_SCISSOR_TEST);
	}

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsClearScreenAndZBuffer");
#endif

//#ifdef WIN32
//	frmClock1=frmClock2;
//	QueryPerformanceCounter(&frmClock2);
//	printf("* %d\n",frmClock2.LowPart-frmClock1.LowPart);
//#endif
}

void FsClearStencilBuffer(void)
{
	glClear(GL_STENCIL_BUFFER_BIT);
}

void FsSetPointLight(const YsVec3 &/*cameraPosition*/ ,const YsVec3 &lightPosition,FSENVIRONMENT env)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsSetPointLight In ");
#endif

	/* const GLfloat light[4]=
	{
		(GLfloat)lightPosition.x(),
		(GLfloat)lightPosition.y(),
		(GLfloat)lightPosition.z(),
		1.0f
	}; */

	YsDisregardVariable(lightPosition);
	YsDisregardVariable(env);
	// Not supported yet.

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsSetPointLight Out");
#endif
}

void FsSetDirectionalLight(const YsVec3 &/*cameraPosition*/ ,const YsVec3 &lightDirection,FSENVIRONMENT env)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsSetDirectionalLight In");
#endif

	const GLfloat light[4]=
	{
		(GLfloat)lightDirection.x(),
		(GLfloat)lightDirection.y(),
		(GLfloat)lightDirection.z(),
		0.0f
	};

	GLfloat dif[4];
	GLfloat amb[4];
	GLfloat spc[4];

	switch(env)
	{
	case FSDAYLIGHT:
		dif[0]=0.6F;
		dif[1]=0.6F;
		dif[2]=0.6F;
		dif[3]=1.0F;

		amb[0]=0.3F;
		amb[1]=0.3F;
		amb[2]=0.3F;
		amb[3]=1.0F;

		spc[0]=0.9F;
		spc[1]=0.9F;
		spc[2]=0.9F;
		spc[3]=1.0F;
		break;
	case FSNIGHT:
		dif[0]=0.05F;
		dif[1]=0.05F;
		dif[2]=0.05F;
		dif[3]=1.0F;

		amb[0]=0.05F;
		amb[1]=0.05F;
		amb[2]=0.05F;
		amb[3]=1.0F;

		spc[0]=0.0F;
		spc[1]=0.0F;
		spc[2]=0.0F;
		spc[3]=1.0F;
		break;
	}

	YsGLSLSetShared3DRendererDirectionalLightfv(0,light);
	YsGLSLSetShared3DRendererLightColor(0,dif);
	YsGLSLSetShared3DRendererAmbientColor(amb);
	YsGLSLSetShared3DRendererSpecularColor(spc);

	YsGLSLUse3DRenderer(YsGLSLSharedFlash3DRenderer());
	switch(env)
	{
	case FSDAYLIGHT:
		YsGLSLSet3DRendererUniformFlashSize(YsGLSLSharedFlash3DRenderer(),0.1f);
		YsGLSLSet3DRendererFlashRadius(YsGLSLSharedFlash3DRenderer(),0.6f,1.0f);
		break;
	case FSNIGHT:
		YsGLSLSet3DRendererUniformFlashSize(YsGLSLSharedFlash3DRenderer(),1.0f);
		YsGLSLSet3DRendererFlashRadius(YsGLSLSharedFlash3DRenderer(),0.2f,1.0f);
		break;
	}
	YsGLSLEndUse3DRenderer(YsGLSLSharedFlash3DRenderer());


#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsSetDirectionalLight Out");
#endif
}

void FsFogOn(const YsColor &col,const double &visibility)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsFogOn In");
#endif

	const GLfloat fogColor[]=
	{
		(GLfloat)col.Rf(),
		(GLfloat)col.Gf(),
		(GLfloat)col.Bf(),
		1.0f
	};

	// From GLSL comment
	// f  0:Completely fogged out   1:Clear
	// f=e^(-d*d)
	// d  0:Clear      Infinity: Completely fogged out
	// 99% fogged out means:  e^(-d*d)=0.01  What�fs d?
	// -d*d=loge(0.01)
	// -d*d= -4.60517
	// d=2.146
	// If visibility=V, d=2.146 at fogZ=V -> fogDensity=2.146/V

	YsGLSLSetShared3DRendererFog(1,2.146/(GLfloat)visibility,fogColor);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsFogOn Out");
#endif
}

void FsFogOff(void)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsFogOff In");
#endif

	const float fogColor[]={0.7F,0.7F,0.7F,0.0F};
	YsGLSLSetShared3DRendererFog(0,1.0,fogColor);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsFogOff Out");
#endif
}

static void FsSetupViewport(void)
{
	if(0!=FsVrHudRenderTargetActive())
	{
		// The VR HUD off-screen pass draws into the full HUD texture.
		int w,h;
		FsVrGetHudRenderSize(&w,&h);
		glViewport(0,0,w,h);
		return;
	}
	if(0!=FsVrIsActive())
	{
		// VR framebuffer viewport of the eye, bottom-left origin already.
		int x0,y0,wid,hei;
		FsVrGetEyeViewport(FsGetActiveSplitWindow(),x0,y0,wid,hei);
		glViewport(x0,y0,wid,hei);
		return;
	}

	int x0,y0,wid,hei;
	int mainWid,mainHei;
	FsGetWindowViewport(x0,y0,wid,hei); // x0,y0 is top-left corner.  OpenGL takes bottom-left corner.
	FsGetWindowSize(mainWid,mainHei);
	y0+=hei;  // Now y0 is bottom of the view port.
	glViewport(x0,mainHei-y0,wid,hei);
}

void FsBeginRenderShadowMap(const YsMatrix4x4 &projTfm,const YsMatrix4x4 &viewTfm,int texWid,int texHei)
{
	glViewport(0,0,texWid,texHei);

	GLfloat projMat[16];
	projTfm.GetOpenGlCompatibleMatrix(projMat);

	YsMatrix4x4 viewTfmLH;
	viewTfmLH.ScaleZ(-1.0);
	viewTfmLH*=viewTfm;

	GLfloat viewMatLH[16];
	viewTfmLH.GetOpenGlCompatibleMatrix(viewMatLH);

	if(0!=FsVrIsMultiview())
	{
		// Multiview compile mode: every shared renderer's 'projection'
		// uniform is a two-view mat4[2] indexed by gl_ViewID_OVR, and the
		// mono setter only writes view 0 (leaving view 1 holding the last
		// scene pass's eye-1 matrix -- garbage for a light-space pass).  A
		// shadow map is light-space and view-INdependent, so both views get
		// the SAME projection: the two layers of the multiview shadow FBO
		// (see FsVrBindShadowMapMultiviewFbo above) render identically, and
		// layer 0 is what gets blitted out for sampling.
		GLfloat projStereo[32];
		for(int i=0; i<16; ++i)
		{
			projStereo[i]=projMat[i];
			projStereo[16+i]=projMat[i];
		}
		YsGLSLSetShared3DRendererProjectionStereo(projStereo);
	}
	else
	{
		YsGLSLSetShared3DRendererProjection(projMat);
	}
	YsGLSLSetShared3DRendererModelView(viewMatLH);

	{
		auto fsBitmapFontRenderer=YsGLSLSharedBitmapFontRenderer();
		YsGLSLUseBitmapFontRenderer(fsBitmapFontRenderer);
		YsGLSLSetBitmapFontRendererProjectionfv(fsBitmapFontRenderer,projMat);
		YsGLSLSetBitmapFontRendererModelViewfv(fsBitmapFontRenderer,viewMatLH);
		YsGLSLSetBitmapFontRendererViewportSize(fsBitmapFontRenderer,texWid,texHei);
		YsGLSLEndUseBitmapFontRenderer(fsBitmapFontRenderer);
	}

	glDisable(GL_POLYGON_OFFSET_FILL);
	glDepthMask(GL_TRUE); // Man!  It took several days to find this single line was missing!
	glDepthFunc(GL_LEQUAL);  // Actually, two lines.
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	glEnable(GL_CULL_FACE);
}

void FsEndRenderShadowMap(void)
{
	glBindFramebuffer(GL_FRAMEBUFFER,0);
}

static void SwitchTexture(int texIdent)
{
	switch(texIdent)
	{
	case 0:
		glActiveTexture(GL_TEXTURE0);
		break;
	case 1:
		glActiveTexture(GL_TEXTURE1);
		break;
	case 2:
		glActiveTexture(GL_TEXTURE2);
		break;
	case 3:
		glActiveTexture(GL_TEXTURE3);
		break;
	case 4:
		glActiveTexture(GL_TEXTURE4);
		break;
	case 5:
		glActiveTexture(GL_TEXTURE5);
		break;
	case 6:
		glActiveTexture(GL_TEXTURE6);
		break;
	case 7:
		glActiveTexture(GL_TEXTURE7);
		break;
	case 8:
		glActiveTexture(GL_TEXTURE8);
		break;
	case 9:
		glActiveTexture(GL_TEXTURE9);
		break;
	}
}

/*! Shadow-map texture must be bound to the texture-unit samplerIdent before this call.
    Binding is a responsibility of YsTextureManager.

    ShadowMapIdent is an identifier for the renderer {0 or 1}.
*/
void FsEnableShadowMap(
    const YsMatrix4x4 &viewTfm,const YsMatrix4x4 &shadowProjTfm,const YsMatrix4x4 &shadowViewTfm,
    int samplerIdent,int shadowMapIdent)
{
	YsGLSLShaded3DRenderer renderer;

	YsMatrix4x4 viewInv;
	viewInv.ScaleZ(-1.0); // Right->Left Tfm
	viewInv*=viewTfm;
	viewInv.Invert();

	YsMatrix4x4 shadowViewTfmLH;
	shadowViewTfmLH.ScaleZ(-1.0);
	shadowViewTfmLH*=shadowViewTfm;

	YsMatrix4x4 overAllTfm=shadowProjTfm*shadowViewTfmLH*viewInv;
	GLfloat overAllMat[16];
	overAllTfm.GetOpenGlCompatibleMatrix(overAllMat);

	YsGLSLSet3DRendererUniformShadowMapMode(renderer,shadowMapIdent,YSGLSL_SHADOWMAP_USE);
	YsGLSLSet3DRendererUniformShadowMapTexture(renderer,shadowMapIdent,samplerIdent);
	YsGLSLSet3DRendererUniformShadowMapTransformation(renderer,shadowMapIdent,overAllMat);

	YsGLSLSet3DRendererUniformLightDistOffset(renderer,shadowMapIdent,0.0001);
	YsGLSLSet3DRendererUniformLightDistScaling(renderer,shadowMapIdent,1.0001);
}

void FsDisableShadowMap(int samplerIdent,int shadowMapIdent)
{
	YsGLSLShaded3DRenderer renderer;
	YsGLSLSet3DRendererUniformShadowMapMode(renderer,shadowMapIdent,YSGLSL_SHADOWMAP_NONE);

	SwitchTexture(samplerIdent);
	glBindTexture(GL_TEXTURE_2D,0);
}

// ---- VR single-pass-stereo shadow-map render support ----------------------
// See fsvr.h's FsVrShadowFboDataPointer doc comment for the full WHY: while
// multiview is active every shared-renderer program carries
// layout(num_views=2), and OVR_multiview2 refuses (INVALID_OPERATION,
// verified on ANGLE) any draw into a framebuffer whose view count differs --
// which the per-cascade single-layer depth FBOs do.  So the multiview shadow
// pass draws into the shared two-layer depth-array FBO the web layer
// publishes, then depth-blits its layer 0 into the cascade's ordinary 2D
// depth texture, which the scene pass samples exactly like the flat path.
// Emscripten/WebGL2-only machinery: the GLES2 headers this file builds
// against have no glBlitFramebuffer, but the web build links with
// -sMAX_WEBGL_VERSION=2 whose GL library provides it -- declare it (and the
// two framebuffer-target enums) locally, guarded to the web build, the same
// spirit as ystexturemanager_gl.cpp's GL_DEPTH_COMPONENT24 fallback.  The
// desktop builds compile these to no-ops: multiview never engages there
// (FsVrIsMultiview is only raised by the WebXR layer).
#ifdef __EMSCRIPTEN__
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif
extern "C" void glBlitFramebuffer(
    GLint srcX0,GLint srcY0,GLint srcX1,GLint srcY1,
    GLint dstX0,GLint dstY0,GLint dstX1,GLint dstY1,
    GLbitfield mask,GLenum filter);
#endif

int FsVrShadowMapMultiviewReady(int texWid,int texHei)
{
	const float *shadowFbo=FsVrShadowFboDataPointer();
	return (0.0f!=shadowFbo[0] &&
	        (float)texWid==shadowFbo[3] &&
	        (float)texHei==shadowFbo[4] ? 1 : 0);
}

void FsVrBindShadowMapMultiviewFbo(void)
{
	const float *shadowFbo=FsVrShadowFboDataPointer();
	glBindTexture(GL_TEXTURE_2D,0); // Same MacOSX/feedback discipline as YsTextureManager::Unit::BindFrameBuffer.
	glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)shadowFbo[1]);
}

void FsVrBlitShadowMapFromMultiview(int texWid,int texHei)
{
#ifdef __EMSCRIPTEN__
	// Caller contract: the cascade's own single-layer depth FBO is currently
	// bound as GL_FRAMEBUFFER (i.e. both READ and DRAW) -- re-point only READ
	// at the layer-0 view of the multiview depth array and blit.  Equal
	// rectangles + NEAREST: both are hard GLES3 requirements for a
	// DEPTH_BUFFER_BIT blit (and the formats match by construction --
	// DEPTH_COMPONENT24 on both sides, see setupShadowFbo in fswebxr.cpp and
	// ystexturemanager_gl.cpp's MakeActualTexture).  No restore needed: the
	// caller's very next call is FsEndRenderShadowMap, which rebinds
	// GL_FRAMEBUFFER (both targets) anyway.
	const float *shadowFbo=FsVrShadowFboDataPointer();
	glBindFramebuffer(GL_READ_FRAMEBUFFER,(GLuint)shadowFbo[2]);
	glBlitFramebuffer(0,0,texWid,texHei,0,0,texWid,texHei,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
#else
	(void)texWid;
	(void)texHei;
#endif
}


// Cached scene camera state for the VR single-pass-stereo HUD composite.
// fsLastSceneProjectionStereo is the projection[2] array folded by
// FsSetSceneProjection; fsLastSceneModelView is the world->eye0 modelView last
// uploaded by FsSetCameraPosition.  SimDrawAllScreen retrieves both through the
// getters below so the cockpit-anchored HUD quad shares the scene's matrices.
static GLfloat fsLastSceneProjectionStereo[32]=
{
	1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1,
	1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};
static GLfloat fsLastSceneModelView[16]=
{
	1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};

static void FsGetLastSceneProjectionStereofv(GLfloat out[32])
{
	for(int i=0; i<32; ++i)
	{
		out[i]=fsLastSceneProjectionStereo[i];
	}
}

static void FsGetLastSceneModelViewfv(GLfloat out[16])
{
	for(int i=0; i<16; ++i)
	{
		out[i]=fsLastSceneModelView[i];
	}
}

void FsSetSceneProjection(const class FsProjection &prj)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsSetSceneProjection In");
#endif

	FsSetupViewport();
	int xx0,yy0,wid,hei;

	double lft,rit,top,btm;

	if(0!=FsVrIsActive())
	{
		// Asymmetric per-eye frustum from the VR runtime, re-built at the
		// near/far range requested by the caller so that the depth-slicing
		// draw passes keep working in VR.
		FsVrGetEyeViewport(FsGetActiveSplitWindow(),xx0,yy0,wid,hei);
		FsVrGetEyeFrustum(FsGetActiveSplitWindow(),prj.nearz,prj.farz,lft,rit,btm,top);
	}
	else
	{
		FsGetWindowViewport(xx0,yy0,wid,hei); // x0,y0 is top-left corner.  OpenGL takes bottom-left corner.

		lft=(double)(   -prj.cx)*prj.nearz/prj.prjPlnDist;
		rit=(double)(wid-prj.cx)*prj.nearz/prj.prjPlnDist;
		top=(double)(    prj.cy)*prj.nearz/prj.prjPlnDist;
		btm=(double)(prj.cy-hei)*prj.nearz/prj.prjPlnDist;
	}



	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


	// Anti-aliasing options >>
	/*
#ifndef __EMSCRIPTEN__
	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_POLYGON_SMOOTH);
	glHint(GL_POLYGON_SMOOTH_HINT,GL_NICEST);
#endif
	*/
	// Anti-aliasing options <<


	GLfloat projMat[16];
	YsGLMakeFrustum(projMat,(GLfloat)lft,(GLfloat)rit,(GLfloat)btm,(GLfloat)top,(GLfloat)prj.nearz,(GLfloat)prj.farz);
	if(0!=FsVrIsActive() && 0!=FsVrIsMultiview())
	{
		// Single-pass stereo: the scene pass renders from the eye-0 pose
		// (SimDrawAllScreen), so fold each eye's difference into its view of
		// the projection array: projection[i] = P_i * V_i * inverse(V_0).
		// V_i are the GL-convention eye-view matrices from the VR runtime;
		// the composition happens entirely in GL space, downstream of the
		// engine's LH->GL modelView, so no z-flip conjugation is needed.
		YsMatrix4x4 eye0View;
		eye0View.CreateFromOpenGlCompatibleMatrix(FsVrEyeViewMatrix(0));
		YsMatrix4x4 eye0ViewInv=eye0View;
		eye0ViewInv.Invert();

		GLfloat stereoProj[32];
		for(int eye=0; eye<FsVrNumEye; ++eye)
		{
			double eLft,eRit,eBtm,eTop;
			FsVrGetEyeFrustum(eye,prj.nearz,prj.farz,eLft,eRit,eBtm,eTop);
			GLfloat eyeProjMat[16];
			YsGLMakeFrustum(eyeProjMat,(GLfloat)eLft,(GLfloat)eRit,(GLfloat)eBtm,(GLfloat)eTop,(GLfloat)prj.nearz,(GLfloat)prj.farz);

			YsMatrix4x4 eyeProj;
			eyeProj.CreateFromOpenGlCompatibleMatrix(eyeProjMat);
			YsMatrix4x4 eyeView;
			eyeView.CreateFromOpenGlCompatibleMatrix(FsVrEyeViewMatrix(eye));

			YsMatrix4x4 combined=eyeProj*eyeView*eye0ViewInv;
			GLfloat combinedMat[16];
			combined.GetOpenGlCompatibleMatrix(combinedMat);
			for(int i=0; i<16; ++i)
			{
				stereoProj[eye*16+i]=combinedMat[i];
			}
		}
		YsGLSLSetShared3DRendererProjectionStereo(stereoProj);
		// Cache for the VR HUD-quad composite (SimDrawAllScreen), which must
		// use the exact same per-view projection array as the scene pass.
		for(int i=0; i<32; ++i)
		{
			fsLastSceneProjectionStereo[i]=stereoProj[i];
		}
	}
	else
	{
		YsGLSLSetShared3DRendererProjection(projMat);
	}

	{
		auto fsBitmapFontRenderer=YsGLSLSharedBitmapFontRenderer();
		YsGLSLUseBitmapFontRenderer(fsBitmapFontRenderer);
		YsGLSLSetBitmapFontRendererProjectionfv(fsBitmapFontRenderer,projMat);
		YsGLSLSetBitmapFontRendererViewportSize(fsBitmapFontRenderer,wid,hei);
		YsGLSLEndUseBitmapFontRenderer(fsBitmapFontRenderer);
	}


#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsSetSceneProjection Out");
#endif
}

void FsSet2DDrawing(void)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsSet2DDrawing In");
#endif

	FsSetupViewport();

	int viewport[4],wid,hei;
	glGetIntegerv(GL_VIEWPORT,viewport);
	wid=viewport[2];
	hei=viewport[3];


	{
		auto fsBitmapFontRenderer=YsGLSLSharedBitmapFontRenderer();
		YsGLSLUseBitmapFontRenderer(fsBitmapFontRenderer);
		YsGLSLSetBitmapFontRendererViewportSize(fsBitmapFontRenderer,wid,hei);
		YsGLSLEndUseBitmapFontRenderer(fsBitmapFontRenderer);
	}

	glDepthFunc(GL_ALWAYS);
	glDepthMask(GL_FALSE);


	glDisable(GL_CULL_FACE);

	YsGLSLUsePlain2DRenderer(YsGLSLSharedPlain2DRenderer());
	YsGLSLUseWindowCoordinateInPlain2DDrawing(YsGLSLSharedPlain2DRenderer(),1);
	YsGLSLEndUsePlain2DRenderer(YsGLSLSharedPlain2DRenderer());

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsSet2DDrawing Out");
#endif
}

// ---- VR single-pass-stereo HUD composite ---------------------------------
// SimDrawAllScreen (core) drives these while VR + multiview + HUD-enable are
// all on.  FsVrBeginHudRender/FsVrEndHudRender bracket the off-screen 2D HUD
// pass into the two-layer multiview HUD framebuffer; FsVrDrawHudQuad then
// composites that texture array onto a cockpit-anchored quad in the scene FBO.
// Defined in the fssimplewindow emscripten back-end: make FsGetWindowSize
// report the HUD texture size for the duration of the off-screen HUD pass, so
// pixel-space HUD placement lands on the HUD texture.  (fssimplewindow stays
// dependency-free of fsvr; only the engine, which links both, bridges them.)
extern "C" void FsSetWindowSizeOverride(int active,int w,int h);

void FsVrBeginHudRender(void)
{
	const float *hud=FsVrHudDataPointer();
	GLuint hudFbo=(GLuint)hud[1];
	int texW=(int)hud[3];
	int texH=(int)hud[4];

	FsVrSetHudRenderTarget(1,texW,texH);
	FsSetWindowSizeOverride(1,texW,texH);
	glBindFramebuffer(GL_FRAMEBUFFER,hudFbo);
	glViewport(0,0,texW,texH);
	glClearColor(0.0f,0.0f,0.0f,0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void FsVrEndHudRender(void)
{
	FsVrSetHudRenderTarget(0,0,0);
	FsSetWindowSizeOverride(0,0,0);
	// The web layer redirects a bind(0) to the active multiview scene FBO for
	// the lifetime of the session, so this restores the scene target.
	glBindFramebuffer(GL_FRAMEBUFFER,0);
}

void FsVrDrawHudQuad(const float corner[12])
{
	if(NULL==fsHudQuadRenderer)
	{
		return;
	}

	const float *hud=FsVrHudDataPointer();
	GLuint hudTexArray=(GLuint)hud[2];

	// Restore the scene (eye-0) viewport for the composite into the multiview
	// framebuffer (FsVrEndHudRender left the HUD-texture-sized viewport).
	int x0,y0,wid,hei;
	FsVrGetEyeViewport(0,x0,y0,wid,hei);
	glViewport(x0,y0,wid,hei);

	GLfloat proj[32],modelView[16];
	FsGetLastSceneProjectionStereofv(proj);
	FsGetLastSceneModelViewfv(modelView);
	YsGLSLSetHudQuadRendererProjectionStereofv(fsHudQuadRenderer,proj);
	YsGLSLSetHudQuadRendererModelViewfv(fsHudQuadRenderer,modelView);

	// Save the state we touch, restore it after so no leak into later draws.
	GLboolean wasBlend=glIsEnabled(GL_BLEND);
	GLboolean wasDepthTest=glIsEnabled(GL_DEPTH_TEST);
	GLboolean wasCull=glIsEnabled(GL_CULL_FACE);
	GLint prevBlendSrc=GL_SRC_ALPHA,prevBlendDst=GL_ONE_MINUS_SRC_ALPHA;
	glGetIntegerv(GL_BLEND_SRC_RGB,&prevBlendSrc);
	glGetIntegerv(GL_BLEND_DST_RGB,&prevBlendDst);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST); // HUD glass draws over the scene regardless of depth.
	glDisable(GL_CULL_FACE);

	YsGLSLRenderHudQuad(fsHudQuadRenderer,corner,hudTexArray);

	if(GL_FALSE==wasBlend){ glDisable(GL_BLEND); }
	glBlendFunc((GLenum)prevBlendSrc,(GLenum)prevBlendDst);
	if(GL_FALSE!=wasDepthTest){ glEnable(GL_DEPTH_TEST); }
	if(GL_FALSE!=wasCull){ glEnable(GL_CULL_FACE); }
}

// ---- VR single-pass-stereo collimated gunsight reticle -------------------
// Draws the gun crosshair as real world-space 3D line geometry in the scene
// FBO, rendered through each eye's OWN stereo projection (the cached scene
// matrices), instead of baking it into the shared flat HUD texture.  Because
// it is genuine 3D geometry far along the boresight (SimDrawAllScreen places
// it 2000 m ahead), stereo parallax makes it read as collimated at optical
// infinity: head translation off the boresight axis no longer shifts the
// apparent aim point the way the fixed-distance HUD glass does.  lineVtx holds
// 8 world-space vertices (4 line segments, GL_LINES).  Mirrors FsVrDrawHudQuad's
// matrix + state pattern, but on the shared VariColor3D renderer (the same
// flat 3D line renderer FsDrawLine3d uses) instead of the textured HUD-quad one.
void FsVrDrawReticle(const float lineVtx[24],const YsColor &col)
{
	GLfloat proj[32],modelView[16];
	FsGetLastSceneProjectionStereofv(proj);
	FsGetLastSceneModelViewfv(modelView);
	YsGLSLSetShared3DRendererProjectionStereo(proj);
	YsGLSLSetShared3DRendererModelView(modelView);

	// Restore the scene (eye-0) viewport (the HUD off-screen pass left the
	// HUD-texture-sized viewport; FsVrDrawHudQuad already restored it, but do
	// not rely on draw order).
	int x0,y0,wid,hei;
	FsVrGetEyeViewport(0,x0,y0,wid,hei);
	glViewport(x0,y0,wid,hei);

	// A collimated pipper must be full-bright regardless of range: turn fog
	// off (it would wash a 2000 m reticle toward the fog colour).  Fog is
	// re-established by the next frame's scene pass (FsFogOn during
	// SimDrawScreen), and nothing fog-sensitive draws after this within the
	// frame (the GUI quad composite uses the un-fogged HUD-quad renderer).
	FsFogOff();

	// Save the state we touch, restore it after so no leak into later draws.
	GLboolean wasBlend=glIsEnabled(GL_BLEND);
	GLboolean wasDepthTest=glIsEnabled(GL_DEPTH_TEST);
	GLboolean wasCull=glIsEnabled(GL_CULL_FACE);
	GLint prevBlendSrc=GL_SRC_ALPHA,prevBlendDst=GL_ONE_MINUS_SRC_ALPHA;
	glGetIntegerv(GL_BLEND_SRC_RGB,&prevBlendSrc);
	glGetIntegerv(GL_BLEND_DST_RGB,&prevBlendDst);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST); // Collimated pipper overlays terrain, never occluded.
	glDisable(GL_CULL_FACE);

	GLfloat colBuf[8*4];
	for(int i=0; i<8; ++i)
	{
		colBuf[i*4  ]=col.Rf();
		colBuf[i*4+1]=col.Gf();
		colBuf[i*4+2]=col.Bf();
		colBuf[i*4+3]=1.0f;
	}

	auto *renderer=YsGLSLSharedVariColor3DRenderer();
	YsGLSLUse3DRenderer(renderer);
	YsGLSLDrawPrimitiveVtxColfv(renderer,GL_LINES,8,lineVtx,colBuf);
	YsGLSLEndUse3DRenderer(renderer);

	if(GL_FALSE==wasBlend){ glDisable(GL_BLEND); }
	glBlendFunc((GLenum)prevBlendSrc,(GLenum)prevBlendDst);
	if(GL_FALSE!=wasDepthTest){ glEnable(GL_DEPTH_TEST); }
	if(GL_FALSE!=wasCull){ glEnable(GL_CULL_FACE); }
}

// ---- VR single-pass-stereo hand-held HOTAS prop draw bracket --------------
// See fsopengl.h's doc comment.  Unlike the reticle/tint above (which draw
// raw world-space vertices and so need the TRUE camera transform composed
// into the shared renderer's modelview), FsVisualDnm::Draw(pos,att) is its
// own self-contained model matrix (see fscontrol.cpp's DrawJoystick: "New
// FsVisual::Draw assumes the viewpoint is at the origin looking straight
// ahead") -- it overrides-and-restores the shared renderer's modelview
// itself (ysshellextgl_gl2.cpp's Render), so this bracket only needs to
// fix up what Render does NOT touch: the projection (in case the HUD's 2D
// off-screen pass left an ortho projection bound) and the viewport (same
// reason FsVrDrawHudQuad restores it), plus depth testing.
static GLboolean fsVrHandPropWasDepthTest=GL_TRUE;

void FsVrBeginHandPropDraw(void)
{
	GLfloat proj[32],modelView[16];
	FsGetLastSceneProjectionStereofv(proj);
	FsGetLastSceneModelViewfv(modelView);
	YsGLSLSetShared3DRendererProjectionStereo(proj);
	YsGLSLSetShared3DRendererModelView(modelView);

	int x0,y0,wid,hei;
	FsVrGetEyeViewport(0,x0,y0,wid,hei);
	glViewport(x0,y0,wid,hei);

	fsVrHandPropWasDepthTest=glIsEnabled(GL_DEPTH_TEST);
	glDisable(GL_DEPTH_TEST); // A hand-held prop must not be swallowed by nearer cockpit geometry (same discipline as FsVrDrawReticle).
}

void FsVrEndHandPropDraw(void)
{
	if(GL_FALSE!=fsVrHandPropWasDepthTest)
	{
		glEnable(GL_DEPTH_TEST);
	}
}

// ---- VR single-pass-stereo in-flight-GUI-dialog composite ----------------
// Same shape as the HUD trio above, driven by FsVrGuiDataPointer instead of
// FsVrHudDataPointer.  FsVrSetHudRenderTarget/FsSetWindowSizeOverride are
// reused as-is (see fsvr.h's doc comment on FsVrSetHudRenderTarget): the HUD
// and GUI off-screen passes never run concurrently within a frame.
void FsVrBeginGuiRender(void)
{
	const float *gui=FsVrGuiDataPointer();
	GLuint guiFbo=(GLuint)gui[1];
	int texW=(int)gui[3];
	int texH=(int)gui[4];

	FsVrSetHudRenderTarget(1,texW,texH);
	FsSetWindowSizeOverride(1,texW,texH);
	glBindFramebuffer(GL_FRAMEBUFFER,guiFbo);
	glViewport(0,0,texW,texH);
	glClearColor(0.0f,0.0f,0.0f,0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void FsVrEndGuiRender(void)
{
	FsVrSetHudRenderTarget(0,0,0);
	FsSetWindowSizeOverride(0,0,0);
	// The web layer redirects a bind(0) to the active multiview scene FBO for
	// the lifetime of the session, so this restores the scene target.
	glBindFramebuffer(GL_FRAMEBUFFER,0);
}

// ---- VR main-menu off-screen pass ----------------------------------------
// Same shape as HUD/GUI above, but driven by FsVrMenuDataPointer (the
// main-menu state block).  The menu FBO is a plain mono RGBA 2D texture
// (not a multiview texture-array), allocated by setupMenu in fswebxr.cpp
// when the WebXR layers path is available.  FsVrSetHudRenderTarget /
// FsSetWindowSizeOverride are reused as-is (same shared active/size pair
// used for HUD and GUI -- see fsvr.h's doc comment on FsVrSetHudRenderTarget:
// the three passes never run concurrently within a frame, so one set of
// state variables is enough).
void FsVrBeginMenuRender(void)
{
	const float *menuData=FsVrMenuDataPointer();
	const GLuint menuFbo=(GLuint)menuData[1];
	const int texW=(int)menuData[3];
	const int texH=(int)menuData[4];
	FsVrSetHudRenderTarget(1,texW,texH);
	FsSetWindowSizeOverride(1,texW,texH);
	glBindFramebuffer(GL_FRAMEBUFFER,menuFbo);
	FsVrSetMenuPassActive(1);
}

void FsVrEndMenuRender(void)
{
	FsVrSetMenuPassActive(0);
	glBindFramebuffer(GL_FRAMEBUFFER,0);
	FsVrSetHudRenderTarget(0,0,0);
	FsSetWindowSizeOverride(0,0,0);
}

void FsVrDrawGuiQuad(const float corner[12])
{
	if(NULL==fsHudQuadRenderer)
	{
		return;
	}

	const float *gui=FsVrGuiDataPointer();
	GLuint guiTexArray=(GLuint)gui[2];

	// Restore the scene (eye-0) viewport for the composite into the multiview
	// framebuffer (FsVrEndGuiRender left the GUI-texture-sized viewport).
	int x0,y0,wid,hei;
	FsVrGetEyeViewport(0,x0,y0,wid,hei);
	glViewport(x0,y0,wid,hei);

	GLfloat proj[32],modelView[16];
	FsGetLastSceneProjectionStereofv(proj);
	FsGetLastSceneModelViewfv(modelView);
	YsGLSLSetHudQuadRendererProjectionStereofv(fsHudQuadRenderer,proj);
	YsGLSLSetHudQuadRendererModelViewfv(fsHudQuadRenderer,modelView);

	// Save the state we touch, restore it after so no leak into later draws.
	GLboolean wasBlend=glIsEnabled(GL_BLEND);
	GLboolean wasDepthTest=glIsEnabled(GL_DEPTH_TEST);
	GLboolean wasCull=glIsEnabled(GL_CULL_FACE);
	GLint prevBlendSrc=GL_SRC_ALPHA,prevBlendDst=GL_ONE_MINUS_SRC_ALPHA;
	glGetIntegerv(GL_BLEND_SRC_RGB,&prevBlendSrc);
	glGetIntegerv(GL_BLEND_DST_RGB,&prevBlendDst);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST); // GUI glass draws over the scene (and over the HUD quad) regardless of depth.
	glDisable(GL_CULL_FACE);

	YsGLSLRenderHudQuad(fsHudQuadRenderer,corner,guiTexArray);

	if(GL_FALSE==wasBlend){ glDisable(GL_BLEND); }
	glBlendFunc((GLenum)prevBlendSrc,(GLenum)prevBlendDst);
	if(GL_FALSE!=wasDepthTest){ glEnable(GL_DEPTH_TEST); }
	if(GL_FALSE!=wasCull){ glEnable(GL_CULL_FACE); }
}

// ---- VR single-pass-stereo G-load blackout/redout full-field tint -------
// Same matrix/state-save pattern as FsVrDrawReticle (world-space geometry
// through the shared VariColor3D renderer, already stereo-projected by this
// frame's FsSetSceneProjection), but a FILLED quad (GL_TRIANGLE_FAN, like
// fsgroundskygl2.0.cpp's GL_TRIANGLE_STRIP/GL_TRIANGLES precedent on this
// same renderer) instead of GL_LINES, since this is a solid colour overlay,
// not line art.
void FsVrDrawFullScreenTint(const float corner[12],float r,float g,float b,float alpha)
{
	if(alpha<=0.0f)
	{
		return; // Early-out: no GPU cost / no state churn when there is no G-load effect active.
	}

	GLfloat proj[32],modelView[16];
	FsGetLastSceneProjectionStereofv(proj);
	FsGetLastSceneModelViewfv(modelView);
	YsGLSLSetShared3DRendererProjectionStereo(proj);
	YsGLSLSetShared3DRendererModelView(modelView);

	// Restore the scene (eye-0) viewport (the HUD/GUI off-screen passes leave
	// their own texture-sized viewport bound; whichever ran last, always
	// reset it here rather than relying on draw order).
	int x0,y0,wid,hei;
	FsVrGetEyeViewport(0,x0,y0,wid,hei);
	glViewport(x0,y0,wid,hei);

	// A blackout/redout tint must stay full-strength regardless of fog (same
	// reasoning as FsVrDrawReticle -- fog is re-established next frame).
	FsFogOff();

	// Save the state we touch, restore it after so no leak into later draws.
	GLboolean wasBlend=glIsEnabled(GL_BLEND);
	GLboolean wasDepthTest=glIsEnabled(GL_DEPTH_TEST);
	GLboolean wasCull=glIsEnabled(GL_CULL_FACE);
	GLint prevBlendSrc=GL_SRC_ALPHA,prevBlendDst=GL_ONE_MINUS_SRC_ALPHA;
	glGetIntegerv(GL_BLEND_SRC_RGB,&prevBlendSrc);
	glGetIntegerv(GL_BLEND_DST_RGB,&prevBlendDst);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST); // Covers everything -- scene, HUD glass, reticle -- regardless of depth.
	glDisable(GL_CULL_FACE);

	GLfloat colBuf[4*4];
	for(int i=0; i<4; ++i)
	{
		colBuf[i*4  ]=r;
		colBuf[i*4+1]=g;
		colBuf[i*4+2]=b;
		colBuf[i*4+3]=alpha;
	}

	auto *renderer=YsGLSLSharedVariColor3DRenderer();
	YsGLSLUse3DRenderer(renderer);
	YsGLSLDrawPrimitiveVtxColfv(renderer,GL_TRIANGLE_FAN,4,corner,colBuf);
	YsGLSLEndUse3DRenderer(renderer);

	if(GL_FALSE==wasBlend){ glDisable(GL_BLEND); }
	glBlendFunc((GLenum)prevBlendSrc,(GLenum)prevBlendDst);
	if(GL_FALSE!=wasDepthTest){ glEnable(GL_DEPTH_TEST); }
	if(GL_FALSE!=wasCull){ glEnable(GL_CULL_FACE); }
}

void FsBeginDrawShadow(void)  // Set polygon offset -1,-1 and enable.
{
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-3,-3);
}

void FsEndDrawShadow(void)    // Disable polygon offset.
{
	glDisable(GL_POLYGON_OFFSET_FILL);
}

void FsSetCameraPosition(const YsVec3 &pos,const YsAtt3 &att,YSBOOL zClear)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsSetCameraPosition In");
#endif

	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);

	if(zClear==YSTRUE)
	{
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	glEnable(GL_CULL_FACE);

	YsMatrix4x4 tfm;
	tfm.Scale(1.0,1.0,-1.0);
	tfm.RotateXY(-att.b());
	tfm.RotateZY(-att.p());
	tfm.RotateXZ(-att.h());
	tfm.Translate(-pos);
	GLfloat modelViewMat[16];
	tfm.GetOpenGlCompatibleMatrix(modelViewMat);
	YsGLSLSetShared3DRendererModelView(modelViewMat);

	// Cache the world->eye0 modelView for the VR HUD-quad composite.
	for(int i=0; i<16; ++i)
	{
		fsLastSceneModelView[i]=modelViewMat[i];
	}

	auto fsBitmapFontRenderer=YsGLSLSharedBitmapFontRenderer();
	YsGLSLUseBitmapFontRenderer(fsBitmapFontRenderer);
	YsGLSLSetBitmapFontRendererModelViewfv(fsBitmapFontRenderer,modelViewMat);
	YsGLSLEndUseBitmapFontRenderer(fsBitmapFontRenderer);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsSetCameraPosition Out");
#endif
}

void FsFlushScene(void)
{
	// This function is for compabitiliby with Blue Impulse 3DG-SDK only.
	// Nothing to do here.
	glFlush();
}

void FsDrawString(int x,int y,const char str[],const YsColor &col)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawString In");
#endif

	fsDirectFixedFontRenderer.RenderAsciiString(x,y,str,col);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawString Out");
#endif
}

void FsDrawLine(int x1,int y1,int x2,int y2,const YsColor &col)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawLine In");
#endif

	const GLfloat vtx[2*2]=
	{
		(GLfloat)x1,
		(GLfloat)y1,
		(GLfloat)x2,
		(GLfloat)y2,
	};
	const GLfloat color[4*2]=
	{
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f,
	};

	YsGLSLPlain2DRenderer *renderer=YsGLSLSharedPlain2DRenderer();
	YsGLSLUsePlain2DRenderer(renderer);
	YsGLSLDrawPlain2DPrimitivefv(renderer,GL_LINES,2,vtx,color);
	YsGLSLEndUsePlain2DRenderer(renderer);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawLine Out");
#endif
}

void FsDrawRect(int x1,int y1,int x2,int y2,const YsColor &col,YSBOOL fill)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawRect In");
#endif

	const GLfloat vtx[2*4]=
	{
		(GLfloat)x1,(GLfloat)y1,
		(GLfloat)x2,(GLfloat)y1,
		(GLfloat)x2,(GLfloat)y2,
		(GLfloat)x1,(GLfloat)y2
	};
	const GLfloat color[4*4]=
	{
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f
	};

	YsGLSLPlain2DRenderer *renderer=YsGLSLSharedPlain2DRenderer();
	YsGLSLUsePlain2DRenderer(renderer);

	if(fill==YSTRUE)
	{
		YsGLSLDrawPlain2DPrimitivefv(renderer,GL_TRIANGLE_FAN,4,vtx,color);
	}
	else
	{
		YsGLSLDrawPlain2DPrimitivefv(renderer,GL_LINE_LOOP,4,vtx,color);
	}

	YsGLSLEndUsePlain2DRenderer(renderer);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawRect Out");
#endif
}

void FsDrawCircle(int x,int y,int rad,const YsColor &col,YSBOOL fill)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawCircle In");
#endif

	static YsArray <GLfloat> vtx,color;

	if(vtx.GetN()<128)
	{
		vtx.Set(128,NULL);
	}
	if(color.GetN()<256)
	{
		color.Set(256,NULL);
	}

	for(int i=0; i<64; i++)
	{
		const GLfloat a=(GLfloat)YsPi*((GLfloat)i)/32.0f;
		vtx[i*2  ]=(GLfloat)x+(GLfloat)rad*cosf(a);
		vtx[i*2+1]=(GLfloat)y+(GLfloat)rad*sinf(a);
		color[i*4  ]=col.Rf();
		color[i*4+1]=col.Gf();
		color[i*4+2]=col.Bf();
		color[i*4+3]=1.0f;
	}

	YsGLSLPlain2DRenderer *renderer=YsGLSLSharedPlain2DRenderer();
	YsGLSLUsePlain2DRenderer(renderer);
	if(fill==YSTRUE)
	{
		YsGLSLDrawPlain2DPrimitivefv(renderer,GL_TRIANGLE_FAN,64,vtx,color);
	}
	else
	{
		YsGLSLDrawPlain2DPrimitivefv(renderer,GL_LINE_LOOP,64,vtx,color);
	}
	YsGLSLEndUsePlain2DRenderer(renderer);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawCircle Out");
#endif
}

void FsDrawPolygon(int n,int plg[],const YsColor &col)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawPolygon In");
#endif

	YsArray <GLfloat,32> vtx;
	YsArray <GLfloat,64> color;
	vtx.Set(n*2,NULL);
	color.Set(n*4,NULL);
	for(int i=0; i<n; ++i)
	{
		vtx[i*2  ]=(GLfloat)plg[i*2];
		vtx[i*2+1]=(GLfloat)plg[i*2+1];
		color[i*4  ]=col.Rf();
		color[i*4+1]=col.Gf();
		color[i*4+2]=col.Bf();
		color[i*4+3]=1.0f;
	}

	YsGLSLPlain2DRenderer *renderer=YsGLSLSharedPlain2DRenderer();
	YsGLSLUsePlain2DRenderer(renderer);
	YsGLSLDrawPlain2DPrimitivefv(renderer,GL_TRIANGLE_FAN,n,vtx,color);
	YsGLSLEndUsePlain2DRenderer(renderer);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawPolygon Out");
#endif
}

void FsDrawDiamond(int x,int y,int r,const YsColor &col,YSBOOL fill)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawDiamond In");
#endif

	const GLfloat vtx[8]=
	{
		(GLfloat)(x+r),(GLfloat)(y  ),
		(GLfloat)(x  ),(GLfloat)(y+r),
		(GLfloat)(x-r),(GLfloat)(y  ),
		(GLfloat)(x  ),(GLfloat)(y-r)
	};
	const GLfloat color[16]=
	{
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f
	};

	YsGLSLPlain2DRenderer *renderer=YsGLSLSharedPlain2DRenderer();
	YsGLSLUsePlain2DRenderer(renderer);

	if(fill==YSTRUE)
	{
		YsGLSLDrawPlain2DPrimitivefv(renderer,GL_TRIANGLE_FAN,4,vtx,color);
	}
	else
	{
		YsGLSLDrawPlain2DPrimitivefv(renderer,GL_LINE_LOOP,4,vtx,color);
	}

	YsGLSLEndUsePlain2DRenderer(renderer);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawDiamond Out");
#endif
}

void FsDrawX(int x,int y,int r,const YsColor &col)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawX In");
#endif

	const GLfloat vtx[8]=
	{
		(GLfloat)(x-r),(GLfloat)(y-r),
		(GLfloat)(x+r),(GLfloat)(y+r),
		(GLfloat)(x+r),(GLfloat)(y-r),
		(GLfloat)(x-r),(GLfloat)(y+r)
	};

	const GLfloat color[16]=
	{
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f
	};

	YsGLSLPlain2DRenderer *renderer=YsGLSLSharedPlain2DRenderer();
	YsGLSLUsePlain2DRenderer(renderer);

	YsGLSLDrawPlain2DPrimitivefv(renderer,GL_LINES,4,vtx,color);

	YsGLSLEndUsePlain2DRenderer(renderer);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawX Out");
#endif
}

void FsDrawPoint(int x,int y,const YsColor &col)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawPoint In");
#endif

	const GLfloat vtx[2]=
	{
		(GLfloat)x,(GLfloat)y
	};
	const GLfloat color[4]=
	{
		col.Rf(),col.Gf(),col.Bf(),1.0f
	};

	YsGLSLPlain2DRenderer *renderer=YsGLSLSharedPlain2DRenderer();
	YsGLSLUsePlain2DRenderer(renderer);
	YsGLSLDrawPlain2DPrimitivefv(renderer,GL_POINTS,1,vtx,color);
	YsGLSLEndUsePlain2DRenderer(renderer);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawPoint Out");
#endif
}

void FsDrawPoint2Pix(int x,int y,const YsColor &col)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawPoint2Pix In");
#endif

	const GLfloat vtx[8]=
	{
		(GLfloat)(x  ),(GLfloat)(y  ),
		(GLfloat)(x+1),(GLfloat)(y  ),
		(GLfloat)(x+1),(GLfloat)(y+1),
		(GLfloat)(x  ),(GLfloat)(y+1)
	};
	const GLfloat color[16]=
	{
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f
	};
	YsGLSLPlain2DRenderer *renderer=YsGLSLSharedPlain2DRenderer();
	YsGLSLUsePlain2DRenderer(renderer);
	YsGLSLDrawPlain2DPrimitivefv(renderer,GL_POINTS,4,vtx,color);
	YsGLSLEndUsePlain2DRenderer(renderer);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawPoint2Pix Out");
#endif
}

void FsDrawTitleBmp(const YsBitmap &bmp,YSBOOL tile)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawTitleBmp In");
#endif
	YsGLSLBitmapRenderer *renderer=YsGLSLSharedBitmapRenderer();
	YsGLSLUseBitmapRenderer(renderer);

	GLuint texId[1];
	glGenTextures(1,texId);

	int x,y;
	const int bmpWid=bmp.GetWidth();
	const int bmpHei=bmp.GetHeight();
	const unsigned char *bmpPtr=bmp.GetRGBABitmapPointer();

	int wid,hei;
	FsGetWindowSize(wid,hei);

	// zoomFactor=float(wid)/float(bmp.GetWidth());
	// glPixelZoom(zoomFactor,zoomFactor);

	GLenum prevActiveTexture;
	GLuint prevBinding2d;

	glGetIntegerv(GL_ACTIVE_TEXTURE,(GLint *)&prevActiveTexture);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D,(GLint *)&prevBinding2d);

	glBindTexture(GL_TEXTURE_2D,texId[0]);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);  // NPOT-safe for WebGL1
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,bmpWid,bmpHei,0,GL_RGBA,GL_UNSIGNED_BYTE,bmpPtr);

	if(tile==YSTRUE)
	{
		for(x=0; x<wid; x+=bmpWid)
		{
			for(y=hei-1; y>0; y-=bmpHei)
			{
				YsGLSLRenderTexture2D(renderer,x,y,YSGLSL_HALIGN_LEFT,YSGLSL_VALIGN_BOTTOM,bmpWid,bmpHei,texId[0]);
			}
		}
	}
	else
	{
		YsGLSLRenderTexture2D(renderer,wid-bmpWid,hei-1,YSGLSL_HALIGN_LEFT,YSGLSL_VALIGN_BOTTOM,bmpWid,bmpHei,texId[0]);
	}

	glBindTexture(GL_TEXTURE_2D,prevBinding2d);
	glActiveTexture(prevActiveTexture);

	glDeleteTextures(1,texId);

	YsGLSLEndUseBitmapRenderer(renderer);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawTitleBmp Out");
#endif
}

void FsDrawBmp(const YsBitmap &bmp,int x,int y)
{
#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawBmp In");
#endif

	const int bmpWid=bmp.GetWidth();
	const int bmpHei=bmp.GetHeight();
	const unsigned char *rgba=bmp.GetRGBABitmapPointer();

	YsGLSLBitmapRenderer *renderer=YsGLSLSharedBitmapRenderer();
	YsGLSLUseBitmapRenderer(renderer);

	// The bitmap (system-font text) carries alpha; without blending it
	// shows up as a solid rectangle.
	const GLboolean blendWasEnabled=glIsEnabled(GL_BLEND);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	// 2D overlay text must draw unconditionally.  The cockpit/canopy pass
	// leaves the stencil test in a state that can reject these pixels
	// (driver-dependent), which blanked the messages.
	const GLboolean stencilWasEnabled=glIsEnabled(GL_STENCIL_TEST);
	glDisable(GL_STENCIL_TEST);

	YsGLSLRenderRGBABitmap2D(renderer,x,y,YSGLSL_HALIGN_LEFT,YSGLSL_VALIGN_TOP,bmpWid,bmpHei,rgba);

	if(GL_TRUE==stencilWasEnabled)
	{
		glEnable(GL_STENCIL_TEST);
	}
	if(GL_TRUE!=blendWasEnabled)
	{
		glDisable(GL_BLEND);
	}

	YsGLSLEndUseBitmapRenderer(renderer);

#ifdef YSOGLERRORCHECK
	FsOpenGlShowError("FsDrawBmp Out");
#endif
}

void FsDrawLine3d(const YsVec3 &p1,const YsVec3 &p2,const YsColor &col)
{
	const GLfloat vtx[]=
	{
		(GLfloat)p1.x(),
		(GLfloat)p1.y(),
		(GLfloat)p1.z(),
		(GLfloat)p2.x(),
		(GLfloat)p2.y(),
		(GLfloat)p2.z()
	};
	const GLfloat color[4*2]=
	{
		col.Rf(),col.Gf(),col.Bf(),1.0f,
		col.Rf(),col.Gf(),col.Bf(),1.0f,
	};

	auto *renderer=YsGLSLSharedVariColor3DRenderer();
	YsGLSLUse3DRenderer(renderer);
	YsGLSLDrawPrimitiveVtxColfv(renderer,GL_LINES,2,vtx,color);
	YsGLSLEndUse3DRenderer(renderer);
}


void FsDrawMask(const YsColor &fgCol,const YsColor &bgCol,int x0,int y0,int wid,int hei)
{
	YsDisregardVariable(bgCol);

	const GLfloat vtx[8]=
	{
		(GLfloat)(x0    ),(GLfloat)(y0),
		(GLfloat)(x0+wid),(GLfloat)(y0),
		(GLfloat)(x0+wid),(GLfloat)(y0+hei),
		(GLfloat)(x0    ),(GLfloat)(y0+hei)
	};

	GLfloat r,g,b,a;
	if(fgCol.Ri()>200 && fgCol.Gi()>200 && fgCol.Bi()>200)
	{
		r=0.2f;
		g=0.2f;
		b=0.2f;
		a=0.8f;
	}
	else
	{
		r=0.8f;
		g=0.8f;
		b=0.8f;
		a=0.8f;
	}

	const GLfloat color[16]=
	{
		r,g,b,a,
		r,g,b,a,
		r,g,b,a,
		r,g,b,a
	};

	YsGLSLPlain2DRenderer *renderer=YsGLSLSharedPlain2DRenderer();
	YsGLSLUsePlain2DRenderer(renderer);
	YsGLSLDrawPlain2DPrimitivefv(renderer,GL_TRIANGLE_FAN,4,vtx,color);
	YsGLSLEndUsePlain2DRenderer(renderer);
}

void FsSetClipRect(int x0,int y0,int wid,int hei)
{
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT,viewport);
	glScissor(x0,viewport[3]-y0-hei,wid,hei+1);
	glEnable(GL_SCISSOR_TEST);
}

void FsClearClipRect(void)
{
	glDisable(GL_SCISSOR_TEST);
}


void FsGraphicsTest(int i)
{
	auto bitmapRenderer=YsGLSLSharedBitmapRenderer();
	YsGLSLUseBitmapRenderer(bitmapRenderer);
	YsGLSLRenderTexture2D(bitmapRenderer,0,0,YSGLSL_HALIGN_LEFT,YSGLSL_VALIGN_TOP,256,256,i);
	YsGLSLEndUseBitmapRenderer(bitmapRenderer);
}

