#pragma once

#include "Mode.hpp"

#include "MeshBuffer.hpp"
#include "WalkMesh.hpp"
#include "Sound.hpp"
#include "Scene.hpp"
#include "GL.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

// The 'GameMode' mode is the main gameplay mode:

struct GameMode : public Mode {
	GameMode();
	virtual ~GameMode();

	//handle_event is called when new mouse or keyboard events are received:
	// (note that this might be many times per frame or never)
	//The function should return 'true' if it handled the event.
	virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;

	//update is called at the start of a new frame, after events are handled:
	virtual void update(float elapsed) override;

	//draw is called after update:
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//starts up a 'quit/resume' pause menu:
	void show_pause_menu(bool checkwin, bool win);

	//------- game state -------

	glm::uvec2 board_size = glm::uvec2(5,4);
	std::vector< MeshBuffer::Mesh const * > board_meshes;
	//std::vector< glm::quat > board_rotations;

	glm::uvec2 cursor = glm::vec2(0,0);

//CHANGED
//checks win/loss conditions every time update is called
void liveDie();
void initGame();
//walkmesh stuff
/*
std::vector<glm::vec3> const vertices_;
std::vector< glm::uvec3 > const triangles_;
WalkMesh wmesh = WalkMesh(vertices_, triangles_);
WalkMesh::WalkPoint walk_point;
glm::vec3 playerPos;
glm::vec3 player_up;
glm::vec3 player_forward;
glm::vec3 player_right;
uint32_t playerSpeed;
*/
glm::vec3 escapePos;
glm::vec3 monsterPos;
std::vector<uint32_t> monsterDim;
std::vector<uint32_t> escapeDim;
std::vector<uint32_t> playerDim;

//when this reaches zero, the 'dot' sample is triggered at the small crate:
float roar_countdown = 5.0f;
float monsterPos_countdown = 4.0f;
//this 'loop' sample is played at the large crate:
Scene scene;
Scene::Object *monster = nullptr;
Scene::Object *dungeon = nullptr;
Scene::Object *large_crate = nullptr;
Scene::Object *small_crate = nullptr;
Scene::Camera *camera = nullptr;
bool mouse_captured = false;
struct {
		bool forward = false;
		bool backward = false;
		bool left = false;
		bool right = false;
} controls;

/*
	struct {
		bool roll_left = false;
		bool roll_right = false;
		bool roll_up = false;
		bool roll_down = false;
	} controls;
*/
};
