//
// .apg Viewer in OpenGL 2.1
// Anton Gerdelan
// antongerdelan.net
// First version 3 Jan 2014
// Revised: 26 Dec 2014
// uses the Assimp asset importer library http://assimp.sourceforge.net/
//

#include "mesh_loader.hpp"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

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
	point_count = 0;
	bone_count = 0;
	anim_count = 0;
	anim_node_count = 0;
}

/* convert assimp's weirdly constructed row-order matrices in one of mine */ 
mat4 convert_assimp_matrix (aiMatrix4x4 m) {
	return mat4 (
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		m.a4, m.b4, m.c4, m.d4
	);
}

bool get_bone_index_for_name (const Mesh& mesh, const aiString& name,
	int& index) {
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

bool initialise_node_and_children (Mesh& mesh, Anim_Node* my_node,
	aiNode* ass_node) {
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
	my_node->id = mesh.anim_node_count++;
	my_node->bone_index = index;
	my_node->num_children = ass_node->mNumChildren;
	if (my_node->num_children >= MAX_CHILDREN) {
		fprintf (stderr, "ERROR: too many children for bone. max %i\n",
			MAX_CHILDREN);
		return false;
	}
	
	// recurse to children
	for (i = 0; i < my_node->num_children; i++) {
		my_node->children[i] = new Anim_Node;
		assert (my_node->children[i] != NULL);
		if (!initialise_node_and_children (mesh, my_node->children[i],
			ass_node->mChildren[i])) {
			fprintf (stderr, "ERROR: creating child of anim node\n");
			return false;
		}
	}
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
	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
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

				// orthogonalise and normalise the tangent so we can use it in
				// something approximating a T,N,B inverse matrix
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
			for (unsigned int b_i = 0; b_i < mesh->mNumBones; b_i++) {
				const aiBone* bone = mesh->mBones[b_i];
				result.bone_names.push_back (bone->mName);
				// store offset matrix for bone (bone pos in mesh)
				result.bone_offset_mats[curr_bone] = convert_assimp_matrix (
					bone->mOffsetMatrix);
				// store bone id for each vertex weighted by this bone
				// TODO NOTE: this only allows 1 bone influencing each vertex
				//printf ("db: weights loop\n");
				for (unsigned int w_i = 0; w_i < bone->mNumWeights; w_i++) {
					aiVertexWeight weight = bone->mWeights[w_i];
					unsigned int vertex_id = weight.mVertexId;
					if (MAX_VERTICES <= vertex_id) {
						fprintf (stderr, "ERROR: vertex %i referred to by bone is bigger \
							than max size of array; %i\n", vertex_id, MAX_VERTICES);
						exit (1);
					}
					result.vbone_ids[vertex_id] = curr_bone;
				}
				curr_bone++;
			} // end of for numbones loop
		} // end of hasbones
	} // end of mesh loop
	
	result.point_count = result.vps.size () / 3;
	result.bone_count = curr_bone;
	
	// set up animation tree with some mappings to get rid of bone name searching
	aiNode* ass_root_node = scene->mRootNode;
	//printf ("root node is named %s\n", &ass_root_node->mName.data[0]);
	result.root_node = new Anim_Node;
	assert (result.root_node != NULL);
	assert (initialise_node_and_children (result, result.root_node,
		ass_root_node));
	
	// TODO only one animation supported atm
	result.anim_count = scene->mNumAnimations;
	if (result.anim_count > 1) {
		fprintf (stderr, "WARNING! mesh has more than 1 animation. only 1 is \
		stored"
		"NOTE: to fix this store the root node in a vector of 'Animation'"
		"structures, where each also contains the name of the animation\n");
		exit (1);
	}
	//printf ("db: anim loop\n");
	for (unsigned int a_i = 0; a_i < result.anim_count; a_i++) {
		const aiAnimation* anim = scene->mAnimations[a_i];
		result.anim_duration = anim->mDuration;
	
		for (unsigned int c_i = 0; c_i < anim->mNumChannels; c_i++) {
			const aiNodeAnim* ass_node_anim = anim->mChannels[c_i];
			
			// look for node in tree by name ffs this is ridiculous assimp!
			Anim_Node* my_node = _get_anim_node_by_name (
				result,
				ass_node_anim->mNodeName
			);
			assert (my_node != NULL);
	
			// store key-frames
			// add position keyframes
			for (unsigned int p_i = 0; p_i < ass_node_anim->mNumPositionKeys;
				p_i++) {
				aiVectorKey vk = ass_node_anim->mPositionKeys[p_i];
				pos_key pkf;
				pkf.time = vk.mTime / anim->mTicksPerSecond;
				aiVector3D vector = vk.mValue;
				pkf.v = vec3 (vector.x, vector.y, vector.z);
				my_node->pos_keyframes.push_back (pkf);
			}
			// add scale keyframes
			for (unsigned int p_i = 0; p_i < ass_node_anim->mNumScalingKeys; p_i++) {
				aiVectorKey vk = ass_node_anim->mScalingKeys[p_i];
				pos_key pkf;
				pkf.time = vk.mTime / anim->mTicksPerSecond;
				aiVector3D vector = vk.mValue;
				pkf.v = vec3 (vector.x, vector.y, vector.z);
				my_node->scale_keyframes.push_back (pkf);
			}
			// add rotation keyframes
			for (unsigned int p_i = 0; p_i < ass_node_anim->mNumRotationKeys;
				p_i++) {
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
	aiReleaseImport (scene);
	
	return result;
}

