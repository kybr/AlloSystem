/*
Allocore Example: FBO

Description:
This demonstrates how to use an FBO
MipMaps:

FBOs won't generate the mip maps automatically
If texture filterMin is set to a MIPMAP option, then the texture will need to have mipmaps generated manually (after the FBO is unbound), using tex.generateMipmap();


Author:
Graham Wakefield, 2012
Mod:
Keehong Youn 2015 (port to gl3)

*/

#include "allocore/al_Allocore.hpp"

using namespace al;

int w=256, h=256;

Graphics gl;
Texture fbotex(w, h);
RBO rbo;
FBO fbo;

rnd::Random<> rng;

struct MyWindow : Window{

	bool onCreate(){

		// both depth and color attachees must be valid on the GPU before use:

		// resize calls bind, bind calls validate, validate calls onCreate
		rbo.resize(w, h);
		fbotex.validate();

		fbo.attachRBO(rbo, FBO::DEPTH_ATTACHMENT);
		fbo.attachTexture2D(fbotex.id(), FBO::COLOR_ATTACHMENT0);
		printf("fbo status %s\n", fbo.statusString());

		return true;
	}

	bool onFrame(){

		fbo.begin();

		// 	// capture green-world to texture:
			gl.viewport(0, 0, fbotex.width(), fbotex.height());
			gl.clearColor(0, 0.5, 0, 0);
			gl.clear(Graphics::COLOR_BUFFER_BIT | Graphics::DEPTH_BUFFER_BIT);

		// 	// gl.projection(Matrix4d::perspective(45, 1, 0.1, 10));
		// 	// gl.modelView(Matrix4d::identity());

		// 	// gl.begin(gl.TRIANGLES);
		// 	// gl.color(1, 1, 0);
		// 	// 	gl.vertex(rng.uniformS()*0.5, rng.uniformS()*0.5, -5);
		// 	// 	gl.vertex(rng.uniformS()*0.5, rng.uniformS()*0.5, -5);
		// 	// 	gl.vertex(rng.uniformS()*0.5, rng.uniformS()*0.5, -5);
		// 	// gl.end();

		fbo.end();

		// generation of Mipmaps must be done manually for FBO-bound textures:
		// (to see why, comment this code out and then make the window very small).
		//fbotex.generateMipmap();

		// show in blue-world:

		/* Do not pass the window size to glViewport or
		other pixel-based OpenGL calls. The window size is
		in screen coordinates, not pixels. Use the framebuffer size,
		which is in pixels, for pixel-based calls. */
		gl.viewport(0, 0, fbWidth(), fbHeight());
		gl.clearColor(0, 0, 0.5, 0);
		gl.clear(Graphics::COLOR_BUFFER_BIT | Graphics::DEPTH_BUFFER_BIT);

		// gl.projection(Matrix4d::ortho(0, 1, 1, 0, -1, 1));
		// gl.modelView(Matrix4d::identity());
		// fbotex.quad(gl, 0.8, 0.8, 0.1, 0.1);
		// static const GLfloat red[] = {1.0, 0.0, 0.0, 1.0};
		// glClearBufferfv(GL_COLOR, 0, red);
		// glClearColor(1.0, 0.0, 0.0, 1.0);
		// glClear(GL_COLOR_BUFFER_BIT);

		return true;
	}
};

MyWindow win;

int main(){

	win.append(*new StandardWindowKeyControls);
	win.create(Window::Dim(800, 600));

	MainLoop::start();
	return 0;
}
