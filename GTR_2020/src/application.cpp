#include "application.h"
#include "utils.h"
#include "mesh.h"
#include "texture.h"

#include "fbo.h"
#include "shader.h"
#include "input.h"
#include "includes.h"
#include "prefab.h"
#include "gltf_loader.h"
#include "renderer.h"
#include "Scene.h"
#include <cmath>
#include <string>
#include <cstdio>

Application* Application::instance = nullptr;
Vector4 bg_color(0.5, 0.5, 0.5, 1.0);


GTR::Prefab* prefab = nullptr;
GTR::Renderer* renderer = nullptr;
FBO* fbo = nullptr;
Texture* texture = nullptr;

float cam_speed = 10;

const int SHADOWMAP_WIDTH = 1024;
const int SHADOWMAP_HEIGHT = 1024;


GTR::Scene* scene = nullptr;
GTR::PrefabEntity* prefab_car = nullptr;
GTR::PrefabEntity* prefab_floor = nullptr;
GTR::PrefabEntity* prefab_model = nullptr;
GTR::PrefabEntity* prefab_drone = nullptr;

GTR::Light* light_spot = nullptr;
GTR::Light* light_directional = nullptr;
GTR::Light* light_point = nullptr;

FBO* deffered = nullptr;
FBO* ssao = nullptr;

Application::Application(int window_width, int window_height, SDL_Window* window)
{
	this->window_width = window_width;
	this->window_height = window_height;
	this->window = window;
	instance = this;
	must_exit = false;
	render_debug = true;
	render_gui = true;

	render_wireframe = false;

	fps = 0;
	frame = 0;
	time = 0.0f;
	elapsed_time = 0.0f;
	mouse_locked = false;

	show_deferred = false;
	shadowmap = false;
	show_pbr = false;
	show_forward = true;
	show_gbuffers = false;
	shadow_front = false;
	shadow_AA = false;
	show_ssao = false;
	is_ssao = false;
	iradiance = false;
	show_probes = false;
	deffered = new FBO();
	deffered->create(window_width, window_height);
	ssao = new FBO();
	ssao->create(window_width, window_height);
	probe_texture = NULL;
	show_irr_buffer = false;
	make_irr = false;
	show_reflections = false;
	make_reflections = false;
	volumetric = false;
	//loads and compiles several shaders from one single file
    //change to "data/shader_atlas_osx.txt" if you are in XCODE
	if(!Shader::LoadAtlas("data/shader_atlas.txt"))
        exit(1);
    checkGLErrors();

	// Create camera
	camera = new Camera();
	//camera->lookAt(Vector3(-150.f, 150.0f, 250.f), Vector3(0.f, 0.0f, 0.f), Vector3(0.f, 1.f, 0.f));
	camera->lookAt(Vector3(5250.f, 1930.f, 13722.f), Vector3(2550.f, 1000.0f,-3100.f), Vector3(0.f, 1.f, 0.f));
	camera->setPerspective( 45.f, window_width/(float)window_height, 1.0f, 100000.f);

	//This class will be the one in charge of rendering all 
	renderer = new GTR::Renderer(); //here so we have opengl ready in constructor

	//Lets load some object to render
	//prefab = GTR::Prefab::Get("data/prefabs/gmc/scene.gltf");

	//hide the cursor
	SDL_ShowCursor(!mouse_locked); //hide or show the mouse

	scene = GTR::Scene::getInstance();

	//floor
	prefab_floor = new GTR::PrefabEntity();
	prefab_floor->prefab = new GTR::Prefab();
	prefab_floor->id = 0;

	//inicializar instancia
	prefab_car = new GTR::PrefabEntity();
	prefab_model = new GTR::PrefabEntity();
	prefab_car->id = 1;
	prefab_drone = new GTR::PrefabEntity();
	prefab_drone->id = 2;
	//FLOOR
	Mesh* mesh = new Mesh();
	mesh->createPlane(10000);
	GTR::Material* material = new GTR::Material();
	prefab_floor->prefab->root.mesh = mesh;

	prefab_floor->factor = 1;

#ifndef _DEBUG //NO CARGE TEXTURA DEL FLOOR
	material->color_texture = Texture::Get("data/textures/brown_mud_leaves_01_diff_1k.png");

#endif 
	material->name = "Floor";
	prefab_floor->prefab->root.material = material;
	//cargar
	prefab_car->prefab = GTR::Prefab::Get("data/prefabs/gmc/scene.gltf");
	prefab_car->model.setScale(0.4, 0.4, 0.4);
	prefab_model->prefab = GTR::Prefab::Get("data/prefabs/brutalism/scene.gltf");
	prefab_model->model.setTranslation(0.0, 0.0, 0.0);
	prefab_model->model.setScale(1000, 1000, 1000);
	prefab_drone->prefab = GTR::Prefab::Get("data/prefabs/drone/scene.gltf");
	
	prefab_drone->model.setScale(3,3, 3);
	prefab_drone->model.setTranslation2(-1000, 3000, 0.0);

	scene->entities.push_back(prefab_floor);
	//scene->entities.push_back(prefab_car);
	scene->entities.push_back(prefab_model);
	scene->entities.push_back(prefab_drone);


	//SPOT

	light_spot = new GTR::Light();
	light_spot->lightSets(vec3(1, 1, 0), vec3(-2270, 6000, 5000), 2, GTR::Light::eLightType::SPOT, 10000, this->window_width, this->window_height);

	light_spot->light_vector.set(0, -1, -1);
	light_spot->intensity = 3.0;
	light_spot->flag = 1;

	light_spot->cameraLight->setPerspective(2 * light_spot->spotCosineCutoff, 1, light_spot->cameraLight->near_plane, light_spot->cameraLight->far_plane);

	if (light_spot->flag)
	{
		light_spot->shadow_fbo = new FBO();
		light_spot->shadow_fbo->create(SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT); //tamaño del shadowmap
	}
	//___________________________________
	//DIRECTIONAL
	light_directional = new GTR::Light();
	light_directional->light_vector.set(-0.7, 0.6, 1);
	light_directional->lightSets(vec3(1, 1, 1), vec3(-300, 80, 0), 3, GTR::Light::eLightType::DIRECTIONAL, 100.0, this->window_width, this->window_height);

	light_directional->intensity = 0.5;
	light_directional->flag = 1;

	

	if (light_directional->flag)
	{
		light_directional->shadow_fbo = new FBO();
		light_directional->shadow_fbo->create(SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT); //tamaño del shadowmap
	}
	//________________________________
	light_point = new GTR::Light();
	light_point->lightSets(vec3(0, 1, 0), vec3(1688, 350, -16), 4, GTR::Light::eLightType::POINT, 2500, this->window_width, this->window_height);


	//scene->entities.push_back(light_spot);
	scene->entities.push_back(light_directional);
	//scene->entities.push_back(light_point);

	random_points = renderer->generateSpherePoints(64, 5.0, true);

	ssao_blur = new Texture();
	ssao_blur->create(window_width, window_height);
	


	reflections_fbo = new FBO();
	reflections_fbo->create(64, 64, 1, GL_RGBA, GL_FLOAT);

	volumetric_fbo = new FBO();
	volumetric_fbo->create(64, 64, 1, GL_RGBA, GL_FLOAT);


	environment = renderer->CubemapFromHDRE("data/textures/panorama.hdre");

	//probe_position.set(100, 50, 0.0);
	//sh.coeffs->set(1, 1, 1);
	renderer->computeIradiance();
	//renderer->computeReflections();
	
}

//what to do when the image has to be draw
void Application::render(void)
{
	//be sure no errors present in opengl before start
	checkGLErrors();

	//set the clear color (the background color)
	glClearColor(bg_color.x, bg_color.y, bg_color.z, bg_color.w );

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    checkGLErrors();
   
	//set the camera as default (used by some functions in the framework)
	camera->enable();

	//set default flags
	glDisable(GL_BLEND);
    
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	if(render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	//lets render something
	if (show_forward) {
		renderer->renderSkybox();
		shadow_forward(camera);
		glDisable(GL_CULL_FACE);

	}

	if (show_deferred) { 	//Deffered
		show_forward = false;
		glDisable(GL_DEPTH_TEST);
		renderer->renderGbuffers(camera, true);
		if (is_ssao) {
			renderer->renderSsao(camera);
		}

		deffered->bind();
		renderer->renderDeferred(camera, true, true);
		renderer->gBuffers_fbo->depth_texture->copyTo(deffered->depth_texture);
		shadow_forward(camera,true,false,true);
		deffered->unbind();
		
		glViewport(0, 0, window_width, window_height);
		deffered->color_textures[0]->toViewport();
		
		if (volumetric) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			volumetric_fbo->color_textures[0]->toViewport();
		}
		
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CCW);

		if (show_probes) {
			camera->enable();
			for (int i = 0; i < renderer->probes.size(); ++i)
				renderer->renderProbes(renderer->probes[i].pos, 50, (float*)&renderer->probes[i].sh);
		}

		if (show_reflections) {
			camera->enable();
			for (int i = 0; i < renderer->probes.size(); ++i)
				renderer->renderReflectionsProbes(renderer->reflection_probes[i]->pos, 50, renderer->reflection_probes[i]->cubemap);
		}

		if (show_irr_buffer) {
			probe_texture->toViewport();
		}

		if (show_gbuffers) {
			renderer->ShowGbuffers();
		}
		if (show_ssao) {
			// SSAO
			glViewport(0, 0, window_width, window_height);
			ssao_blur->toViewport();
		}

	}

	//Draw the floor grid, helpful to have a reference point
	/*if(render_debug)
		drawGrid();*/

    glDisable(GL_DEPTH_TEST);
    //render anything in the gui after this

	//the swap buffers is done in the main loop after this function
}

void Application::shadow_forward(Camera* camera, bool shadow, bool shadowmap, bool transparent) {
	renderer->renderEntitiesOfScene(camera, shadow, shadowmap, transparent); //forward

	for (int i = 0; i < scene->entities.size(); i++) {
		if (scene->entities[i]->type == GTR::Light::LIGHT) {
			GTR::Light* light_entities = (GTR::Light*)scene->entities[i];

			if (light_entities->flag == 1) {
				light_entities->shadow_fbo->bind();
				glColorMask(false, false, false, false);
				glClear(GL_DEPTH_BUFFER_BIT);
				glDisable(GL_BLEND);
				glEnable(GL_DEPTH_TEST);
				glEnable(GL_CULL_FACE);
				if (render_wireframe)
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				else
					glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

				light_entities->cameraLight->enable();
				renderer->renderEntitiesOfScene(light_entities->cameraLight, true, false, false);

				light_entities->shadow_fbo->unbind();
				glColorMask(true, true, true, true);

				glDisable(GL_CULL_FACE);
				glDisable(GL_DEPTH_TEST);
			}

		}

	}
	int num_light = 0;

	for (int i = 0; i < scene->entities.size(); i++) {
		if (scene->entities[i]->type == GTR::Light::LIGHT) { //RECORREMOS SOLO LAS LUCES
			GTR::Light* light_entities = (GTR::Light*)scene->entities[i];
			if (light_entities->flag == 1) {
				glDisable(GL_BLEND);
				glEnable(GL_DEPTH_TEST);

				Shader* shader_depth = Shader::Get("depth");
				shader_depth->enable();

				shader_depth->setUniform("u_camera_nearfar", Vector2(light_entities->cameraLight->near_plane, light_entities->cameraLight->far_plane));
				light_entities->InicialLightCamera(window_width, window_height);

				if (show_forward) {
					glViewport(num_light * 300, 0, 300, 300);
					if (light_entities->light_type == GTR::Light::eLightType::DIRECTIONAL) {
						light_entities->shadow_fbo->depth_texture->toViewport(); //ortografica 

					}
					else {
						light_entities->shadow_fbo->depth_texture->toViewport(shader_depth);

					}
				}
				glViewport(0, 0, window_width, window_height);
				shader_depth->disable();

				num_light = num_light + 1;

			}
		}
	}
}

void Application::update(double seconds_elapsed)
{
	float speed = seconds_elapsed * cam_speed; //the speed is defined by the seconds_elapsed so it goes constant
	float orbit_speed = seconds_elapsed * 0.5;
	
	//async input to move the camera around
	if (Input::isKeyPressed(SDL_SCANCODE_LSHIFT)) speed *= 10; //move faster with left shift
	if (Input::isKeyPressed(SDL_SCANCODE_W) || Input::isKeyPressed(SDL_SCANCODE_UP)) camera->move(Vector3(0.0f, 0.0f, 1.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_S) || Input::isKeyPressed(SDL_SCANCODE_DOWN)) camera->move(Vector3(0.0f, 0.0f,-1.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_A) || Input::isKeyPressed(SDL_SCANCODE_LEFT)) camera->move(Vector3(1.0f, 0.0f, 0.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_D) || Input::isKeyPressed(SDL_SCANCODE_RIGHT)) camera->move(Vector3(-1.0f, 0.0f, 0.0f) * speed);

	//mouse input to rotate the cam
	#ifndef SKIP_IMGUI
	if (!ImGuizmo::IsUsing())
	#endif
	{
		if (mouse_locked || Input::mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT)) //move in first person view
		{
			camera->rotate(-Input::mouse_delta.x * orbit_speed * 0.5, Vector3(0, 1, 0));
			Vector3 right = camera->getLocalVector(Vector3(1, 0, 0));
			camera->rotate(-Input::mouse_delta.y * orbit_speed * 0.5, right);
		}
		else //orbit around center
		{
			bool mouse_blocked = false;
			#ifndef SKIP_IMGUI
						mouse_blocked = ImGui::IsAnyWindowHovered() || ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive();
			#endif
			if (Input::mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT) && !mouse_blocked) //is left button pressed?
			{
				camera->orbit(-Input::mouse_delta.x * orbit_speed, Input::mouse_delta.y * orbit_speed);
			}
		}
	}
	
	//move up or down the camera using Q and E
	if (Input::isKeyPressed(SDL_SCANCODE_Q)) camera->moveGlobal(Vector3(0.0f, -1.0f, 0.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_E)) camera->moveGlobal(Vector3(0.0f, 1.0f, 0.0f) * speed);

	//to navigate with the mouse fixed in the middle
	SDL_ShowCursor(!mouse_locked);
	#ifndef SKIP_IMGUI
		ImGui::SetMouseCursor(mouse_locked ? ImGuiMouseCursor_None : ImGuiMouseCursor_Arrow);
	#endif
	if (mouse_locked)
	{
		Input::centerMouse();
		//ImGui::SetCursorPos(ImVec2(Input::mouse_position.x, Input::mouse_position.y));
	}
}

void Application::renderDebugGizmo()
{
	if (!prefab)
		return;

	//example of matrix we want to edit, change this to the matrix of your entity
	Matrix44& matrix = prefab->root.model;

	#ifndef SKIP_IMGUI

	static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
	static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
	if (ImGui::IsKeyPressed(90))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	if (ImGui::IsKeyPressed(69))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	if (ImGui::IsKeyPressed(82)) // r Key
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	float matrixTranslation[3], matrixRotation[3], matrixScale[3];
	ImGuizmo::DecomposeMatrixToComponents(matrix.m, matrixTranslation, matrixRotation, matrixScale);
	ImGui::InputFloat3("Tr", matrixTranslation, 3);
	ImGui::InputFloat3("Rt", matrixRotation, 3);
	ImGui::InputFloat3("Sc", matrixScale, 3);
	ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix.m);

	if (mCurrentGizmoOperation != ImGuizmo::SCALE)
	{
		if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
			mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
			mCurrentGizmoMode = ImGuizmo::WORLD;
	}
	static bool useSnap(false);
	if (ImGui::IsKeyPressed(83))
		useSnap = !useSnap;
	ImGui::Checkbox("", &useSnap);
	ImGui::SameLine();
	static Vector3 snap;
	switch (mCurrentGizmoOperation)
	{
	case ImGuizmo::TRANSLATE:
		//snap = config.mSnapTranslation;
		ImGui::InputFloat3("Snap", &snap.x);
		break;
	case ImGuizmo::ROTATE:
		//snap = config.mSnapRotation;
		ImGui::InputFloat("Angle Snap", &snap.x);
		break;
	case ImGuizmo::SCALE:
		//snap = config.mSnapScale;
		ImGui::InputFloat("Scale Snap", &snap.x);
		break;
	}
	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
	ImGuizmo::Manipulate(camera->view_matrix.m, camera->projection_matrix.m, mCurrentGizmoOperation, mCurrentGizmoMode, matrix.m, NULL, useSnap ? &snap.x : NULL);
	#endif
}


//called to render the GUI from
void Application::renderDebugGUI(void)
{
#ifndef SKIP_IMGUI //to block this code from compiling if we want

	ImGui::Text(getGPUStats().c_str());					   // Display some text (you can use a format strings too)

	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::ColorEdit4("BG color", bg_color.v);
	ImGui::Checkbox("GBuffers", &show_gbuffers); //4 pantallas
	ImGui::Checkbox("PBR", &show_pbr); //metalico
	ImGui::Checkbox("Forward", &show_forward); //renderizar scene con multi
	ImGui::Checkbox("Deferred", &show_deferred); //renderizar scene con gbuffers
	ImGui::Checkbox("Shadow Front", &shadow_front); //renderizar scene con gbuffers
	ImGui::Checkbox("Shadow Anti aliasing", &shadow_AA); //renderizar scene con gbuffers
	ImGui::Checkbox("Show SSAO", &show_ssao); //renderizar por ssao
	ImGui::Checkbox("SSAO", &is_ssao);
	ImGui::Checkbox("Show probes", &show_probes);
	ImGui::Checkbox("Irradiance", &iradiance);
	ImGui::Checkbox("Show Iradiance buffer", &show_irr_buffer);
	ImGui::Checkbox("Show reflection probes", &show_reflections);
	ImGui::Checkbox("Volumetric directional", &volumetric);

	//add info to the debug panel about the camera
	if (ImGui::TreeNode(camera, "Camera")) {
		camera->renderInMenu();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode(renderer->probes[0].sh.coeffs, "Sh")) {
#ifndef SKIP_IMGUI
		bool changed = false;
		changed |= ImGui::SliderFloat3("Coeff 0", &renderer->probes[0].sh.coeffs[0].x, -2, 2);
		changed |= ImGui::SliderFloat3("Coeff 1", &renderer->probes[0].sh.coeffs[1].x, -2, 2);
		changed |= ImGui::SliderFloat3("Coeff 2", &renderer->probes[0].sh.coeffs[2].x, -2, 2);
		changed |= ImGui::SliderFloat3("Coeff 3", &renderer->probes[0].sh.coeffs[3].x, -2, 2);
		changed |= ImGui::SliderFloat3("Coeff 4", &renderer->probes[0].sh.coeffs[4].x, -2, 2);
		changed |= ImGui::SliderFloat3("Coeff 5", &renderer->probes[0].sh.coeffs[5].x, -2, 2);
		changed |= ImGui::SliderFloat3("Coeff 6", &renderer->probes[0].sh.coeffs[6].x, -2, 2);
		changed |= ImGui::SliderFloat3("Coeff 7", &renderer->probes[0].sh.coeffs[7].x, -2, 2);
		changed |= ImGui::SliderFloat3("Coeff 8", &renderer->probes[0].sh.coeffs[8].x, -2, 2);

#endif
		ImGui::TreePop();
	}
	//IMGUI LUCES
	if (ImGui::TreeNode(light_spot, "Light spot")) {
		light_spot->renderInMenu();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode(light_point, "Light point")) {
		light_point->renderInMenu();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode(light_directional, "Light directional")) {
		light_directional->renderInMenu();
		ImGui::TreePop();
	}
	//example to show prefab info: first param must be unique!
	if (prefab && ImGui::TreeNode(prefab, "Prefab")) {
		prefab->root.renderInMenu();
		ImGui::TreePop();
	}

#endif
}

//Keyboard event handler (sync input)
void Application::onKeyDown( SDL_KeyboardEvent event )
{
	switch(event.keysym.sym)
	{
		case SDLK_ESCAPE: must_exit = true; break; //ESC key, kill the app
		case SDLK_F1: render_debug = !render_debug; break;
		case SDLK_f: camera->center.set(0, 0, 0); camera->updateViewMatrix(); break;
		case SDLK_F5: Shader::ReloadAll(); break;
	}
}

void Application::onKeyUp(SDL_KeyboardEvent event)
{
}

void Application::onGamepadButtonDown(SDL_JoyButtonEvent event)
{

}

void Application::onGamepadButtonUp(SDL_JoyButtonEvent event)
{

}

void Application::onMouseButtonDown( SDL_MouseButtonEvent event )
{
	if (event.button == SDL_BUTTON_MIDDLE) //middle mouse
	{
		//Input::centerMouse();
		mouse_locked = !mouse_locked;
		SDL_ShowCursor(!mouse_locked);
	}
}

void Application::onMouseButtonUp(SDL_MouseButtonEvent event)
{
}

void Application::onMouseWheel(SDL_MouseWheelEvent event)
{
	bool mouse_blocked = false;

	#ifndef SKIP_IMGUI
		ImGuiIO& io = ImGui::GetIO();
		if(!mouse_locked)
		switch (event.type)
		{
			case SDL_MOUSEWHEEL:
			{
				if (event.x > 0) io.MouseWheelH += 1;
				if (event.x < 0) io.MouseWheelH -= 1;
				if (event.y > 0) io.MouseWheel += 1;
				if (event.y < 0) io.MouseWheel -= 1;
			}
		}
		mouse_blocked = ImGui::IsAnyWindowHovered();
	#endif

	if (!mouse_blocked && event.y)
	{
		if (mouse_locked)
			cam_speed *= 1 + (event.y * 0.1);
		else
			camera->changeDistance(event.y * 0.5);
	}
}

void Application::onResize(int width, int height)
{
    std::cout << "window resized: " << width << "," << height << std::endl;
	glViewport( 0,0, width, height );
	camera->aspect =  width / (float)height;
	window_width = width;
	window_height = height;
}

