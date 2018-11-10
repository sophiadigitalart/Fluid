#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/params/Params.h"
#include "cinder/Rand.h"
// Settings
#include "SDASettings.h"
// Session
#include "SDASession.h"
// Log
#include "SDALog.h"
// Spout
#include "CiSpoutOut.h"
// fluid
#include "cinderfx/Fluid2D.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace SophiaDigitalArt;

class FluidApp : public App {

public:
	FluidApp();
	void mouseMove(MouseEvent event) override;
	void mouseDown(MouseEvent event) override;
	void mouseDrag(MouseEvent event) override;
	void mouseUp(MouseEvent event) override;
	void touchesBegan(ci::app::TouchEvent event);
	void touchesMoved(ci::app::TouchEvent event);
	void touchesEnded(ci::app::TouchEvent event);
	void keyDown(KeyEvent event) override;
	void keyUp(KeyEvent event) override;
	void fileDrop(FileDropEvent event) override;
	void update() override;
	void draw() override;
	void cleanup() override;
	void setUIVisibility(bool visible);
private:
	// Settings
	SDASettingsRef					mSDASettings;
	// Session
	SDASessionRef					mSDASession;
	// Log
	SDALogRef						mSDALog;
	// fluid
	float						mVelScale;
	float						mDenScale;
	float						mRgbScale;
	ci::vec2					mPrevPos;
	ci::Colorf					mColor;
	std::map<int, ci::Colorf>	mTouchColors;
	cinderfx::Fluid2D			mFluid2D;
	ci::gl::Texture2dRef		mTex;
	ci::params::InterfaceGl		mParams;
	// imgui
	float							color[4];
	float							backcolor[4];
	int								playheadPositions[12];
	int								speeds[12];

	float							f = 0.0f;
	char							buf[64];
	unsigned int					i, j;

	bool							mouseGlobal;

	string							mError;
	// fbo
	bool							mIsShutDown;
	Anim<float>						mRenderWindowTimer;
	void							positionRenderWindow();
	bool							mFadeInDelay;
	SpoutOut 						mSpoutOut;
};


FluidApp::FluidApp()
	: mSpoutOut("SDAFluid", app::getWindowSize())
{
	// Settings
	mSDASettings = SDASettings::create();
	// Session
	mSDASession = SDASession::create(mSDASettings);
	//mSDASettings->mCursorVisible = true;
	setUIVisibility(mSDASettings->mCursorVisible);
	mSDASession->getWindowsResolution();

	mouseGlobal = false;
	mFadeInDelay = true;
	// fluid
	glEnable(GL_TEXTURE_2D);

	mFluid2D.enableDensity();
	mFluid2D.enableRgb();
	mFluid2D.enableVorticityConfinement();

	mDenScale = 50;
	mRgbScale = 50;

	mFluid2D.set(192, 192);
	mFluid2D.setDensityDissipation(0.99f);
	mFluid2D.setRgbDissipation(0.99f);
	mVelScale = 3.0f*std::max(mFluid2D.resX(), mFluid2D.resY());

	mParams = params::InterfaceGl("Params", ivec2(300, 400));
	mParams.addParam("Stam Step", mFluid2D.stamStepAddr());
	mParams.addSeparator();
	mParams.addParam("Velocity Input Scale", &mVelScale, "min=0 max=10000 step=1");
	mParams.addParam("Density Input Scale", &mDenScale, "min=0 max=1000 step=1");
	mParams.addParam("Rgb Input Scale", &mRgbScale, "min=0 max=1000 step=1");
	mParams.addSeparator();
	mParams.addParam("Velocity Dissipation", mFluid2D.velocityDissipationAddr(), "min=0.0001 max=1 step=0.0001");
	mParams.addParam("Density Dissipation", mFluid2D.densityDissipationAddr(), "min=0.0001 max=1 step=0.0001");
	mParams.addParam("Rgb Dissipation", mFluid2D.rgbDissipationAddr(), "min=0.0001 max=1 step=0.0001");
	mParams.addSeparator();
	mParams.addParam("Velocity Viscosity", mFluid2D.velocityViscosityAddr(), "min=0.000001 max=10 step=0.000001");
	mParams.addParam("Density Viscosity", mFluid2D.densityViscosityAddr(), "min=0.000001 max=10 step=0.000001");
	mParams.addParam("Rgb Viscosity", mFluid2D.rgbViscosityAddr(), "min=0.000001 max=10 step=0.000001");
	mParams.addSeparator();
	mParams.addParam("Vorticity Confinement", mFluid2D.enableVorticityConfinementAddr());
	mParams.addSeparator();
	std::vector<std::string> boundaries;
	boundaries.push_back("None"); boundaries.push_back("Wall"); boundaries.push_back("Wrap");
	mParams.addParam("Boundary Type", boundaries, mFluid2D.boundaryTypeAddr());
	mParams.addSeparator();
	mParams.addParam("Enable Buoyancy", mFluid2D.enableBuoyancyAddr());
	mParams.addParam("Buoyancy Scale", mFluid2D.buoyancyScaleAddr(), "min=0 max=100 step=0.001");
	mParams.addParam("Vorticity Scale", mFluid2D.vorticityScaleAddr(), "min=0 max=1 step=0.001");


	// windows
	mIsShutDown = false;
	mRenderWindowTimer = 0.0f;
	//timeline().apply(&mRenderWindowTimer, 1.0f, 2.0f).finishFn([&] { positionRenderWindow(); });

}
void FluidApp::positionRenderWindow() {
	mSDASettings->mRenderPosXY = ivec2(mSDASettings->mRenderX, mSDASettings->mRenderY);//20141214 was 0
	setWindowPos(mSDASettings->mRenderX, mSDASettings->mRenderY);
	setWindowSize(mSDASettings->mRenderWidth, mSDASettings->mRenderHeight);
}
void FluidApp::setUIVisibility(bool visible)
{
	if (visible)
	{
		showCursor();
	}
	else
	{
		hideCursor();
	}
}
void FluidApp::fileDrop(FileDropEvent event)
{
	mSDASession->fileDrop(event);
}
void FluidApp::update()
{
	mSDASession->setFloatUniformValueByIndex(mSDASettings->IFPS, getAverageFps());
	mSDASession->update();
	mFluid2D.step();
}
void FluidApp::cleanup()
{
	if (!mIsShutDown)
	{
		mIsShutDown = true;
		CI_LOG_V("shutdown");
		// save settings
		mSDASettings->save();
		mSDASession->save();
		quit();
	}
}
void FluidApp::mouseMove(MouseEvent event)
{
	if (!mSDASession->handleMouseMove(event)) {
		// let your application perform its mouseMove handling here
	}
}
void FluidApp::mouseDown(MouseEvent event)
{
	mPrevPos = event.getPos();
	mColor.r = Rand::randFloat();
	mColor.g = Rand::randFloat();
	mColor.b = Rand::randFloat();
}
void FluidApp::mouseDrag(MouseEvent event)
{
	float x = (event.getX() / (float)getWindowWidth())*mFluid2D.resX();
	float y = (event.getY() / (float)getWindowHeight())*mFluid2D.resY();

	if (event.isLeftDown()) {
		vec2 dv = vec2(event.getPos()) - mPrevPos;
		mFluid2D.splatVelocity(x, y, mVelScale*dv);
		mFluid2D.splatRgb(x, y, mRgbScale*mColor);
		if (mFluid2D.isBuoyancyEnabled()) {
			mFluid2D.splatDensity(x, y, mDenScale);
		}
	}

	mPrevPos = event.getPos();
}
void FluidApp::mouseUp(MouseEvent event)
{
	if (!mSDASession->handleMouseUp(event)) {
		// let your application perform its mouseUp handling here
	}
}
void FluidApp::touchesBegan(TouchEvent event)
{
	const std::vector<TouchEvent::Touch>& touches = event.getTouches();
	for (std::vector<TouchEvent::Touch>::const_iterator cit = touches.begin(); cit != touches.end(); ++cit) {
		Colorf color;
		color.r = Rand::randFloat();
		color.g = Rand::randFloat();
		color.b = Rand::randFloat();
		mTouchColors[cit->getId()] = color;
	}
}

void FluidApp::touchesMoved(TouchEvent event)
{
	const std::vector<TouchEvent::Touch>& touches = event.getTouches();
	for (std::vector<TouchEvent::Touch>::const_iterator cit = touches.begin(); cit != touches.end(); ++cit) {
		if (mTouchColors.find(cit->getId()) == mTouchColors.end())
			continue;
		vec2 prevPos = cit->getPrevPos();
		vec2 pos = cit->getPos();
		float x = (pos.x / (float)getWindowWidth())*mFluid2D.resX();
		float y = (pos.y / (float)getWindowHeight())*mFluid2D.resY();
		vec2 dv = pos - prevPos;
		mFluid2D.splatVelocity(x, y, mVelScale*dv);
		mFluid2D.splatRgb(x, y, mRgbScale*mTouchColors[cit->getId()]);
		if (mFluid2D.isBuoyancyEnabled()) {
			mFluid2D.splatDensity(x, y, mDenScale);
		}
	}
}

void FluidApp::touchesEnded(TouchEvent event)
{
	const std::vector<TouchEvent::Touch>& touches = event.getTouches();
	for (std::vector<TouchEvent::Touch>::const_iterator cit = touches.begin(); cit != touches.end(); ++cit) {
		mTouchColors.erase(cit->getId());
	}
}

void FluidApp::keyDown(KeyEvent event)
{
	
		switch (event.getCode()) {
		case KeyEvent::KEY_r:
			mFluid2D.initSimData();
			break;
		case KeyEvent::KEY_ESCAPE:
			// quit the application
			quit();
			break;
		case KeyEvent::KEY_h:
			// mouse cursor and ui visibility
			mSDASettings->mCursorVisible = !mSDASettings->mCursorVisible;
			setUIVisibility(mSDASettings->mCursorVisible);
			break;
		}
	
}
void FluidApp::keyUp(KeyEvent event)
{
	if (!mSDASession->handleKeyUp(event)) {
	}
}

void FluidApp::draw()
{
	gl::clear(Color::black());
	if (mFadeInDelay) {
		mSDASettings->iAlpha = 0.0f;
		if (getElapsedFrames() > mSDASession->getFadeInDelay()) {
			mFadeInDelay = false;
			timeline().apply(&mSDASettings->iAlpha, 0.0f, 1.0f, 1.5f, EaseInCubic());
		}
	}

	//RenderFluidRgb( mFluid2D, getWindowBounds() );
	float* data = const_cast<float*>((float*)mFluid2D.rgb().data());
	Surface32f surf(data, mFluid2D.resX(), mFluid2D.resY(), mFluid2D.resX() * sizeof(Colorf), SurfaceChannelOrder::RGB);

	if (!mTex) {
		mTex = gl::Texture::create(surf);
	}
	else {
		mTex->update(surf);
	}
	gl::draw(mTex, getWindowBounds());


	// Spout Send
	mSpoutOut.sendViewport();
	mParams.draw();
	getWindow()->setTitle(mSDASettings->sFps + " fps SDAFluid");
}

void prepareSettings(App::Settings *settings)
{
	settings->setWindowSize(640, 480);
}

CINDER_APP(FluidApp, RendererGl, prepareSettings)
