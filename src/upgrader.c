//
// this is a little utility to upgrade meshes from previous format to current
// Anton Gerdelan
// antongerdelan.net
// First version 27 Dec 2014
//

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#define VERSION "27DEC2014"

int main (int argc, char** argv) {
	FILE *fi, *fo;
	char op_fn[256], line[256];
	int vcount = 0;
	float bounding_radius = 0.0f;

	if (argc < 2) {
		printf ("usage: ./upgrade mesh.apg\n");
		return 0;
	}
	sprintf (op_fn, "updated/%s", argv[1]);
	fi = fopen (argv[1], "r");
	assert (fi);
	fo = fopen (op_fn, "w");
	assert (fo);
	while (fgets (line, 256, fi)) {
		if (strncmp (line, "@Anton", 6) == 0) {
			printf ("hdr\n");
			fprintf (fo, "@Anton's custom mesh format v.%s http://antongerdelan.net/\n", VERSION);
		} else if (strncmp (line, "@vert", 5) == 0) {
			sscanf (line, "@vert_count %i", &vcount);
			printf ("found %i vps\n", vcount);
			fputs (line, fo);
		} else if (strncmp (line, "@vp", 3) == 0) {
			int i;
			
			fputs (line, fo);
			for (i = 0; i < vcount; i++) {
				float x, y, z;
				
				if (i > 0) {
					fprintf (fo, "\n");
				}
				fscanf (fi, "%f %f %f", &x, &y, &z);
				fprintf (fo, "%.2f %.2f %.2f", x, y, z);
				
				//
				// work out distance from origin
				float d = sqrt (x * x + y * y + z * z);
				if (d > bounding_radius) {
					bounding_radius = d;
				}
			}
		} else if (strncmp (line, "@vn", 3) == 0) {
			int i;
			
			fputs (line, fo);
			for (i = 0; i < vcount; i++) {
				float x, y, z;
				
				if (i > 0) {
					fprintf (fo, "\n");
				}
				fscanf (fi, "%f %f %f", &x, &y, &z);
				fprintf (fo, "%.3g %.3g %.3g", x, y, z);
			}
		} else if (strncmp (line, "@vt ", 4) == 0) {
			int i;
			
			fputs (line, fo);
			for (i = 0; i < vcount; i++) {
				float x, y;
				
				if (i > 0) {
					fprintf (fo, "\n");
				}
				fscanf (fi, "%f %f", &x, &y);
				fprintf (fo, "%.3g %.3g", x, y);
			}
		} else if (strncmp (line, "@vtan", 5) == 0) {
			int i;
			
			fputs (line, fo);
			for (i = 0; i < vcount; i++) {
				float x, y, z, w;
				
				if (i > 0) {
					fprintf (fo, "\n");
				}
				fscanf (fi, "%f %f %f %f", &x, &y, &z, &w);
				if (isnan (x)) {
					x = 0.0f;
				}
				if (isnan (y)) {
					x = 0.0f;
				}
				if (isnan (z)) {
					x = 0.0f;
				}
				if (isnan (w)) {
					x = 0.0f;
				}
				fprintf (fo, "%.3g %.3g %.3f %.3g", x, y, z, w);
			}
		} else {
			fputs (line, fo);
		}
	}
	
	//
	// add bounding radius tag
	{
		char str[256];
		
		sprintf (str, "@bounding_radius %.2f", bounding_radius);
		fputs (str, fo);
	}
	
	fclose (fi);
	fclose (fo);
	
	return 0;
}
