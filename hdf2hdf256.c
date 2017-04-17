/* HDF2HDF256 : A small utility to convert a full-sectored HDF image file 
with 8 bit data into a HDF half-sectored image file. That is, to convert from:

00000000  52 53 2d 49 44 45 1a 10  00 80 00 00 00 00 00 00  |RS-IDE..........|
00000010  00 00 00 00 00 00 8a 84  d2 03 00 00 08 00 00 00  |................|
00000020  00 02 20 00 03 00 00 d4  00 00 20 20 20 20 64 72  |.. .......    dr|
00000030  69 6d 20 20 6d 61 64 65  20 20 20 20 20 20 20 20  |im  made        |
00000040  20 20 20 20 20 20 20 20  20 20 20 20 20 20 20 20  |                |
00000050  20 20 20 20 20 20 20 20  20 20 20 20 20 20 20 20  |                |
00000060  20 20 20 20 20 20 20 20  20 20 20 20 20 20 20 20  |                |
00000070  20 20 20 20 01 00 00 00  00 03 00 00 00 02 00 00  |    ............|
00000080  50 00 4c 00 55 00 53 00  49 00 44 00 45 00 44 00  |P.L.U.S.I.D.E.D.|
00000090  4f 00 53 00 20 00 20 00  20 00 20 00 20 00 20 00  |O.S. . . . . . .|
000000a0  01 00 00 00 00 00 00 00  00 00 00 00 00 00 0f 00  |................|
000000b0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|

To...

00000000  52 53 2d 49 44 45 1a 10  01 80 00 00 00 00 00 00  |RS-IDE..........|
00000010  00 00 00 00 00 00 8a 84  d2 03 00 00 08 00 00 00  |................|
00000020  00 02 20 00 03 00 00 d4  00 00 20 20 20 20 64 72  |.. .......    dr|
00000030  69 6d 20 20 6d 61 64 65  20 20 20 20 20 20 20 20  |im  made        |
00000040  20 20 20 20 20 20 20 20  20 20 20 20 20 20 20 20  |                |
00000050  20 20 20 20 20 20 20 20  20 20 20 20 20 20 20 20  |                |
00000060  20 20 20 20 20 20 20 20  20 20 20 20 20 20 20 20  |                |
00000070  20 20 20 20 01 00 00 00  00 03 00 00 00 02 00 00  |    ............|
00000080  50 4c 55 53 49 44 45 44  4f 53 20 20 20 20 20 20  |PLUSIDEDOS      |
00000090  01 00 00 00 00 00 00 0f  00 00 00 00 00 00 00 00  |................|

(C)2012 Miguel Angel Rodriguez Jodar (McLeod/IdeaFix). GPL licensed

Usage: hdf2hdf256 infile.hdf outfile.hdf

*/

#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
	FILE *fi, *fo;
	int leido;
	int i;
	unsigned char sector[512];
	unsigned char sector256[256];
	
	if (argc<3)
	{
		fprintf (stderr, "Usage: hdf2hdf256 infile.hdf outfile.hdf\n");
		return EXIT_FAILURE;
	}
	
	fi = fopen(argv[1],"rb");
	if (!fi)
	{
		fprintf (stderr, "Unable to open file '%s'\n", argv[1]);
		return EXIT_FAILURE;
	}
	
	fo = fopen(argv[2],"wb");
	if (!fo)
	{
		fprintf (stderr, "Unable to create file '%s'\n", argv[2]);
		return EXIT_FAILURE;
	}
	
	/* Read the HDF header (first 128 bytes of input file */
	leido = fread (sector, 1, 128, fi);
	if (leido<128)
	{
		fprintf (stderr, "ERROR. Premature end of file for input file.\n");
		return EXIT_FAILURE;
	}
	
	sector[8]|=1; /* set bit 0 of offset 8 to signal that this is a HDF256 file */
	fwrite (sector, 1, 128, fo);


	/* Convert and write the actual disk image */
	leido = fread (sector, 1, 512, fi);
	while (leido==512)  /* I assume remainder data to come in 512 byte chunks */
	{
		for (i=0;i<256;i++)           /* take every other byte */
			sector256[i]=sector[i*2]; /* from the 512 byte sector */
		fwrite (sector256, 1, 256, fo);
		leido = fread (sector, 1, 512, fi);
	}
	
	fclose (fi);
	fclose (fo);
	return EXIT_SUCCESS;
}
	
