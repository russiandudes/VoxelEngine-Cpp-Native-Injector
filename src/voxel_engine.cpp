#include <iostream>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <ctime>

// GLM
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;

#include "graphics/Shader.h"
#include "graphics/Texture.h"
#include "graphics/Mesh.h"
#include "graphics/VoxelRenderer.h"
#include "graphics/LineBatch.h"
#include "window/Window.h"
#include "window/Events.h"
#include "window/Camera.h"
#include "loaders/png_loading.h"
#include "voxels/voxel.h"
#include "voxels/Chunk.h"
#include "voxels/Chunks.h"
#include "voxels/Block.h"
#include "voxels/WorldGenerator.h"
#include "voxels/ChunksController.h"
#include "files/files.h"
#include "files/WorldFiles.h"
#include "lighting/LightSolver.h"
#include "lighting/Lightmap.h"
#include "lighting/Lighting.h"
#include "physics/Hitbox.h"
#include "physics/PhysicsSolver.h"

#include "objects/Player.h"

#include "declarations.h"
#include "world_render.h"

int WIDTH = 1280;
int HEIGHT = 720;

Chunks* chunks;
WorldFiles* wfile;

bool occlusion = false;


// Save all world data to files
void write_world(){
	for (unsigned int i = 0; i < chunks->volume; i++){
		Chunk* chunk = chunks->chunks[i];
		if (chunk == nullptr)
			continue;
		wfile->put((const char*)chunk->voxels, chunk->x, chunk->z);
	}

	wfile->write();
}

// Deleting world data from memory
void close_world(){
	delete chunks;
	delete wfile;
}

int main() {
	setup_definitions();

	Window::initialize(WIDTH, HEIGHT, "Window 2.0");
	Events::initialize();

	int result = initialize_assets();
	if (result){
		Window::terminate();
		return result;
	}

	Camera* camera = new Camera(vec3(-320,255,32), radians(90.0f));

	wfile = new WorldFiles("world/", REGION_VOL * (CHUNK_VOL * 2 + 8));
	chunks = new Chunks(34,1,34, 0,0,0);


	Player* player = new Player(vec3(camera->position), 5.0f, camera);
	wfile->readPlayer(player);
	camera->rotation = mat4(1.0f);
	camera->rotate(player->camY, player->camX, 0);

	VoxelRenderer renderer(1024*1024);
	PhysicsSolver physics(vec3(0,-16.0f,0));

	Lighting lighting(chunks);

	init_renderer();

	ChunksController chunksController(chunks, &lighting);

	float lastTime = glfwGetTime();
	float delta = 0.0f;

	int choosenBlock = 1;
	long frame = 0;

	glfwSwapInterval(1);

	while (!Window::isShouldClose()){
		frame++;
		float currentTime = glfwGetTime();
		delta = currentTime - lastTime;
		lastTime = currentTime;

		if (frame % 240 == 0)
			std::cout << 1.0/delta << std::endl;

		if (Events::jpressed(GLFW_KEY_O)){
			occlusion = !occlusion;
		}

		if (Events::jpressed(GLFW_KEY_ESCAPE)){
			Window::setShouldClose(true);
		}
		if (Events::jpressed(GLFW_KEY_TAB)){
			Events::toogleCursor();
		}

		for (int i = 1; i < 10; i++){
			if (Events::jpressed(GLFW_KEY_0+i)){
				choosenBlock = i;
			}
		}

		// Controls
		Hitbox* hitbox = player->hitbox;
		bool sprint = Events::pressed(GLFW_KEY_LEFT_CONTROL);
		bool shift = Events::pressed(GLFW_KEY_LEFT_SHIFT) && hitbox->grounded && !sprint;

		float speed = player->speed;
		int substeps = (int)(delta * 1000);
		substeps = (substeps <= 0 ? 1 : (substeps > 100 ? 100 : substeps));
		physics.step(chunks, hitbox, delta, substeps, shift);
		camera->position.x = hitbox->position.x;
		camera->position.y = hitbox->position.y + 0.5f;
		camera->position.z = hitbox->position.z;

		float dt = min(1.0f, delta * 16);
		if (shift){
			speed *= 0.25f;
			camera->position.y -= 0.2f;
			camera->zoom = 0.9f * dt + camera->zoom * (1.0f - dt);
		} else if (sprint){
			speed *= 1.5f;
			camera->zoom = 1.1f * dt + camera->zoom * (1.0f - dt);
		} else {
			camera->zoom = dt + camera->zoom * (1.0f - dt);
		}
		if (Events::pressed(GLFW_KEY_SPACE) && hitbox->grounded){
			hitbox->velocity.y = 6.0f;
		}

		vec3 dir(0,0,0);
		if (Events::pressed(GLFW_KEY_W)){
			dir.x += camera->dir.x;
			dir.z += camera->dir.z;
		}
		if (Events::pressed(GLFW_KEY_S)){
			dir.x -= camera->dir.x;
			dir.z -= camera->dir.z;
		}
		if (Events::pressed(GLFW_KEY_D)){
			dir.x += camera->right.x;
			dir.z += camera->right.z;
		}
		if (Events::pressed(GLFW_KEY_A)){
			dir.x -= camera->right.x;
			dir.z -= camera->right.z;
		}
		if (length(dir) > 0.0f)
			dir = normalize(dir);
		hitbox->velocity.x = dir.x * speed;
		hitbox->velocity.z = dir.z * speed;

		chunks->setCenter(wfile, camera->position.x,0,camera->position.z);
		chunksController._buildMeshes(&renderer, frame);
		chunksController.loadVisible(wfile);

		if (Events::_cursor_locked){
			player->camY += -Events::deltaY / Window::height * 2;
			player->camX += -Events::deltaX / Window::height * 2;

			if (player->camY < -radians(89.0f)){
				player->camY = -radians(89.0f);
			}
			if (player->camY > radians(89.0f)){
				player->camY = radians(89.0f);
			}

			camera->rotation = mat4(1.0f);
			camera->rotate(player->camY, player->camX, 0);
		}

		{
			vec3 end;
			vec3 norm;
			vec3 iend;
			voxel* vox = chunks->rayCast(camera->position, camera->front, 10.0f, end, norm, iend);
			if (vox != nullptr){
				lineBatch->box(iend.x+0.5f, iend.y+0.5f, iend.z+0.5f, 1.005f,1.005f,1.005f, 0,0,0,0.5f);

				if (Events::jclicked(GLFW_MOUSE_BUTTON_1)){
					int x = (int)iend.x;
					int y = (int)iend.y;
					int z = (int)iend.z;
					chunks->set(x,y,z, 0);
					lighting.onBlockSet(x,y,z,0);
				}
				if (Events::jclicked(GLFW_MOUSE_BUTTON_2)){
					int x = (int)(iend.x)+(int)(norm.x);
					int y = (int)(iend.y)+(int)(norm.y);
					int z = (int)(iend.z)+(int)(norm.z);
					if (!physics.isBlockInside(x,y,z, hitbox)){
						chunks->set(x, y, z, choosenBlock);
						lighting.onBlockSet(x,y,z, choosenBlock);
					}
				}
			}
		}
		draw_world(camera, shader, texture, crosshairShader, linesShader, chunks, occlusion);

		Window::swapBuffers();
		Events::pullEvents();
	}

	wfile->writePlayer(player);
	write_world();
	close_world();

	finalize_renderer();
	finalize_assets();
	Window::terminate();
	return 0;
}
