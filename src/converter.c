//
// .apg Viewer in OpenGL 2.1
// Anton Gerdelan
// antongerdelan.net
// First version 3 Jan 2014
// Revised: 26 Dec 2014
// uses the Assimp asset importer library http://assimp.sourceforge.net/
//

#include "mesh_loader.hpp"
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#define VERSION "26DEC2014"

/* TODO
root transform for anim or whole mesh? think anim
*/

/*struct Animation {
	double duration;
};*/

Mesh mesh;
//Animation* animations = NULL;
char** my_argv;
char output_file_name[2048];
int vertex_count;
int bone_count;
int animation_count;
int vp_comps = 3; // dimensionality. 3 is xyz, 2 is xy
int vn_comps = 3; // dimensionality. 3 is xyz, 2 is xy
int vt_comps = 2; // dimensionality. 2 is st
int vb_comps = 1; // dimensionality. 1 is 1 bone per vertex
int vw_comps = 1; // dimensionality. 1 is 1 bone per vertex
int offs_mat_comps = 16; // 4x4
int vtan_comps = 4; // x y z det
int my_argc;
bool has_vp; // positions
bool has_vn; // normals
bool has_vt; // texture coords
bool has_vb; // bone indices
bool has_vw; // bone weights
bool has_vtan; // tangents
bool has_vbitan; // bi-tangents
bool has_skeleton;
bool bin_mode; // binary write mode

void count_pos_keys (Anim_Node* node, int& keys, double& duration);
void count_rot_keys (Anim_Node* node, int& keys, double& duration);
void print_hierarchy (FILE* f, Anim_Node* node, int parent_id);
void print_tra_keys (FILE* f, Anim_Node* node);
void print_rot_keys (FILE* f, Anim_Node* node);

void count_pos_keys (Anim_Node* node, int& keys, double& duration) {
	int i;
	
	if (node->pos_keyframes.size () > 0) {
		double longest_t;
		keys += (int)node->pos_keyframes.size ();
		longest_t = node->pos_keyframes[
			node->pos_keyframes.size () - 1].time;
		if (longest_t > duration) {
			duration = longest_t;
		}
	}
	for (i = 0; i < node->num_children; i++) {
		count_pos_keys (node->children[i], keys, duration);
	}
}

void count_rot_keys (Anim_Node* node, int& keys, double& duration) {
	int i;
	
	if (node->pos_keyframes.size () > 0) {
		double longest_t;
		keys += (int)node->pos_keyframes.size ();
		longest_t = node->pos_keyframes[
			node->pos_keyframes.size () - 1].time;
		if (longest_t > duration) {
			duration = longest_t;
		}
	}
	for (i = 0; i < node->num_children; i++) {
		count_rot_keys (node->children[i], keys, duration);
	}
}

void print_hierarchy (FILE* f, Anim_Node* node, int parent_id) {
	int i;
	
	if (bin_mode) {
		fwrite (&parent_id, sizeof (int), 1, f);
		fwrite (&node->bone_index, sizeof (int), 1, f);
	} else {
		fprintf (f, "parent %i bone_id %i\n", parent_id, node->bone_index);
	}
	for (i = 0; i < node->num_children; i++) {
		print_hierarchy (f, node->children[i], node->id);
	}
}

void print_tra_keys (FILE* f, Anim_Node* node) {
	int comps = 3;
	int count = 0;
	int i;
	
	count = (int)node->pos_keyframes.size ();
	if (count > 0) {
		if (bin_mode) {
			char c = 't';
			fwrite (&c, 1, 1, f);
			fwrite (&node->id, sizeof (int), 1, f);
			fwrite (&count, sizeof (int), 1, f);
		} else {
			fprintf (f, "@tra_keys node %i count %i comps %i\n", node->id, count, comps);
		}
		for (i = 0; i < count; i++) {
			if (bin_mode) {
				fwrite (&node->pos_keyframes[i].time, sizeof (float), 1, f);
				fwrite (node->pos_keyframes[i].v.v, sizeof (float), 3, f);
			} else {
				fprintf (
					f,
					"t %f TRA %f %f %f\n",
					node->pos_keyframes[i].time,
					node->pos_keyframes[i].v.v[0],
					node->pos_keyframes[i].v.v[1],
					node->pos_keyframes[i].v.v[2]
				);
			}
		}
	}
	for (i = 0; i < node->num_children; i++) {
		print_tra_keys (f, node->children[i]);
	}
}

void print_rot_keys (FILE* f, Anim_Node* node) {
	int comps = 4;
	int count = 0;
	int i;
	
	count = (int)node->rot_keyframes.size ();
	if (count > 0) {
		if (bin_mode) {
			char c = 'r';
			fwrite (&c, 1, 1, f);
			fwrite (&node->id, sizeof (int), 1, f);
			fwrite (&count, sizeof (int), 1, f);
		} else {
			fprintf (f, "@rot_keys node %i count %i comps %i\n", node->id, count, comps);
		}
		for (i = 0; i < count; i++) {
			if (bin_mode) {
				fwrite (&node->pos_keyframes[i].time, sizeof (float), 1, f);
				fwrite (node->rot_keyframes[i].q.q, sizeof (float), 4, f);
			} else {
				fprintf (
					f,
					"t %f ROT %f %f %f %f\n",
					node->rot_keyframes[i].time,
					node->rot_keyframes[i].q.q[0],
					node->rot_keyframes[i].q.q[1],
					node->rot_keyframes[i].q.q[2],
					node->rot_keyframes[i].q.q[3]
				);
			}
		}
	}
	
	for (i = 0; i < node->num_children; i++) {
		print_rot_keys (f, node->children[i]);
	}
}

bool write_output (const char* file_name) {
	FILE* f = NULL;
	Anim_Node* root_node = NULL;
	int i, j;
	
	printf ("ASCII write mode\n");
	f = fopen (file_name, "w");
	if (!f) {
		fprintf (stderr, "ERROR: opening file %s for writing\n", file_name);
		return false;
	}
	
	fprintf (f, "@Anton's custom mesh format v.%s http://antongerdelan.net/\n",
		 VERSION);
	fprintf (f, "@vert_count %i\n", vertex_count);
	if (has_vp) {
		fprintf (f, "@vp comps %i\n", vp_comps);
		for (i = 0; i < vertex_count * vp_comps; i++) {
			if (i % vp_comps != 0) {
				fprintf (f, " ");
			}
			//
			// assuming units in meters - prints to the centimeter
			fprintf (f, "%.2f", mesh.vps[i]);
			if (i % vp_comps == vp_comps - 1) {
				fprintf (f, "\n");
			}
		}
	}
	if (has_vn) {
		fprintf (f, "@vn comps %i\n", vn_comps);
		for (i = 0; i < vertex_count * vn_comps; i ++) {
			if (i % vn_comps != 0) {
				fprintf (f, " ");
			}
			//
			// 3 s.f. - normalised later anyway
			fprintf (f, "%.3g", mesh.vns[i]);
			if (i % vn_comps == vn_comps - 1) {
				fprintf (f, "\n");
			}
		}
	}
	if (has_vt) {
		fprintf (f, "@vt comps %i\n", vt_comps);
		for (i = 0; i < vertex_count * vt_comps; i ++) {
			if (i % vt_comps != 0) {
				fprintf (f, " ");
			}
			//
			// 3 s.f. - normalised later anyway
			fprintf (f, "%.3g", mesh.vts[i]);
			if (i % vt_comps == vt_comps - 1) {
				fprintf (f, "\n");
			}
		}
	}
	if (has_vtan) {
		fprintf (f, "@vtan comps %i\n", vtan_comps);
		for (i = 0; i < vertex_count * vtan_comps; i ++) {
			float ttan;
			
			//
			// make sure is not going to print "nan" due to assimp error
			ttan = mesh.vtangents[i];
			if (isnan(ttan)) {
				ttan = 0.0f;
			}
			
			if (i % vtan_comps != 0) {
				fprintf (f, " ");
			}
			//
			// 3 s.f. - normalised later anyway
			fprintf (f, "%.3g", ttan);
			if (i % vtan_comps == vtan_comps - 1) {
				fprintf (f, "\n");
			}
		}
	}
	if (has_vb) {
		fprintf (f, "@vb comps %i\n", vb_comps);
		for (i = 0; i < vertex_count; i++) {
			fprintf (f, "%i\n", mesh.vbone_ids[i]);
		}
	}
	if (has_vw) {
		fprintf (f, "@vw comps %i\n", vw_comps);
		for (i = 0; i < vertex_count; i++) {
			float bone_weight = 0.0f;
			fprintf (f, "%f\n", bone_weight);
		}
	}
	if (has_skeleton) {
		fprintf (
			f, "@skeleton bones %i animations %i\n", bone_count, animation_count);
		
		fprintf (f, "@root_transform comps %i\n", offs_mat_comps);
		for (j = 0; j < offs_mat_comps; j++) {
			if (0 != j) {
				fprintf (f, " ");
			}
			fprintf (f, "%f", mesh.root_transform.m[j]);
		}
		fprintf (f, "\n");
		fprintf (f, "@offset_mat comps %i\n", offs_mat_comps);
		for (i = 0; i < bone_count; i++) {
			// column-order
			for (j = 0; j < offs_mat_comps; j++) {
				if (0 != j) {
					fprintf (f, " ");
				}
				fprintf (f, "%f", mesh.bone_offset_mats[i].m[j]);
			}
			fprintf (f, "\n");
		}
		
		fprintf (f, "@hierarchy nodes %i\n", mesh.anim_node_count);
		root_node = mesh.root_node;
		print_hierarchy (f, root_node, -1);
		
		for (i = 0; i < animation_count; i++) {
			double duration = 0.0;
			//int tra_comps = 3;
			int pos_keys = 0;
			//int rot_comps = 4;
			int rot_keys = 0;
			
			count_pos_keys (root_node, pos_keys, duration);
			count_rot_keys (root_node, rot_keys, duration);
			
			fprintf (f, "@animation name TODO duration %f\n", duration);
			print_tra_keys (f, root_node);
			print_rot_keys (f, root_node);
		}
	}
	
	fclose (f);
	return true;
}

//
// TODO
// probably needs count before vn vp vt etc? in case of 0. or make mandatory
// obj-like format would prob be better for points?
bool write_output_bin (const char* file_name) {
	FILE* f = NULL;
	Anim_Node* root_node = NULL;
	int i, j;
	char hdr[32];
	
	printf ("binary write mode\n");
	f = fopen (file_name, "wb");
	if (!f) {
		fprintf (stderr, "ERROR: opening file %s for writing\n", file_name);
		return false;
	}
	
	//
	// write header
	sprintf (hdr, "BINAPGv%s", VERSION);
	fwrite (hdr, 1, 256, f);
	fwrite (&vertex_count, sizeof (int), 1, f);
	for (i = 0; i < vertex_count * vp_comps; i++) {
		fwrite (&mesh.vps[i], sizeof (float), 1, f);
	}
	for (i = 0; i < vertex_count * vn_comps; i ++) {
		fwrite (&mesh.vns[i], sizeof (float), 1, f);
	}
	for (i = 0; i < vertex_count * vt_comps; i ++) {
		fwrite (&mesh.vts[i], sizeof (float), 1, f);
	}
	fwrite (&vtan_comps, sizeof (int), 1, f);
	for (i = 0; i < vertex_count * vtan_comps; i ++) {
		fwrite (&mesh.vtangents[i], sizeof (float), 1, f);
	}
	for (i = 0; i < vertex_count; i++) {
		fwrite (&mesh.vbone_ids[i], sizeof (int), 1, f);
	}
	for (i = 0; i < vertex_count; i++) {
		float bone_weight = 0.0f;
		fwrite (&bone_weight, sizeof (float), 1, f);
	}
	fwrite (&bone_count, sizeof (int), 1, f);
	fwrite (&animation_count, sizeof (int), 1, f);
	fwrite (&offs_mat_comps, sizeof (int), 1, f);
	for (j = 0; j < offs_mat_comps; j++) {
		fwrite (&mesh.root_transform.m[j], sizeof (float), 1, f);
	}
	fwrite (&offs_mat_comps, sizeof (int), 1, f);
	for (i = 0; i < bone_count; i++) {
		// column-order
		for (j = 0; j < offs_mat_comps; j++) {
			fwrite (&mesh.bone_offset_mats[i].m[j], sizeof (float), 1, f);
		}
	}
	fwrite (&mesh.anim_node_count, sizeof (int), 1, f);
	root_node = mesh.root_node;
	print_hierarchy (f, root_node, -1);
		
	for (i = 0; i < animation_count; i++) {
		double duration = 0.0;
		//int tra_comps = 3;
		int pos_keys = 0;
		//int rot_comps = 4;
		int rot_keys = 0;
		
		count_pos_keys (root_node, pos_keys, duration);
		count_rot_keys (root_node, rot_keys, duration);
		
		fwrite (&duration, sizeof (float), 1, f);
		print_tra_keys (f, root_node);
		print_rot_keys (f, root_node);
	}
	
	fclose (f);
	return true;
}

bool read_input (const char* file_name) {
	// load mesh using assimp
	bool correct_coords = true;
	int len;
	
	len = strlen (file_name);
	if ('o' == file_name[len - 3] && 'b' == file_name[len - 2] && 'j' == file_name[len - 1]) {
		printf (".obj found, not correcting coordinate system...\n");
		correct_coords = false;
	}
	mesh = load_mesh (file_name, correct_coords);
	
	// set global variables to be used for output
	vertex_count = mesh.point_count;
	bone_count = mesh.bone_count;
	if (mesh.vps.size () > 0) {
		printf ("positions found\n");
		has_vp = true;
	}
	if (mesh.vns.size () > 0) {
		printf ("normals found\n");
		has_vn = true;
	}
	if (mesh.vts.size () > 0) {
		printf ("texcoords found\n");
		has_vt = true;
	}
	if (mesh.vtangents.size () > 0) {
		printf ("tangents found\n");
		has_vtan = true;
	}
	if (bone_count > 0) {
		has_vb = true;
		has_skeleton = true;
	}
	animation_count = mesh.anim_count;
	/*animations = (Animation*)malloc (animation_count * sizeof (Animation));
	for (int i = 0; i < animation_count; i++) {
		if (i > 0) {
			fprintf (stderr, "WARNING: assimp loader does not store unique duration "
				"for each animation\n");
		}
		animations[i].duration = mesh.anim_duration;
	}*/
	return true;
}

//
// check if argument is in command line and if so return argc index
// else return -1
// i stole this from the DOOM source code
int check_arg (const char* str) {
	int i;
	for (i = 0; i < my_argc; i++) {
		if (strcmp (str, my_argv[i]) == 0) {
			return i;
		}
	}
	return -1;
}

int main (int argc, char** argv) {
	int a = -1;
	
	my_argc = argc;
	my_argv = argv;
	if (argc < 2) {
		printf ("usage: ./conv INPUT_FILE [-o OUTPUT_FILE] [-bin]\n");
		return 0;
	}
	if (check_arg ("-help") > -1) {
		printf (
			"usage: ./conv INPUT_FILE [-o OUTPUT_FILE] [-bin]\n"
			"  -o specify output file. default changes extension to .apg\n"
			"  -bin creates binary version of .apg. default is ASCII text-based\n"
			"example: ./conv skull.obj -o skull.apg -bin\n"
		);
		return 0;
	}
	a = check_arg ("-o");
	if (a > -1) {
		assert (argc > a + 1);
		strcpy (output_file_name, my_argv[a + 1]);
	} else {
		strcpy (output_file_name, argv[1]);
		output_file_name[strlen (argv[1]) - 4] = '\0';
		strcat (output_file_name, ".apg");
	}
	if (check_arg ("-bin") > -1) {
		bin_mode = true;
	}
	
	printf ("converting %s to %s\n", argv[1], output_file_name);
	
	assert (read_input (argv[1]));
	if (bin_mode) {
		assert (write_output_bin (output_file_name));
	} else {
		assert (write_output (output_file_name));
	}
	/*if (animations) {
		free (animations);
		animations = NULL;
	}*/
	return 0;
}
