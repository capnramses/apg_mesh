#
# APG mesh format exporter (.apg)
# 1st v 28 may 2015 anton gerdelan
# 2nd v is a modified v of the Blender ply exporter 29 May 2015
#

bl_info = {
    "name": "APG mesh format (.apg)",
    "author": "Anton Gerdelan",
    "version": (0, 1),
    "blender": (2, 74, 0),
    "location": "File > Import-Export > APG (.apg) ",
    "description": "Import-Export APG Format",
    "warning": "",
    "wiki_url": ""
                "",
    "category": "Import-Export",
}

import bpy
import math

#v.co - vp
#v.no - vn ?? v.normal it looks like
#mesh.face.uv[index]; - vt

def apg_write (filepath,
              mesh,
              use_normals=True,
              use_uv_coords=True,
              use_colors=False,
              ):

    # write the faces to a file
    file = open(filepath, "w")

    # Be sure tessface & co are available!
    if not mesh.tessfaces and mesh.polygons:
        mesh.calc_tessface()

    has_uv = bool(mesh.tessface_uv_textures)
    # this is for vertex colours
    has_vcol = bool(mesh.tessface_vertex_colors)

    if not has_uv:
        use_uv_coords = False
    if not has_vcol:
        use_colors = False

    if not use_uv_coords:
        has_uv = False
    # for vertexcolours
    if not use_colors:
        has_vcol = False
    
    if has_uv:
        active_uv_layer = mesh.tessface_uv_textures.active
        if not active_uv_layer:
            use_uv_coords = False
            has_uv = False
        else:
            active_uv_layer = active_uv_layer.data

    if has_vcol:
        active_col_layer = mesh.tessface_vertex_colors.active
        if not active_col_layer:
            use_colors = False
            has_vcol = False
        else:
            active_col_layer = active_col_layer.data

    # in case
    color = uvcoord = uvcoord_key = normal = normal_key = None

    mesh_verts = mesh.vertices  # save a lookup
    ply_verts = []  # list of dictionaries
    # vdict = {} # (index, normal, uv) -> new index
    vdict = [{} for i in range(len(mesh_verts))]
    ply_faces = [[] for f in range(len(mesh.tessfaces))]
    vert_count = 0
    # loop through all the faces, getting index and face instance of each
    # vertices are looped through within each face
    for i, f in enumerate(mesh.tessfaces):
        smooth = not use_normals or f.use_smooth
        if not smooth:
            normal = f.normal[:]
            normal_key = normal # ANTON -- removed rounded vec3 here

        if has_uv:
            uv = active_uv_layer[i]
            uv = uv.uv1, uv.uv2, uv.uv3, uv.uv4
        if has_vcol:
            col = active_col_layer[i]
            col = col.color1[:], col.color2[:], col.color3[:], col.color4[:]

        f_verts = f.vertices

        pf = ply_faces[i]
        
        # loop through vertices of each faace
        for j, vidx in enumerate(f_verts):
            v = mesh_verts[vidx]

            if smooth:
                normal = v.normal[:]
                normal_key = normal # ANTON removed rounding

            if has_uv:
                uvcoord = uv[j][0], uv[j][1]
                uvcoord_key = uvcoord #rvec2d(uvcoord)

            if has_vcol:
                color = col[j]
                #color = (int(color[0]),
                #         int(color[1]),
                #         int(color[2]),
                #         )
            key = normal_key, uvcoord_key, color

            vdict_local = vdict[vidx]
            pf_vidx = vdict_local.get(key)  # Will be None initially

            if pf_vidx is None:  # same as vdict_local.has_key(key)
                pf_vidx = vdict_local[key] = vert_count
                ply_verts.append((vidx, normal, uvcoord, color))
                vert_count += 1

            pf.append(pf_vidx)

    file.write ("@Anton's mesh format v.27DEC2014. Blender %s export. https://github.com/capnramses/apg_mesh/\n" % bpy.app.version_string)

    # per-vertex data
    file.write ("@vert_count %i\n" % len(ply_verts))
    # vertex position coordinates
    file.write ("@vp_comps 3\n")
    # record bounding radius, assuming origin is centre
    b_rad = 0.0
    for i, v in enumerate(ply_verts):
        file.write ("%.2f %.2f %.2f\n" % mesh_verts[v[0]].co[:])  # co
        x = mesh_verts[v[0]].co[0]
        y = mesh_verts[v[0]].co[1]
        z = mesh_verts[v[0]].co[2]
        dist = math.sqrt (x * x + y * y + z * z)
        if (dist > b_rad):
            b_rad = dist
    # vertex normals
    if use_normals:
        file.write ("@vn_comps 3\n")
        for i, v in enumerate(ply_verts):
            # g is for sfigs rather than dplaces
            file.write ("%.3f %.3f %.3f\n" % v[1])
    if use_uv_coords:
        file.write ("@vt_comps 2\n")
        for i, v in enumerate(ply_verts):
            file.write ("%.3f %.3f\n" % v[2])  # uv
    if use_colors:
        file.write ("@vc_comps 3\n")
        for i, v in enumerate(ply_verts):
            file.write ("%.3f %.3f %.3f\n" % v[3])  # col
    
    # TODO - tangents
    
    # per-mesh data
    # TODO skeleton
    # TODO hierarchy
    # TODO animation
    # TODO tra_keys
    # TODO rot_keys
    # TODO sca_keys
    
    file.write ("@bounding_radius %.2f\n" % b_rad)
    file.close()
    return {'FINISHED'}

def save(operator,
         context,
         filepath="",
         use_mesh_modifiers=True,
         use_normals=True,
         use_uv_coords=True,
         use_colors=True,
         global_matrix=None
         ):

    scene = context.scene
    obj = context.active_object

    if global_matrix is None:
        from mathutils import Matrix
        global_matrix = Matrix()

    if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT')

    if use_mesh_modifiers and obj.modifiers:
        mesh = obj.to_mesh(scene, True, 'PREVIEW')
    else:
        mesh = obj.data.copy()

    if not mesh:
        raise Exception("Error, could not get mesh data from active object")

    mesh.transform(global_matrix * obj.matrix_world)
    if use_normals:
        mesh.calc_normals()

    ret = apg_write(filepath, mesh,
                    use_normals=use_normals,
                    use_uv_coords=use_uv_coords,
                    use_colors=use_colors,
                    )

    if use_mesh_modifiers:
        bpy.data.meshes.remove(mesh)

    return ret

# ExportHlper is a hlper class, defines fname and invoke() which calls file
# selector
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.types import Operator
from bpy_extras.io_utils import (ImportHelper,
                                 ExportHelper,
                                 orientation_helper_factory,
                                 axis_conversion,
                                 )

class APGExporter(Operator, ExportHelper):
    """This is the tooltip/generated docs blurb"""
    bl_idname = "export_data.apg_mesh"
    bl_label = "Export APG"
    global_scale = 1.0

    filename_ext = ".apg"
    filter_glob = StringProperty(
        default="*.apg",
        options={'HIDDEN'}
        )

    def execute(self, context):

        from mathutils import Matrix
        keywords = self.as_keywords(ignore=("axis_forward",
                                            "axis_up",
                                            "global_scale",
                                            "check_existing",
                                            "filter_glob",
                                            ))
        global_matrix = axis_conversion(to_forward='Y',
                                        to_up='Z',
                                        ).to_4x4() * Matrix.Scale(self.global_scale, 4)
        keywords["global_matrix"] = global_matrix

        filepath = self.filepath
        filepath = bpy.path.ensure_ext(filepath, self.filename_ext)

        return save(self, context, **keywords)

def menu_export(self, context):
    self.layout.operator(APGExporter.bl_idname, text="APG Mesh (.apg)")


def register():
    bpy.utils.register_class(APGExporter)
    bpy.types.INFO_MT_file_export.append(menu_export)


def unregister():
    bpy.utils.unregister_class(APGExporter)
    bpy.types.INFO_MT_file_export.remove(menu_export)

if __name__ == "__main__":
    register()
