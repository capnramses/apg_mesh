#Converter to .apg Animated 3d Mesh Format#

.apg is a simplified ASCII mesh format that supports tangents and skinned
animations, and can be easily parsed without needing a library. The downside is
that there is no data optimisation.

I got tired of the complexity of animated mesh formats, so wrote my own. I'm
sharing this, not because I think others will use my format, but that it might
show how easy it is to do yourself a simple way.

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
mesh is given at the top of the file). Example of vertex points "vp" block:

    @vp comps 3
    0.298388 2.503322 -0.224913
    -0.298635 2.503061 -0.224913
    ...

##Per-Mesh Data##

* single-line header with date of format being used
* count of vertices (to make it easier to parse in a single pass)
* a skeleton hierarchy, with probably the best/most simple format ever
* skeleton root transform
* skeleton offset matrix
* key-framed animations; translation, rotation, and scale keys

All of these are also optional. This is the main feature of the format.

##Dependencies##

I use the AssImp library (http://assimp.sourceforge.net/) in the converter tool
as a general-purpose importer - it imports a huge range of mesh formats - but
no library is required to read the output .apg files.

##Future Ideas##

###Binary format###

I have a binary option but it actually isn't very beneficial in terms of size.
I might look at this again later. Limited-precision floats and .obj-like
optimisation would be key.

###Tinkering with data-point accuracy to reduce file size###

* If points are assumed to be in meters, then we can probably round to the
centimetre or millimetre to reduce file size to ~5 bytes per value.
* normals could be rounded to 4 s.f. as they are normalised later anyway.
* tangents could be something similar to normals.

###Further reducing or expanding tags###

Some tags contain redundant information which could be tidied. Animation names
are not respected because I've had difficulty exporting these myself from
Blender.

###Blender Export Script###

I started writing an exporter to .apg from Blender.
