//
// .apg Viewer in OpenGL 2.1
// Anton Gerdelan
// antongerdelan.net
// First version 3 Jan 2014
// Revised: 26 Dec 2014
// uses the Assimp asset importer library http://assimp.sourceforge.net/
//
#include "maths_funcs.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

int width = 800;
int height = 800;
GLFWwindow* window;
bool cam_dirty = true;

GLuint shader_programme;
GLint params = -1;
GLint P_loc = -1;
GLint V_loc = -1;

GLuint no_skin_shader_programme;
GLint no_skin_P_loc = -1;
GLint no_skin_V_loc = -1;

#define MAX_BONES 32
GLint B_locs[MAX_BONES];
GLuint psp;
GLint pP_loc = -1;
GLint pV_loc = -1;
GLint pM_loc = -1;

#define MAX_ANIM_NAME_LEN 64
struct TraAnimKey {
	vec3 tra;
	double time;
};
struct RotAnimKey {
	versor rot;
	double time;
};
struct Channel {
	TraAnimKey* tra_keys;
	int tra_keys_count;
	RotAnimKey* rot_keys;
	int rot_keys_count;
};
// an animation to be played using the skeleton hierarchy
struct Animation {
	char name[MAX_ANIM_NAME_LEN];
	double duration;
	// mem order of channels corresponds to anim nodes in hierarchy
	Channel* channels;
	int num_channels;
};

Animation* animations = NULL;
int animation_count = 0;
// the skeleton hierarchy
mat4 root_transform_mat;
mat4* offset_mats = NULL;
mat4* current_bone_mats = NULL;
int* anim_node_parents = NULL;
int anim_node_children[MAX_BONES][MAX_BONES];
int anim_node_num_children[MAX_BONES];
int* anim_node_bone_ids = NULL;
int bone_count = 0;
// vertex mesh
GLuint vao = 0;
int vert_count = 0;

void print_all_keys () {
	for (int i = 0; i < animation_count; i++) {
		printf ("animation %i:\n", i);
		for (int j = 0; j < animations[i].num_channels; j++) {
			printf (" a%ichannel %i:\n", i, j);
			for (int k = 0; k < animations[i].channels[j].tra_keys_count; k++) {
				printf ("  a%ic%i tra_key %i\n", i, j, k);
				printf ("t %f\n", animations[i].channels[j].tra_keys[k].time);
				print (animations[i].channels[j].tra_keys[k].tra);
			}
			for (int k = 0; k < animations[i].channels[j].rot_keys_count; k++) {
				printf ("  a%ic%i rot_key %i\n", i, j, k);
				printf ("t %f\n", animations[i].channels[j].rot_keys[k].time);
				print (animations[i].channels[j].rot_keys[k].rot);
			}
		}
	}
}

void _recurse_anim_tree (
	Animation* animations,
	double anim_time,
	int my_anim_node,
	mat4 parent_mat
);

void _recurse_anim_tree (
	Animation* animations,
	double anim_time,
	int my_anim_node,
	mat4 parent_mat
) {
	mat4 trans_mat, rot_mat, node_mat, global_trans_mat;

	if (!animations) {
		return;
	}

	trans_mat = identity_mat4 ();
	rot_mat = identity_mat4 ();
	// position
	if (animations->channels[my_anim_node].tra_keys_count > 1) {
		int previous_frame_index = 0;
		int next_frame_index = 1;
		double prev_t, next_t, t_factor;
		vec3 vf, vi, p;
		
		// work out previous frame and next frame numbers
		for (int i = 0; i <
			animations->channels[my_anim_node].tra_keys_count - 1; i++) {
			double t;
			
			t = animations->channels[my_anim_node].tra_keys[i].time;
			if (t >= anim_time) {
				break;
			}
			previous_frame_index = i;
			next_frame_index = i + 1;
		}
		// get prev and next pos and time
		prev_t =
			animations->channels[my_anim_node].tra_keys[previous_frame_index].time;
		next_t =
			animations->channels[my_anim_node].tra_keys[next_frame_index].time;
		t_factor = (anim_time - prev_t) / (next_t - prev_t);
		// interp position
		vf = animations->channels[my_anim_node].tra_keys[next_frame_index].tra;
		vi = animations->channels[my_anim_node].tra_keys[previous_frame_index].tra;
		p = vf * t_factor + vi * (1.0 - t_factor);
		trans_mat = translate (identity_mat4 (), p);
	} else if (1 == animations->channels[my_anim_node].tra_keys_count) {
		vec3 vf;
		
		vf = animations->channels[my_anim_node].tra_keys[0].tra;
		trans_mat = translate (identity_mat4 (), vf);
	}
	
	// interp rotation
	if (animations->channels[my_anim_node].rot_keys_count > 1) {
		int previous_frame_index = 0;
		int next_frame_index = 1;
		double prev_t, next_t, t_factor;
		versor qf, qi, slerped;

		for (int i = 0; i <
			animations->channels[my_anim_node].rot_keys_count - 1; i++) {
			double t;
			
			t = animations->channels[my_anim_node].rot_keys[i].time;
			if (t >= anim_time) {
				break;
			}
			previous_frame_index = i;
			next_frame_index = i + 1;
		}
		// get prev and next pos and time
		prev_t =
			animations->channels[my_anim_node].rot_keys[previous_frame_index].time;
		next_t =
			animations->channels[my_anim_node].rot_keys[next_frame_index].time;
		t_factor = (anim_time - prev_t) / (next_t - prev_t);
		// get the two quaternions
		qf = animations->channels[my_anim_node].rot_keys[next_frame_index].rot;
		qi = animations->channels[my_anim_node].rot_keys[previous_frame_index].rot;
		slerped = slerp (qi, qf, t_factor);
		rot_mat = quat_to_mat4 (slerped);
	} else if (1 == animations->channels[my_anim_node].rot_keys_count) {
		versor qf;
		
		qf = animations->channels[my_anim_node].rot_keys[0].rot;
		rot_mat = quat_to_mat4 (qf);
	}
	
	node_mat = trans_mat * rot_mat; // * scale mat at end
	global_trans_mat = parent_mat * node_mat;

	// update bone mats if bone linked to this node
	if (anim_node_bone_ids[my_anim_node] > -1) {
		current_bone_mats[anim_node_bone_ids[my_anim_node]] =
			root_transform_mat * global_trans_mat *
			offset_mats[anim_node_bone_ids[my_anim_node]];
	}
	//print (offset_mats[1]);

	// transform children
	for (int i = 0 ; i < anim_node_num_children[my_anim_node]; i++) {
		_recurse_anim_tree (
			animations,
			anim_time,
			anim_node_children[my_anim_node][i],
			global_trans_mat
		);
	}
}

bool load_mesh (const char* file_name) {
	FILE* f;
	float* vps = NULL;
	float* vns = NULL;
	float* vts = NULL;
	float* vtans = NULL;
	float* vbs = NULL; // HACK: as float cos i don't trust GL with ints
	int vp_comps = 0;
	int vn_comps = 0;
	int vt_comps = 0;
	int vtan_comps = 0;
	int vb_comps = 0;
	int offset_mat_comps = 0;
	int current_anim_index = -1;
	char line[1024];
	GLuint points_vbo = 0;
	GLuint normals_vbo = 0;
	GLuint texcoords_vbo = 0;
	GLuint bone_ids_vbo = 0;
	
	printf ("loading mesh %s\n", file_name);
	f = fopen (file_name, "r");
	if (!f) {
		fprintf (stderr, "ERROR opening file %s\n", file_name);
		return false;
	}
	
	while (fgets (line, 1024, f)) {
		if ('@' == line[0]) {
			char code_str[32];
			
			sscanf (line, "@%s\n", code_str);
			printf ("code: %s\n", code_str);
			if (strcmp (code_str, "Anton's") == 0) {
			} else if (strcmp (code_str, "vert_count") == 0) {
				sscanf (line, "@vert_count %i\n", &vert_count);
			} else if (strcmp (code_str, "vp") == 0) {
				sscanf (line, "@vp comps %i\n", &vp_comps);
				vps = (float*)malloc (vert_count * vp_comps * sizeof (float));
				for (int i = 0; i < vert_count * vp_comps; i++) {
					fscanf (f, "%f", &vps[i]);
				}
				fscanf (f, "\n");
			} else if (strcmp (code_str, "vn") == 0) {
				sscanf (line, "@vn comps %i\n", &vn_comps);
				vns = (float*)malloc (vert_count * vn_comps * sizeof (float));
				for (int i = 0; i < vert_count * vn_comps; i++) {
					fscanf (f, "%f", &vns[i]);
				}
				fscanf (f, "\n");
			} else if (strcmp (code_str, "vt") == 0) {
				sscanf (line, "@vt comps %i\n", &vt_comps);
				vts = (float*)malloc (vert_count * vt_comps * sizeof (float));
				for (int i = 0; i < vert_count * vt_comps; i++) {
					fscanf (f, "%f", &vts[i]);
				}
				fscanf (f, "\n");
			} else if (strcmp (code_str, "vtan") == 0) {
				sscanf (line, "@vtan comps %i\n", &vtan_comps);
				vtans = (float*)malloc (vert_count * vtan_comps * sizeof (float));
				for (int i = 0; i < vert_count * vtan_comps; i++) {
					fscanf (f, "%f", &vtans[i]);
				}
				fscanf (f, "\n");
			} else if (strcmp (code_str, "vb") == 0) {
				sscanf (line, "@vb comps %i\n", &vb_comps);
				vbs = (float*)malloc (vert_count * vb_comps * sizeof (float));
				for (int i = 0; i < vert_count * vb_comps; i++) {
					fscanf (f, "%f", &vbs[i]);
				}
				fscanf (f, "\n");
			//@skeleton bones 2 animations 1
			} else if (strcmp (code_str, "skeleton") == 0) {
				sscanf (
					line,
					"@skeleton bones %i animations %i\n",
					&bone_count, &animation_count
				);
				animations = (Animation*)malloc (animation_count * sizeof (Animation));
				/*
				struct Animation {
					char name[MAX_ANIM_NAME_LEN];
					double duration;
					// mem order of channels corresponds to anim nodes in hierarchy
					Channel* channels;
					int num_channels;
				};
				*/
				for (int i = 0; i < animation_count; i++) {
					animations[i].name[0] = '\0';
					animations[i].duration = 0.0;
					animations[i].channels = NULL;
					animations[i].num_channels = 0;
				}
				
				current_bone_mats = (mat4*)malloc (bone_count * sizeof (mat4));
				for (int i = 0; i < bone_count; i++) {
					current_bone_mats[i] = identity_mat4 ();
				}
			//@root_transform comps 16
			} else if (strcmp (code_str, "root_transform") == 0) {
				int mat_comps = 0;
				
				sscanf (line, "@root_transform comps %i\n", &mat_comps);
				for (int j = 0; j < mat_comps; j++) {
					fscanf (f, "%f", &root_transform_mat.m[j]);
				}
				printf ("root transform mat:");
				print (root_transform_mat);
				fscanf (f, "\n");
			//@offset_mat comps 16
			} else if (strcmp (code_str, "offset_mat") == 0) {
				sscanf (line, "@offset_mat comps %i\n", &offset_mat_comps);
				offset_mats = (mat4*)malloc (bone_count * sizeof (mat4));
				for (int i = 0; i < bone_count; i++) {
					for (int j = 0; j < offset_mat_comps; j++) {
						fscanf (f, "%f", &offset_mats[i].m[j]);
					}
					fscanf (f, "\n");
					//print (offset_mats[i]);
				}
			//@hierarchy nodes 5
			} else if (strcmp (code_str, "hierarchy") == 0) {
				int nodes = 0;
				
				sscanf (line, "@hierarchy nodes %i\n", &nodes);
				anim_node_parents = (int*)malloc (nodes * sizeof (int));
				anim_node_bone_ids = (int*)malloc (nodes * sizeof (int));
				//parent -1 bone_id -1
				for (int i = 0; i < nodes; i++) {
					fscanf (
						f,
						"parent %i bone_id %i\n",
						&anim_node_parents[i], &anim_node_bone_ids[i]
					);
					//printf ("%i %i\n", anim_node_parents[i], anim_node_bone_ids[i]);
				}
				// work out children of each node
				for (int i = 0; i < nodes; i++) {
					for (int j = 0; j < nodes; j++) {
						if (anim_node_parents[j] == i) {
							anim_node_children[i][anim_node_num_children[i]++] = j;
						}
					}
				}
				
				for (int i = 0; i < animation_count; i++) {
					animations[i].channels = (Channel*)malloc (nodes * sizeof (Channel));
					/*
					struct Channel {
						TraAnimKey* tra_keys;
						int tra_keys_count;
						RotAnimKey* rot_keys;
						int rot_keys_count;
					};
					*/
					for (int j = 0; j < nodes; j++) {
						animations[i].channels[j].tra_keys = NULL;
						animations[i].channels[j].tra_keys_count = 0;
						animations[i].channels[j].rot_keys = NULL;
						animations[i].channels[j].rot_keys_count = 0;
					}
					animations[i].num_channels = nodes;
				}
				
			//@animation name TODO duration 0.000000
			} else if (strcmp (code_str, "animation") == 0) {
				current_anim_index++;
				sscanf (
					line,
					"@animation name %s duration %lf\n",
					animations[current_anim_index].name,
					&animations[current_anim_index].duration
				);
				
			//@tra_keys count 12 comps 3
			} else if (strcmp (code_str, "tra_keys") == 0) {
				int node = 0;
				int count = 0;
				int comps = 0;
				
				sscanf (
					line, "@tra_keys node %i count %i comps %i\n", &node, &count, &comps
				);
				animations[current_anim_index].channels[node].tra_keys =
					(TraAnimKey*)malloc (count * sizeof (TraAnimKey));
				/*
				vec3 tra;
				double time;
				*/
				for (int i = 0; i < count; i++) {
					animations[current_anim_index].channels[node].tra_keys[i].tra =
						vec3 (0.0f, 0.0f, 0.0f);
					animations[current_anim_index].channels[node].tra_keys[i].time = 0.0;
				}
				animations[current_anim_index].channels[node].tra_keys_count = count;
				
				//t 0.040000 TRA 0.000000 0.000000 0.000000
				for (int i = 0; i < count; i++) {
					fscanf (f, "t %lf TRA %f %f %f\n",
						&animations[current_anim_index].channels[node].tra_keys[i].time,
						&animations[current_anim_index].channels[node].tra_keys[i].tra.v[0],
						&animations[current_anim_index].channels[node].tra_keys[i].tra.v[1],
						&animations[current_anim_index].channels[node].tra_keys[i].tra.v[2]
					);
					//print (animations[current_anim_index].channels[node].tra_keys[i].tra);
				}
				
			//@rot_keys count 12 comps 4
			} else if (strcmp (code_str, "rot_keys") == 0) {
				int node = 0;
				int count = 0;
				int comps = 0;
				sscanf (
					line, "@rot_keys node %i count %i comps %i\n", &node, &count, &comps
				);
				animations[current_anim_index].channels[node].rot_keys =
					(RotAnimKey*)malloc (count * sizeof (RotAnimKey));
				/*
				versor rot;
				double time;
				*/
				for (int i = 0; i < count; i++) {
					animations[current_anim_index].channels[node].rot_keys[i].rot.q[0] = 0.0f;
					animations[current_anim_index].channels[node].rot_keys[i].rot.q[1] = 0.0f;
					animations[current_anim_index].channels[node].rot_keys[i].rot.q[2] = 0.0f;
					animations[current_anim_index].channels[node].rot_keys[i].rot.q[3] = 0.0f;
					animations[current_anim_index].channels[node].rot_keys[i].time = 0.0;
				}
				animations[current_anim_index].channels[node].rot_keys_count = count;
				//t 0.040000 ROT 0.707107 0.707107 0.000000 0.000000
				for (int i = 0; i < count; i++) {
					fscanf (f, "t %lf ROT %f %f %f %f\n",
						&animations[current_anim_index].channels[node].rot_keys[i].time,
						&animations[current_anim_index].channels[node].rot_keys[i].rot.q[0],
						&animations[current_anim_index].channels[node].rot_keys[i].rot.q[1],
						&animations[current_anim_index].channels[node].rot_keys[i].rot.q[2],
						&animations[current_anim_index].channels[node].rot_keys[i].rot.q[3]
					);
					//print (animations[current_anim_index].channels[node].rot_keys[i].rot);
				} // endfor
			} // endif
		}
	}
	fclose (f);
	
	glGenBuffers (1, &points_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, points_vbo);
	glBufferData (
		GL_ARRAY_BUFFER,
		vp_comps * vert_count * sizeof (GLfloat),
		vps,
		GL_STATIC_DRAW
	);
	
	glGenBuffers (1, &normals_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, normals_vbo);
	glBufferData (
		GL_ARRAY_BUFFER,
		vn_comps * vert_count * sizeof (GLfloat),
		vns,
		GL_STATIC_DRAW
	);
	
	glGenBuffers (1, &texcoords_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, texcoords_vbo);
	glBufferData (
		GL_ARRAY_BUFFER,
		vt_comps * vert_count * sizeof (GLfloat),
		vts,
		GL_STATIC_DRAW
	);
	
	glGenBuffers (1, &bone_ids_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, bone_ids_vbo);
	glBufferData (
		GL_ARRAY_BUFFER,
		vb_comps * vert_count * sizeof (GLfloat),
		vbs,
		GL_STATIC_DRAW
	);
	glGenVertexArrays (1, &vao);
	glBindVertexArray (vao);
	glEnableVertexAttribArray (0);
	glBindBuffer (GL_ARRAY_BUFFER, points_vbo);
	glVertexAttribPointer (0, vp_comps, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray (1);
	glBindBuffer (GL_ARRAY_BUFFER, normals_vbo);
	glVertexAttribPointer (1, vn_comps, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray (2);
	glBindBuffer (GL_ARRAY_BUFFER, texcoords_vbo);
	glVertexAttribPointer (2, vt_comps, GL_FLOAT, GL_FALSE, 0, NULL);
	if (animation_count > 0) {
		glEnableVertexAttribArray (3);
		glBindBuffer (GL_ARRAY_BUFFER, bone_ids_vbo);
		glVertexAttribPointer (3, 1, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	printf ("mesh gpu data created\n");
	
	if (vps) {
		free (vps);
	}
	if (vns) {
		free (vns);
	}
	if (vts) {
		free (vts);
	}
	return true;
}

bool start_gl () {
	//
	// fire up opengl
	if (!glfwInit ()) {
		fprintf (stderr, "ERROR: could not start GLFW3\n");
		return false;
	}
	glfwWindowHint (GLFW_SAMPLES, 16);
	window = glfwCreateWindow (width, height, "Custom Skinned Mesh Format", NULL,
		NULL);
	if (!window) {
		fprintf (stderr, "ERROR: could not open window with GLFW3\n");
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent (window);
	glewExperimental = GL_TRUE;
	glewInit ();
	const GLubyte* renderer = glGetString (GL_RENDERER);
	const GLubyte* version = glGetString (GL_VERSION);
	printf ("Renderer: %s\n", renderer);
	printf ("OpenGL version supported %s\n", version);
	return true;
}

void print_shader_info_log (unsigned int shader_index) {
	int max_length = 2048;
	int actual_length = 0;
	char log[2048];
	glGetShaderInfoLog (shader_index, max_length, &actual_length, log);
	printf ("shader info log for GL index %u\n%s\n", shader_index, log);
}

bool create_shaders () {
	const char* vertex_shader =
	"#version 120\n"
	"attribute vec3 vp, vn;"
	"attribute vec2 vt;"
	"attribute float bone_id;"
	"uniform mat4 P, V, B[32];"
	"uniform mat4 bone_mats[16];"
	"varying vec3 n_eye, p_eye, l_pos_eye;"
	"varying vec2 st;"
	//"flat out float bone_id_f;"
	"varying float bone_id_f;"
	"void main () {"
	"  bone_id_f = bone_id;"
	"  p_eye = (V * vec4 (vp, 1.0)).xyz;"
	"  n_eye = (V * vec4 (vn, 0.0)).xyz;"
	"  l_pos_eye = (V * vec4 (2.0, 2.0, 15.0, 1.0)).xyz;"
	"  st = vt;"
	"  gl_Position = P * V * B[int (bone_id)] * vec4 (vp, 1.0);"
	"}";
	// * bone_mats[int (bone_id)] * 

	const char* fragment_shader =
	"#version 120\n"
	"varying vec3 n_eye, p_eye, l_pos_eye;"
	"varying vec2 st;"
	"varying float bone_id_f;"
	"void main () {"
	"  gl_FragColor = vec4 (1.0, 1.0, 1.0, 1.0);"
	"  if (bone_id_f < 0.1) {"
	"    gl_FragColor = vec4 (1.0, 0.0, 0.0, 1.0);"
	"  } else if (bone_id_f < 1.1) {"
	"    gl_FragColor = vec4 (0.0, 1.0, 0.0, 1.0);"
	"  } else if (bone_id_f < 2.1) {"
	"    gl_FragColor = vec4 (0.0, 0.0, 1.0, 1.0);"
	"  } else if (bone_id_f < 3.1) {"
	"    gl_FragColor = vec4 (1.0, 1.0, 0.0, 1.0);"
	"  } else if (bone_id_f < 4.1) {"
	"    gl_FragColor = vec4 (1.0, 0.0, 1.0, 1.0);"
	"  } else if (bone_id_f < 5.1) {"
	"    gl_FragColor = vec4 (0.0, 1.0, 1.0, 1.0);"
	"  } else if (bone_id_f < 6.1) {"
	"    gl_FragColor = vec4 (0.0, 0.5, 1.0, 1.0);"
	"  }"
	"}";

	GLuint vs = glCreateShader (GL_VERTEX_SHADER);
	glShaderSource (vs, 1, &vertex_shader, NULL);
	glCompileShader (vs);
	glGetShaderiv (vs, GL_COMPILE_STATUS, &params);
	if (params != GL_TRUE) {
		print_shader_info_log (vs);
		return false;
	}
	GLuint fs = glCreateShader (GL_FRAGMENT_SHADER);
	glShaderSource (fs, 1, &fragment_shader, NULL);
	glCompileShader (fs);
	glGetShaderiv (fs, GL_COMPILE_STATUS, &params);
	if (params != GL_TRUE) {
		print_shader_info_log (fs);
		return false;
	}
	shader_programme = glCreateProgram ();
	glAttachShader (shader_programme, fs);
	glAttachShader (shader_programme, vs);
	glBindAttribLocation (shader_programme, 0, "vp");
	glBindAttribLocation (shader_programme, 1, "vn");
	glBindAttribLocation (shader_programme, 2, "vt");
	glBindAttribLocation (shader_programme, 3, "bone_id");
	glLinkProgram (shader_programme);
	glGetProgramiv (shader_programme, GL_LINK_STATUS, &params);
	assert (params == GL_TRUE);
	P_loc = glGetUniformLocation (shader_programme, "P");
	V_loc = glGetUniformLocation (shader_programme, "V");
	glUseProgram (shader_programme);
	for (int i = 0; i < MAX_BONES; i++) {
		char name[64];
		sprintf (name, "B[%i]", i);
		B_locs[i] = glGetUniformLocation (shader_programme, name);
		glUniformMatrix4fv (B_locs[i], 1, GL_FALSE, identity_mat4 ().m);
	}
	
	const char* no_skin_vertex_shader =
	"#version 120\n"
	"attribute vec3 vp, vn;"
	"attribute vec2 vt;"
	"uniform mat4 P, V;"
	"varying vec3 n_eye, p_eye, l_pos_eye;"
	"varying vec2 st;"
	"void main () {"
	"  p_eye = (V * vec4 (vp, 1.0)).xyz;"
	"  n_eye = (V * vec4 (vn, 0.0)).xyz;"
	"  l_pos_eye = (V * vec4 (2.0, 2.0, 15.0, 1.0)).xyz;"
	"  st = vt;"
	"  gl_Position = P * V * vec4 (vp, 1.0);"
	"}";
	// * bone_mats[int (bone_id)] * 

	const char* no_skin_fragment_shader =
	"#version 120\n"
	"varying vec3 n_eye, p_eye, l_pos_eye;"
	"varying vec2 st;"
	"void main () {"
	"  gl_FragColor = vec4 (1.0, 1.0, 1.0, 1.0);"
	"}";
	
	vs = glCreateShader (GL_VERTEX_SHADER);
	glShaderSource (vs, 1, &no_skin_vertex_shader, NULL);
	glCompileShader (vs);
	glGetShaderiv (vs, GL_COMPILE_STATUS, &params);
	if (params != GL_TRUE) {
		print_shader_info_log (vs);
		return false;
	}
	fs = glCreateShader (GL_FRAGMENT_SHADER);
	glShaderSource (fs, 1, &no_skin_fragment_shader, NULL);
	glCompileShader (fs);
	glGetShaderiv (fs, GL_COMPILE_STATUS, &params);
	if (params != GL_TRUE) {
		print_shader_info_log (fs);
		return false;
	}
	no_skin_shader_programme = glCreateProgram ();
	glAttachShader (no_skin_shader_programme, fs);
	glAttachShader (no_skin_shader_programme, vs);
	glBindAttribLocation (no_skin_shader_programme, 0, "vp");
	glBindAttribLocation (no_skin_shader_programme, 1, "vn");
	glBindAttribLocation (no_skin_shader_programme, 2, "vt");
	glLinkProgram (no_skin_shader_programme);
	glGetProgramiv (shader_programme, GL_LINK_STATUS, &params);
	assert (params == GL_TRUE);
	no_skin_P_loc = glGetUniformLocation (no_skin_shader_programme, "P");
	no_skin_V_loc = glGetUniformLocation (no_skin_shader_programme, "V");
	
	/*glUniformMatrix4fv (
		g_prop_skinned_bone_mat_locs[0],
		bone_count,
		GL_FALSE,
		g_props[i].current_bone_transforms[0].m
	);*/
	
	const char* point_vs =
	"#version 120\n"
	"attribute float v;"
	"uniform mat4 P, V, M;"
	"void main () {"
	"  gl_PointSize = 10.0;"
	"  gl_Position = P * V  * inverse (M) * vec4 (0.0, 0.0, 0.0, 1.0);"
	"}";
	const char* point_fs =
	"#version 120\n"
	"void main () {"
	"  gl_FragColor = vec4 (1.0, 1.0, 1.0, 1.0);"
	"}";
	GLuint pvs = glCreateShader (GL_VERTEX_SHADER);
	glShaderSource (pvs, 1, &point_vs, NULL);
	glCompileShader (pvs);
	glGetShaderiv (vs, GL_COMPILE_STATUS, &params);
	if (params != GL_TRUE) {
		print_shader_info_log (vs);
		return false;
	}
	GLuint pfs = glCreateShader (GL_FRAGMENT_SHADER);
	glShaderSource (pfs, 1, &point_fs, NULL);
	glCompileShader (pfs);
	glGetShaderiv (fs, GL_COMPILE_STATUS, &params);
	if (params != GL_TRUE) {
		print_shader_info_log (fs);
		return false;
	}
	psp = glCreateProgram ();
	glAttachShader (psp, pfs);
	glAttachShader (psp, pvs);
	glLinkProgram (psp);
	glGetProgramiv (shader_programme, GL_LINK_STATUS, &params);
	assert (params == GL_TRUE);
	pP_loc = glGetUniformLocation (psp, "P");
	pV_loc = glGetUniformLocation (psp, "V");
	pM_loc = glGetUniformLocation (psp, "M");

	return true;
}

int main (int argc, char** argv) {
	mat4 P, V;
	double dur = 0.0;
	float bpoints = 0.0f;
	double previous_seconds;
	double anim_timer = 0.0;
	GLuint bpoints_vbo = 0;
	GLuint bpoints_vao = 0;
	
	if (argc < 2) {
		printf ("usage: ./viewer FILE.apg\n");
		return 0;
	}
	assert (start_gl ());
	assert (load_mesh (argv[1]));
	printf ("mesh loaded. %i verts\n", vert_count);
	assert (create_shaders ());
	
	glGenBuffers (1, &bpoints_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, bpoints_vbo);
	glBufferData (
		GL_ARRAY_BUFFER,
		1 * sizeof (GLfloat),
		&bpoints,
		GL_STATIC_DRAW
	);
	glGenVertexArrays (1, &bpoints_vao);
	glBindVertexArray (bpoints_vao);
	glEnableVertexAttribArray (0);
	glBindBuffer (GL_ARRAY_BUFFER, bpoints_vbo);
	glVertexAttribPointer (0, 1, GL_FLOAT, GL_FALSE, 0, NULL);
	
	V = look_at (
		vec3 (0.0f, 10.0f, 10.0f),
		vec3 (0.0f, 0.0f, 0.0),
		normalise (vec3 (0.0f, 10.0f, -10.0f))
	);
	P = perspective (67.0f, (float)width / (float)height, 0.01f, 100.0f);
	
	print_all_keys ();
	
	if (animation_count > 0) {
		dur = animations[0].duration;
	}
	
	glClearColor (0.2, 0.2, 0.2, 1.0);
	glDepthFunc (GL_LESS);
	previous_seconds = glfwGetTime ();
	while (!glfwWindowShouldClose (window)) {
		double current_seconds, elapsed_seconds;
		
		current_seconds = glfwGetTime ();
		elapsed_seconds = current_seconds - previous_seconds;
		previous_seconds = current_seconds;
		anim_timer += elapsed_seconds;
		if (anim_timer > dur) {
			anim_timer = 0.0;
		}
		
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable (GL_DEPTH_TEST);
		if (animation_count > 0) {
			glUseProgram (shader_programme);
			if (cam_dirty) {
				glUniformMatrix4fv (P_loc, 1, GL_FALSE, P.m);
				glUniformMatrix4fv (V_loc, 1, GL_FALSE, V.m);
			}
			
			// anim update
			_recurse_anim_tree (
				animations,
				anim_timer,
				0,
				identity_mat4 ()
			);
			//printf ("0 and 1 of %i\n", bone_count);
			//print (current_bone_mats[0]);
			//print (current_bone_mats[1]);
			glUniformMatrix4fv (B_locs[0], bone_count, GL_FALSE,
				current_bone_mats[0].m);
			glBindVertexArray (vao);
			glDrawArrays (GL_TRIANGLES, 0, vert_count);
		
			glDisable (GL_DEPTH_TEST);
			glEnable (GL_PROGRAM_POINT_SIZE);
			glUseProgram (psp);
			if (cam_dirty) {
				glUniformMatrix4fv (pP_loc, 1, GL_FALSE, P.m);
				glUniformMatrix4fv (pV_loc, 1, GL_FALSE, V.m);
			}
			glBindVertexArray (bpoints_vao);
			for (int i = 0; i < bone_count; i++) {
				glUniformMatrix4fv (pM_loc, 1, GL_FALSE, offset_mats[i].m);
				glBindVertexArray (bpoints_vao);
				glDrawArrays (GL_POINTS, 0, 1);
			}
			glDisable (GL_PROGRAM_POINT_SIZE);
		} else {
			glUseProgram (no_skin_shader_programme);
			if (cam_dirty) {
				glUniformMatrix4fv (no_skin_P_loc, 1, GL_FALSE, P.m);
				glUniformMatrix4fv (no_skin_V_loc, 1, GL_FALSE, V.m);
			}
			glBindVertexArray (vao);
			glDrawArrays (GL_TRIANGLES, 0, vert_count);
		}
		
		cam_dirty = false;
		glfwPollEvents ();
		glfwSwapBuffers (window);
		if (GLFW_PRESS == glfwGetKey (window, GLFW_KEY_ESCAPE)) {
			glfwSetWindowShouldClose (window, 1);
		}
	} // endwhile
	glfwTerminate();
	
	return 0;
}
