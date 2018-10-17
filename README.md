comptex
=======

About
-----
Comptex is a tool for converting images to various OpenGL compressed texture
formats, and saving the result in a simple file format which can contain either
a single compressed image, or a full mipmap tree. Multiple images can be given
on the command line, one for each of the mipmap levels explicitly, or
alternatively comptex can generate the whole mipmap tree automatically from a
single image for level 0.

Comptex uses OpenGL to compress the textures, and generate mipmaps. Which means
that it needs to connect to the window system and create an OpenGL context, and
therefore can't run on a headless machine.

Comptex depends on freeglut for OpenGL context creation, and libimago
(`http://github.com/jtsiomb/libimago`) for image loading.

License
-------
Author: John Tsiombikas <nuclear@member.fsf.org>

I place this program in the public domain. Feel free to do anything you want
with it.

Usage examples
--------------
To see a list of all supported compressed texture formats, run:

    ./comptex -l

To load `foo.png`, DXT1 compress it, and make a full mipmap tree for it, run:

    ./comptex -genmip -dxt1-rgb foo.png

To load `a.png`, `b.png`, `c.png`, and `d.png` as mipmap levels 0 to 3,
compressed with ETC2, run:

    ./comptex -etc2-rgb -mip 0 a.png -mip 1 b.png -mip 2 c.png -mip 3 d.png

To print information (format, size, levels) about a compressed texture file, run:

    ./comptex -info foo.tex

File format
-----------
The compressed texture format written by comptex is extremely simple, comprising
of a header, followed by the compressed data for each mip level contained in the
file.

    struct header {
        char magic[8];          /* magic identifier "COMPTEX0" */
        uint32_t glfmt;         /* OpenGL internal format number */
        uint16_t flags;         /* bit0: compressed format */
        uint16_t levels;        /* number of mipmap levels (at least 1) */
        uint32_t width, height; /* dimensions of mipmap level 0 */
        struct level_desc datadesc[20]; /* offset:size for each lvl */
        char unused[8];
    };

Each element of the level data descriptor array is an offset from the end of the
header to the start of the compressed data for the corresponding mipmap level,
and the size in bytes of the compressed data.

Build
-----
Just type `make`, followed by `make install` as root, to install in
`/usr/local`. If you wish to install elsewhere change the `PREFIX` variable at
the top of the `Makefile`, or just copy the `comptex` binary manually.
