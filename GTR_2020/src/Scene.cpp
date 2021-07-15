#include "Scene.h"
#include "utils.h"
#include "camera.h"


using namespace GTR;

Scene* Scene::scene = NULL;

//Singelton Scene
Scene* Scene::getInstance()
{
	if (scene == NULL)
	{
		scene = new Scene();
	}

	return scene;
}

//constructor Scene
Scene::Scene(){

	ambient.set(0.2, 0.2, 0.2);
	gamma = 2.2;
}

//destructor Scene
GTR::Scene::~Scene()
{
	delete[] &ambient;
	entities.clear();
	
}




GTR::BaseEntity::~BaseEntity(){//Destructor
	delete[] &id;
	delete[] &model;
	delete[] &visible;
	delete[] &type;
}

//constructor de Light
Light::Light() {
	color.set(1, 0, 0);
	intensity = 1.0f;
	spotCosineCutoff = 45; 
	light_position.set(0, 5, 200);
	light_vector= model.frontVector(); //Direccion
	spotExponent=10;   
	max_distance= 1000.0f;
	light_type = POINT;
	type = LIGHT;
	cameraLight = new Camera();
	bias = 0.001;
}

//desctructor Light

GTR::Light::~Light()
{
	delete[] &color;
	delete[] &intensity;
	delete[] &spotCosineCutoff;
	delete[] &light_vector;
	delete[] &spotExponent;
	delete[] &max_distance;
	delete[] &light_type;
	delete[] &type;
	delete[] cameraLight;
	delete[] &bias;

}




//set de los atributos de light
void GTR::Light::lightSets( Vector3 color, Vector3 position, int id, eLightType LightType, float max_distance, int window_width, int window_height)
{
	this->color.set(color.x, color.y, color.z);
	this->model.setTranslation(position.x, position.y, position.z);
	this->light_position = this->model.getTranslation();
	this->id = id;
	this->light_type = LightType;/*
	this->light_vector.set(0, 1, -1);*/
	this->flag = 0; 
	this->bias = 0.001; 
	this->max_distance = 10000; 
	InicialLightCamera( window_width,  window_height); 

}

//Inicializar los valores de la camara de la luz para hacer el shadowmap
void GTR::Light::InicialLightCamera(int window_width, int window_height) {
	this->cameraLight = new Camera();
	
	switch (this->light_type) {
	case 0: //POINT
		break; 
	case 1: //SPOT
		this->cameraLight->lookAt(this->light_position, this->light_position + this->light_vector, Vector3(0, 1, 0));
		this->cameraLight->setPerspective(45.f, 1, 1.0f, this->max_distance);		
		break;
	case 2: //DIRECTIONAL
		
		vec3 cam_position = Vector3(10.2,10,-100);
		this->cameraLight->lookAt(this->light_vector+cam_position, cam_position, Vector3(0.f, 1.f, 0.f));
		this->cameraLight->setOrthographic(-10000, 10000, 0, 10000, 0.1f, 10000);
		break;
	}
}

//Atributos que pasaos a la interfaz de Imgui
void GTR::Light::renderInMenu()
{
#ifndef SKIP_IMGUI

	ImGui::Combo("Light Type", (int*)&light_type, "POINT\0SPOT\0DIRECTIONAL", 3);
	ImGui::ColorEdit4("Color", color.v); // Edit 4 floats representing a color + alpha
	ImGui::SliderFloat("Intensity", (float*)&intensity, 0, 50);


	switch (light_type)
	{

	case 0: //POINT
		ImGui::SliderFloat("Max Distance", (float*)&max_distance, 0, 10000);
		ImGui::SliderFloat3("Light position", &light_position.x, -10000, +10000);
		break;
	case 1: //SPOT
		ImGui::SliderFloat3("Light position", &light_position.x, -10000, +10000);
		ImGui::SliderFloat3("Light Direction", &light_vector.x, -1, +1);
		ImGui::SliderFloat("Cutoff", (float*)&spotCosineCutoff, 0, 180);
		ImGui::SliderFloat("spotExponent", (float*)&spotExponent, 0, 180);
		ImGui::SliderFloat("FOV", &this->cameraLight->fov, 15, 180);
		ImGui::SliderFloat("bias", &this->bias, 0.000, 0.002);
		ImGui::SliderFloat("Far", (float*)&max_distance, 0, 10000);
		

		break;
	case 2: //DIRECTIONAL
		ImGui::SliderFloat3("Light Direction", &light_vector.x, -1, +1);
		ImGui::SliderFloat3("Light eye", &this->cameraLight->eye.x, -1000, +1000);
		ImGui::SliderFloat("Bias", &this->bias, 0.000, 0.01);


		break;
	}
#endif

}


PrefabEntity::PrefabEntity() :BaseEntity() { //incicializamos los atributos
	prefab = NULL;
	type = PREFAB;
	this->factor = 1.0;
}
GTR::PrefabEntity::~PrefabEntity()//destructor 
{
	delete[] &factor;
	delete[] prefab;
	
}
;

GTR::BaseEntity::BaseEntity() //constructor
{
	id = 0;
	visible = true;
	type = BASE_NODE;

}
