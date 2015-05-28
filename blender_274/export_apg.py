#
# APG mesh format exporter (.apg)
# 1st v 28 may 2015 anton gerdelan
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

#v.co - vp
#v.no - vn
#mesh.face.uv[index]; - vt

def faceToTriangles(face):
	triangles = []
	if len(face) == 4:
		triangles.append([face[0], face[1], face[2]])
		triangles.append([face[2], face[3], face[0]])
	else:
		triangles.append(face)

	return triangles

def max_in_face (face):
	rv = 0.0
	for v in face:
		for c in v:
			if (abs (c) > rv):
				rv = c
	return rv

def faceValues(face, mesh, matrix):
	fv = []
	for verti in face.vertices:
		fv.append((matrix * mesh.vertices[verti].co)[:])
	return fv

def faceToLine(face):
	return "".join([("%.2f %.2f %.2f\n" % v) for v in face])

def apg_write (context, filepath):
	scene = context.scene

	brad = 0.0
	faces = []
	for obj in context.selected_objects:
		if obj.type != 'MESH':
			try:
				me = obj.to_mesh(scene, True, "PREVIEW")
			except:
				me = None
			is_tmp_mesh = True
		else:
			me = obj.data
			if not me.tessfaces and me.polygons:
				me.calc_tessface()
			is_tmp_mesh = False

		if me is not None:
			matrix = obj.matrix_world.copy()
			for face in me.tessfaces:
				fv = faceValues(face, me, matrix)
				faces.extend(faceToTriangles(fv))

			if is_tmp_mesh:
				bpy.data.meshes.remove(me)

	# write the faces to a file
	file = open(filepath, "w")
	file.write ("@Anton's mesh format v.27DEC2014. Blender export. https://github.com/capnramses/apg_mesh/\n")
	vc = 3 * len(faces)
	file.write ("@vert_count %i\n@vp_comps 3\n" % vc)
	for face in faces:
		fm = max_in_face (face)
		if (fm > brad):
			brad = fm
		file.write(faceToLine(face))
	file.write ("@vn_comps 3\n")
	file.write ("@vt_comps 2\n")
	file.write ("@vtan_comps 4\n")
	file.write ("@bounding radius %.2f\n" % brad)
	file.close()

# ExportHlper is a hlper class, defines fname and invoke() which calls file
# selector
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.types import Operator

class APGExporter(Operator, ExportHelper):
	"""This is the tooltip/generated docs blurb"""
	bl_idname = "export_data.apg_mesh"
	bl_label = "Export APG"

	filename_ext = ".apg"
	filter_glob = StringProperty(
		default="*.apg",
		options={'HIDDEN'}
		)

	def execute(self, context):
		apg_write(context, self.filepath)

		return {'FINISHED'}

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
