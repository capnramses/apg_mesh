//
// .apg Viewer in OpenGL 2.1
// Anton Gerdelan
// antongerdelan.net
// First version 3 Jan 2014
// Revised: 26 Dec 2014
// uses the Assimp asset importer library http://assimp.sourceforge.net/
//

#ifndef _MESH_LOADER_H_
#define _MESH_LOADER_H_

#define MESH_VAO_VP_AND_BONES 0
#define MESH_VAO_VP_VT_AND_BONES 1
#define MESH_VAO_VP_VN_VT_AND_BONES 2
#define MESH_VAO_VP_VN_VT 3
#define MESH_VAO_VP_VT 4
#define MAX_VERTICES 65536

/* INSTRUCTIONS

1. loading a mesh.

* set up shader first and get attrib locations

unsigned int mesh_index = load_managed_mesh (
	const char* file_name,
	bool has_vp,
	bool has_vn,
	bool has_vt,
	bool has_bones,
	int vp_loc,
	int vn_loc,
	int vt_loc,
	int vbones_loc
);

unsigned int my_vao = g_loaded_meshes[mesh_index].vao;

* alternatively, create specific vao from g_loaded_meshes[mesh_index] vbos

2. updating an animation

double duration = g_loaded_meshes[mesh_index].duration;
double time_into_anim = ...;
calc_mesh_animation (g_loaded_meshes[mesh_index], time_into_anim);
for (int i = 0; i < g_loaded_meshes[mesh_index].bone_count; i++) {
	my_bones[i] = g_loaded_meshes[mesh_index].current_bone_transforms[i];
}

3. rendering a mesh

* update shader uniforms for array of my_bones[]
* bind my_vao
* draw g_loaded_meshes[mesh_index].point_count
*/

#include "maths_funcs.hpp"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// DER: The three includes below are apparently for an old version of assimp. changed it must be.

/*
#include <assimp/assimp.h> // C++ importer interface
#include <assimp/aiScene.h> // Output data structure
#include <assimp/aiPostProcess.h> // Post processing flags
*/

#include <vector>

// must correspond to max array size in shader
#define MAX_BONES 32
// this doesn't have any real size limit except the total number of bones
#define MAX_CHILDREN 32

struct Anim_Node;

// a position or scale key-frame for an Anim_Node
struct pos_key {
	double time;
	vec3 v;
};

// a rotation key-frame for an Anim_Node. uses quaternions
struct rot_key {
	double time;
	versor q;
};

// an animated joint in the armature. may or may not be weighted to a bone
struct Anim_Node {
	Anim_Node ();
	
	std::vector<pos_key> pos_keyframes;
	std::vector<pos_key> scale_keyframes;
	std::vector<rot_key> rot_keyframes;
	int id;
	int num_children;
	int bone_index;
	bool has_bone;
	Anim_Node* children[MAX_CHILDREN];
};

struct Anim_Node_Name_Map {
	Anim_Node_Name_Map ();
	
	aiString ass_name;
	Anim_Node* anim_node_ptr;
};

// a mesh loaded from a file
struct Mesh {
	Mesh ();
	
	std::string file_name;
	std::vector<Anim_Node_Name_Map> node_name_map;
	std::vector<aiString> bone_names;
	mat4 root_transform;
	mat4 bone_offset_mats[MAX_BONES];
	mat4 current_bone_transforms[MAX_BONES];
	Anim_Node* root_node;
	double anim_duration;
	/*unsigned int vao;
	unsigned int points_vbo;
	unsigned int normals_vbo;
	unsigned int st_vbo;
	unsigned int tangents_vbo;
	unsigned int bone_ids_vbo;*/
	
	std::vector<float> vps, vts, vns, vtangents;
	int vbone_ids[MAX_VERTICES];
	
	unsigned int point_count;
	unsigned int bone_count;
	unsigned int anim_count;
	unsigned int anim_node_count;
};

// load mesh. generates VBOs but not VAO
Mesh load_mesh (const char* file_name, bool correct_coords);

void calc_mesh_animation (Mesh& mesh, double anim_time);

// loads a mesh if not already loaded. gets index
unsigned int load_managed_mesh (
	const char* file_name,
	bool has_vp,
	bool has_vn,
	bool has_vt,
	bool has_vtangents,
	bool has_bones,
	int vp_loc,
	int vn_loc,
	int vt_loc,
	int vtangents_loc,
	int vbones_loc
);

extern std::vector<Mesh> g_loaded_meshes;

#endif
