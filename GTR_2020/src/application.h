/*  by Javi Agenjo 2013 UPF  javi.agenjo@gmail.com
	This class encapsulates the game, is in charge of creating the game, getting the user input, process the update and render.
*/

#ifndef APPLICATION_H
#define APPLICATION_H

#include "includes.h"
#include "camera.h"
#include "utils.h"
#include "texture.h"
#include "sphericalharmonics.h"

class Application
{
public:
	static Application* instance;

	Camera* camera = nullptr;
	//window
	SDL_Window* window;
	int window_width;
	int window_height;

	//some globals
	long frame;
    float time;
	float elapsed_time;
	int fps;
	bool must_exit;
	bool render_debug;
	bool render_gui;

	//some vars
	bool mouse_locked; //tells if the mouse is locked (blocked in the center and not visible)
	bool render_wireframe; //in case we want to render everything in wireframe mode

	//deferred
	bool show_deferred;
	bool show_forward;
	bool show_gbuffers;

	//shadows
	bool shadowmap;
	bool shadow_front;
	bool shadow_AA;
	
	//pbr
	bool show_pbr;

	//ssao
	std::vector<Vector3> random_points;
	Texture* ssao_blur;
	bool show_ssao;
	bool is_ssao;

	//iradiance
	bool iradiance;
	bool show_probes;
	bool show_irr_buffer;
	bool make_irr;
	Texture* probe_texture;
	Vector3 probe_position;
	SphericalHarmonics sh;

	Texture* environment;
	bool show_reflections;
	bool make_reflections;
	FBO* reflections_fbo;
	
	bool volumetric;
	FBO* volumetric_fbo;
	Application( int window_width, int window_height, SDL_Window* window );

	//main functions
	void render( void );
	void update( double dt );

	void renderDebugGUI(void);
	void renderDebugGizmo();

	//events
	void onKeyDown( SDL_KeyboardEvent event );
	void onKeyUp(SDL_KeyboardEvent event);
	void onMouseButtonDown( SDL_MouseButtonEvent event );
	void onMouseButtonUp(SDL_MouseButtonEvent event);
	void onMouseWheel(SDL_MouseWheelEvent event);
	void onGamepadButtonDown(SDL_JoyButtonEvent event);
	void onGamepadButtonUp(SDL_JoyButtonEvent event);
	void onResize(int width, int height);

	void shadow_forward(Camera* camera, bool shadow = false, bool shadowmap = false, bool transparent = false);

};


#endif 