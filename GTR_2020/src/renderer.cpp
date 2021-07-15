#include "renderer.h"

#include "camera.h"
#include "shader.h"
#include "mesh.h"
#include "texture.h"
#include "prefab.h"
#include "material.h"
#include "utils.h"
#include "application.h"
#include "Scene.h"
#include "extra/hdre.h"

using namespace GTR;

void GTR::Renderer::renderVolumetric() {

	Application::instance->volumetric_fbo->bind();
	Shader* shader = NULL;
	Mesh* quad = Mesh::getQuad(); //buffer cuadrado
	GTR::Scene* scene = GTR::Scene::scene;
	Camera* camera = Camera::current;

	for (int i = 0; i < scene->entities.size(); i++) {
		GTR::Light* light_entities = (GTR::Light*)scene->entities[i];
		if (light_entities->type == Light::eBasetype::LIGHT) {
			if (light_entities->light_type == Light::eLightType::DIRECTIONAL) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				shader = Shader::Get("volumetricDirectional");
				shader->enable();

				shader->setUniform("u_depth_texture", gBuffers_fbo->depth_texture, 10);
				Matrix44 inv_vp = camera->viewprojection_matrix;
				inv_vp.inverse();
				shader->setUniform("u_inverse_viewprojection", inv_vp);
				shader->setUniform("u__viewprojection", camera->viewprojection_matrix);
				shader->setUniform3("u_camera_position", camera->eye);
				shader->setUniform("u_iRes", Vector2(1.0 / (float)deferred_fbo->width, 1.0 / (float)deferred_fbo->height));
				shader->setUniform("u_light_color", light_entities->color);
				shader->setUniform("u_light_vector", light_entities->light_vector);
				shader->setUniform("u_shadow_viewproj", light_entities->cameraLight->viewprojection_matrix);
				shader->setUniform("u_bias", light_entities->bias);
				shader->setUniform("shadowmap", light_entities->shadow_fbo->depth_texture, 11);
				shader->setUniform("u_shadowmap_width", (float)light_entities->shadow_fbo->depth_texture->width);
				shader->setUniform("u_shadowmap_height", (float)light_entities->shadow_fbo->depth_texture->height);
				shader->setTexture("u_shadowmap_AA", light_entities->shadow_fbo->depth_texture, 12);
				quad->render(GL_TRIANGLES);
				shader->disable();
				glDisable(GL_BLEND);
			}
		}
	}

	Application::instance->volumetric_fbo->unbind();
}

void GTR::Renderer::computeReflections() {
	Application* application = Application::instance;

	application->make_reflections = true;
	reflection_probes.clear();
	start_pos_refl = Vector3(-1000, 10, 1000);
	end_pos_refl = Vector3(2400, 2000, -3000);

	//define how many probes you want per dimension
	dim_refl = Vector3(1, 1,1);

	delta_refl = (end_pos_refl - start_pos_refl);
	delta_refl.x /= (dim_refl.x - 1);
	delta_refl.y /= (dim_refl.y - 1);
	delta_refl.z /= (dim_refl.z - 1);

	for (int x = 0; x < dim_refl.x; ++x) {
		for (int y = 0; y < dim_refl.y; ++y) {
			for (int z = 0; z < dim_refl.z; ++z)
			{
				// create the probe
				sReflectionProbe* probe = new sReflectionProbe();

				//set it up
				probe->pos = start_pos_refl + delta_refl * Vector3(x, y, z);
				probe->cubemap = new Texture();
				probe->cubemap->createCubemap(
					512, 512,
					NULL,
					GL_RGB, GL_UNSIGNED_INT, false);

				//add it to the list
				reflection_probes.push_back(probe);
				probe->cubemap->bind();
				glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			}
		}
	}
	/*if (!reflections_fbo) {
		reflections_fbo = new FBO();
		reflections_fbo->create(64, 64, 1, GL_RGBA, GL_FLOAT);
	}*/
	Camera cam;
	cam.setPerspective(90, 1, 0.1, 1000);

	for (int iP = 0; iP < probes.size(); ++iP) {
		int probe_index = iP;
		for (int i = 0; i < 6; ++i)
		{
			//assign cubemap face to FBO
			application->reflections_fbo->setTexture(reflection_probes[probe_index]->cubemap, i);

			//render view
			Vector3 eye = reflection_probes[probe_index]->pos;
			Vector3 center = reflection_probes[probe_index]->pos + cubemapFaceNormals[i][2];
			Vector3 up = cubemapFaceNormals[i][1];
			cam.lookAt(eye, center, up);
			cam.enable();
			//bind FBO
			application->reflections_fbo->bind();
			application->shadow_forward(&cam);
			application->reflections_fbo->unbind();
		}

		//generate the mipmaps
		reflection_probes[probe_index]->cubemap->generateMipmaps();
	}
	Application::instance->make_reflections = false;
}

void GTR::Renderer::renderReflectionsProbes(Vector3 pos, float size, Texture* cubemap) {
	Camera* camera = Camera::current;

	Shader* shader = Shader::Get("ReflectionProbes");
	Mesh* mesh = Mesh::Get("data/meshes/sphere.obj");

	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	Matrix44 model;
	model.setTranslation(pos.x, pos.y, pos.z);
	model.scale(size, size, size);

	shader->enable();
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);
	shader->setTexture("u_texture", cubemap, 0);
	mesh->render(GL_TRIANGLES);
	shader->disable();


}

Texture* GTR::Renderer::CubemapFromHDRE(const char* filename)
{
	HDRE* hdre = new HDRE();
	if (!hdre->load(filename))
	{
		delete hdre;
		return NULL;
	}

	Texture* texture = new Texture();
	texture->createCubemap(hdre->width, hdre->height, (Uint8**)hdre->getFaces(0), hdre->header.numChannels == 3 ? GL_RGB : GL_RGBA, GL_FLOAT);
	for (int i = 1; i < 6; ++i)
		texture->uploadCubemap(texture->format, texture->type, false, (Uint8**)hdre->getFaces(i), GL_RGBA32F, i);
	return texture;
}

void GTR::Renderer::renderSkybox() {
	if (!Application::instance->environment)
		return;
	Mesh* mesh = Mesh::Get("data/meshes/sphere.obj");
	Camera* camera = Camera::current;
	Shader* shader = Shader::Get("skybox");
	shader->enable();
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	Matrix44 model;
	model.setTranslation(camera->eye.x, camera->eye.y, camera->eye.z);
	model.scale(10, 10, 10);
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);
	shader->setUniform("u_texture", Application::instance->environment, 0);
	mesh->render(GL_TRIANGLES);

	shader->disable();
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}

void GTR::Renderer::computeIradiance() {

	//memset(&Application::instance->sh, 0, sizeof(Application::instance->sh));
	/*if (loadIrradiance())
		return;*/

	Application::instance->make_irr = true;
	probes.clear();

	start_pos = Vector3(-1000, 10, 1000);
	end_pos = Vector3(2400, 2000, -3000);

	//define how many probes you want per dimension
	dim = Vector3(5, 5, 5);

	delta = (end_pos - start_pos);
	delta.x /= (dim.x - 1);
	delta.y /= (dim.y - 1);
	delta.z /= (dim.z - 1);

	for (int x = 0; x < dim.x; ++x) {
		for (int y = 0; y < dim.y; ++y) {
			for (int z = 0; z < dim.z; ++z) 
			{
				sProbe p;
				p.local.set(x, y, z);

				//index in the linear array
				p.index = x + y * dim.x + z * dim.x * dim.y;

				//and its position
				p.pos = start_pos + delta * Vector3(x, y, z);
				probes.push_back(p);
			}
		}
	}

	if (!irr_fbo) {
		irr_fbo = new FBO();
		irr_fbo->create(64, 64, 1, GL_RGBA, GL_FLOAT);
	}

	for (int iP = 0; iP < probes.size(); ++iP) {
		Camera cam;
		cam.setPerspective(90, 1, 0.1, 1000);
		int probe_index = iP;
		FloatImage images[6];
		for (int i = 0; i < 6; ++i) {

			//compute camera orientation using defined vectors
			Vector3 eye = probes[probe_index].pos;
			Vector3 front = cubemapFaceNormals[i][2];
			Vector3 center = probes[probe_index].pos + front;
			Vector3 up = cubemapFaceNormals[i][1];

			cam.lookAt(eye, center, up);
			cam.enable();

			//render the scene from this point of view
			irr_fbo->bind();
			renderEntitiesOfScene(&cam);
			//Application::instance->shadow_forward(&cam);
			irr_fbo->unbind();

			images[i].fromTexture(irr_fbo->color_textures[0]);
			/*glViewport(100*i, 0,100,100);
			irr_fbo->color_textures[0]->toViewport();*/

		}
		probes[probe_index].sh = computeSH(images);
	}
	Application::instance->make_irr = false;
	glDisable(GL_DEPTH_TEST);
	Application::instance->camera->enable();
	createTextureProbes();
	saveIrradiance();
}

void GTR::Renderer::createTextureProbes() {
	if (Application::instance->probe_texture)
		delete Application::instance->probe_texture;

	//create the texture to store the probes (do this ONCE!!!)

	Application::instance->probe_texture = new Texture(9, //9 coefficients per probe
		probes.size(), //as many rows as probes
		GL_RGB, //3 channels per coefficient
		GL_FLOAT); //they require a high range
		//we must create the color information for the texture. because every SH are 27 floats in the RGB,RGB,... order, we can create an array of SphericalHarmonics and use it as pixels of the texture
	SphericalHarmonics* sh_data = NULL;
	sh_data = new SphericalHarmonics[dim.x * dim.y * dim.z];

	//here we fill the data of the array with our probes in x,y,z order...
	for (int i = 0; i < probes.size(); ++i)
	{
		//int index = floor(probes[i].local.x + probes[i].local.y * dim.x + probes[i].local.z * (dim.x * dim.y));
		sh_data[i] = probes[i].sh ;
	}
	//now upload the data to the GPU
	Application::instance->probe_texture->upload(GL_RGB, GL_FLOAT, false, (uint8*)sh_data);

	//disable any texture filtering when reading
	Application::instance->probe_texture->bind();
	glClear(GL_COLOR_BUFFER_BIT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	//always free memory after allocating it!!!
	delete[] sh_data;
}

bool GTR::Renderer::loadIrradiance() {

	//load probes info from disk
	FILE* f = fopen("irradiance.bin", "rb");
	if (!f)
		return false;

	//read header
	sIrrHeader header;
	fread(&header, sizeof(header), 1, f);

	//copy info from header to our local vars
	start_pos = header.start;
	end_pos = header.end;
	dim = header.dims;
	delta = header.delta;
	int num_probes = header.num_probes;

	//allocate space for the probes
	probes.resize(num_probes);


	//read from disk directly to our probes container in memory
	fread(&probes[0], sizeof(sProbe), probes.size(), f);
	fclose(f);

	//build the texture again…
	createTextureProbes();
	return true;

}

void GTR::Renderer::saveIrradiance() {

	//saveIrradianceToDisk ---------------------------------
	//fill header structure
	//createTextureProbes();

	header.start = start_pos;
	header.end = end_pos;
	header.dims = dim;
	header.delta = delta;
	header.num_probes = dim.x * dim.y * dim.z;

	//write to file header and probes data
	FILE* f = fopen("irradiance.bin", "wb");
	fwrite(&header, sizeof(header), 1, f);
	fwrite(&(probes[0]), sizeof(sProbe), probes.size(), f);
	fclose(f);
}

void GTR::Renderer::renderProbes(Vector3 pos, float size, float* coeffs) {
	Camera* camera = Camera::current;
	Shader* shader = Shader::Get("probes");
	Mesh* mesh = Mesh::Get("data/meshes/sphere.obj");

	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	Matrix44 model;
	model.setTranslation(pos.x, pos.y, pos.z);
	model.scale(size, size, size);

	shader->enable();
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);
	shader->setUniform3Array("u_coeffs", coeffs, 9);
	shader->setTexture("u_depth_texture", gBuffers_fbo->depth_texture, 0);
	shader->setUniform("u_iRes", Vector2(1.0 / (float)gBuffers_fbo->depth_texture->width, 1.0 / (float)gBuffers_fbo->depth_texture->height));
	mesh->render(GL_TRIANGLES);
	shader->disable();


}

std::vector<Vector3> GTR::Renderer::generateSpherePoints(int num, float radius, bool hemi)
{
	std::vector<Vector3> points;
	points.resize(num);
	for (int i = 0; i < num; i += 3)
	{
		Vector3& p = points[i];
		float u = random();
		float v = random();
		float theta = u * 2.0 * PI;
		float phi = acos(2.0 * v - 1.0);
		float r = cbrt(random() * 0.9 + 0.1) * radius;
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);
		float sinPhi = sin(phi);
		float cosPhi = cos(phi);
		p.x = r * sinPhi * cosTheta;
		p.y = r * sinPhi * sinTheta;
		p.z = r * cosPhi;
		if (hemi && p.z < 0)
			p.z *= -1.0;
	}
	return points;
}

void GTR::Renderer::renderSsao(Camera* camera) {

	glDisable(GL_DEPTH_TEST);

	GTR::Scene* scene = GTR::Scene::scene;
	Mesh* quad = Mesh::getQuad();
	Matrix44 inv_vp = camera->viewprojection_matrix;
	inv_vp.inverse();

	if (!ssao_fbo) {
		ssao_fbo = new FBO();
		ssao_fbo->create(Application::instance->window_width, Application::instance->window_height);
	}

	Shader* shader = Shader::Get("ssao");
	shader->enable();
	//bind the texture we want to change
	gBuffers_fbo->bind();

	//disable using mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	//enable bilinear filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	gBuffers_fbo->unbind();

	gBuffers_fbo->color_textures[1]->bind();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	//enable bilinear filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	gBuffers_fbo->color_textures[1]->unbind();

	ssao_fbo->bind();

	shader->setUniform("u_normal_texture", gBuffers_fbo->color_textures[1], 0);
	shader->setUniform("u_depth_texture", gBuffers_fbo->depth_texture, 3);
	shader->setUniform("u_iRes", Vector2(1.0 / (float)gBuffers_fbo->depth_texture->width, 1.0 / (float)gBuffers_fbo->depth_texture->height));
	shader->setUniform("u_camera_nearfar", Vector2(camera->near_plane, camera->far_plane));
	shader->setUniform("u_inverse_viewprojection", inv_vp);
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform3("u_camera_position", camera->eye);
	//send random points so we can fetch around
	shader->setUniform3Array("u_points", (float*)&Application::instance->random_points[0], Application::instance->random_points.size());

	quad->render(GL_TRIANGLES);

	ssao_fbo->unbind();

	//ssao_fbo->color_textures[0]->toViewport();

	Shader* shader_blur = Shader::Get("ssao_blur");
	shader_blur->enable();
	shader_blur->setUniform("u_offset", Vector2(1.0 / (float)ssao_fbo->color_textures[0]->width, 1.0 / (float)ssao_fbo->color_textures[0]->height));
	shader_blur->setTexture("u_texture", ssao_fbo->color_textures[0], 0);

	ssao_fbo->color_textures[0]->copyTo(Application::instance->ssao_blur, shader_blur);


}

void GTR::Renderer::renderGbuffers(Camera* camera, bool shadow) {

	gbuffers = true;

	GTR::Scene* scene = GTR::Scene::scene;

	int w = Application::instance->window_width;
	int h = Application::instance->window_height;
	if (gBuffers_fbo == NULL) {
		gBuffers_fbo = new FBO();
		gBuffers_fbo->create(w, h, 4, GL_RGBA);
	}


	gBuffers_fbo->bind();
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	renderSkybox();
	renderEntitiesOfScene(camera, shadow); //render gbuffers

	gBuffers_fbo->unbind();
	glDisable(GL_DEPTH_TEST);

	gbuffers = false;

}

void GTR::Renderer::ShowGbuffers() {
	int w = Application::instance->window_width;
	int h = Application::instance->window_height;
	GTR::Scene* scene = GTR::Scene::scene;

	Shader* shader_depth = Shader::Get("depth");
	shader_depth->enable();
	GTR::Light* light_entitie = (GTR::Light*)scene->entities[3]; //solo spot 
	shader_depth->setUniform("u_camera_nearfar", Vector2(light_entitie->cameraLight->near_plane, light_entitie->cameraLight->far_plane));
	shader_depth->disable();

	glViewport(0, 0, w / 2, h / 2);//a bajo izq
	gBuffers_fbo->color_textures[0]->toViewport(); //color
	glViewport(w / 2, 0, w / 2, h / 2);//a bajo derecha
	gBuffers_fbo->color_textures[1]->toViewport(); //norm

	glViewport(w / 2, h / 2, w / 2, h / 2); //arriba derecha
	gBuffers_fbo->depth_texture->toViewport(shader_depth); //pos

	glViewport(0, h / 2, w / 2, h / 2); //arriba iz
	gBuffers_fbo->color_textures[2]->toViewport(); //dept
	glViewport(0, 0, w, h); //volvemos a pantalla completa
}

void GTR::Renderer::renderDeferred(Camera* camera, bool shadow, bool shadowmap)
{
	int w = Application::instance->window_width;
	int h = Application::instance->window_height;

	if (deferred_fbo == NULL) {
		deferred_fbo = new FBO();
		deferred_fbo->create(w, h, 4, GL_RGBA, GL_FLOAT);
	}
	GTR::Scene* scene = GTR::Scene::scene;
	glClearColor(0, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	renderSkybox();
	if (Application::instance->iradiance)
		setUniformsDeffered_irradiance(camera, gBuffers_fbo, shadow);
	else
		setUniformsDeffered(camera, gBuffers_fbo, shadow);
	

	if (Application::instance->volumetric)
		renderVolumetric();

	//set default flags
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	if (Application::instance->shadow_front == true) {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
	}

	for (int i = 0; i < scene->entities.size(); i++) {
		if (scene->entities[i]->type == GTR::Light::LIGHT) {
			GTR::Light* light_entities = (GTR::Light*)scene->entities[i];

			if (light_entities->flag == 1) {
				light_entities->shadow_fbo->bind();
				light_entities->cameraLight->enable();
				setUniformsDeffered(light_entities->cameraLight, gBuffers_fbo, true, true); //shadowmap
				light_entities->shadow_fbo->unbind();
				glColorMask(true, true, true, true);

				glDisable(GL_CULL_FACE);
				glDisable(GL_DEPTH_TEST);
			}
		}
	}

	Shader* shader_depth = Shader::Get("depth");
	shader_depth->enable();
	GTR::Light* light_entitie = (GTR::Light*)scene->entities[3]; //solo spot 
	shader_depth->setUniform("u_camera_nearfar", Vector2(light_entitie->cameraLight->near_plane, light_entitie->cameraLight->far_plane));
	shader_depth->disable();
	
	if (shadowmap){
		glViewport(0, 0, 300, 300);
		light_entitie->shadow_fbo->depth_texture->toViewport(shader_depth);
	}
	
	//set default flags
	glDisable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

}

void Renderer::renderDrawSphere(Vector3 positionLight, Vector3 colorLight) {
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glPushMatrix();
	//// Draw sphere (possible styles: FILL, LINE, POINT).
	glColor3f(colorLight.x, colorLight.y, colorLight.z);
	glTranslated(positionLight.x, positionLight.y, positionLight.z);
	GLUquadric* sphere = gluNewQuadric();
	gluSphere(sphere, 5.0f, 200, 200);
	gluDeleteQuadric(sphere);
	glPopMatrix();
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);

}

void Renderer::renderDrawCone(Vector3 positionLight, Vector3 colorLight) {
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
	glPushMatrix();
	GLUquadricObj* theQuadric = gluNewQuadric();
	gluQuadricDrawStyle(theQuadric, GLU_FILL);
	gluQuadricNormals(theQuadric, GLU_SMOOTH);
	glColor3f(colorLight.x, colorLight.y, colorLight.z);
	glTranslated(positionLight.x, positionLight.y, positionLight.z);
	gluCylinder(theQuadric, 0, 10, 10, 200, 200);

	gluDeleteQuadric(theQuadric);
	glPopMatrix();
	glDisable(GL_TEXTURE_2D);
	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);

}

void Renderer::renderEntitiesOfScene(Camera* camera, bool shadow, bool shadowmap, bool transparent)
{
	GTR::Scene* scene = GTR::Scene::scene;
	for (int i = 0; i < scene->entities.size(); i++) { 
		if (scene->entities[i]->type == GTR::BaseEntity::LIGHT) {
			GTR::Light* light_entities = (GTR::Light*)scene->entities[i];
			if (light_entities->light_type == GTR::Light::POINT)
				renderDrawSphere(light_entities->light_position, light_entities->color);
			else if (light_entities->light_type == GTR::Light::SPOT)
				renderDrawCone(light_entities->light_position, light_entities->color);
		}
	}

	for (int i = 0; i < scene->entities.size(); i++) { 
		if (scene->entities[i]->type == GTR::BaseEntity::PREFAB) {
			GTR::PrefabEntity* prefab_entities = (GTR::PrefabEntity*)scene->entities[i];

			if (scene->entities[i]->id == 1) {
				for (int j = 0; j < 5; j++) {
					float angelrad = PI / 180;
					prefab_entities->model.setTranslation(j * 300 - 500, 0, 0);
					prefab_entities->model.setRotation(angelrad * 15 + j * 10, vec3(0, 40, 0));
					renderPrefab(prefab_entities->model, prefab_entities->prefab, camera, shadow, shadowmap, transparent);

				}
			}
			else {
				renderPrefab(prefab_entities->model, prefab_entities->prefab, camera, shadow, shadowmap, transparent);
			}
		}
	}

}

//renders all the prefab
void Renderer::renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera, bool shadow, bool shadowmap, bool transparent)
{
	//assign the model to the root node
	renderNode(model, &prefab->root, camera, shadow, shadowmap, transparent);
}

//renders a node of the prefab and its children
void Renderer::renderNode(const Matrix44& prefab_model, GTR::Node* node, Camera* camera, bool shadow, bool shadowmap, bool transparent)
{
	if (!node->visible)
		return;
	//compute global matrix
	Matrix44 node_model = node->getGlobalMatrix(true) * prefab_model;

	//does this node have a mesh? then we must render it
	if (node->mesh && node->material)
	{
		//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
		BoundingBox world_bounding = transformBoundingBox(node_model, node->mesh->box);

		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize))
		{
			//render node mesh
			if (Application::instance->show_forward || Application::instance->make_irr || Application::instance->make_reflections)
				renderMeshWithMaterial(node_model, node->mesh, node->material, camera, shadow, shadowmap, transparent);
			if (transparent)
				defferedBlend(node_model, node->mesh, node->material, camera, shadow, shadowmap, transparent);
			if (!Application::instance->show_forward && !transparent && !Application::instance->make_irr) //GBUFFERS
				setUniformsGbuffers(node_model, node->mesh, node->material, camera, shadow);

			//node->mesh->renderBounding(node_model, true);
		}
	}
	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		renderNode(prefab_model, node->children[i], camera, shadow, shadowmap, transparent);
}

//renders a mesh given its transform and material MULTI
void Renderer::renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera, bool shadow, bool shadowmap, bool transparent)
{
	//in case there is nothing to do
	if (material->alpha_mode == GTR::AlphaMode::BLEND && shadow) {
		return;
	}
	if (!mesh || !mesh->getNumVertices() || !material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	Shader* shader = NULL;
	Texture* texture = NULL;

	GTR::Scene* scene = GTR::Scene::scene;
	texture = material->color_texture;



	//select if render both sides of the triangles
	if (material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);
	//chose a shader
	if (shadowmap) {
		shader = Shader::Get("flat");
	}
	else
		shader = Shader::Get("multi"); //llamas a multi
	glDepthFunc(GL_LEQUAL);

	if (material->alpha_mode == GTR::AlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	if (texture == NULL)
		texture = Texture::getWhiteTexture(); //a 1x1 white texture				

								  //no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	if (material->emissive_texture) {
		shader->setUniform("u_emissive_texture", material->emissive_texture, 5);
		shader->setUniform1("u_is_emissive", 1);
		shader->setUniform("u_emissive_factor", material->emissive_factor);
	}
	else
		shader->setUniform1("u_is_emissive", 0);

	//FACTOR OF TEXTURE
	int factor = 1;
	shader->setUniform1("u_factor", 1);

	for (int j = 0; j < scene->entities.size(); j++) {
		if (scene->entities[j]->type == GTR::PrefabEntity::PREFAB) {
			GTR::PrefabEntity* entidad = (GTR::PrefabEntity*)scene->entities[j]; //downcast
			if (entidad->prefab->root.material == material) {
				shader->setUniform1("u_factor", entidad->factor);
			}
		}
	}

	//BASIC ATRIBUTS
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);
	shader->setUniform("u_color", material->color);
	if (texture) {
		shader->setUniform("u_texture", texture, 0);

	}

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::AlphaMode::MASK ? material->alpha_cutoff : 0);

	//SHADOW
	shader->setUniform1("u_flag", 0);



	int num_light = 0;

	for (int i = 0; i < scene->entities.size(); i++) {
		if (scene->entities[i]->type == GTR::Light::LIGHT) {
			GTR::Light* light_entities = (GTR::Light*)scene->entities[i];
			//PHONG
			shader->setUniform3("u_light_vector", light_entities->light_vector);
			shader->setUniform3("u_light_position", light_entities->light_position);
			shader->setUniform3("u_light_color", light_entities->color);
			shader->setUniform("u_light_intensity", light_entities->intensity);
			shader->setUniform("u_spotExponent", light_entities->spotExponent);
			shader->setUniform("u_spotCosineCutoff", light_entities->spotCosineCutoff);
			shader->setUniform1("u_light_type", light_entities->light_type);
			shader->setUniform("u_light_maxdist", light_entities->max_distance);

			//SHADOW
			if (light_entities->flag == 1) {
				shader->setUniform("shadowmap", light_entities->shadow_fbo->depth_texture, 6);
				shader->setUniform("u_shadowmap_width", (float)light_entities->shadow_fbo->depth_texture->width);
				shader->setUniform("u_shadowmap_height", (float)light_entities->shadow_fbo->depth_texture->height);
				shader->setTexture("u_shadowmap_AA", light_entities->shadow_fbo->depth_texture, 7);
				if (Application::instance->shadow_AA)
					shader->setUniform("u_is_shadow_AA", 1);
				else
					shader->setUniform("u_is_shadow_AA", 0);
				if (Application::instance->shadow_front)
					shader->setUniform("u_is_front_shadow", 1);
				else
					shader->setUniform("u_is_front_shadow", 0);
			}

			shader->setUniform("u_bias", light_entities->bias);
			shader->setUniform("u_shadow_viewproj", light_entities->cameraLight->viewprojection_matrix);
			shader->setUniform1("u_flag", light_entities->flag);

			if (num_light == 0) {
				if (Application::instance->show_pbr) {
					if (material->alpha_mode == GTR::AlphaMode::BLEND)
					{
						glEnable(GL_BLEND);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					}
					else
						glDisable(GL_BLEND);
					if (material->metallic_roughness_texture) {
						shader->setUniform("u_roughness_texture", material->metallic_roughness_texture, 2);
						shader->setUniform1("u_is_roughness", 1);
						shader->setUniform("u_roughness_factor", material->roughness_factor);
						shader->setUniform("u_metalic_factor", material->metallic_factor);
					}
					else {
						shader->setUniform("u_roughness_texture", Texture::getWhiteTexture(), 2);
						shader->setUniform1("u_is_roughness", 0);
						shader->setUniform("u_roughness_factor", 0);
						shader->setUniform("u_metalic_factor", 0);
					}

				}
				else {
					shader->setUniform1("u_is_roughness", 0);
					shader->setUniform1("u_is_emissive", 0);

				}
				shader->setUniform3("u_ambient_light", scene->ambient);
			}
			else {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				glEnable(GL_BLEND);

				shader->setUniform3("u_ambient_light", vec3(0, 0, 0));//si no es la primera iteracion k sea 0
			}

			mesh->render(GL_TRIANGLES);

			num_light++;
		}

	}
	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glDepthFunc(GL_LESS); //as default
}

void Renderer::setUniformsGbuffers(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera, bool shadow, bool shadowmap, bool transparent) {
	if (material->alpha_mode != GTR::AlphaMode::BLEND && transparent) {
		return;
	}

	//in case there is nothing to do
	if (material->alpha_mode == GTR::AlphaMode::BLEND && shadow) {
		return;
	}
	if (!mesh || !mesh->getNumVertices() || !material)
		return;
	assert(glGetError() == GL_NO_ERROR);


	//define locals to simplify coding
	Shader* shader = NULL;
	Texture* texture = NULL;

	GTR::Scene* scene = GTR::Scene::scene;
	texture = material->color_texture;


	//select if render both sides of the triangles
	if (material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);
	//chose a shader
	if (shadowmap) {
		shader = Shader::Get("flat");
	}
	else
		shader = Shader::Get("gbuffers");
	glDepthFunc(GL_LEQUAL);

	if (material->alpha_mode == GTR::AlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);
	if (texture == NULL)
		texture = Texture::getWhiteTexture(); //a 1x1 white texture

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();
	/*if (material->emissive_texture) {
		shader->setUniform("u_emissive_texture", material->emissive_texture, 5);
		shader->setUniform("u_emissive_factor", material->emissive_factor);
	}*/

	/*int factor = 1;*/
	//shader->setUniform1("u_factor", 1);

	for (int j = 0; j < scene->entities.size(); j++) {
		if (scene->entities[j]->type == GTR::PrefabEntity::PREFAB) {
			GTR::PrefabEntity* entidad = (GTR::PrefabEntity*)scene->entities[j]; //downcast
			if (entidad->prefab->root.material == material) {
				shader->setUniform("u_factor", entidad->factor);
			}
		}
	}

	shader->setUniform("u_is_oclussive", material->occlusion_texture ? 1 : 0);
	shader->setTexture("u_oclussive_texture", material->occlusion_texture ? material->occlusion_texture : Texture::getBlackTexture(), 0);

	shader->setUniform("u_is_emissive", material->emissive_texture ? 1 : 0);
	shader->setTexture("u_emissive_texture", material->emissive_texture ? material->emissive_texture : Texture::getBlackTexture(), 1);

	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);

	shader->setUniform("u_color", material->color);
	shader->setUniform("u_texture", texture, 0);
	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::AlphaMode::MASK ? material->alpha_cutoff : 0);

	if (Application::instance->show_pbr) {
		if (material->alpha_mode == GTR::AlphaMode::BLEND)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else
			glDisable(GL_BLEND);
		if (material->metallic_roughness_texture) {
			shader->setUniform("u_roughness_texture", material->metallic_roughness_texture, 3);
			shader->setUniform1("u_is_roughness", 1);
			shader->setUniform("u_roughness_factor", material->roughness_factor);
			shader->setUniform("u_metalic_factor", material->metallic_factor);
		}
		else {
			shader->setUniform("u_roughness_texture", Texture::getWhiteTexture(), 4);
			shader->setUniform1("u_is_roughness", 0);
			shader->setUniform("u_roughness_factor", 0);
			shader->setUniform("u_metalic_factor", 0);
		}
	}
	else {
		shader->setUniform1("u_is_roughness", 0);
	}

	// do the draw call that renders the mesh into the screen
	mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glDepthFunc(GL_LESS);

}

void Renderer::setUniformsDeffered(Camera* camera, FBO* gbuffers, bool shadow, bool shadowmap) {


	int w = Application::instance->window_width;
	int h = Application::instance->window_height;

	Mesh* sphere = Mesh::Get("data/meshes/sphere.obj");
	Mesh* quad = Mesh::getQuad(); //buffer cuadrado
	Shader* shader = NULL;
	if (shadowmap) {
		shader = Shader::getDefaultShader("flat");
	}

	else {
		shader = Shader::getDefaultShader("deferret");
	}

	shader->enable();

	shader->setUniform("u_color", gbuffers->color_textures[0], 0);
	shader->setUniform("u_color_texture", gbuffers->color_textures[0], 1);

	shader->setUniform("u_emissive_texture", gbuffers->color_textures[3], 2);
	shader->setUniform("u_position_texture", gbuffers->color_textures[2], 3);
	shader->setUniform("u_normal_texture", gbuffers->color_textures[1], 4);
	shader->setUniform("u_depth_texture", gbuffers->depth_texture, 5);
	Matrix44 inv_vp = camera->viewprojection_matrix;
	inv_vp.inverse();
	shader->setUniform("u_inverse_viewprojection", inv_vp);
	shader->setUniform3("u_camera_position", camera->eye);
	shader->setUniform("u_iRes", Vector2(1.0 / (float)deferred_fbo->width, 1.0 / (float)deferred_fbo->height));

	// lights
	GTR::Scene* scene = GTR::Scene::scene;
	shader->setUniform3("u_ambient_light", scene->ambient);

	int num_light = 0;
	shader->setUniform("u_flag", 0);
	for (int i = 0; i < scene->entities.size(); i++) {
		GTR::Light* light_entities = (GTR::Light*)scene->entities[i];
		if (light_entities->type == Light::eBasetype::LIGHT) {


			if (num_light == 0) {
				glDisable(GL_BLEND);
			}
			else {
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
			}

			shader->setUniform3("u_light_position", light_entities->light_position);
			shader->setUniform1("u_light_type", light_entities->light_type);
			shader->setUniform("u_light_intensity", light_entities->intensity);
			shader->setUniform3("u_light_color", light_entities->color);
			shader->setUniform("u_light_maxdist", light_entities->max_distance);
			shader->setUniform3("u_light_vector", light_entities->light_vector);
			shader->setUniform("u_spotCosineCutoff", light_entities->spotCosineCutoff);
			shader->setUniform("u_spotExponent", light_entities->spotExponent);


			//SHADOW

			shader->setUniform("u_flag", light_entities->flag);

			if (light_entities->flag == 1) {
				shader->setUniform("u_shadow_viewproj", light_entities->cameraLight->viewprojection_matrix);
				shader->setUniform("u_bias", light_entities->bias);
				shader->setUniform("shadowmap", light_entities->shadow_fbo->depth_texture, 6);
				shader->setUniform("u_shadowmap_width", (float)light_entities->shadow_fbo->depth_texture->width);
				shader->setUniform("u_shadowmap_height", (float)light_entities->shadow_fbo->depth_texture->height);
				shader->setTexture("u_shadowmap_AA", light_entities->shadow_fbo->depth_texture, 7);
				if (Application::instance->shadow_AA)
					shader->setUniform("u_is_shadow_AA", 1);
				else
					shader->setUniform("u_is_shadow_AA", 0);
				if (Application::instance->shadow_front)
					shader->setUniform("u_is_front_shadow", 1);
				else
					shader->setUniform("u_is_front_shadow", 0);
			}
			//ssao
			if (Application::instance->is_ssao) {
				shader->setTexture("u_ao_texture", Application::instance->ssao_blur, 8);
				shader->setUniform("u_is_ssao", 1);
			}
			else
				shader->setUniform("u_is_ssao", 0);

			quad->render(GL_TRIANGLES);
			num_light++;
		}

	}
	
	shader->disable();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
}


void GTR::Renderer::defferedBlend(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera, bool shadow, bool shadowmap, bool transparent) {

	if (material->alpha_mode != GTR::AlphaMode::BLEND) { //me quedo con las prefabs transparentes para hacer la segunda pasada 
		return;
	}

	if (!mesh || !mesh->getNumVertices() || !material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	Shader* shader = NULL;
	Texture* texture = NULL;

	GTR::Scene* scene = GTR::Scene::scene;
	texture = material->color_texture;


	//select if render both sides of the triangles
	if (material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);
	//chose a shader

	shader = Shader::Get("defferetBlend");

	if (texture == NULL)
		texture = Texture::getWhiteTexture(); //a 1x1 white texture

	//no shader? then nothing to render
	shader->enable();

	glDepthFunc(GL_LEQUAL);
	glDisable(GL_DEPTH_TEST);

	if (material->alpha_mode == GTR::AlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);


	//FACTOR OF TEXTURE
	int factor = 1;
	shader->setUniform1("u_factor", 1);

	for (int j = 0; j < scene->entities.size(); j++) {
		if (scene->entities[j]->type == GTR::PrefabEntity::PREFAB) {
			GTR::PrefabEntity* entidad = (GTR::PrefabEntity*)scene->entities[j]; //downcast
			if (entidad->prefab->root.material == material) {
				shader->setUniform1("u_factor", entidad->factor);
			}
		}
	}

	//BASIC ATRIBUTS
	shader->setUniform("u_depth_texture", gBuffers_fbo->depth_texture, 8);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);
	shader->setUniform("u_color", material->color);
	if (texture) {
		shader->setUniform("u_texture", texture, 0);
	}

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)

	//SHADOW
	shader->setUniform1("u_flag", 0);

	if (material->emissive_texture) {
		shader->setUniform("u_emissive_texture", material->emissive_texture, 4);
		shader->setUniform1("u_is_emissive", 1);
		shader->setUniform("u_emissive_factor", material->emissive_factor);
	}
	else
		shader->setUniform1("u_is_emissive", 0);

	int num_light = 0;

	for (int i = 0; i < scene->entities.size(); i++) {
		if (scene->entities[i]->type == GTR::Light::LIGHT) {
			GTR::Light* light_entities = (GTR::Light*)scene->entities[i];
			//PHONG
			shader->setUniform3("u_light_vector", light_entities->light_vector);
			shader->setUniform3("u_light_position", light_entities->light_position);
			shader->setUniform3("u_light_color", light_entities->color);
			shader->setUniform("u_light_intensity", light_entities->intensity);
			shader->setUniform("u_spotExponent", light_entities->spotExponent);
			shader->setUniform("u_spotCosineCutoff", light_entities->spotCosineCutoff);
			shader->setUniform1("u_light_type", light_entities->light_type);
			shader->setUniform("u_light_maxdist", light_entities->max_distance);

			//SHADOW
			if (light_entities->flag == 1) {
				shader->setUniform("u_shadow_viewproj", light_entities->cameraLight->viewprojection_matrix);
				shader->setUniform("u_bias", light_entities->bias);
				shader->setUniform("shadowmap", light_entities->shadow_fbo->depth_texture, 5);
				shader->setUniform("u_shadowmap_width", (float)light_entities->shadow_fbo->depth_texture->width);
				shader->setUniform("u_shadowmap_height", (float)light_entities->shadow_fbo->depth_texture->height);
				shader->setTexture("u_shadowmap_AA", light_entities->shadow_fbo->depth_texture, 7);
				if (Application::instance->shadow_AA)
					shader->setUniform1("u_is_shadow_AA", 1);
				else
					shader->setUniform1("u_is_shadow_AA", 0);
				if (Application::instance->shadow_front)
					shader->setUniform1("u_is_front_shadow", 1);
				else
					shader->setUniform("u_is_front_shadow", 0);
			}

			shader->setUniform1("u_flag", light_entities->flag);

			if (num_light == 0) {
				if (Application::instance->show_pbr) {
					/*if (material->alpha_mode == GTR::AlphaMode::BLEND)
					{
						glEnable(GL_BLEND);
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					}
					else*/
					//glDisable(GL_BLEND);
					if (material->metallic_roughness_texture) {
						shader->setUniform("u_roughness_texture", material->metallic_roughness_texture, 2);
						shader->setUniform("u_roughness_factor", material->roughness_factor);
						shader->setUniform("u_metalic_factor", material->metallic_factor);
					}
					else {
						shader->setUniform("u_roughness_texture", Texture::getWhiteTexture(), 3);
						shader->setUniform1("u_is_roughness", 0);
						shader->setUniform("u_roughness_factor", 0);
						shader->setUniform("u_metalic_factor", 0);
					}
					shader->setUniform("u_iRes", Vector2(1.0 / (float)Application::instance->window_width, 1.0 / (float)Application::instance->window_height));
					//ssao
					if (Application::instance->is_ssao) {
						shader->setTexture("u_ao_texture", Application::instance->ssao_blur, 9);
						shader->setUniform("u_is_ssao", 1);
					}
					else
						shader->setUniform("u_is_ssao", 0);
				}
				else {
					shader->setUniform1("u_is_roughness", 0);

				}
				shader->setUniform3("u_ambient_light", scene->ambient);
			}
			else {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				glEnable(GL_BLEND);

				shader->setUniform3("u_ambient_light", vec3(0, 0, 0));
			}

			mesh->render(GL_TRIANGLES);

			num_light++;
		}

	}
	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);//as default
}

void::GTR::Renderer::setUniformsDeffered_irradiance(Camera* camera, FBO* gbuffers, bool shadow, bool shadowmap) {

	int w = Application::instance->window_width;
	int h = Application::instance->window_height;

	Mesh* sphere = Mesh::Get("data/meshes/sphere.obj");
	Mesh* quad = Mesh::getQuad(); //buffer cuadrado
	Shader* shader = NULL;
	if (shadowmap) {
		shader = Shader::getDefaultShader("flat");
	}

	else {
		shader = Shader::getDefaultShader("deferret_irradiance");
	}

	shader->enable();

	shader->setUniform("u_color", gbuffers->color_textures[0], 0);
	shader->setUniform("u_color_texture", gbuffers->color_textures[0], 1);

	shader->setUniform("u_emissive_texture", gbuffers->color_textures[3], 2);
	shader->setUniform("u_position_texture", gbuffers->color_textures[2], 3);
	shader->setUniform("u_normal_texture", gbuffers->color_textures[1], 4);
	shader->setUniform("u_depth_texture", gbuffers->depth_texture, 5);
	Matrix44 inv_vp = camera->viewprojection_matrix;
	inv_vp.inverse();
	shader->setUniform("u_inverse_viewprojection", inv_vp);
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform3("u_camera_position", camera->eye);
	shader->setUniform("u_iRes", Vector2(1.0 / (float)deferred_fbo->width, 1.0 / (float)deferred_fbo->height));

	//irradiance
	shader->setUniform("u_irr_start", start_pos);
	shader->setUniform("u_irr_end", end_pos);
	shader->setUniform("u_irr_normal_distance", 1.0f);
	shader->setUniform("u_irr_delta", delta);
	shader->setUniform("u_irr_dims", dim);
	shader->setUniform("u_num_probes", (int)probes.size());

	shader->setTexture("u_probes_texture", Application::instance->probe_texture, 10);


	// lights
	GTR::Scene* scene = GTR::Scene::scene;
	shader->setUniform3("u_ambient_light", scene->ambient);

	int num_light = 0;
	shader->setUniform("u_flag", 0);
	for (int i = 0; i < scene->entities.size(); i++) {
		GTR::Light* light_entities = (GTR::Light*)scene->entities[i];
		if (light_entities->type == Light::eBasetype::LIGHT) {


			if (num_light == 0) {
				glDisable(GL_BLEND);
			}
			else {
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
			}

			shader->setUniform3("u_light_position", light_entities->light_position);
			shader->setUniform1("u_light_type", light_entities->light_type);
			shader->setUniform("u_light_intensity", light_entities->intensity);
			shader->setUniform3("u_light_color", light_entities->color);
			shader->setUniform("u_light_maxdist", light_entities->max_distance);
			shader->setUniform3("u_light_vector", light_entities->light_vector);
			shader->setUniform("u_spotCosineCutoff", light_entities->spotCosineCutoff);
			shader->setUniform("u_spotExponent", light_entities->spotExponent);


			//SHADOW

			shader->setUniform("u_flag", light_entities->flag);

			if (light_entities->flag == 1) {
				shader->setUniform("u_shadow_viewproj", light_entities->cameraLight->viewprojection_matrix);
				shader->setUniform("u_bias", light_entities->bias);
				shader->setUniform("shadowmap", light_entities->shadow_fbo->depth_texture, 6);
				shader->setUniform("u_shadowmap_width", (float)light_entities->shadow_fbo->depth_texture->width);
				shader->setUniform("u_shadowmap_height", (float)light_entities->shadow_fbo->depth_texture->height);
				shader->setTexture("u_shadowmap_AA", light_entities->shadow_fbo->depth_texture, 7);
				if (Application::instance->shadow_AA)
					shader->setUniform("u_is_shadow_AA", 1);
				else
					shader->setUniform("u_is_shadow_AA", 0);
				if (Application::instance->shadow_front)
					shader->setUniform("u_is_front_shadow", 1);
				else
					shader->setUniform("u_is_front_shadow", 0);
			}
			//ssao
			if (Application::instance->is_ssao) {
				shader->setTexture("u_ao_texture", Application::instance->ssao_blur, 8);
				shader->setUniform("u_is_ssao", 1);
			}
			else
				shader->setUniform("u_is_ssao", 0);

			quad->render(GL_TRIANGLES);
			num_light++;
		}

	}
	//quad->render(GL_TRIANGLES);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);



	shader->disable();
}