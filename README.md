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

All of these are also optional. This is the main feature of the format.

##Dependencies##

Converter:

* AssImp http://assimp.sourceforge.net/

OpenGL-based Viewer:

* GLFW3 http://www.glfw.org/docs/latest/
* GLEW http://glew.sourceforge.net/

##Future Ideas##

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
