#Converter to .apg Animated 3d Mesh Format#

.apg is a simplified ASCII mesh format that supports tangents and skinned
animations, and can be easily parsed without needing a library. The downside is
that there is no data optimisation.

I got tired of the complexity of animated mesh formats, so wrote my own. I'm
sharing this, not because I think others will use my format, but that it might
show how easy it is to do yourself a simple way.

##Usage##

Converter:

  ./conv input.dae [-o output.apg] [-bin]

Viewer:

  ./view mesh.apg

##Motivation##

* Can easily read with a few lines of C - no libraries required
* Simplified animation and skeleton structure
* Easy to visually scan to find problems
* Minimal formatting tags
* Do all parsing and memory allocation in single pass
* Includes tangents for normal mapping

The idea is to have an ASCII file with tag lines beginning with the '@' symbol.
These then give the size and type of data that follows. This means that you can
read lines in a loop until you get to an @ symbol, allocate the size of memory
required, and then run another loop, knowing in advance how many lines and data
points to read until the end of the block.

##Per-Vertex Data##

* points
* normals
* texture coordinates
* tangents
* bone id

All of these are optional, and given in un-optimised blocks - one vertex'
values per-line.

Each block is preceded by a tag line that gives the number of components per
line i.e. 3 for XYZ points. This makes it easy to dynamically allocate memory
during a single pass, whilst parsing the file (the number of vertices in the
mesh is given at the top of the file).

Example of vertex points "vp" block:

    @vert_count 1836
    @vp comps 3
    -0.30 0.01 0.45
    -0.24 0.01 0.44
    ...

Points are assumed to be in meters. I then round to the centimeter to reduce
file size to ~5 bytes per value.

Example of normals in "vn" block:

    @vn comps 3
    0 1 -0
    0 1 -0
    ...
Normals and tangents are rounded to 3 s.f. as they are normalised later anyway.

Example of texture coordinates in "vt" block:

    @vt comps 2
    0.752 0.912
    0.5 0.994
    ...

Texture coordinates are rounded to 3 s.f.

Example of tangents in "vtan" block:

    @vtan comps 4
    -0.875 0 0.484 -1
    -0.875 0 0.484 -1
    ...

Tangents can contain a fourth coordinate convention to be used if desired.

##Per-Mesh Data##

* single-line header with date of format being used
* count of vertices (to make it easier to parse in a single pass)
* a skeleton hierarchy, with probably the best/most simple format ever
* skeleton root transform
* skeleton offset matrix
* key-framed animations; translation, rotation, and scale keys
* a bounding radius (useful for frustum culling)

All of these are also optional.

The simple skeleton and animation structure is the main feature of the format -
it's designed to drop almost directly into shader uniforms. To make the system
clear there are two concepts:

Firstly, a skeleton of "bones" which can deform vertices:

    @skeleton bones 2 animations 1

That's it! The bones here mean that each vertex will have bone id 0 or 1, and
that my vertex shader will have an array of 2 animation matrices as a uniform. 

I give my animation hierarchy in a separate structure. Not all bones in the
skeleton will directly affect a vertex; it's possible to have intermediate
animated bones aren't weighted to any vertices, but still affect bones further
down in the hierarchy that are. We don't
want these to send a matrix to the vertex shader - these are to be calculated
away in animation code. To separate these concepts I use a more generic "node"
in my animation hierarchy. Nodes may or may not be directly tied to a bone - if
so I provide the index of that bone. If the node is purely an intermediate then
I give -1 as the bone index:

    @hierarchy nodes 5
    parent -1 bone_id -1
    parent 0 bone_id -1
    parent 1 bone_id 0
    parent 2 bone_id 1
    parent 0 bone_id -1

Each node has an identifying index, which is its order of appearance (the first
one is 0, the second one is 1, etc.). A node may have a parent, given by index
number, or no parent, indicated by -1. This gives a tree hierarchy in a simple
collection of lines. In my mesh I only
created 2 deforming bones in a skeleton, but Blender has exported a tree of 5
nodes - this includes the skeleton as a whole, and the higher-level container
object, and some other attached object (a light source?). In any case, it's
fine to represent all of these here and preserve and inherited transformations.
Unused nodes can be pruned.

Animations are defined as a series of keys.

    @animation name TODO duration 1.000000

Each node in the hierarchy can have series' of rotation, translation, and
scale keys. Here we have a series of translation keys. Each one has its own
time value 't'. We can see this affects node number 2, giving 6 XYZ translation
values at different times in the animation sequence:

    @tra_keys node 2 count 6 comps 3
    t 0.040000 TRA -0.727390 0.475569 -0.236947
    t 0.281117 TRA -0.727390 0.475569 -0.236947
    t 0.445874 TRA -0.727390 0.475569 -0.236947
    t 0.627053 TRA -0.727390 0.475569 -0.236947
    t 0.797642 TRA -0.727390 0.475569 -0.236947
    t 1.000000 TRA -0.727390 0.475569 -0.236947

Finally, the mesh format also provides a bounding radius, which it calculates
from the vertex with the biggest straight-line distance from the origin:

    @bounding_radius 3.01

This is intended to be used with frustum culling algorithms.

##Dependencies##

Converter:

* AssImp http://assimp.sourceforge.net/

OpenGL-based Viewer:

* GLFW3 http://www.glfw.org/docs/latest/
* GLEW http://glew.sourceforge.net/

##Future Ideas##

###Bone Weights###

I haven't tested any meshes with vertices that are weighted to more than one
bone. It should be fairly trivial to add weights after each bone id, and have
comps >1 for the bone id block.

###Binary format###

I have a binary option but it actually isn't very beneficial in terms of size.
I might look at this again later. Limited-precision floats and .obj-like
optimisation would be key.

###Further reducing or expanding tags###

Some tags contain redundant information which could be tidied. Animation names
are not respected because I've had difficulty exporting these myself from
Blender.

###Blender Export Script###

I started writing an exporter to .apg from Blender.
