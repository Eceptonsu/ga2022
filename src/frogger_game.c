#pragma once
#include <stdio.h>
#include "ecs.h"
#include "fs.h"
#include "gpu.h"
#include "heap.h"
#include "render.h"
#include "timer_object.h"
#include "transform.h"
#include "wm.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>
#include <stdio.h>

typedef struct transform_component_t
{
	transform_t transform;
} transform_component_t;

typedef struct camera_component_t
{
	mat4f_t projection;
	mat4f_t view;
} camera_component_t;

typedef struct model_component_t
{
	gpu_mesh_info_t* mesh_info;
	gpu_shader_info_t* shader_info;
} model_component_t;

typedef struct player_component_t
{
	int index;
	float speed;
} player_component_t;

typedef struct name_component_t
{
	char name[32];
} name_component_t;

typedef struct frogger_game_t
{
	heap_t* heap;
	fs_t* fs;
	wm_window_t* window;
	render_t* render;

	timer_object_t* timer;

	ecs_t* ecs;
	int transform_type;
	int camera_type;
	int model_type;
	int player_type;
	int name_type;
	
	int difficulty;
	int num_lines;
	int num_traffic;

	ecs_entity_ref_t player_ent;
	ecs_entity_ref_t camera_ent;

	gpu_mesh_info_t cube_mesh;
	gpu_shader_info_t cube_shader;
	gpu_shader_info_t traffic_shader;
	fs_work_t* vertex_shader_work;
	fs_work_t* fragment_shader_work;
} frogger_game_t;

typedef struct ImVec4
{
	float x, y, z, w;
} ImVec4;

typedef struct engine_info_t
{
	bool orthoView;
	float viewDistance;
	float horizontalPan;
	float verticalPan;
	float viewDistanceP;
	float horizontalPanP;
	float verticalPanP;
	float yaw;
	float pitch;
	float roll;

	int difficulty;
	float playerSpeed;
	ImVec4 playerColor;
} engine_info_t;

static void load_resources(frogger_game_t* game);
static void unload_resources(frogger_game_t* game);
static void spawn_player(frogger_game_t* game, int index);
static void spawn_camera(frogger_game_t* game);
static void update_players(frogger_game_t* game, engine_info_t* engine_info);
static void update_camera(frogger_game_t* game, engine_info_t* engine_info);
static void draw_models(frogger_game_t* game, engine_info_t* engine_info);

frogger_game_t* frogger_game_create(heap_t* heap, fs_t* fs, wm_window_t* window, render_t* render, int difficulty)
{
	if (difficulty <= 0 || difficulty > 5) 
	{
		printf("INVALID DIFFICULTY!\nThe difficulties avaliable are 1, 2, 3");
		return NULL;
	}

	frogger_game_t* game = heap_alloc(heap, sizeof(frogger_game_t), 8);
	game->heap = heap;
	game->fs = fs;
	game->window = window;
	game->render = render;

	game->timer = timer_object_create(heap, NULL);
	
	game->ecs = ecs_create(heap);
	game->transform_type = ecs_register_component_type(game->ecs, "transform", sizeof(transform_component_t), _Alignof(transform_component_t));
	game->camera_type = ecs_register_component_type(game->ecs, "camera", sizeof(camera_component_t), _Alignof(camera_component_t));
	game->model_type = ecs_register_component_type(game->ecs, "model", sizeof(model_component_t), _Alignof(model_component_t));
	game->player_type = ecs_register_component_type(game->ecs, "player", sizeof(player_component_t), _Alignof(player_component_t));
	game->name_type = ecs_register_component_type(game->ecs, "name", sizeof(name_component_t), _Alignof(name_component_t));

	game->difficulty = difficulty;
	game->num_lines = 2 + difficulty;
	game->num_traffic = difficulty * 3;

	load_resources(game);
	spawn_player(game, 0);
	
	for (int i = 0; i < game->num_lines * game->num_traffic; i++)
	{
		spawn_player(game, i+1);
	}

	spawn_camera(game);

	return game;
}

void frogger_game_destroy(frogger_game_t* game)
{
	ecs_destroy(game->ecs);
	timer_object_destroy(game->timer);
	unload_resources(game);
	heap_free(game->heap, game);
}

void frogger_game_update(frogger_game_t* game, engine_info_t* engine_info)
{
	timer_object_update(game->timer);
	ecs_update(game->ecs);
	update_players(game, engine_info);
	update_camera(game, engine_info);
	draw_models(game, engine_info);
	render_push_done(game->render);
}

static void load_resources(frogger_game_t* game)
{
	game->vertex_shader_work = fs_read(game->fs, "shaders/greenCube.vert", game->heap, false, false);
	game->fragment_shader_work = fs_read(game->fs, "shaders/greenCube.frag", game->heap, false, false);
	game->cube_shader = (gpu_shader_info_t)
	{
		.vertex_shader_data = fs_work_get_buffer(game->vertex_shader_work),
		.vertex_shader_size = fs_work_get_size(game->vertex_shader_work),
		.fragment_shader_data = fs_work_get_buffer(game->fragment_shader_work),
		.fragment_shader_size = fs_work_get_size(game->fragment_shader_work),
		.uniform_buffer_count = 1,
	};

	game->vertex_shader_work = fs_read(game->fs, "shaders/randomCube.vert", game->heap, false, false);
	game->fragment_shader_work = fs_read(game->fs, "shaders/randomCube.frag", game->heap, false, false);
	game->traffic_shader = (gpu_shader_info_t)
	{
		.vertex_shader_data = fs_work_get_buffer(game->vertex_shader_work),
		.vertex_shader_size = fs_work_get_size(game->vertex_shader_work),
		.fragment_shader_data = fs_work_get_buffer(game->fragment_shader_work),
		.fragment_shader_size = fs_work_get_size(game->fragment_shader_work),
		.uniform_buffer_count = 1,
	};

	static vec3f_t cube_verts[] =
	{
		{ -1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f,  1.0f },
		{  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f,  1.0f },
		{  1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f,  0.0f },
		{ -1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f,  0.0f },
		{ -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f,  0.0f },
		{  1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f,  1.0f },
		{  1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f,  1.0f },
		{ -1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f,  0.0f },
	};
	static uint16_t cube_indices[] =
	{
		0, 1, 2,
		2, 3, 0,
		1, 5, 6,
		6, 2, 1,
		7, 6, 5,
		5, 4, 7,
		4, 0, 3,
		3, 7, 4,
		4, 5, 1,
		1, 0, 4,
		3, 2, 6,
		6, 7, 3
	};
	game->cube_mesh = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = cube_verts,
		.vertex_data_size = sizeof(cube_verts),
		.index_data = cube_indices,
		.index_data_size = sizeof(cube_indices),
	};
}

static void unload_resources(frogger_game_t* game)
{
	fs_work_destroy(game->fragment_shader_work);
	fs_work_destroy(game->vertex_shader_work);
}

static void spawn_player(frogger_game_t* game, int index)
{
	uint64_t k_player_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->player_type) |
		(1ULL << game->name_type);
	game->player_ent = ecs_entity_add(game->ecs, k_player_ent_mask);

	transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->transform_type, true);
	transform_identity(&transform_comp->transform);

	float player_speed;
	gpu_shader_info_t* shader;
	if (index == 0) 
	{
		transform_comp->transform.translation.z = -15.0f;
		name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->name_type, true);
		strcpy_s(name_comp->name, sizeof(name_comp->name), "player");
		player_speed = 5.0f;
		shader = &game->cube_shader;
	}
	else
	{
		transform_comp->transform.translation.z = -15.0f + (1 + (index-1) / game->num_traffic) * (25.0f / game->num_lines);
		transform_comp->transform.translation.y = -37.5f + (1 + (index-1) % game->num_traffic) * 5.0f;
		transform_comp->transform.scale.y = (rand() / (float)RAND_MAX) * (game->difficulty * 2.5f) + 1;

		name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->name_type, true);
		strcpy_s(name_comp->name, sizeof(name_comp->name), "traffic");
		player_speed = (float) (game->difficulty * 5 + rand() % 5);
		shader = &game->traffic_shader;
	}

	player_component_t* player_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->player_type, true);
	player_comp->index = index;
	player_comp->speed = player_speed;

	model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->model_type, true);
	model_comp->mesh_info = &game->cube_mesh;
	model_comp->shader_info = shader;
}

static void spawn_camera(frogger_game_t* game)
{
	uint64_t k_camera_ent_mask =
		(1ULL << game->camera_type) |
		(1ULL << game->name_type);
	game->camera_ent = ecs_entity_add(game->ecs, k_camera_ent_mask);

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "camera");

	camera_component_t* camera_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->camera_type, true);
}

static void update_players(frogger_game_t* game, engine_info_t* engine_info)
{
	float dt = (float)timer_object_get_delta_ms(game->timer) * 0.001f;

	uint32_t key_mask = wm_get_key_mask(game->window);

	uint64_t k_query_mask = (1ULL << game->transform_type) | (1ULL << game->player_type);

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query))
	{
		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
		player_component_t* player_comp = ecs_query_get_component(game->ecs, &query, game->player_type);
		name_component_t* name_comp = ecs_query_get_component(game->ecs, &query, game->name_type);

		if (!strcmp(name_comp->name, "player"))
		{
			transform_t move;
			transform_identity(&move);

			if (key_mask & k_key_up)
			{
				move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), engine_info->playerSpeed*dt));
				if (transform_comp->transform.translation.z > 15.0f)
				{
					transform_comp->transform.translation.z = -15.0f;
				}
			}
			if (key_mask & k_key_down)
			{
				if (transform_comp->transform.translation.z > -15.0f)
				{
					move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), -engine_info->playerSpeed*dt));
				}
			}
			if (key_mask & k_key_left)
			{
				move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), -engine_info->playerSpeed*dt));
			}
			if (key_mask & k_key_right)
			{
				move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), engine_info->playerSpeed*dt));
			}

			transform_multiply(&transform_comp->transform, &move);

			for (ecs_query_t query_collision = ecs_query_create(game->ecs, k_query_mask);
				ecs_query_is_valid(game->ecs, &query_collision);
				ecs_query_next(game->ecs, &query_collision))
			{
				transform_component_t* other_transform_comp = ecs_query_get_component(game->ecs, &query_collision, game->transform_type);
				player_component_t* other_player_comp = ecs_query_get_component(game->ecs, &query_collision, game->player_type);
				name_component_t* other_name_comp = ecs_query_get_component(game->ecs, &query_collision, game->name_type);

				transform_identity(&move);
				if (!strcmp(other_name_comp->name, "traffic"))
				{
					if (transform_comp->transform.translation.z > other_transform_comp->transform.translation.z-2.0f 
						&& transform_comp->transform.translation.z < other_transform_comp->transform.translation.z+2.0f
						&& transform_comp->transform.translation.y > other_transform_comp->transform.translation.y - 2.0f - (other_transform_comp->transform.scale.y-1)
						&& transform_comp->transform.translation.y < other_transform_comp->transform.translation.y + 2.0f + (other_transform_comp->transform.scale.y-1)
						)
					{
						transform_comp->transform.translation.z = -15.0f;
					}
				}
			}
		}
		else 
		{
			transform_t move;
			transform_identity(&move);

			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), player_comp->speed * dt));
			if (transform_comp->transform.translation.y > 37.5f)
			{
				move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), -75.0f));
			}

			transform_multiply(&transform_comp->transform, &move);
		}
	}
}

static void update_camera(frogger_game_t* game, engine_info_t* engine_info)
{
	uint64_t k_camera_query_mask = (1ULL << game->camera_type);
	for (ecs_query_t camera_query = ecs_query_create(game->ecs, k_camera_query_mask);
		ecs_query_is_valid(game->ecs, &camera_query);
		ecs_query_next(game->ecs, &camera_query))
	{
		camera_component_t* camera_comp = ecs_query_get_component(game->ecs, &camera_query, game->camera_type);

		if (engine_info->orthoView)
		{
			mat4f_make_ortho(&camera_comp->projection, 9.0f / 16.0f * engine_info->viewDistance, -9.0f / 16.0f * engine_info->viewDistance, -engine_info->viewDistance, engine_info->viewDistance, -1000.0f, 1000.0f);

			vec3f_t eye_pos = vec3f_add(vec3f_scale(vec3f_y(), -engine_info->horizontalPan),
										vec3f_scale(vec3f_z(), -engine_info->verticalPan));
			vec3f_t forward = vec3f_forward();
			vec3f_t up = vec3f_up();

			mat4f_make_lookat(&camera_comp->view, &eye_pos, &forward, &up);
		}
		else
		{
			mat4f_make_perspective(&camera_comp->projection, (float)M_PI / 2.0f, 16.0f / 9.0f, 0.1f, 100.0f);

			vec3f_t eye_pos = vec3f_add((vec3f_add(vec3f_scale(vec3f_forward(), engine_info->viewDistanceP), 
										 vec3f_scale(vec3f_y(), engine_info->horizontalPanP))),
										 vec3f_scale(vec3f_z(), engine_info->verticalPanP));

			// Update the "front" and "up" vector based on yaw and pitch and roll
			vec3f_t forward = vec3f_forward_euler(engine_info->yaw, engine_info->pitch, engine_info->roll);
			vec3f_t up = vec3f_up_euler(engine_info->yaw, engine_info->pitch, engine_info->roll);
			mat4f_make_lookat(&camera_comp->view, &eye_pos, &forward, &up);
		}
	}
}

static void draw_models(frogger_game_t* game, engine_info_t* engine_info)
{
	uint64_t k_camera_query_mask = (1ULL << game->camera_type);
	for (ecs_query_t camera_query = ecs_query_create(game->ecs, k_camera_query_mask);
		ecs_query_is_valid(game->ecs, &camera_query);
		ecs_query_next(game->ecs, &camera_query))
	{
		camera_component_t* camera_comp = ecs_query_get_component(game->ecs, &camera_query, game->camera_type);

		uint64_t k_model_query_mask = (1ULL << game->transform_type) | (1ULL << game->model_type);
		for (ecs_query_t query = ecs_query_create(game->ecs, k_model_query_mask);
			ecs_query_is_valid(game->ecs, &query);
			ecs_query_next(game->ecs, &query))
		{
			transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
			model_component_t* model_comp = ecs_query_get_component(game->ecs, &query, game->model_type);
			ecs_entity_ref_t entity_ref = ecs_query_get_entity(game->ecs, &query);

			struct
			{
				mat4f_t projection;
				mat4f_t model;
				mat4f_t view;
				ImVec4 color;
			} uniform_data;
			uniform_data.projection = camera_comp->projection;
			uniform_data.view = camera_comp->view;
			uniform_data.color = engine_info->playerColor;
			transform_to_matrix(&transform_comp->transform, &uniform_data.model);
			gpu_uniform_buffer_info_t uniform_info = { .data = &uniform_data, sizeof(uniform_data) };

			render_push_model(game->render, &entity_ref, model_comp->mesh_info, model_comp->shader_info, &uniform_info);
		}
	}
}
