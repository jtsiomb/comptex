#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#include <GL/freeglut.h>
#include <imago2.h>

#define MAGIC	"COMPTEX0"
#define MAX_LEVELS	20

/* flags */
enum {
	COMPRESSED	= 1,
};

struct level_desc {
	uint32_t offset, size;	/* offsets are relative to the end of the header */
} __attribute__ ((packed));

struct header {
	char magic[8];			/* magic identifier COMPTEX0 */
	uint32_t glfmt;			/* OpenGL internal format number */
	uint16_t flags;			/* see flags enum above */
	uint16_t levels;		/* number of mipmap levels (at least 1) */
	uint32_t width, height;	/* dimensions of mipmap level 0 */
	struct level_desc datadesc[MAX_LEVELS];	/* level data descriptors (offset:size pairs) */
	char unused[8];
} __attribute__ ((packed));

struct miplevel {
	void *data;
	int size;
};

void disp(void);
void reshape(int x, int y);
void keyb(unsigned char key, int x, int y);
unsigned char *get_comp_image(int level, int *sz);
unsigned char *texcomp(unsigned int tofmt, int level, unsigned char *pixels, int xsz, int ysz,
		unsigned int fromfmt, unsigned int fromtype, int *compsz);
int level0size(int x, int level);
const char *mkfname(const char *infile);
int save_miptree(void);
int print_info(const char *fname);

struct {
	unsigned int fmt;
	const char *name;
	int avail;
} formats[] = {
	{0x083f0, "dxt1-rgb", 0},		/* GL_COMPRESSED_RGB_S3TC_DXT1_EXT */
	{0x083f2, "dxt3-rgba", 0},		/* GL_COMPRESSED_RGBA_S3TC_DXT3_EXT */
	{0x083f3, "dxt5-rgba", 0},		/* GL_COMPRESSED_RGBA_S3TC_DXT5_EXT */
	{0x09274, "etc2-rgb", 0},		/* GL_COMPRESSED_RGB8_ETC2 */
	{0x09275, "etc2-srgb", 0},		/* GL_COMPRESSED_SRGB8_ETC2 */
	{0x09276, "etc2-rgba1", 0},	/* GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 */
	{0x09277, "etc2-srgba1", 0},	/* GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 */
	{0x09278, "etc2-rgba", 0},		/* GL_COMPRESSED_RGBA8_ETC2_EAC */
	{0x09279, "etc2-srgba", 0},	/* GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC */
	{0x09270, "eac-r11", 0},		/* GL_COMPRESSED_R11_EAC */
	{0x09271, "eac-sr11", 0},		/* GL_COMPRESSED_SIGNED_R11_EAC */
	{0x09272, "eac-rg11", 0},		/* GL_COMPRESSED_RG11_EAC */
	{0x09273, "eac-srg11", 0},		/* GL_COMPRESSED_SIGNED_RG11_EAC */
	{0x093b0, "astc4-rgba", 0},	/* GL_COMPRESSED_RGBA_ASTC_4 */
	{0x093b1, "astc5-rgba", 0},	/* GL_COMPRESSED_RGBA_ASTC_5 */
	{0x093b3, "astc6-rgba", 0},	/* GL_COMPRESSED_RGBA_ASTC_6 */
	{0x093b5, "astc8-rgba", 0},	/* GL_COMPRESSED_RGBA_ASTC_8 */
	{0x093b8, "astc10-rgba", 0},	/* GL_COMPRESSED_RGBA_ASTC_10 */
	{0x093bc, "astc12-rgba", 0},	/* GL_COMPRESSED_RGBA_ASTC_12 */
	{0x093d0, "astc4-srgba", 0},	/* GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4 */
	{0x093d1, "astc5-srgba", 0},	/* GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5 */
	{0x093d3, "astc6-srgba", 0},	/* GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6 */
	{0x093d5, "astc8-srgba", 0},	/* GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8 */
	{0x093d8, "astc10-srgba", 0},	/* GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10 */
	{0x093dc, "astc12-srgba", 0},	/* GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12 */
	{0, 0, 0}
};

unsigned int tex;
struct miplevel miptree[MAX_LEVELS];
int cur_width, cur_height;
const char *outfname;
const char *infname;
int genmip;
int cfmt;

int main(int argc, char **argv)
{
	int i, j, num_fmt;
	int level = 0;
	int *fmtlist;
	int visible = 0;

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutCreateWindow("Compressed texture converter");
	glutPositionWindow(10, 10);
	glutHideWindow();

	glutDisplayFunc(disp);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyb);

	glutMainLoopEvent();

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glEnable(GL_TEXTURE_2D);

	glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &num_fmt);
	if((fmtlist = malloc(num_fmt * sizeof *fmtlist))) {
		glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, fmtlist);

		for(i=0; i<num_fmt; i++) {
			for(j=0; formats[j].fmt; j++) {
				if(fmtlist[i] == formats[j].fmt) {
					formats[j].avail = 1;
					break;
				}
			}
		}
		free(fmtlist);
	}

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			for(j=0; formats[j].name; j++) {
				if(strcmp(argv[i] + 1, formats[j].name) == 0) {
					cfmt = formats[j].fmt;
					break;
				}
			}

			if(formats[j].name) {
				continue;
			}

			if(strcmp(argv[i], "-o") == 0) {
				outfname = argv[++i];
				if(!outfname || !*outfname) {
					fprintf(stderr, "-o must be followed by the output filename\n");
					return 1;
				}

			} else if(strcmp(argv[i], "-list") == 0 || strcmp(argv[i], "-l") == 0) {
				printf("Compressed texture formats:\n");
				for(j=0; formats[j].name; j++) {
					printf(" %s (%s)\n", formats[j].name, formats[j].avail ? "available" : "not available");
				}
				return 0;

			} else if(strcmp(argv[i], "-genmip") == 0) {
				genmip = 1;

			} else if(strcmp(argv[i], "-nogenmip") == 0) {
				genmip = 0;

			} else if(strcmp(argv[i], "-mip") == 0) {
				char *endp;
				level = strtol(argv[++i], &endp, 10);
				if(*endp) {
					fprintf(stderr, "-mip must be followed by a number (level)\n");
					return 1;
				}

			} else if(strcmp(argv[i], "-show") == 0) {
				visible = 1;
				glutShowWindow();
				glutMainLoopEvent();

			} else if(strcmp(argv[i], "-info") == 0) {
				if(print_info(argv[++i]) == -1) {
					return 1;
				}
				return 0;

			} else if(strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "-h") == 0) {
				printf("Usage: %s [options] <file1> [<file2> ... <fileN>]\n", argv[0]);
				printf("Options:\n");
				printf(" -o <file>: output file name\n");
				printf(" -<compressed texture format>: see -list\n");
				printf(" -l,-list: print all supported compressed texture formats\n");
				printf(" -genmip: enable automatic mipmap generation, and output a full tree\n");
				printf(" -nogenmip: disable automatic mipmap generation, and output only provided levels\n");
				printf(" -mip <N>: following image(s) correspond to mipmap level N\n");
				printf(" -show: show the mipmap tree\n");
				printf(" -info <file>: print information about compressed file and exit\n");
				printf(" -h,-help: print usage and exit\n");
				return 0;

			} else {
				fprintf(stderr, "invalid option %s\n", argv[i]);
			}

		} else {
			struct img_pixmap img;
			unsigned char *compix = 0;
			int compsize;
			int width0, height0;

			if(!cfmt) {
				fprintf(stderr, "you need to specify a texture compression format\n");
				return 1;
			}

			img_init(&img);
			printf("loading %s for level %d\n", argv[i], level);
			if(img_load(&img, argv[i]) == -1) {
				fprintf(stderr, "failed to load image: %s\n", argv[i]);
				return 1;
			}
			width0 = level0size(img.width, level);
			height0 = level0size(img.height, level);

			if(miptree[level].data || (cur_width > 0 && (width0 != cur_width || height0 != cur_height))) {
				/* conflict. do we just have the start of a new image and we should
				 * save the existing miptree and start over, or is it an error?
				 */
				if(miptree[0].data) {	/* if we have a 0-th mipmap, let's assume it's not an error */
					if(save_miptree() == -1) {
						return 1;
					}

					for(i=0; i<MAX_LEVELS; i++) {
						free(miptree[i].data);
						miptree[i].data = 0;
					}

					cur_width = cur_height = 0;
					infname = 0;

				} else {
					if(miptree[level].data) {
						fprintf(stderr, "trying to set a second image of mip level %d\n", level);
					} else {
						fprintf(stderr, "mip %d size (%dx%d) belongs to a %dx%d texture. current: %dx%d\n",
								level, img.width, img.height, width0, height0, cur_width, cur_height);
					}
					return 1;
				}
			}

			if(level == 0) {
				infname = argv[i];
			}

			if(cur_width <= 0) {
				cur_width = width0;
				cur_height = height0;
				glutReshapeWindow(width0 + width0 / 2, height0);
			}

			if(!(compix = texcomp(cfmt, level, img.pixels, img.width, img.height,
					img_glfmt(&img), img_gltype(&img), &compsize))) {
				img_destroy(&img);
				return 1;
			}
			miptree[level].data = compix;
			miptree[level].size = compsize;

			glutPostRedisplay();
			glutMainLoopEvent();
		}
	}

	if(miptree[0].data) {
		if(save_miptree() == -1) {
			return 1;
		}
	}

	if(visible) {
		glutPostRedisplay();
		glutMainLoop();
	}
	return 0;
}

void disp(void)
{
	int x = 0, y = 0;
	int xsz, ysz;
	int has_mipmaps;

	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &xsz);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &ysz);

	glClear(GL_COLOR_BUFFER_BIT);

	has_mipmaps = miptree[1].data || genmip;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			has_mipmaps ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);

	glBegin(GL_QUADS);
	glTexCoord2f(0, 1); glVertex2f(0, 0);
	glTexCoord2f(1, 1); glVertex2f(xsz, 0);
	glTexCoord2f(1, 0); glVertex2f(xsz, ysz);
	glTexCoord2f(0, 0);	glVertex2f(0, ysz);

	if(has_mipmaps) {
		x += xsz;
		xsz /= 2;
		ysz /= 2;

		while(xsz & ysz) {
			glTexCoord2f(0, 1); glVertex2f(x, y);
			glTexCoord2f(1, 1); glVertex2f(x + xsz, y);
			glTexCoord2f(1, 0); glVertex2f(x + xsz, y + ysz);
			glTexCoord2f(0, 0);	glVertex2f(x, y + ysz);

			y += ysz;

			xsz /= 2;
			ysz /= 2;
		}
	}
	glEnd();

	glutSwapBuffers();
	assert(glGetError() == GL_NO_ERROR);
}

void reshape(int x, int y)
{
	glViewport(0, 0, x, y);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, x, 0, y, -1, 1);
}

void keyb(unsigned char key, int x, int y)
{
	if(key == 27) {
		exit(0);
	}
}

unsigned char *get_comp_image(int level, int *sz)
{
	unsigned char *compix;
	int comp_size = 0;

	glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &comp_size);
	*sz = comp_size;

	if(!(compix = malloc(comp_size))) {
		fprintf(stderr, "failed to allocate compressed pixel buffer\n");
		glDeleteTextures(1, &tex);
		return 0;
	}
	glGetCompressedTexImage(GL_TEXTURE_2D, level, compix);
	return compix;
}

unsigned char *texcomp(unsigned int tofmt, int level, unsigned char *pixels, int xsz, int ysz,
		unsigned int fromfmt, unsigned int fromtype, int *compsz)
{
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, level == 0 && genmip ? 1 : 0);
	glTexImage2D(GL_TEXTURE_2D, level, tofmt, xsz, ysz, 0, fromfmt, fromtype, pixels);
	if(glGetError()) {
		fprintf(stderr, "failed to compress texture\n");
		glDeleteTextures(1, &tex);
		return 0;
	}

	return get_comp_image(level, compsz);
}

int level0size(int x, int level)
{
	if(level <= 0) return x;
	return level0size(x * 2, level - 1);
}

const char *mkfname(const char *infile)
{
	static char buf[512];
	const char *fname;
	char *suffix;

	if((fname = strrchr(infile, '/'))) {
		fname++;
	} else {
		fname = infile;
	}
	strcpy(buf, fname);

	if(!(suffix = strrchr(buf, '.'))) {
		suffix = buf + strlen(buf);
	}
	strcpy(suffix, ".tex");
	return buf;
}

int mip_levels(int w, int h)
{
	int n = 0;
	while(w & h) {
		w /= 2;
		h /= 2;
		n++;
	}
	return n;
}

int save_miptree(void)
{
	int i, levels;
	FILE *fp;
	struct header hdr;
	uint32_t offs = 0;

	const char *fname = outfname ? outfname : mkfname(infname);

	assert(miptree[0].data);

	/* fill in all mipmap levels if genmip is true */
	if(genmip) {
		levels = mip_levels(cur_width, cur_height);
		for(i=1; i<levels; i++) {
			if(!miptree[i].data) {
				miptree[i].data = get_comp_image(i, &miptree[i].size);
			}
		}

	} else {
		for(i=0; i<MAX_LEVELS; i++) {
			if(!miptree[i].data) {
				break;
			}
		}
		levels = i;
	}
	assert(levels >= 1);

	if(!(fp = fopen(fname, "wb"))) {
		fprintf(stderr, "failed to open %s for writing: %s\n", fname, strerror(errno));
		return -1;
	}

	memset(&hdr, 0, sizeof hdr);
	memcpy(hdr.magic, MAGIC, sizeof hdr.magic);
	hdr.glfmt = cfmt;
	hdr.flags = COMPRESSED;
	hdr.levels = levels;
	hdr.width = cur_width;
	hdr.height = cur_height;

	for(i=0; i<levels; i++) {
		hdr.datadesc[i].offset = offs;
		hdr.datadesc[i].size = miptree[i].size;
		offs += miptree[i].size;
	}
	if(fwrite(&hdr, 1, sizeof hdr, fp) < sizeof hdr) {
		fprintf(stderr, "failed to write texture file header: %s: %s\n", fname, strerror(errno));
		fclose(fp);
		return -1;
	}

	for(i=0; i<levels; i++) {
		if(fwrite(miptree[i].data, 1, miptree[i].size, fp) < miptree[i].size) {
			fprintf(stderr, "failed to write mipmap level %d (size: %d) to %s: %s\n", i,
					miptree[i].size, fname, strerror(errno));
			fclose(fp);
			return -1;
		}
	}

	fclose(fp);
	return 0;
}

int print_info(const char *fname)
{
	int i;
	FILE *fp;
	struct header hdr;

	if(!(fp = fopen(fname, "rb"))) {
		fprintf(stderr, "failed to open file: %s: %s\n", fname, strerror(errno));
		return -1;
	}
	if(fread(&hdr, 1, sizeof hdr, fp) != sizeof hdr) {
		fprintf(stderr, "failed to read compressed file header: %s: %s\n", fname, strerror(errno));
		fclose(fp);
		return -1;
	}
	fclose(fp);

	if(memcmp(hdr.magic, MAGIC, sizeof hdr.magic) != 0) {
		fprintf(stderr, "file %s is not a compressed texture file\n", fname);
		return -1;
	}

	for(i=0; formats[i].fmt; i++) {
		if(hdr.glfmt == formats[i].fmt) {
			break;
		}
	}
	printf("%s: %dx%d - %s - %d mip %s\n", fname, hdr.width, hdr.height,
			formats[i].name ? formats[i].name : "unknown", hdr.levels,
			hdr.levels > 1 ? "levels" : "level");

	for(i=0; i<hdr.levels; i++) {
		printf("  mip %d at offset: %d, size: %d bytes\n", i, hdr.datadesc[i].offset,
				hdr.datadesc[i].size);
	}
	return 0;
}
