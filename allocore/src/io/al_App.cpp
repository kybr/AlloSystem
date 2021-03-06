#include <stdio.h>
#include "allocore/io/al_App.hpp"

namespace al{
//______________________________________________________________________________

Viewpoint::Viewpoint(const Pose& transform)
:	mViewport(0,0,0,0),
	mParentTransform(NULL), mTransform(transform),
	mAnchorX(0), mAnchorY(0), mStretchX(1), mStretchY(1),
	mLens(NULL), mClearColor(NULL)
{}

Viewpoint& Viewpoint::anchor(float ax, float ay){
	mAnchorX=ax; mAnchorY=ay; return *this;
}

Viewpoint& Viewpoint::stretch(float sx, float sy){
	mStretchX=sx; mStretchY=sy; return *this;
}

Frustumd Viewpoint::frustum() const {
	Frustumd fr;
	lens().frustum(fr, worldTransform(), viewport().aspect());
	return fr;
}

void Viewpoint::onParentResize(int w, int h){
	mViewport.l = w * anchorX();
	mViewport.b = h * anchorY();
	mViewport.w = w * stretchX();
	mViewport.h = h * stretchY();
}

//______________________________________________________________________________

ViewpointWindow::ViewpointWindow(){
	append(mStandardKeyControls);
}

ViewpointWindow::ViewpointWindow(
	const Dim& dims,
	const std::string title,
	double fps,
	DisplayMode mode
)
:	ViewpointWindow()
{
	create(dims, title, fps, mode);
}

bool ViewpointWindow::onResize(int w, int h){
	//printf("ViewpointWindow onResize: %d %d\n", w, h);
	for(auto * vp : mViewpoints){
		vp->onParentResize(w, h);
		//printf("%g %g %g %g\n", vp->viewport().l, vp->viewport().b, vp->viewport().w, vp->viewport().h);
	}

	mResized = true;

	return true;
}

ViewpointWindow& ViewpointWindow::add(Viewpoint& v){
	mViewpoints.push_back(&v);

	// If the window is already created, then we need to manually update the
	// Viewpoint. Otherwise, this happens through ViewpointWindow::onResize().
	if(created()){
		v.onParentResize(width(), height());
	}
	return *this;
}

//______________________________________________________________________________

struct SceneInputHandler : public InputEventHandler{
	ViewpointWindow& win;
	App& app;

	SceneInputHandler(ViewpointWindow& w, App& a): win(w), app(a){}

	bool onKeyDown(const Keyboard& k){
		app.onKeyDown(win, k);
		switch(k.key()){
			case Keyboard::TAB:
				app.stereo().stereo(!app.stereo().stereo());
				return false;
			default:;
		}
		return true;
	}
	bool onKeyUp(const Keyboard& k){ app.onKeyUp(win,k); return true;}
	bool onMouseDown(const Mouse& m){ app.onMouseDown(win,m); return true;}
	bool onMouseUp(const Mouse& m){ app.onMouseUp(win,m); return true;}
	bool onMouseDrag(const Mouse& m){ app.onMouseDrag(win,m); return true;}
	bool onMouseMove(const Mouse& m){ app.onMouseMove(win,m); return true;}
};

// attached to each ViewpointWindow
struct SceneWindowHandler : public WindowEventHandler{
	ViewpointWindow& win;
	App& app;

	SceneWindowHandler(ViewpointWindow& w, App& a): win(w), app(a){}

	bool onCreate(){

		// FIXME: only do this if actually using fixed pipeline
		#ifdef AL_GRAPHICS_SUPPORTS_FIXED_PIPELINE
		// Enable color material to simplify cases where materials are not used 
		// explicitly (e.g., only mesh colors are used).
		glEnable(GL_COLOR_MATERIAL);
			#ifdef AL_GRAPHICS_SUPPORTS_COLOR_MATERIAL_SPEC
			glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
			#endif
		#endif

		app.onCreate(win);
		return true;
	}
	bool onDestroy(){ app.onDestroy(win); return true; }
	bool onResize(int dw, int dh){ app.onResize(win, dw,dh); return true; }

	virtual bool onFrame();
};

bool SceneWindowHandler::onFrame(){

	if(app.clockNav() == &win){
		double dt = win.spfActual();
		//double dt = win.spf(); // use theoretical dt for smooth control
		app.nav().smooth(::pow(0.0001, dt));
		app.nav().step(dt * 40./*FPS*/);
		//app.nav().smooth(0.8);
		//app.nav().step(1.);
	}

	if(app.clockAnimate() == &win){
		app.onAnimate(win.spfActual());
	}

	// Quatd accumulated rotations are very precise, so we should rarely need
	// to normalize it
	if(app.nav().quat().magSqr() > 1.0000001){
		app.nav().quat().normalize();
	}

	app.navDraw() = app.nav();

	Graphics& g = app.graphics();
	g.depthTesting(true);
	//g.lighting(false); // should be disabled by default

	struct DrawFunc : public Drawable {
		App& app;
		Viewpoint& vp;
		ViewpointWindow& win;

		DrawFunc(App& a, Viewpoint& v, ViewpointWindow& w)
		:	app(a), vp(v), win(w){}

		void onDraw(Graphics& g){
			g.pushMatrix(g.MODELVIEW);
			for(auto& drawCall : win.drawCalls()) drawCall();
			app.onDraw(g,vp);
			g.popMatrix(g.MODELVIEW);
		}
	};

	// Draw scene for each viewpoint (pose + viewport)
	for(auto * vpPtr : win.viewpoints()){

		Viewpoint& vp = *vpPtr;

		// if no camera, set to default scene camera
		if(!vp.hasLens()) vp.lens(app.lens());
		const Lens& lens = vp.lens();
		Stereographic& stereo = app.stereo();

		Color defaultClearColor = stereo.clearColor();
		if(!vp.hasClearColor()){
			vp.clearColor(const_cast<Color&>(stereo.clearColor()));
		}
		else{
			stereo.clearColor(vp.clearColor());
		}

		bool doClear = (stereo.clearColor().a >= 1) || win.mResized;

		// Interpolate between existing color buffer and background color
		if(!doClear){

			// TODO: Not 100% sure what to do with depth buffer.
			// If we don't clear it, then if depth testing is enabled we won't
			// get the expected overdraw blend. By clearing it, we effectively
			// "flatten" the previous scene.
			glEnable(GL_SCISSOR_TEST);
			glScissor(vp.viewport().l, vp.viewport().b, vp.viewport().w, vp.viewport().h);
			glClear(GL_DEPTH_BUFFER_BIT);
			glDisable(GL_SCISSOR_TEST);

			if(stereo.clearColor().a != 0){
				glViewport(vp.viewport().l, vp.viewport().b, vp.viewport().w, vp.viewport().h);
				g.pushMatrix(g.MODELVIEW);
				g.loadIdentity();
				g.pushMatrix(g.PROJECTION);
				g.loadIdentity();
					Mesh& mesh = g.mesh();
					mesh.reset();
					mesh.primitive(Graphics::TRIANGLE_STRIP);
					mesh.color(stereo.clearColor());
					mesh.vertex(-1,-1);
					mesh.vertex( 1,-1);
					mesh.vertex(-1, 1);
					mesh.vertex( 1, 1);
					g.blendTrans();
					//glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
						g.draw(mesh);
					g.blendOff();
				g.popMatrix();
				g.popMatrix(g.MODELVIEW);
			}
		}

		DrawFunc drawFunc(app, vp, win);
		stereo.draw(g, lens, vp.worldTransform(), vp.viewport(), drawFunc, doClear);
		stereo.clearColor(defaultClearColor);
	}

	win.mResized = false;

	return true;
}


App::App()
:	mName(""),
	mNavControl(mNav),
	mClockAnimate(0), mClockNav(0),
	mOSCRecv(12001, "", 0.001),
	mOSCSend(12000, "bossanova.mat.ucsb.edu", 0)
{}


App::~App(){
	// delete factory objects
	for(auto * win : mFacWindows) delete win;
	for(auto * vp : mFacViewpoints) delete vp;

	if(oscSend().opened() && !name().empty()) sendDisconnect();
}

void App::initAudio(
	double audioRate, int audioBlockSize,
	int audioOutputs, int audioInputs
){
	mAudioIO.callback = [this](AudioIOData& io){
		if(clockNav() == &audioIO()){
			nav().smooth(0.95);
			nav().step(1./4);
		}

		if(clockAnimate() == &audioIO()){
			onAnimate(io.secondsPerBuffer());
		}

		io.frame(0);
		onSound(audioIO());
	};
	mAudioIO.framesPerSecond(audioRate);
	mAudioIO.framesPerBuffer(audioBlockSize);
	mAudioIO.channelsOut(audioOutputs);
	mAudioIO.channelsIn(audioInputs);
}


ViewpointWindow * App::initWindow(
	const Window::Dim& dims,
	const std::string title,
	double fps,
	Window::DisplayMode mode,
	int flags
){
	auto& win = *new ViewpointWindow;
	win.dimensions(dims);
	if(title.empty()){ // if no title, use app name, if any
		if(!name().empty()) win.title(name());
	} else {
		win.title(title);
	}

	win.fps(fps);
	win.displayMode(mode);

	auto& newVP = *new Viewpoint;
	mFacViewpoints.push_back(&newVP);
	newVP.parentTransform(nav().transformed());
	win.add(newVP);

	mFacWindows.push_back(&win);
	add(win);
	return &win;
}


App& App::add(ViewpointWindow& win){

	win.append(mNavControl);
	win.append(*new SceneWindowHandler(win, *this));
	win.append(*new SceneInputHandler(win, *this));
	mWindows.push_back(&win);

	// TODO: for now, first window will clock master Nav
	if(mWindows.size() == 1){
		mClockNav = mWindows[0];
	}
	return *this;
}


void App::start(){
	if(!clockAnimate() && !mWindows.empty()){
		clockAnimate(mWindows[0]);
	}
	if(usingAudio()) mAudioIO.start();
	if(oscSend().opened() && !name().empty()) sendHandshake();

	/*
	// factories OKAY
	for(unsigned i=0; i<mFacViewpoints.size(); ++i)
		printf("%p\n", mFacViewpoints[i].parentTransform());

	// window pointers OKAY
	for(unsigned j=0; j<windows().size(); ++j){
		ViewpointWindow& w = *windows()[j];
		for(unsigned i=0; i<w.viewpoints().size(); ++i){
			Viewpoint& vp = *w.viewpoints()[i];
			printf("%d,%d: %p\n", j,i, vp.parentTransform());
			printf("anchor : %g %g\n", vp.anchorX(), vp.anchorY());
			printf("stretch: %g %g\n", vp.stretchX(), vp.stretchY());
		}
	}
	//*/

	/* Add a handler to close i/o when Main exits.
	This is done to ensure members of derived classes are not accessed by i/o 
	threads after they have been destructed! We must stop all i/o before any
	destructors are called.
	*/
	struct AppMainHandler : Main::Handler{
		App& app;
		AppMainHandler(App& a): app(a){}

		void onExit(){
			//printf("App exiting\n");
			app.audioIO().close();

			// Ensures windows get destroyed in case, for example, the user does
			// a hard exit with exit(0).
			for(auto * win : app.mFacWindows) win->destroy();

			// ctrl-q will destroy all windows before stopping the main loop
			// so we call onExit last
			app.onExit();
		}
	} appMainHandler(*this);

	Main::get().add(appMainHandler);

	if(windows().size()){
		// create the windows
		for(auto& w : windows()) w->create();

		mStartTime = timeNow();

		// start the main loop
		Main::get().start();
	}
	else{
		mStartTime = timeNow();
		#ifdef AL_EMSCRIPTEN
		Main::get().start();
		#else
		printf("\nPress 'enter' to quit...\n"); getchar();
		#endif
		// ensure exit handler gets called
		Main::get().exit();
	}
}

double App::appTime() const {
	return timeNow() - mStartTime;
}

void App::sendHandshake(){
	oscSend().send("/handshake", name(), oscRecv().port());
}

void App::sendDisconnect(){
	oscSend().send("/disconnectApplication", name());
}

bool App::usingAudio() const {
	return audioIO().callback != nullptr;
}


/// Get a pick ray from screen space coordinates
 // i.e. use mouse xy
Rayd App::getPickRay(const ViewpointWindow& w, int screenX, int screenY){
	Rayd r;
	Vec3d screenPos;
	screenPos.x = (screenX*1. / w.width()) * 2. - 1.;
	screenPos.y = ((w.height() - screenY)*1. / w.height()) * 2. - 1.;
	screenPos.z = -1.;
	Vec3d worldPos = stereo().unproject(screenPos);
	r.origin().set(worldPos);

	screenPos.z = 1.;
	worldPos = stereo().unproject(screenPos);
	r.direction().set( worldPos );
	r.direction() -= r.origin();
	r.direction().normalize();
	return r;
}


//class Clocked{
//public:
//	virtual void onUpdate(double dt){}
//protected:
//};
//
//
//class Clock{
//public:
//
//	void add(Clocked& v){ mListeners.push_back(&v); }
//
//	void update(double dt){
//		Listeners::iterator it = mListeners.begin();
//		while(it != mListeners.end()){
//			(*it)->onUpdate(dt);
//		}
//	}
//
//protected:
//	typedef std::vector<Clocked *> Listeners;
//	Listeners mListeners;
//};

} // al::

