#
# Anton's .apg Mesh Exporter for Blender 2.74
# Attempt v. 3 - from scratch in JavaScript-esque style
#

#
# TODO
# bone id(s)
# bone weight(s)
# tangents
# skeleton heirarchy
# animations
#

#http://wiki.blender.org/index.php/Dev:2.5/Py/Scripts/Guidelines/Addons
bl_info = {
    "name": "APG mesh exporter (.apg)",
    "description": "Exports APG Format",
    "author": "Anton Gerdelan",
    "version": (0, 2),
    "blender": (2, 74, 0),
    "location": "File > Import-Export > APG (.apg) ",
    "warning": "", # used for warning icon and text in addons panel
    "wiki_url": "",
    "category": "Import-Export"
}

import bpy, math

def calc_bsphere (mesh):
    """get the bounding sphere from a mesh's bounding box"""
    bbox = mesh.bound_box
    max_dist = 0.0
    for v in bbox:
        dist = math.sqrt (v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
        max_dist = max (max_dist, dist)
    return max_dist

#
# 03  ->  ac  &&   d
# 12  ->  b   &&  ef
#
def triangulate (quad_indices):
    """triangulate 4 quad vertex indices in 2 sets of 3 triangle indices"""
    tris_indices = []
    tris_indices.append (quad_indices[0]) #a
    tris_indices.append (quad_indices[1]) #b
    tris_indices.append (quad_indices[3]) #c
    tris_indices.append (quad_indices[3]) #d
    tris_indices.append (quad_indices[1]) #e
    tris_indices.append (quad_indices[2]) #f
    return tris_indices

def exp_apg (filepath):
    """export .apg file"""
    print ("\nstarted anton's mesh export script")

    scene = bpy.context.scene
    mesh = None
    mdata = None # mesh.data
    faces = None
    armature = None
    brad = 0.0 # bounding radius

    # find the first mesh in the scene
    for obj in scene.objects:
        if obj.type == "MESH":
            print ("found mesh object \"%s\"" % obj.name)
            mesh = obj
            break
    else:
        raise Exception ("ERROR: did not find mesh object in scene")

    # find the first armature in the scene
    for obj in scene.objects:
        if obj.type == "ARMATURE":
            print ("found armature object \"%s\"" % obj.name)
            armature = obj
            break
    else:
        print ("WARNING: did not find armature object in scene")

    # make a copy of mesh data as it won't let me operate on the original
    mdata = mesh.data.copy ()

    # tessellate
    if not mdata.tessfaces and mdata.polygons:
        mdata.calc_tessface ()
        
    # check if required bits are there
    has_uvs = bool (mdata.tessface_uv_textures)
    if not has_uvs:
        raise Exception ("ERROR: did not find UVs in mesh")
    # get active uv layer as THE uv layer
    uvldata = mdata.tessface_uv_textures.active.data

    print ("calculating normals")
    mdata.calc_normals ()

    print ("calculating tangents")
    mdata.calc_tangents ()

    # get all the unique vertex points in the mesh
    # in a bpy_prop_collection
    # each MeshVertex in collection:
    #  .co (x,y,z)
    #  .index integer -- seem to be pre-sorted by index anyway
    #  .normal (x,y,z)

    iverts = mesh.data.vertices
    #ivps = [] # indexed vertex points
    #ivns = [] # indexed normals
    ivtans = [] # indexed tangents

    # get ivps and ivns
    #for v in iverts:
        # vector type to list type
        #ivp = list (v.co)
        #ivps.append (ivp)
        #ivn = list (v.normal)
        #ivns.append (ivn)
    #print ("%i unique vps" % len (ivps))

    # get ivtans
    for loop in mdata.loops:
        tan = list (loop.tangent)
        tan.append (loop.bitangent_sign)
        ivtans.append (tan)
     
    #unindexed lists
    vps = []
    vts = []
    vns = []
    vtans = []
    # loop over faces (seems to always be quads)
    for i, f in enumerate (mdata.tessfaces):
        vp = vt = vn = vtan = None
        
        # set flat normal
        if not f.use_smooth:
            vn = f.normal[:]
        # uv
        uvset = uvldata[i]
        # makes a tuple i guess (parentheses)
        quad_uv = [uvset.uv1, uvset.uv2, uvset.uv3, uvset.uv4] # quad has 4
        tri_uv = []
        tri_uv.append (quad_uv[0])
        tri_uv.append (quad_uv[1])
        tri_uv.append (quad_uv[3])
        tri_uv.append (quad_uv[3])
        tri_uv.append (quad_uv[1])
        tri_uv.append (quad_uv[2])
        # loop over verts in face
        q_indices = f.vertices
        t_indices = triangulate (q_indices);
        for j, v_i in enumerate (t_indices):
            v = iverts[v_i]
            vp = v.co[:]
            if f.use_smooth:
                vn = v.normal[:]
            vt = tri_uv[j][0], tri_uv[j][1]
            vtan = ivtans[v_i]
            
            # append values to main lists
            vps.append (vp)
            vts.append (vt)
            vns.append (vn)
            vtans.append (vtan)
            
    # get the bounding box
    print ("calculating bounding radius")
    brad = calc_bsphere (mesh)

    # write to file
    file = open (filepath, "w")

    file.write ("@Anton's mesh format v.27DEC2014. Blender %s export script v0.3. "
                "https://github.com/capnramses/apg_mesh/\n" % bpy.app.version_string)

    # per-vertex data
    file.write ("@vert_count %i\n" % len (vps))

    # vertex position coordinates
    file.write ("@vp comps 3\n")
    for i, vp in enumerate (vps):
        file.write ("%.2f %.2f %.2f\n" % vp)
        
    # vertex normals
    file.write ("@vn comps 3\n")
    for i, vn in enumerate (vns):
        file.write ("%.3f %.3f %.3f\n" % vn)
        
    # texture coords
    file.write ("@vt comps 2\n")
    for i, vt in enumerate (vts):
        file.write ("%.3f %.3f\n" % vt)
            
    # vertex tangents
    file.write ("@vtan comps 4\n")
    for i, vtan in enumerate (vtans):
        # it's okay printing a tuple like this but not a list. ffs
        file.write ("%.3f %.3f %.3f %.1f\n" % tuple(vtan))

    file.write ("@bounding_radius %.2f\n" % brad)

    file.close ()
        
    print ("ended antons script")
    return {'FINISHED'} # this lets blender know the operator finished successfully.

# ExportHlper is a hlper class, defines fname and invoke() which calls file
# selector
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.types import Operator
from bpy_extras.io_utils import ExportHelper
class ExportAPG (bpy.types.Operator, ExportHelper):
    """Anton's .apg Export Script""" # blender will use this as a tooltip for menu items and buttons.
    bl_idname = "export_data.apg_mesh" # unique identifier for buttons and menu items to reference.
    bl_label = "export .apg" # display name in the interface.
    bl_options = {'REGISTER', 'UNDO'}  # enable undo for the operator.
    
    filename_ext = ".apg"
    filter_glob = StringProperty (default="*.apg", options={'HIDDEN'})
    
    def execute (self, context): # execute() is called by blender when running the operator
        filepath = self.filepath
        filepath = bpy.path.ensure_ext (filepath, self.filename_ext)
        return exp_apg (filepath)

def menu_export (self, context):
    self.layout.operator(ExportAPG.bl_idname, text="APG Mesh (.apg)")

def register ():
    bpy.utils.register_class (ExportAPG)
    bpy.types.INFO_MT_file_export.append (menu_export)

def unregister ():
    bpy.utils.unregister_class (ExportAPG)
    bpy.types.INFO_MT_file_export.remove (menu_export)

# This allows you to run the script directly from blenders text editor
# to test the addon without having to install it.
# ANTON: ummm it worked anyway. i guess it means with a class
if __name__ == "__main__":
    register ()
