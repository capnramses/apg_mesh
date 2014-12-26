#include "mesh_loader.h"
//#include "gl_utils.h"
//#include <GL/glew.h> // include GLEW and new version of GL on Windows
//#include <GLFW/glfw3.h> // GLFW helper library
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

std::vector<Mesh> g_loaded_meshes;

Anim_Node::Anim_Node () {
	int i;
	
	num_children = 0;
	bone_index = -1;
	has_bone = false;
	for (i = 0; i < MAX_CHILDREN; i++) {
		children[i] = NULL;
	}
}

Anim_Node_Name_Map::Anim_Node_Name_Map () {
	anim_node_ptr = NULL;
}

Mesh::Mesh () {
	int i;
	
	root_transform = identity_mat4 ();
	for (i = 0; i < MAX_BONES; i++) {
		bone_offset_mats[i] = identity_mat4 ();
		current_bone_transforms[i] = identity_mat4 ();
	}
	root_node = NULL;
	anim_duration = 0.0;
/*	vao = 0;
	points_vbo = 0;
	normals_vbo = 0;
	st_vbo = 0;
	bone_ids_vbo = 0;*/
	point_count = 0;
	bone_count = 0;
	anim_count = 0;
	anim_node_count = 0;
}

/* convert assimp's weirdly constructed row-order matrices in one of mine */ 
mat4 convert_assimp_matrix (aiMatrix4x4 m) {
	return mat4 (
		1.0f, 0.0f, 0.0f, m.a4,
		0.0f, 1.0f, 0.0f, m.b4,
		0.0f, 0.0f, 1.0f, m.c4,
		0.0f, 0.0f, 0.0f, m.d4
	);
}

bool get_bone_index_for_name (const Mesh& mesh, const aiString& name, int& index) {
	unsigned int i;
	
	for (i = 0; i < mesh.bone_names.size (); i++) {
		if (name == mesh.bone_names[i]) {
			index = i;
			return true;
		}
	}
	index = -1;
	return false;
}

bool initialise_node_and_children (Mesh& mesh, Anim_Node* my_node, aiNode* ass_node) {
	Anim_Node_Name_Map map;
	int i;

	assert (my_node != NULL);
	// get name and work out if a bone is directly controlled by this node
	aiString ass_name = ass_node->mName;
	
	// make entry in name ptr map for later
	map.ass_name = ass_name;
	map.anim_node_ptr = my_node;
	mesh.node_name_map.push_back (map);
	
	int index = -1;
	my_node->has_bone = get_bone_index_for_name (mesh, ass_name, index);
	/*if (my_node->has_bone) {
		printf ("node %s directly controls bone %i\n", &ass_name.data[0], index);
	} else {
		printf ("node %s does not directly control a bone\n", &ass_name.data[0]);
	}*/
	my_node->id = mesh.anim_node_count++;
	my_node->bone_index = index;
	my_node->num_children = ass_node->mNumChildren;
	//printf ("node %s has %i children\n", &ass_name.data[0], my_node->num_children);
	if (my_node->num_children >= MAX_CHILDREN) {
		fprintf (stderr, "ERROR: too many children for bone. max %i\n", MAX_CHILDREN);
		return false;
	}
	
	// recurse to children
	for (i = 0; i < my_node->num_children; i++) {
		my_node->children[i] = new Anim_Node;
		assert (my_node->children[i] != NULL);
		if (!initialise_node_and_children (mesh, my_node->children[i], ass_node->mChildren[i])) {
			fprintf (stderr, "ERROR: creating child of anim node\n");
			return false;
		}
	}
	//printf ("node and children created\n");
	
	return true;
}

Anim_Node* _get_anim_node_by_name (const Mesh& mesh, const aiString& name) {
	Anim_Node* node_ptr = NULL;
	unsigned int i = 0;
	
	for (i = 0; i < mesh.node_name_map.size (); i++) {
		if (name == mesh.node_name_map[i].ass_name) {
			return mesh.node_name_map[i].anim_node_ptr;
		}
	}
	return node_ptr;
}

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
) {
	unsigned int i;
	
	printf ("loading mesh %s\n", file_name);
	// check if already loaded
	for (i = 0; i < g_loaded_meshes.size (); i++) {
		if (g_loaded_meshes[i].file_name == file_name) {
			printf ("mesh %s was already loaded, returning vao\n", file_name);
			return i;
		}
	}
	// if not an .obj then correct the coordinates
	bool correct_coords = true;
	int len = strlen (file_name);
	if ('o' == file_name[len - 3] && 'b' == file_name[len - 2] && 'j' == file_name[len - 1]) {
		printf (".obj found, not correcting coordinate system...\n");
		correct_coords = false;
	}
	
	// otherwise load it
	Mesh m = load_mesh (file_name, correct_coords);
	// create a suitable vao
/*	m.vao = 0;
	glGenVertexArrays (1, &m.vao);
	glBindVertexArray (m.vao);
	
	int attrib_count = 0;
	if (has_vp) {
		glBindBuffer (GL_ARRAY_BUFFER, m.points_vbo);
		glVertexAttribPointer (vp_loc, 3, GL_FLOAT, GL_FALSE, 0, (GLubyte*)NULL);
		attrib_count++;
	}
	if (has_vn) {
		glBindBuffer (GL_ARRAY_BUFFER, m.normals_vbo);
		glVertexAttribPointer (vn_loc, 3, GL_FLOAT, GL_FALSE, 0, (GLubyte*)NULL);
		attrib_count++;
	}
	if (has_vt) {
		glBindBuffer (GL_ARRAY_BUFFER, m.st_vbo);
		glVertexAttribPointer (vt_loc, 2, GL_FLOAT, GL_FALSE, 0, (GLubyte*)NULL);
		attrib_count++;
	}
	if (has_result.vtangents) {
		glBindBuffer (GL_ARRAY_BUFFER, m.tangents_vbo);
		glVertexAttribPointer (
			result.vtangents_loc, 4, GL_FLOAT, GL_FALSE, 0, NULL
		);
		attrib_count++;
	}
	if (has_bones) {
		glBindBuffer (GL_ARRAY_BUFFER, m.bone_ids_vbo);
		glVertexAttribPointer (
			vbones_loc, 1, GL_FLOAT, GL_FALSE, 0, (GLubyte*)NULL
		);
		attrib_count++;
	}
	
	for (int i = 0; i < attrib_count; i++) {
		glEnableVertexAttribArray (i);
	}*/
	
	printf ("mesh file loaded\n");
	
	g_loaded_meshes.push_back (m);
	return g_loaded_meshes.size () - 1;
}

Mesh load_mesh (const char* file_name, bool correct_coords) {
	int i;
	
	printf ("loading mesh %s\n", file_name);
	Mesh result;
	result.file_name = file_name;
	if (correct_coords) {
		result.root_transform = rotate_x_deg (identity_mat4 (), -90.0f);
	} else {
		result.root_transform = identity_mat4 ();
	}
	const aiScene* scene = aiImportFile (
		file_name,
		aiProcess_Triangulate | aiProcess_CalcTangentSpace
	);
	if (!scene) {
		fprintf (stderr, "FATAL ERROR: reading mesh %s\n", file_name);
		exit (1);
	}
	printf ("  animations %i\n", (int)scene->mNumAnimations);
	printf ("  cameras %i\n", (int)scene->mNumCameras);
	printf ("  lights %i\n", (int)scene->mNumLights);
	printf ("  materials %i\n", (int)scene->mNumMaterials);
	printf ("  meshes %i\n", (int)scene->mNumMeshes);
	printf ("  textures %i\n", (int)scene->mNumTextures);
	
	// reset to 0 so meshes without anims can use matrix[0] as identity
	for (i = 0; i < MAX_VERTICES; i++) {
		result.vbone_ids[i] = 0;
	}
	unsigned int curr_bone = 0;
	
	// init each mesh in scene record per-vertex data
	//printf ("db: mesh loop\n");
  for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
  	//printf ("    %i vertices in mesh\n", mesh->mNumVertices);
  	//printf ("db: vertex loop\n");
  	for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
  		if (mesh->HasPositions ()) {
  			const aiVector3D* vp = &(mesh->mVertices[v_i]);
  			if (correct_coords) {
  				result.vps.push_back (vp->x);
  				result.vps.push_back (vp->z);
  				result.vps.push_back (-vp->y);
  			} else {
  				result.vps.push_back (vp->x);
  				result.vps.push_back (vp->y);
  				result.vps.push_back (vp->z);
  			}
  		}
  		if (mesh->HasNormals ()) {
  			// assume normals are same coords as points
      	const aiVector3D* vn = &(mesh->mNormals[v_i]);
      	if (correct_coords) {
      		result.vns.push_back (vn->x);
  				result.vns.push_back (vn->z);
  				result.vns.push_back (-vn->y);
  			} else {
  				result.vns.push_back (vn->x);
  				result.vns.push_back (vn->y);
  				result.vns.push_back (vn->z);
  			}
      }
      if (mesh->HasTextureCoords (0)) {
      	const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
	      result.vts.push_back (vt->x);
  			result.vts.push_back (vt->y);
			}
			if (mesh->HasTangentsAndBitangents ()) {
				const aiVector3D* tangent = &(mesh->mTangents[v_i]);
		    const aiVector3D* bitangent = &(mesh->mBitangents[v_i]);
		    const aiVector3D* normal = &(mesh->mNormals[v_i]);
		    
		    // put the three vectors into my vec3 struct format for doing maths
		    vec3 t (tangent->x, tangent->y, tangent->z);
		    vec3 n (normal->x, normal->y, normal->z);
		    vec3 b (bitangent->x, bitangent->y, bitangent->z);
		    
		    // orthogonalise and normalise the tangent so we can use it in something
		    // approximating a T,N,B inverse matrix
		    vec3 t_i = normalise (t - n * dot (n, t));
		    
		    // get determinant of T,B,N 3x3 matrix by dot*cross method
		    float det = (dot (cross (n, t), b));
		    if (det < 0.0f) {
		    	det = -1.0f;
		    } else {
		    	det = 1.0f;
		    }
		    
		    // push back 4d vector for inverse tangent with determinant
		    result.vtangents.push_back (t_i.v[0]);
		    result.vtangents.push_back (t_i.v[1]);
		    result.vtangents.push_back (t_i.v[2]);
		    result.vtangents.push_back (det);
			}
  	}
  	
  	if (mesh->HasBones ()) {
			//printf ("    %i bones\n", mesh->mNumBones);
			//printf ("db: bones loop\n");
			for (unsigned int b_i = 0; b_i < mesh->mNumBones; b_i++) {
				const aiBone* bone = mesh->mBones[b_i];
				//printf ("      name %s\n", &bone->mName.data[0]);
				result.bone_names.push_back (bone->mName);
				//printf ("      weights %i\n", bone->mNumWeights);
				// store offset matrix for bone (bone pos in mesh)
				result.bone_offset_mats[curr_bone] = convert_assimp_matrix (
					bone->mOffsetMatrix
				);
				// store bone id for each vertex weighted by this bone
				// TODO NOTE: this only allows 1 bone influencing each vertex
				//printf ("db: weights loop\n");
				for (unsigned int w_i = 0; w_i < bone->mNumWeights; w_i++) {
					aiVertexWeight weight = bone->mWeights[w_i];
					unsigned int vertex_id = weight.mVertexId;
					if (MAX_VERTICES <= vertex_id) {
						fprintf (stderr, "ERROR: vertex %i referred to by bone is bigger than max size of array; %i\n", vertex_id, MAX_VERTICES);
						exit (1);
					}
					result.vbone_ids[vertex_id] = curr_bone;
				}
				curr_bone++;
			} // end of for numbones loop
		} // end of hasbones
		
	} // end of mesh loop
	
	//printf ("db: create VBOs\n");
	
	// set up vbos
	result.point_count = result.vps.size () / 3;
	result.bone_count = curr_bone;
	
	/*result.points_vbo = 0;
	glGenBuffers (1, &result.points_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, result.points_vbo);
	glBufferData (
		GL_ARRAY_BUFFER, result.point_count * 3 * sizeof (float),
		&result.vps[0],
		GL_STATIC_DRAW
	);
	
	result.normals_vbo = 0;
	glGenBuffers (1, &result.normals_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, result.normals_vbo);
	glBufferData (
		GL_ARRAY_BUFFER, result.point_count * 3 * sizeof (float),
		&result.vns[0],
		GL_STATIC_DRAW
	);
	
	result.st_vbo = 0;
	glGenBuffers (1, &result.st_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, result.st_vbo);
	glBufferData (
		GL_ARRAY_BUFFER, result.point_count * 2 * sizeof (float),
		&result.vts[0],
		GL_STATIC_DRAW
	);
	
	result.tangents_vbo = 0;
	glGenBuffers (1, &result.tangents_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, result.tangents_vbo);
	glBufferData (
		GL_ARRAY_BUFFER,
		result.point_count * 4 * sizeof (float),
		&result.vtangents[0],
		GL_STATIC_DRAW
	);
	
	result.bone_ids_vbo = 0;
	glGenBuffers (1, &result.bone_ids_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, result.bone_ids_vbo);
	glBufferData (
		GL_ARRAY_BUFFER,
		result.point_count * sizeof (float),
		&vbone_ids, GL_STATIC_DRAW
	);
	*/
	//printf ("db: root nodes\n");
	
	// set up animation tree with some mappings to get rid of bone name searching
	aiNode* ass_root_node = scene->mRootNode;
	//printf ("root node is named %s\n", &ass_root_node->mName.data[0]);
	result.root_node = new Anim_Node;
	assert (result.root_node != NULL);
	assert (initialise_node_and_children (result, result.root_node, ass_root_node));
	
	// TODO only one animation supported atm
	result.anim_count = scene->mNumAnimations;
	if (result.anim_count > 1) {
		fprintf (stderr, "WARNING! mesh has more than 1 animation. only 1 is stored"
		"NOTE: to fix this store the root node in a vector of 'Animation'"
		"structures, where each also contains the name of the animation\n");
		exit (1);
	}
	//printf ("db: anim loop\n");
	for (unsigned int a_i = 0; a_i < result.anim_count; a_i++) {
		const aiAnimation* anim = scene->mAnimations[a_i];
  	//printf ("Animation Name: %s\n", &anim->mName.data[0]);
  	//printf (" %f mDuration\n", anim->mDuration);
  	result.anim_duration = anim->mDuration;
  	//printf (" %i mNumChannels (node anims)\n", anim->mNumChannels);
  	//printf (" %i mNumMeshChannels\n", anim->mNumMeshChannels);
  	//printf (" %f mTicksPerSecond\n", anim->mTicksPerSecond);
  	
  	//printf ("db: channel loop\n");
  	for (unsigned int c_i = 0; c_i < anim->mNumChannels; c_i++) {
  		const aiNodeAnim* ass_node_anim = anim->mChannels[c_i];
  		//printf (" anim node name: %s\n", &ass_node_anim->mNodeName.data[0]);
			//printf ("   num pos keys %u\n", ass_node_anim->mNumPositionKeys);
			//printf ("   num rot keys %u\n", ass_node_anim->mNumRotationKeys);
			//printf ("   num scale keys %u\n", ass_node_anim->mNumScalingKeys);
			
			// look for node in tree by name ffs this is ridiculous assimp!
			Anim_Node* my_node = _get_anim_node_by_name (
				result,
				ass_node_anim->mNodeName
			);
			assert (my_node != NULL);
	
			// store key-frames
			// add position keyframes
			//printf ("db: pos keys loop\n");
			for (unsigned int p_i = 0; p_i < ass_node_anim->mNumPositionKeys; p_i++) {
				aiVectorKey vk = ass_node_anim->mPositionKeys[p_i];
				pos_key pkf;
				pkf.time = vk.mTime / anim->mTicksPerSecond;
				aiVector3D vector = vk.mValue;
				pkf.v = vec3 (vector.x, vector.y, vector.z);
				my_node->pos_keyframes.push_back (pkf);
			}
			// add scale keyframes
			//printf ("db: scale keys loop\n");
			for (unsigned int p_i = 0; p_i < ass_node_anim->mNumScalingKeys; p_i++) {
				aiVectorKey vk = ass_node_anim->mScalingKeys[p_i];
				pos_key pkf;
				pkf.time = vk.mTime / anim->mTicksPerSecond;
				aiVector3D vector = vk.mValue;
				pkf.v = vec3 (vector.x, vector.y, vector.z);
				my_node->scale_keyframes.push_back (pkf);
			}
			// add rotation keyframes
			//printf ("db: rot keys loop\n");
			for (unsigned int p_i = 0; p_i < ass_node_anim->mNumRotationKeys; p_i++) {
				aiQuatKey qk = ass_node_anim->mRotationKeys[p_i];
				rot_key rkf;
				rkf.time = qk.mTime / anim->mTicksPerSecond;
				aiQuaternion versor = qk.mValue;
				rkf.q.q[0] = versor.w;
				rkf.q.q[1] = versor.x;
				rkf.q.q[2] = versor.y;
				rkf.q.q[3] = versor.z;
				rkf.q = normalise (rkf.q);
				my_node->rot_keyframes.push_back (rkf);
			}
  		
  	} // end of channels loop
	} // end of animations loop
	
	// free scene
	//printf ("db: release and return\n");
	aiReleaseImport (scene);
	
	return result;		
}

void _recurse_anim_tree (
	const Mesh& mesh,
	double anim_time,
	Anim_Node* my_node,
	mat4 parent_mat
);

void _recurse_anim_tree (
	Mesh& mesh,
	double anim_time,
	Anim_Node* my_node,
	mat4 parent_mat
) {
	mat4 trans_mat, rot_mat, node_mat, global_trans_mat;
	vec3 vf, vi, p;
	int i;
	
	assert (my_node != NULL);
	trans_mat = identity_mat4 ();
	rot_mat = identity_mat4 ();
	
	// position
	if (my_node->pos_keyframes.size () > 1) {
		double prev_t, next_t, t_factor;
		// work out previous frame and next frame numbers
		int previous_frame_index = 0;
		int next_frame_index = 1;
		unsigned int i;
		
		for (i = 0; i < my_node->pos_keyframes.size () - 1; i++) {
			double t;
			
			t = my_node->pos_keyframes[i].time;
			if (t >= anim_time) {
				break;
			}
			previous_frame_index = i;
			next_frame_index = i + 1;
		}
		// get prev and next pos and time
		prev_t = my_node->pos_keyframes[previous_frame_index].time;
		next_t = my_node->pos_keyframes[next_frame_index].time;
		t_factor = (anim_time - prev_t) / (next_t - prev_t);
		// interp position
		vf = my_node->pos_keyframes[next_frame_index].v;
		vi = my_node->pos_keyframes[previous_frame_index].v;
		p = vf * t_factor + vi * (1.0 - t_factor);
		trans_mat = translate (identity_mat4 (), p);
	} else if (1 == my_node->pos_keyframes.size ()) {
		vf = my_node->pos_keyframes[0].v;
		trans_mat = translate (identity_mat4 (), vf);
	}
	
	// interp rotation
	if (my_node->rot_keyframes.size () > 1) {
		double prev_t, next_t, t_factor;
		int previous_frame_index = 0;
		int next_frame_index = 1;
		unsigned int i;

		for (i = 0; i < my_node->rot_keyframes.size () - 1; i++) {
			double t;
			
			t = my_node->rot_keyframes[i].time;
			if (t >= anim_time) {
				break;
			}
			previous_frame_index = i;
			next_frame_index = i + 1;
		}
		// get prev and next pos and time
		prev_t = my_node->rot_keyframes[previous_frame_index].time;
		next_t = my_node->rot_keyframes[next_frame_index].time;
		t_factor = (anim_time - prev_t) / (next_t - prev_t);
		// get the two quaternions
		versor qf = my_node->rot_keyframes[next_frame_index].q;
		versor qi = my_node->rot_keyframes[previous_frame_index].q;
		versor slerped = slerp (qi, qf, t_factor);
		rot_mat = quat_to_mat4 (slerped);
	} else if (1 == my_node->rot_keyframes.size ()) {
		versor qf = my_node->rot_keyframes[0].q;
		rot_mat = quat_to_mat4 (qf);
	}
	
	node_mat = trans_mat * rot_mat; // * scale mat at end
	global_trans_mat = parent_mat * node_mat;

	// update bone mats if bone linked to this node
	if (my_node->has_bone) {
		mesh.current_bone_transforms[my_node->bone_index] = mesh.root_transform * global_trans_mat *
		mesh.bone_offset_mats[my_node->bone_index];
	}

	// transform children
	for (i = 0 ; i < my_node->num_children; i++) {
		_recurse_anim_tree (mesh, anim_time, my_node->children[i], global_trans_mat);
  }
}

void calc_mesh_animation (Mesh& mesh, double anim_time) {
	_recurse_anim_tree (mesh, anim_time, mesh.root_node, identity_mat4 ());
}

