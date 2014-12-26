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

#include "maths_funcs.hpp"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
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
	
	std::vector<float> vps, vts, vns, vtangents;
	int vbone_ids[MAX_VERTICES];
	
	unsigned int point_count;
	unsigned int bone_count;
	unsigned int anim_count;
	unsigned int anim_node_count;
};

// load mesh. generates VBOs but not VAO
Mesh load_mesh (const char* file_name, bool correct_coords);

#endif
