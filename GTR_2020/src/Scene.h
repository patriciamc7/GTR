#pragma once
#include "includes.h"
#include "framework.h"
#include "camera.h"
#include "fbo.h"

namespace GTR {

	class Prefab;
	

	class BaseEntity
	{
	public:
		unsigned int id;
		Matrix44 model;
		bool visible;
		//int quantity;
		enum eBasetype {
			BASE_NODE,
			PREFAB,
			LIGHT
		};
		eBasetype type;
		BaseEntity( ); //Constructor
		virtual ~BaseEntity(); //Destructor
	};

	
	
	class PrefabEntity :public BaseEntity
	{
	public:
		PrefabEntity(); //constructor
		~PrefabEntity();
		Prefab* prefab; //instancia de prefab
		int factor ; 
	};


	
	class Light :public BaseEntity
	{
	public:
		Vector3 color;
		float intensity;
		enum eLightType {
			POINT,
			SPOT,
			DIRECTIONAL
		};

		eLightType light_type;
		Vector3 light_position; 
		Vector3 light_vector;
		float spotCosineCutoff;
		float max_distance;
		float spotExponent;
		int flag; 

		FBO* shadow_fbo; 

		//Shadowmap
		Camera* cameraLight;
		float bias; 
		//function 
		Light();
		~Light();
		void renderInMenu();
		void GTR::Light::lightSets(Vector3 color, Vector3 position, int id, eLightType LightType, float max_distance, int window_width, int window_height);
		void GTR::Light::InicialLightCamera(int window_width, int window_height);
	};

	class Scene
	{
	private:

	public:
		std::vector<BaseEntity*> entities;
		static Scene* getInstance(); //Constructor
		static Scene* scene; //instància
		Vector3 ambient;
		float gamma;
		Scene();
		~Scene();
		
	};

}