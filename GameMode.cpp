#include "GameMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "Sound.hpp"
#include "Scene.hpp"
#include "MeshBuffer.hpp"
#include "WalkMesh.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "vertex_color_program.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>


MeshBuffer::Mesh dungeon_mesh;
MeshBuffer::Mesh walk_mesh;
MeshBuffer::Mesh monster_mesh;
glm::vec3 monsterPos = glm::vec3(0.0f, 20.0f, 1.0f);
glm::vec3 escapePos = glm::vec3(10.0, 10.0, 10.0);
glm::vec3 playerPos = glm::vec3(0.0f, -10.0f, 1.0f);
//float playerSpeed = 10.0; //WALKMESH

Load< MeshBuffer > dungeon_meshes(LoadTagDefault, [](){
	//return new MeshBuffer(data_path("crates.pnc"));
	//MeshBuffer const *ret = new MeshBuffer(data_path("dungeon.pnc"));
	MeshBuffer const *ret = new MeshBuffer(data_path("meshes.pnc"));

	dungeon_mesh = ret->lookup("hall");
	walk_mesh = ret->lookup("walkmesh");
	monster_mesh = ret->lookup("monster");

	return ret;
});

Load< GLuint > dungeon_meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(dungeon_meshes->make_vao_for_program(vertex_color_program->program));
});


Load< Sound::Sample > roar(LoadTagDefault, [](){
	return new Sound::Sample(data_path("roar.wav"));
});

GameMode::GameMode() {
	//----------------
	//set up scene:
	//TODO: this should load the scene from a file!

	initGame();

}

void GameMode::initGame(){
	auto attach_object = [this](Scene::Transform *transform, std::string const &name) {
		Scene::Object *object = scene.new_object(transform);
		object->program = vertex_color_program->program;
		object->program_mvp_mat4 = vertex_color_program->object_to_clip_mat4;
		object->program_mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
		object->program_itmv_mat3 = vertex_color_program->normal_to_light_mat3;
		object->vao = *dungeon_meshes_for_vertex_color_program;
		MeshBuffer::Mesh const &mesh = dungeon_meshes->lookup(name);
		object->start = mesh.start;
		object->count = mesh.count;
		return object;
	};
	{ //do dungeon and monster stuff here
		Scene::Transform *transform1 = scene.new_transform();
		transform1->position = glm::vec3(-0.45f, -8.0f, 0.0f);
		dungeon = attach_object(transform1, "hall");
		Scene::Transform *transform2 = scene.new_transform();
		transform2->position = monsterPos;
		monster = attach_object(transform2, "monster");
	}

	{ //Camera looking at the origin:
		Scene::Transform *transform = scene.new_transform();
		transform->position = playerPos; //player is camera
		//Cameras look along -z, so rotate view to look at origin:
		transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		camera = scene.new_camera(transform);
	}

/* WALKMESH
	std::vector<glm::vec3> const vertices_;
	std::vector< glm::uvec3 > const triangles_;
	WalkMesh wmesh = WalkMesh(vertices_, triangles_);
	WalkMesh::WalkPoint walk_point;
	walk_point = wmesh.start(playerPos);
*/

	//don't need be three dimensional since monster won't fall on top of player
	std::vector<int> playerDim = {3, 3}; 
	std::vector<int> monsterDim = {5, 5};
	std::vector<int> escapeDim = {5, 3};
}

GameMode::~GameMode() {
}

bool GameMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}
	//TODO: use walkmesh::walk
	if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) {
		if (evt.key.keysym.scancode == SDL_SCANCODE_UP) {
			controls.forward = (evt.type == SDL_KEYDOWN);
			return true;
		} 
		else if (evt.key.keysym.scancode == SDL_SCANCODE_DOWN) {
			controls.backward = (evt.type == SDL_KEYDOWN);
			return true;
		} 
		else if (evt.key.keysym.scancode == SDL_SCANCODE_LEFT) {
			controls.left = (evt.type == SDL_KEYDOWN);
			return true;
		}
		else if (evt.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
			controls.right = (evt.type == SDL_KEYDOWN);
			return true;
		}
		else if (evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
			//open pause menu on 'ESCAPE':
			show_pause_menu(false, false);
			return true;
		}
	}
	return false;
}


void GameMode::liveDie() {
	//check for collision between player and monster
	if (camera->transform->position[0]<monsterPos[0]+monsterDim[0] and
		camera->transform->position[0]+playerDim[0]>monsterPos[0] and
		camera->transform->position[1]<monsterPos[1]+monsterDim[1] and
		camera->transform->position[1]+playerDim[1]>monsterDim[1]) {
		//round lost
		show_pause_menu(true, true);
	}
	//check for collision between player and exit of dungeon
	if (camera->transform->position[0]<escapePos[0]+escapeDim[0] and
		camera->transform->position[0]+playerDim[0]>escapePos[0] and
		camera->transform->position[1]<escapePos[1]+escapeDim[1] and
		camera->transform->position[1]+playerDim[1]>escapeDim[1]) {
		//round won
		show_pause_menu(true, false);
	}
	/*
	if (playerPos[0]<monsterPos[0]+monsterDim[0] and
		playerPos[0]+playerDim[0]>monsterPos[0] and
		playerPos[1]<monsterPos[1]+monsterDim[1] and
		playerPos[1]+playerDim[1]>monsterDim[1]) {
		//round lost
		return;
	}
	//check for collision between player and exit of dungeon
	if (playerPos[0]<escapePos[0]+escapeDim[0] and
		playerPos[0]+playerDim[0]>escapePos[0] and
		playerPos[1]<escapePos[1]+escapeDim[1] and
		playerPos[1]+playerDim[1]>escapeDim[1]) {
		//round won
		return;
	}
	*/
	return;
}


void GameMode::update(float elapsed) {
	//check for win/loss condition every time
	liveDie();
	glm::mat3 directions = glm::mat3_cast(camera->transform->rotation);
	float amt = 5.0f * elapsed;
	//player pos is camera->transform->position
	if (controls.right){
		camera->transform->position += amt * directions[0];
	}
	if (controls.left) {
		camera->transform->position -= amt * directions[0];
	}
	if (controls.backward){
		camera->transform->position += amt * directions[2];
	}
	if (controls.forward) {
		camera->transform->position -= amt * directions[2];
	}
	{ //set sound positions:
		glm::mat4 cam_to_world = camera->transform->make_local_to_world();
		Sound::listener.set_position( cam_to_world[3] );
		//camera looks down -z, so right is +x:
		Sound::listener.set_right( glm::normalize(cam_to_world[0]) );
	}
	//randomize where the monster is in the dungeon
	/*
	monsterPos_countdown -= elapsed;
	if (monsterPos_countdown <= 0.0f) {
		//update monster position
		std::cout << "monster on the move" <<std::endl;
		//std::cout << "monster position is "<< monster->transform->position << std::endl;
		monster->transform->position += glm::vec3(5.0, 0, 0);
		//monsterPos = monsterPos+glm::vec3(20.0, 0, 0);
		monsterPos_countdown = 4.0f;
	}
	*/
	roar_countdown -= elapsed;
	if (roar_countdown <= 0.0f) { //CHANGE DUNGEON TO WORLD TO MONSTER TO WORLD
		//roar_countdown = (rand() / float(RAND_MAX) * 2.0f) + 0.5f;
		glm::mat4x3 monster_to_world = monster->transform->make_local_to_world();
		roar->play( monster_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) );
		roar_countdown = 5.0f;
	}
/* WALKMESH
	//update position on walk mesh:
	glm::vec3 step = playerSpeed*elapsed*player_forward;
	wmesh.walk(walk_point, step);
	//update player position:
	playerPos = wmesh.world_point(walk_point);
	//update player orientation:
	glm::vec3 old_player_up = player_up;
	player_up = wmesh.world_normal(walk_point);

	//rotation from old_player_up to player_up
	glm::vec3 v = glm::cross(old_player_up, player_up);
	//glm::vec3 s = glm::l1Norm(v); //angle sine
	float c = glm::dot(old_player_up, player_up); //angle cosine
	glm::mat3 vx = glm::mat3(0, -v[2], v[1], v[2], 0, -v[0], -v[1], v[0], 0);
	glm::mat3 I = glm::mat3(1, 0, 0, 0, 1, 0, 0, 0, 1);
	glm::quat orientation_change = I+vx+(1/(1+c));

	player_forward = orientation_change*player_forward;
	//make sure player_forward is perpendicular to player_up (the earlier rotation should ensure that, but it might drift over time):
	player_forward = glm::normalize(player_forward - player_up * glm::dot(player_up, player_forward));
	//compute rightward direction from forward and up:
	player_right = glm::cross(player_forward, player_up);
*/
}

void GameMode::draw(glm::uvec2 const &drawable_size) {
	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//set up light position + color:
	glUseProgram(vertex_color_program->program);
	glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
	glUniform3fv(vertex_color_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
	glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.4f, 0.4f, 0.45f)));
	glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));
	glUseProgram(0);

	//fix aspect ratio of camera
	camera->aspect = drawable_size.x / float(drawable_size.y);

	scene.draw(camera);

/*
	if (Mode::current.get() == this) {
		glDisable(GL_DEPTH_TEST);
		std::string message;
		if (mouse_captured) {
			message = "ESCAPE TO UNGRAB MOUSE * WASD MOVE";
		} else {
			message = "CLICK TO GRAB MOUSE * ESCAPE QUIT";
		}
		float height = 0.06f;
		float width = text_width(message, height);
		draw_text(message, glm::vec2(-0.5f * width,-0.99f), height, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
		draw_text(message, glm::vec2(-0.5f * width,-1.0f), height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

		glUseProgram(0);
	}
*/
	GL_ERRORS();
}


void GameMode::show_pause_menu(bool checkWin, bool win) {
	//pause roaring
	//if (roar) roar->stop();

	if (checkWin == false) {
		std::shared_ptr< MenuMode > menu = std::make_shared< MenuMode >();
		std::shared_ptr< Mode > game = shared_from_this();
		menu->background = game;
		menu->choices.emplace_back("PAUSED");
		menu->choices.emplace_back("RESUME", [game](){
		//unpause roaring, centering roaring back on monster
		//glm::mat4x3 small_crate_to_world = small_crate->transform->make_local_to_world();
		//if (!roar) roar->play(small_crate_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			Mode::set_current(game);
		});
		menu->choices.emplace_back("QUIT", [](){
			Mode::set_current(nullptr);
		});
		menu->selected = 1;
		Mode::set_current(menu);
	}
/*
	else {
		std::shared_ptr< MenuMode > menu = std::make_shared< MenuMode >();
		std::shared_ptr< Mode > game = shared_from_this();
		menu->background = game;
		if (win == true) {
			menu->choices.emplace_back("YOU WIN");
		}
		else {
			menu->choices.emplace_back("YOU LOSE");
		}
		menu->selected = 1;
		Mode::set_current(menu);
	}
*/
}
