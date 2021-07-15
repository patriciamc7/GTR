#pragma once
#include "prefab.h"
#include "fbo.h"
#include "sphericalharmonics.h"
//forward declarations
class Camera;

namespace GTR {

	class Prefab;
	class Material;

	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{

	public:
		bool gbuffers;
		FBO* deferred_fbo;
		FBO* gBuffers_fbo;
		FBO* ssao_fbo;
		FBO* irr_fbo;
		struct sProbe {
			Vector3 pos; //where is located
			Vector3 local; //its ijk pos in the matrix
			int index; //its index in the linear array
			SphericalHarmonics sh; //coeffs
		};

		//add here your functions
		void renderEntitiesOfScene(Camera* camera, bool shadow = false, bool shadowmap = false, bool transparent = false);
		void renderDrawSphere(Vector3 positionLight, Vector3 colorLight);
		void renderDrawCone(Vector3 positionLight, Vector3 colorLight);

		//GBUFFERS
		void renderGbuffers(Camera* camera, bool shadow = false);
		void setUniformsGbuffers(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera, bool shadow = false, bool shadowmap = false, bool transparent = false);
		void ShowGbuffers();

		//DEFERRED
		void renderDeferred(Camera* camera, bool shadow = false, bool shadowmap = false);
		void defferedBlend(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera, bool shadow = false, bool shadowmap = false, bool transparent = false);
		void setUniformsDeffered(Camera* camera, FBO* deferred_fbo, bool shadow = false, bool shadowmap = false);

		//SSAO
		void renderSsao(Camera* camera);
		std::vector<Vector3> generateSpherePoints(int num, float radius, bool hemi);

		//IRADIANCE
		void setUniformsDeffered_irradiance(Camera* camera, FBO* deferred_fbo, bool shadow = false, bool shadowmap = false);

		struct sIrrHeader {
			Vector3 start;
			Vector3 end;
			Vector3 delta;
			Vector3 dims;
			int num_probes;
		};
		sIrrHeader header;


		//define how many probes you want per dimension
		Vector3 dim;
		Vector3 delta;
		Vector3 start_pos;
		Vector3 end_pos;

		std::vector<sProbe> probes;
		void renderProbes(Vector3 pos, float size, float* coeffs);
		void computeIradiance();
		void createTextureProbes();
		bool loadIrradiance();
		void saveIrradiance();


		//REFLEXIONS
		void renderSkybox();
		Texture* CubemapFromHDRE(const char* filename);
		void computeReflections();
		void renderReflectionsProbes(Vector3 pos, float size, Texture* cubemap);


		//struct to store reflection probes info
		struct sReflectionProbe {
			Vector3 pos;
			Texture* cubemap = NULL;
		};
		//container for the probes
		std::vector<sReflectionProbe*> reflection_probes;
		Vector3 dim_refl;
		Vector3 delta_refl;
		Vector3 start_pos_refl;
		Vector3 end_pos_refl;

		//volumetric 
		void renderVolumetric();


		//to render a whole prefab (with all its nodes)
		void renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera, bool shadow = false, bool shadowmap = false, bool transparent = false);
		//to render one node from the prefab and its children
		void renderNode(const Matrix44& model, GTR::Node* node, Camera* camera, bool shadow = false, bool shadowmap = false, bool transparent = false);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera, bool shadow = false, bool shadowmap = false, bool transparent = false);

	};

};