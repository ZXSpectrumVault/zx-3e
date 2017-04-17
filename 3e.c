#define PROGRAM_VERSION "0.5alpha"

/*

'3e': Utility for handing files in a memory card formatted for a +3E
enhanced Spectrum.

(C)2009-2012 Miguel Angel Rodriguez Jodar (mcleod_ideafix). GPL Licensed.
For support, please check http://www.zxprojects.com


CHANGELOG and USAGE
See the readme.txt file for details.

COMPILING
See the install.txt file for details.

*/

#ifdef WIN32
#define _CRT_SECURE_NO_WARNINGS
#define DIR_SEP '\\'
#else
#define DIR_SEP '/'
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifndef WIN32
#include <libgen.h>
#include <sys/io.h>
#else
#include <io.h>
#endif
#include <fcntl.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef WIN32
#define stricmp strcasecmp
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#ifdef WIN32
typedef unsigned __int64 u64;
#else
typedef unsigned long long u64;
#endif

/*
XDPB structure:

Bytes 0...1	SPT records per track
Byte 2		BSH log2(block size / 128)
Byte 3		BLM block size / 128 - 1
Byte 4		EXM extent mask
Bytes 5...6	DSM last block number
Bytes 7...8	DRM last directory entry number
Byte 9		AL0 directory bit map
Byte 10		AL1 directory bit map
Bytes 11...12	CKS size of checksum vector (bit 15 = permanent)
Bytes 13...14	OFF number of reserved tracks
Byte 15		PSH log2(sector size / 128)
Byte 16		PHM sector size / 128 - 1
Byte 17		Bits 0...1 Sidedness
			0 = Single sided
			1 = Double sided (alternating sides)
			2 = Double sided (successive sides)
		Bits 2...6 Reserved (set to 0)
		Bit 7 Double track
Byte 18		Number of tracks per side
Byte 19		Number of sectors per track
Byte 20		First sector number
Bytes 21...22	Sector size
Byte 23		Gap length (/write)
Byte 24		Gap length (format)
Byte 25		Bit 7 Multi-track operation
			1 = multi-track
			0 = single track
		Bit 6 Modulation mode
			1 = MFM mode
			0 = FM mode
		Bit 5 Skip deleted data address mark
			1 = skip deleted data address mark
			0 = don't skip deleted address mark
		Bits 0...4 = 0
Byte 26		Freeze flag
			00h (0) = auto-detect disk format
			FFh (255) = don't auto-detect disk format

*/
typedef struct stXDPB
{
	u16 spt;
	u8 bsh;
	u8 blm;
	u8 exm;
	u16 dsm;
	u16 drm;
	u8 al0;
	u8 al1;
	u16 cks;
	u16 off;
	u8 psh;
	u8 phm;
	u8 sideness;
	u8 tracks_per_side;
	u8 sectors_per_track;
	u8 first_sector;
	u16 sector_size;
	u8 gap_length_rw;
	u8 gap_length_fmt;
	u8 mlt_track;
	u8 freeze_flag;
} tXDPB;

typedef struct stDiskPar
{
	u16 cyl;
	u8 head;
	u8 sect;
	u16 sect_per_cyl;
	u16 last_pentry;
} tDiskPar;

/*
The 64 bytes partition entry 
Offset	Length	Description
+0	16	Partition name (case-insensitive, space-padded).
+16	16	Partition definition.

+16	1	Partition type (0=free handle)
+17	2	Starting cylinder.
+19	1	Starting head.
+20	2	Ending cylinder.
+22	1	Ending head.
+23	4	Largest logical sector number.
+27	5	Type-specific information.
+32	32	Type-specific information.

Partition types 
number 	Description
#00 	Unused partition/free handle.
#01 	System partition. The first partition on a disk, starting at phisical sector 1 (cylinder 0, head 0 or 1), is always the system partition and contains a list of 64-byte partition entries that define all the partitions on the disk (including the system one). Only one partition of this type is permitted on a disk, and this is always the first partition. The name is always "PLUSIDEDOS" (followed by 6 spaces).
#02 	Swap partition.
#03 	+3DOS partition. The maximum theoretical size for a +3DOS partition is just under 32Mb. The XDPB has logical geometry.
#04 	CP/M partition with XDPB that reflects phisical disk structure. So if the disk has 17 spt, the LSPT is 68. The partition uses always integer number of cylinders and uses whole cylinder (from head 0). Otherwise (when from not track 0) this is converted to reserved tracks (OFF in XDPB). This is required for my DSKHNDLR low level disk drivers.
#05 	Boot partition. This is only one file, stored as a partition. Used to boot a hardware. Eg. Timex FDD 3000 extedend with YABUS.TF, will search the IDEDOS partiton table to find "YABUS.TF" partition. If found, the partition contents is loaded into RAM and started. The partition size is usually 8k to 64kB, what gives 1..2 tracks (or 1..8 track for disks with 17 spt). The number of sectors to load is in partition definition.
#10 	MS-DOS (FAT16) partition.
#20 	UZI(X) partition.
#30 	TR-DOS diskimage partition. Usually 640kB. Sector offset.
#31 	SAMDOS diskimage partition (B-DOS record), 800kB. Sector offset.
#32 	MB-02 diskimage partition. Usually 1804kB. Sector offset.
#FE 	Bad disk space.
#FF 	Free disk space.
*/

typedef struct stParEntry
{
	u8 name[17];
	u8 type;
	u8 drive_letter;
	u16 cyl_start;
	u8 head_start;
	u16 cyl_end;
	u8 head_end;
	u32 last_sector;
	u32 FirstLBA;
	u32 LastLBA;
	tXDPB xdpb; /* only for +3DOS partitions */
	tDiskPar DiskPar; /* only for system partition */
} tParEntry;

/*
Directory entries - The directory is a sequence of directory entries (also called extents), which contain 
32 bytes of the following structure:

    St F0 F1 F2 F3 F4 F5 F6 F7 E0 E1 E2 Xl Bc Xh Rc
    Al Al Al Al Al Al Al Al Al Al Al Al Al Al Al Al
*/
typedef struct stDirEntry
{
	char fname[12];
	u8 type;
	u32 nbytes;
	u8 read_only;
	u8 system_file;
	u8 archived;
	u16 extents[256];
	u16 extused;
	u16 blocks[2048];
	u16 blused;
} tDirEntry;

/*
	Bytes 0...7	- +3DOS signature - 'PLUS3DOS'
	Byte 8		- 1Ah (26) Soft-EOF (end of file)
	Byte 9		- Issue number
	Byte 10		- Version number
	Bytes 11...14	- Length of the file in bytes, 32 bit number,
			    least significant byte in lowest address
	Bytes 15...22	- +3 BASIC header data
	Bytes 23...126	- Reserved (set to 0)
	Byte 127	- Checksum (sum of bytes 0...126 modulo 256)
*/
typedef struct stP3Header
{
	u8 issue;
	u8 version;
	u32 nbytes;
	u8 type;
	u16 length;
	u16 start;
	u16 vars;
} tP3Header;


int HalvedHDF=0;  /* flag for halved sector HDF's */
int HalvedDisks=0;  /* flag for halved sector physical disks or raw images */
int SectorSize=512;  /* normally 512, unless halved disks are in use */

u8 sep=',';   /* separator for formatted output */
u8 ptype=0x7f; /* partition type to store an IDEPLUSDOS hard disk inside an IBM partition */

u32 OffsHeader;  /* offset to the real start of disk image. */
u32 StartIDEDOS;  /* offset to the beginning of the IDEDOS partition table */

tParEntry *ParTable=NULL;
int num_parts=0;

u8 *RawDirectory=NULL;  /* the actual directory (raw data) */

u8 *BlockMap=NULL;   /* List of blocks currently used. */
int blocks_filesystem;

tDirEntry *Directory=NULL;
int num_dentries=0;


#ifdef WIN32
char *basename (char *s)
{
	char *pos;

	pos=strrchr(s,'.');  /* strip extension if any */
	if (pos)
		pos[0]='\0';
	pos=strrchr(s, DIR_SEP);
	if (!pos)
		return s;
	else
		return pos+1;
}
#endif

/* Removes spaces at the beginning and at the end of a string. BEWARE: modifies the source string. */
char *strtrim (char *s)
{
	int i;

	for (i=strlen(s)-1;i>=0;i--)
		if (s[i]!=' ')
			break;
	s[i+1]='\0';
	for (i=0;s[i];i++)
		if (s[i]!=' ')
			break;
	return (&s[i]);
}


u32 next512mult (u32 n)
{
	if (n%512==0)
	    return n;
    else
    	return ((n+512)/512)*512;
} 			


int FileExists (u8 *fname)
{
	int i;

	for (i=0;i<num_dentries;i++)
		if (strncmp(fname, Directory[i].fname, 11)==0)
			return i;
	return -1;
}


int FindPartition (char *part)
{
	int i;

	for (i=0;i<num_parts;i++)
		if (!strncmp (part, ParTable[i].name, strlen(part)))
			return i;
	return -1;
}


/* Read/Writes LE 16 bit and 32 bit values */
u16 read16 (u8 *p)
{
	u16 res;

	res=p[0] | (p[1]<<8);
	return res;
}


u32 read32 (u8 *p)
{
	u32 res;

	res=p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
	return res;
}


void write16 (u8 *p, u16 v)
{
	p[0]=v&0xFF;
	p[1]=(v>>8)&0xFF;
}


void write32 (u8 *p, u32 v)
{
	p[0] = v&0xFF;
	p[1] = (v>>8)&0xFF;
	p[2] = (v>>16)&0xFF;
	p[3] = (v>>24)&0xFF;
}


void Cleanup (void)
{
	if (BlockMap)
		free (BlockMap);
	if (RawDirectory)
		free (RawDirectory);
	if (Directory)
		free (Directory);
	/*if (ParTable)
		free (ParTable);*/

	BlockMap = NULL;
	RawDirectory = NULL;
	Directory = NULL;
	/*ParTable = NULL;*/
	blocks_filesystem=0;
	num_dentries=0;
}


#define OFFS_TO_PARTS 0x1be
#define OFFS_TO_PTYPE 0x4
#define OFFS_TO_PSTART 0x8
int search_plusidedos_shared_disk (int fd, u8 *sector)
{
	int i;
	int bc;
	u32 lba_start;
	
	/* is this a valid MBR */
	if (sector[510]!=0x55 || sector[511]!=0xAA)
		return 0;
	
	/* Search for partition type 7F */
	for (i=0;i<4;i++)
		if (sector[OFFS_TO_PARTS+i*16+OFFS_TO_PTYPE]==ptype)
			break;
	if (i==4) /* if not found, it's not considered a shared disk */
		return 0;
	
	lba_start = read32 (sector+OFFS_TO_PARTS+i*16+OFFS_TO_PSTART);
	StartIDEDOS = lba_start*512;
	lseek (fd, OffsHeader+StartIDEDOS, SEEK_SET);  /* jump to the start of the IDEDOS partition. */
	bc = read (fd, sector, SectorSize);
	if (bc<SectorSize)
		return 0;
	if (strncmp (sector, "PLUSIDEDOS", 10)==0)
		return 1;
	else
		return 0;
}


int get_partition_table (int fd)
{
	u8 *buff; /* partition table buffer. */
	u8 sector[512];
	tParEntry temp;
	u8 *pxdbp;
	u8 *buffer;
	int bc;
	int i=0;
	int part_max, free_part;

	StartIDEDOS = 0;
	lseek (fd, OffsHeader+StartIDEDOS, SEEK_SET);  /* jump over the HDF header, if any. */

	bc = read (fd, sector, SectorSize);  /* read first sector, with system partition */
	if (bc<SectorSize)
		return 0;

	/* first partition must be system partition */
	if (strncmp (sector, "PLUSIDEDOS", 10))
	{	
		/* TODO: Check if this is shared disk */
		if (search_plusidedos_shared_disk (fd, sector)==0) /* this will change StartIDEDOS */
			return 0;
	}
	
	part_max=1+read16(sector+38); /* Max. number of partitions available. */
	buff=(u8 *)malloc(next512mult(part_max*64)); /* disk buffer to read all partitions */
	ParTable=(tParEntry *)malloc(part_max * sizeof(tParEntry));

	lseek (fd, OffsHeader+StartIDEDOS, SEEK_SET);  /* reset file position to first partition. */
	bc = read (fd, buff, next512mult(part_max*64));
	if (bc<0)
	{
		free (buff);
		free (ParTable);
		return 0;
	}
	buffer=buff;
	for (i=0;i<part_max;i++)
	{
		/* Jump over a unused entry */
		if (buffer[16]==0)
		{
			buffer+=64;
			continue;
		}
		
		/* Get partition entry parms */
		strncpy (ParTable[num_parts].name, buffer, 16);
		ParTable[num_parts].name[16]='\0';
		ParTable[num_parts].type=buffer[16];
		if (ParTable[num_parts].type==255)  /* if it's the partition that holds free space, change the name to ----- */
			strcpy (ParTable[num_parts].name, "----------------");
		ParTable[num_parts].cyl_start=read16(buffer+17);
		ParTable[num_parts].head_start=buffer[19];
		ParTable[num_parts].cyl_end=read16(buffer+20);
		ParTable[num_parts].head_end=buffer[22];
		ParTable[num_parts].last_sector=read32(buffer+23);
		if (num_parts==0)
			ParTable[num_parts].FirstLBA=0;
		else
			ParTable[num_parts].FirstLBA=ParTable[num_parts].cyl_start*ParTable[0].DiskPar.head*ParTable[0].DiskPar.sect+
			                             ParTable[num_parts].head_start*ParTable[0].DiskPar.sect;
		ParTable[num_parts].LastLBA=ParTable[num_parts].FirstLBA+ParTable[i].last_sector;
		ParTable[num_parts].drive_letter=buffer[60]; /* drive letter mapped to this partition */

		/* Get XDBP table */
		if (ParTable[num_parts].type==3)
		{
			pxdbp=buffer+32;
			ParTable[num_parts].xdpb.spt=read16(pxdbp);
			ParTable[num_parts].xdpb.bsh=pxdbp[2];
			ParTable[num_parts].xdpb.blm=pxdbp[3];
			ParTable[num_parts].xdpb.exm=pxdbp[4];
			ParTable[num_parts].xdpb.dsm=read16(pxdbp+5);
			ParTable[num_parts].xdpb.drm=read16(pxdbp+7);
			ParTable[num_parts].xdpb.al0=pxdbp[9];
			ParTable[num_parts].xdpb.al1=pxdbp[10];
			ParTable[num_parts].xdpb.cks=read16(pxdbp+11);
			ParTable[num_parts].xdpb.off=read16(pxdbp+13);
			ParTable[num_parts].xdpb.psh=pxdbp[15];
			ParTable[num_parts].xdpb.phm=pxdbp[16];
			ParTable[num_parts].xdpb.sideness=pxdbp[17];
			ParTable[num_parts].xdpb.tracks_per_side=pxdbp[18];
			ParTable[num_parts].xdpb.sectors_per_track=pxdbp[19];
			ParTable[num_parts].xdpb.first_sector=pxdbp[20];
			ParTable[num_parts].xdpb.sector_size=read16(pxdbp+21);
			ParTable[num_parts].xdpb.gap_length_rw=pxdbp[23];
			ParTable[num_parts].xdpb.gap_length_fmt=pxdbp[24];
			ParTable[num_parts].xdpb.mlt_track=pxdbp[25];
			ParTable[num_parts].xdpb.freeze_flag=pxdbp[26];
		}
		else if (ParTable[num_parts].type==1)
		{
			ParTable[num_parts].DiskPar.cyl=read16(buffer+32);
			ParTable[num_parts].DiskPar.head=buffer[34];
			ParTable[num_parts].DiskPar.sect=buffer[35];
			ParTable[num_parts].DiskPar.sect_per_cyl=read16(buffer+36);
			ParTable[num_parts].DiskPar.last_pentry=read16(buffer+38);
		}

		num_parts++;
		buffer+=64;

		/* Assume that the free partition type is the last one. */
		/* Record the position for the "FREE" partition, to put it the last one
		for listing purposes. */
		if (ParTable[num_parts-1].type==255)
			free_part = num_parts-1;
	}
	
	if (free_part!=num_parts-1) /* if it's not the last partition */
	{
		temp=ParTable[num_parts-1];
		ParTable[num_parts-1]=ParTable[free_part];
		ParTable[free_part]=temp;
	}

	free (buff);
	return 1;
}


void show_partition_table (void)
{
	int i;
	char *typ;
	char type_drive[20];

	printf ("\nDisk geometry (C/H/S): %d/%d/%d\n", ParTable[0].DiskPar.cyl, ParTable[0].DiskPar.head, ParTable[0].DiskPar.sect);
	printf ("\nPARTITION TABLE\n");
	printf ("\nNum. Name             Type    CH Begin  CH End   LBA Begin LBA End   Size\n");
	printf ("---------------------------------------------------------------------------\n");
	for (i=0;i<num_parts;i++)
	{
		printf ("%4d %16.16s ", i, ParTable[i].name);
		switch (ParTable[i].type)
		{
			case 0: typ="Free"; break;
			case 1: typ="System"; break;
			case 2: typ="Swap"; break;
			case 3: typ="+3DOS"; break;
			case 255: typ="FREE"; break;
			default: typ="Other"; break;
		}
		if (ParTable[i].drive_letter>='A' && ParTable[i].drive_letter<='Z')
			sprintf (type_drive, "%s %c:", typ, ParTable[i].drive_letter);
		else
			strcpy (type_drive, typ);

		printf ("%-7.7s", type_drive);
		printf ("%5d/%-2d  %5d/%-2d  %-9d %-9d %-4dMB\n", 
			ParTable[i].cyl_start,
			ParTable[i].head_start,
			ParTable[i].cyl_end,
			ParTable[i].head_end,
			ParTable[i].FirstLBA,
			ParTable[i].LastLBA,
			(ParTable[i].LastLBA-ParTable[i].FirstLBA+1)/2048);
	}
	printf ("\n%d partition entries remain unassigned.\n", ParTable[0].DiskPar.last_pentry + 1 - num_parts);
}


void show_partition_table_backend (void)
{
	int i;

	printf ("%d%c%d%c%d\n", ParTable[0].DiskPar.cyl, sep, ParTable[0].DiskPar.head, sep, ParTable[0].DiskPar.sect);
	printf ("%d%c%d\n", num_parts, sep, ParTable[0].DiskPar.last_pentry + 1 - num_parts);
	for (i=0;i<num_parts;i++)
	{
		printf ("%s%c%d%c", strtrim(ParTable[i].name), sep, ParTable[i].type, sep);
		if (ParTable[i].drive_letter>='A' && ParTable[i].drive_letter<='Z')
			printf ("%c%c", ParTable[i].drive_letter, sep);
		else
			printf ("%c", sep);

#ifdef WIN32
		printf ("%d%c%d%c%d%c%d%c%d%c%d%c%I64u\n", 
#else
		printf ("%d%c%d%c%d%c%d%c%d%c%d%c%llu\n", 
#endif		
			ParTable[i].cyl_start, sep, 
			ParTable[i].head_start, sep, 
			ParTable[i].cyl_end, sep, 
			ParTable[i].head_end, sep, 
			ParTable[i].FirstLBA, sep, 
			ParTable[i].LastLBA, sep, 
			(u64)(ParTable[i].LastLBA-ParTable[i].FirstLBA+1)*SectorSize*1L);
	}
}


void show_partition_table_bare (void)
{
	int i;

	for (i=0;i<num_parts;i++)
		if (ParTable[i].type==3)
			printf ("%s\n", strtrim(ParTable[i].name));
}


int show_partition_entry (char *part)
{
	char *typ;
	int np;

	np=FindPartition (part);
	if (np<0)
		np=atoi(part);

	if (np<0 || np>=num_parts)
	{
		fprintf (stderr, "Incorrect partition number.\n");
		return 0;
	}

	if (ParTable[np].type!=3)
	{
		fprintf (stderr, "Details for this partition are not available.\n");
		return 0;
	}

	printf ("\nDETAILS FOR PARTITION: %s\n", ParTable[np].name);
	switch (ParTable[np].type)
	{
		case 0: typ="Free"; break;
		case 1: typ="System"; break;
		case 2: typ="Swap"; break;
		case 3: typ="+3DOS"; break;
		case 255: typ="FREE"; break;
		default: typ="Other"; break;
	}

	printf ("Type: %s", typ);
	if (ParTable[np].drive_letter>='A' && ParTable[np].drive_letter<='Z')
		printf (" mapped to %c:\n", ParTable[np].drive_letter);
	else
		printf ("\n");
	printf ("First LBA address: %d\n", ParTable[np].FirstLBA);
	printf ("Last LBA address: %d\n", ParTable[np].LastLBA);
	printf ("Size (MBytes): %d MB\n", (ParTable[np].LastLBA-ParTable[np].FirstLBA+1)/2048);
	printf ("Reserved bytes from begining of partition: %d\n", ParTable[np].xdpb.spt * ParTable[np].xdpb.off * 128);
	printf ("Block size: %d bytes\n", (ParTable[np].xdpb.blm+1)*128);
	printf ("Directory entries: %d\n", ParTable[np].xdpb.drm+1);
	printf ("Offset to data area (directory size): %Xh\n", (ParTable[np].xdpb.spt * ParTable[np].xdpb.off * 128) + (ParTable[np].xdpb.drm+1)*32);
	printf ("Data size for this filesystem: %d bytes.\n", (ParTable[np].xdpb.dsm+1) * (ParTable[np].xdpb.blm+1) * 128);
	printf ("\nXDBP parms:\n");
	printf (" SPT : %d\t\t", ParTable[np].xdpb.spt);
	printf (" BSH : %d\n", ParTable[np].xdpb.bsh);
	printf (" BLM : %d\t\t", ParTable[np].xdpb.blm);
	printf (" EXM : %d\n", ParTable[np].xdpb.exm);
	printf (" DSM : %d\t\t", ParTable[np].xdpb.dsm);
	printf (" DRM : %d\n", ParTable[np].xdpb.drm);
	printf (" AL0 : %d\t\t", ParTable[np].xdpb.al0);
	printf (" AL1 : %d\n", ParTable[np].xdpb.al1);
	printf (" CKS : %d\t\t", ParTable[np].xdpb.cks);
	printf (" OFF : %d\n", ParTable[np].xdpb.off);
	printf (" PSH : %d\t\t", ParTable[np].xdpb.psh);
	printf (" PHM : %d\n", ParTable[np].xdpb.phm);
	printf (" Sideness : %d\t\t\t", ParTable[np].xdpb.sideness);
	printf (" Tracks per side : %d\n", ParTable[np].xdpb.tracks_per_side);
	printf (" Sectors per track : %d\t", ParTable[np].xdpb.sectors_per_track);
	printf (" First sector # : %d\n", ParTable[np].xdpb.first_sector);
	printf (" Sector size : %d\t\t", ParTable[np].xdpb.sector_size);
	printf (" GAP length R/W : %d\n", ParTable[np].xdpb.gap_length_rw);
	printf (" GAP length fmt : %d\t\t", ParTable[np].xdpb.gap_length_fmt);
	printf (" Multitrack : %d\n", ParTable[np].xdpb.mlt_track);
	printf (" Freeze flag : %d\n", ParTable[np].xdpb.freeze_flag);
	
	return 1;
}


int show_partition_entry_backend (char *part)
{
	int np;

	np=FindPartition (part);
	if (np<0)
		np=atoi(part);

	if (np<0 || np>=num_parts)
	{
		fprintf (stderr, "Incorrect partition number.\n");
		return 0;
	}

	if (ParTable[np].type!=3)
	{
		fprintf (stderr, "Details for this partition are not available.\n");
		return 0;
	}

	printf ("%s%c%d%c", strtrim(ParTable[np].name), sep, ParTable[np].type, sep);
	if (ParTable[np].drive_letter>='A' && ParTable[np].drive_letter<='Z')
		printf ("%c%c", ParTable[np].drive_letter, sep);
	else
		printf ("%c", sep);

	printf ("%d%c", ParTable[np].FirstLBA, sep);
	printf ("%d%c", ParTable[np].LastLBA, sep);
	printf ("%d%c", (ParTable[np].LastLBA-ParTable[np].FirstLBA+1)*SectorSize, sep);
	printf ("%d%c", ParTable[np].xdpb.spt * ParTable[np].xdpb.off * 128, sep);
	printf ("%d%c", (ParTable[np].xdpb.blm+1)*128, sep);
	printf ("%d%c", ParTable[np].xdpb.drm+1, sep);
	printf ("%d%c", (ParTable[np].xdpb.spt * ParTable[np].xdpb.off * 128) + (ParTable[np].xdpb.drm+1)*32, sep);
	printf ("%d%c", (ParTable[np].xdpb.dsm+1) * (ParTable[np].xdpb.blm+1) * 128, sep);
	printf ("%d%c", ParTable[np].xdpb.spt, sep);
	printf ("%d%c", ParTable[np].xdpb.bsh, sep);
	printf ("%d%c", ParTable[np].xdpb.blm, sep);
	printf ("%d%c", ParTable[np].xdpb.exm, sep);
	printf ("%d%c", ParTable[np].xdpb.dsm, sep);
	printf ("%d%c", ParTable[np].xdpb.drm, sep);
	printf ("%d%c", ParTable[np].xdpb.al0, sep);
	printf ("%d%c", ParTable[np].xdpb.al1, sep);
	printf ("%d%c", ParTable[np].xdpb.cks, sep);
	printf ("%d%c", ParTable[np].xdpb.off, sep);
	printf ("%d%c", ParTable[np].xdpb.psh, sep);
	printf ("%d%c", ParTable[np].xdpb.phm, sep);
	printf ("%d%c", ParTable[np].xdpb.sideness, sep);
	printf ("%d%c", ParTable[np].xdpb.tracks_per_side, sep);
	printf ("%d%c", ParTable[np].xdpb.sectors_per_track, sep);
	printf ("%d%c", ParTable[np].xdpb.first_sector, sep);
	printf ("%d%c", ParTable[np].xdpb.sector_size, sep);
	printf ("%d%c", ParTable[np].xdpb.gap_length_rw, sep);
	printf ("%d%c", ParTable[np].xdpb.gap_length_fmt, sep);
	printf ("%d%c", ParTable[np].xdpb.mlt_track, sep);
	printf ("%d\n", ParTable[np].xdpb.freeze_flag);
	
	return 1;
}


int get_directory (int fd, char *part)
{
	int i,j,bc,bl;
	u8 *dentry,*dentr2;
	u16 blnum;
	int np;

	np=FindPartition (part);
	if (np<0)
		np=atoi(part);

	if (np<0 || np>=num_parts)
	{
		fprintf (stderr, "Invalid partition number.\n");
		return 0;
	}

	if (ParTable[np].type!=3)
	{
		fprintf (stderr, "Unable to obtain directory for this partition.\n");
		return 0;
	}

	/* Seek for the beginning of directory */
	lseek (fd, OffsHeader + 						/* OffsHeader from beginning of disk image. */
		       ParTable[np].FirstLBA * SectorSize + 	/* Offset to the beginning of partition */
			   ParTable[np].xdpb.spt * ParTable[np].xdpb.off * 128, 	/* Offset to beginning of directory. */
			   SEEK_SET);

	/* Read the whole directory. Each entry fills 32 bytes */
	RawDirectory=(u8 *)malloc((ParTable[np].xdpb.drm+1)*32);
	bc=read (fd, RawDirectory, (ParTable[np].xdpb.drm+1)*32);
	if (bc<(ParTable[np].xdpb.drm+1)*32)
	{
		fprintf (stderr, "Error reading directory table.\n");
		free(RawDirectory);
		return 0;
	}

	Directory=(tDirEntry *)malloc((ParTable[np].xdpb.drm+1)*sizeof(tDirEntry));
	blocks_filesystem=(ParTable[np].xdpb.dsm+1);
	BlockMap=(u8 *)malloc(blocks_filesystem);
	memset (BlockMap, 0, blocks_filesystem);
	for (i=0;i<(ParTable[np].xdpb.drm+1)*32/((ParTable[np].xdpb.blm+1)*128);i++)
		BlockMap[i]=1;  /* Mark directory blocks as used. */

	num_dentries=0;
	for (i=0;i<=ParTable[np].xdpb.drm;i++)
	{
		dentry=RawDirectory+i*32;  /* point to next dentry */
		if (dentry[0]==0xe5)  /* free dentry */
			continue;

		if (FileExists (dentry+1)>=0)  /* this denotes a file with more than one extent */
			continue;

		Directory[num_dentries].type=dentry[0];
		for (j=0;j<11;j++)
			Directory[num_dentries].fname[j]=dentry[1+j] & 0x7f;
		Directory[num_dentries].fname[11]='\0';
		Directory[num_dentries].read_only=(dentry[9]&0x80)?1:0;
		Directory[num_dentries].system_file=(dentry[10]&0x80)?1:0;
		Directory[num_dentries].archived=(dentry[11]&0x80)?1:0;

		/* Find and add all extents that belong to the same file */
		if (Directory[num_dentries].type<=33)
		{
			Directory[num_dentries].extused=0;
			Directory[num_dentries].blused=0;
			Directory[num_dentries].nbytes=0;
			for (j=0;j<=ParTable[np].xdpb.drm;j++)
			{
				dentr2=RawDirectory+j*32;  /* point to next dentry */
				if (dentr2[0]<=33 && strncmp(Directory[num_dentries].fname, dentr2+1, 11)==0) /* found a new extent for this file */
				{
					Directory[num_dentries].extents[Directory[num_dentries].extused++] = j;
					for (bl=0;bl<8;bl++)  /* Add all blocks that this extent uses to the list of blocks for this file */
					{
						blnum=read16(dentr2+16+bl*2);
						if (blnum)
						{
							Directory[num_dentries].blocks[Directory[num_dentries].blused++]=blnum;							
							Directory[num_dentries].nbytes += (ParTable[np].xdpb.blm+1)*128;
							/* Update block map with blocks used by this file */
							BlockMap[blnum]=1;
						}
						else
							break; /* no more blocks for this file */
					}
					if (dentr2[15]!=128)  /* I assume that a Rc value of 128 means all the extent is used. */
					{
						/* Substract number of bytes in the last logical extent. */
						if (Directory[num_dentries].nbytes % 16384 == 0)
							Directory[num_dentries].nbytes -= 16384;
						else
							Directory[num_dentries].nbytes = 16384*(Directory[num_dentries].nbytes/16384);
						Directory[num_dentries].nbytes += (dentr2[15]-1)*128;  /* number of 128 records filled in last logical extent. */
						Directory[num_dentries].nbytes += (dentr2[13]==0)?128:dentr2[13]; /* add number of bytes in last record (0 means 128) */
					}
				}				
			}
		}
		num_dentries++;
	}
	return np;  /* will be >0, as partition 0 is system partition and cannot be directory processed */
}


int get_3dos_header (int fd, int np, int nf, tP3Header *header)
{
	tP3Header h;
	u8 head[512];
	int bc,res;

	/* Seek for the beginning of directory */
	res=lseek (fd, OffsHeader + 						/* OffsHeader from beginning of disk image. */
		       ParTable[np].FirstLBA * SectorSize + 	/* Offset to the beginning of partition */
			   ParTable[np].xdpb.spt * ParTable[np].xdpb.off * 128 + 	/* Offset to beginning of blocks (directory starts at block 0). */			  
			   (Directory[nf].blocks[0]*(ParTable[np].xdpb.blm+1)*128),  /* Offset to block 0 of this file. */
			   SEEK_SET);
	bc=read (fd, head, 512);  /* actually, we need only the first 128 bytes. */
	if (bc<128)
		return 0;
	if (strncmp(head,"PLUS3DOS",8))
		return 0;  /* return if +3DOS header not found. */

	h.issue=head[9];
	h.version=head[10];
	h.nbytes=read32(head+11);
	h.type=head[15];
	h.length=read16(head+16);
	h.start=read16(head+18);
	h.vars=read16(head+20);

	*header = h;
	return 1;
}


void show_directory (int fd, int np)
{
	int i;
	tP3Header p3h;
	char *typ;

	printf ("Directory for %s\n\n", ParTable[np].name);
	printf ("Name          Disksize  Att  Ver  HdSize    Type        RSize  Start  Vars \n");
	printf ("---------------------------------------------------------------------------\n");

	for (i=0;i<num_dentries;i++)
	{
		if (Directory[i].type>33)
			continue;

		printf ("%-8.8s.%-3.3s  ", Directory[i].fname, Directory[i].fname+8);
		printf ("%-8d  ", Directory[i].nbytes);
		printf ("%c%c%c  ", (Directory[i].read_only)?'R':' ', (Directory[i].system_file)?'S':' ', (Directory[i].archived)?'A':' ');
		if (get_3dos_header (fd, np, i, &p3h))
		{
			printf ("%d.%d  ", p3h.issue, p3h.version);
			printf ("%-8d  ", p3h.nbytes);
			switch (p3h.type)
			{
				case 0: typ="Program"; break;
				case 1: typ="Num array"; break;
				case 2: typ="Char array"; break;
				case 3: typ="Bytes"; break;
				default: typ="Unknown"; break;
			}
			printf ("%-10.10s  ", typ);
			printf ("%-5d  %-5d  %-5d  ", p3h.length, p3h.start, p3h.vars);
		}
		else
			printf ("Headerless file.");

		printf ("\n");
	}
}


void show_directory_backend (int fd, int np)
{
	tP3Header p3h;
	int i;
	char name[9];
	char exts[4];

	printf ("%d\n", num_dentries);
	for (i=0;i<num_dentries;i++)
	{
		if (Directory[i].type>33)
			continue;

		strncpy (name, Directory[i].fname, 8);
		name[8]='\0';
		strncpy (exts, Directory[i].fname+8, 3);
		exts[3]='\0';
		if (strlen(strtrim(exts))>0)
			printf ("%s.%s%c", strtrim(name), strtrim(exts), sep);
		else
			printf ("%s%c", strtrim(name), sep);

		printf ("%d%c", Directory[i].nbytes, sep);
		printf ("%c%c%c%c", (Directory[i].read_only)?'R':' ', (Directory[i].system_file)?'S':' ', (Directory[i].archived)?'A':' ', sep);
		if (get_3dos_header (fd, np, i, &p3h))
		{
			printf ("%d%c%d%c", p3h.issue, sep, p3h.version, sep);
			printf ("%d%c", p3h.nbytes, sep);
			printf ("%d%c", p3h.type, sep);
			printf ("%d%c%d%c%d", p3h.length, sep, p3h.start, sep, p3h.vars);
		}
		else
			printf ("1%c0%c%d%c255%c%d%c0%c32768", sep, sep, Directory[i].nbytes, sep, sep, Directory[i].nbytes, sep, sep);

		printf ("\n");
	}
}


void show_directory_bare (int fd, int np)
{
	int i;
	char name[9];
	char exts[4];

	for (i=0;i<num_dentries;i++)
	{
		if (Directory[i].type>33)
			continue;

		strncpy (name, Directory[i].fname, 8);
		name[8]='\0';
		strncpy (exts, Directory[i].fname+8, 3);
		exts[3]='\0';
		if (strlen(strtrim(exts))>0)
			printf ("%s.%s\n", strtrim(name), strtrim(exts));
		else
			printf ("%s\n", strtrim(name));
	}
}


int PC2CPM (int fd, char *fname, int *npar, int *nfil, char *filename, char *outname)
{
	char *name, *ext;
	char fnam[100];
	int np,i;

	strcpy (fnam, fname);

	name=strchr(fnam,':');
	if (!name)
	{
		fprintf (stderr, "Bad file name: missing partition. Format is: partition:name[.ext]\n");
		return 0;
	}
	name[0]='\0';  /* writes a NULL instead of : */

	np=FindPartition (fnam);
	if (np<0)
		np=atoi(fnam);

	if (np<0 || np>=num_parts || ParTable[np].type!=3)
	{
		fprintf (stderr, "Invalid partition.\n");
		return 0;
	}

	name++; /* skip over the NULL just written. */
	if (outname)
	{
		strcpy (outname, name); /* The file name for the PC */
		for (i=0;i<strlen(outname);i++) outname[i]=tolower(outname[i]); /* To lowercase */
        }
	if (!get_directory(fd, fnam))  /* fnam still holds the partition name. */
		return 0;

	/* Init filename to spaces */
	memset(filename,' ',11);
	filename[11]='\0';

	ext=strrchr(name,'.');  /* search for the last . in filename, if any */
	if (!ext)
	{
		if (strlen(name)<11)
			strncpy (filename, name, strlen(name));
		else
			strncpy (filename, name, 11);
	}
	else
	{
		ext[0]='\0';
		ext++;
		if (strlen(name)<8)
			strncpy (filename, name, strlen(name));
		else
			strncpy (filename, name, 8);
		if (strlen(ext)<3)
			strncpy (filename+8, ext, strlen(ext));
		else
			strncpy (filename+8, ext, 3);
	}
	for (i=0;i<strlen(filename);i++) filename[i]=toupper(filename[i]);
	/* strupr(filename); */

	/* Sanitize it for CP/M. Strip < > . , ; : = ? * [ ] from name, reset
	high bit and check for control codes within name.  */
	for (i=0;i<strlen(filename);i++)
	{
		filename[i]&=0x7F;  /* reset high bit */
		if (filename[i]=='<' || filename[i]=='>' || filename[i]=='.' || filename[i]==',' ||
			filename[i]==';' || filename[i]==':' || filename[i]=='=' || filename[i]=='?' ||
			filename[i]=='*' || filename[i]=='[' || filename[i]==']' || filename[i]=='\\' ||
			filename[i]=='|' || filename[i]<32)
				filename[i]='_';
	}

	*npar=np;
	*nfil=FileExists (filename);
	return 1;
}


int FreeSpace (void)
{
	int i;
	int ac=0;

	for (i=0;i<blocks_filesystem;i++)
		if (!BlockMap[i])
			ac++;
	return ac;
}


int get_file (int fd, char *fname, char *pathname)
{
	char filename[12];
	char outname[512];
	char outfname[512];
	int np,nf;
	int outfd;
	int i,bc,bytes_written;
	int offsblk,blsize;
	u8 *block;

	/* parse filename to extract partition number, name and extension */
	if (PC2CPM(fd, fname, &np, &nf, filename, outname)==0)
	{
		Cleanup();
		return 0;
	}

	if (nf<0)
	{
		fprintf (stderr, "File not found.\n");
		Cleanup();
		return 0;
	}

	sprintf (outfname, "%s%s", pathname, outname);
	outfd=open(outfname, O_CREAT | O_BINARY | O_WRONLY, 0644);
	if (outfd<0)
	{
		fprintf (stderr, "Cannot write output file.\n");
		Cleanup();
		return 0;
	}

	blsize=(ParTable[np].xdpb.blm+1)*128;
	block=(u8 *)malloc(blsize);
	offsblk=OffsHeader +
		    ParTable[np].FirstLBA * SectorSize + 	/* Offset to the beginning of partition */
			ParTable[np].xdpb.spt * ParTable[np].xdpb.off * 128; 	/* Offset to beginning of blocks (directory starts at block 0). */

	bytes_written=0;
	for (i=0;i<Directory[nf].blused-1;i++)
	{
		lseek (fd, offsblk+Directory[nf].blocks[i]*blsize, SEEK_SET);  /* get file pointer to block i */
		bc=read (fd, block, blsize);  /* read block */
		if (bc<blsize)
		{
			fprintf (stderr, "Unable to read block %d from source file.\n", i);
			free (block);
			close (outfd);
			Cleanup();
			return 0;
		}
		bc=write (outfd, block, blsize);
		if (bc<blsize)
		{
			fprintf (stderr, "Unable to write block %d to dest file.\n", i);
			free (block);
			close (outfd);
			Cleanup();
			return 0;
		}
		bytes_written += bc;
	}
	lseek (fd, offsblk+Directory[nf].blocks[i]*blsize, SEEK_SET);  /* get file pointer to last block */
	bc=read (fd, block, blsize);  /* read block */
	write (outfd, block, Directory[nf].nbytes - bytes_written); /* write last bytes */
	close (outfd);
	free (block);

	Cleanup();
	return 1;
}


int del_file (int fd, char *fname)
{
	char filename[12];
	int np,nf;
	int i,bc;
	int offsblk,extent;

	/* parse filename to extract partition number, name and extension */
	if (PC2CPM(fd, fname, &np, &nf, filename, NULL)==0)
	{
		Cleanup();
		return 0;
	}

	if (nf<0)
	{
		fprintf (stderr, "File not found.\n");
		Cleanup();
		return 0;
	}

	offsblk=OffsHeader +
		    ParTable[np].FirstLBA * SectorSize + 	/* Offset to the beginning of partition */
			ParTable[np].xdpb.spt * ParTable[np].xdpb.off * 128; 	/* Offset to beginning of blocks (directory starts at block 0). */

	/* Run over all extents used by this file. */
	for (i=0;i<Directory[nf].extused;i++)
	{
		extent=Directory[nf].extents[i];
		RawDirectory[extent*32]=0xe5;  /* mark this extent as free. */
	}

	/* Update directory to disk. */
	lseek (fd, offsblk, SEEK_SET);
	bc=write (fd, RawDirectory, (ParTable[np].xdpb.drm+1)*32);
	if (bc<(ParTable[np].xdpb.drm+1)*32)
	{
		fprintf (stderr, "Unable to delete file. Disk update failed.\n");
		Cleanup();
		return 0;
	}

	Cleanup();
	return 1;
}


int put_file (int fd, char *inname, char *fname)
{
	char filename[12];
	int np,nf;
	int fdin;
	int bc;
	int offsblk,blsize;
	int filesize;
	int nextents, blocks_last_extent, records_last_lextent, bytes_last_lextent, bytes_last_record;
	int curext, bl, maxbl, ex;
	u16 curbl, blused, logical_extent;
	u8 *block;
	u8 *dirent;

	/* parse filename to extract partition number, name and extension */
	if (PC2CPM(fd, fname, &np, &nf, filename, NULL)==0)
	{
		Cleanup();
		return 0;
	}

	if (nf>=0)  /* if already exists, delete it first. */
	{
		del_file (fd, fname);
		PC2CPM(fd, fname, &np, &nf, filename, NULL); /* get it again, as del_file destroyed it. */
	}

	fdin=open (inname, O_RDONLY | O_BINARY);
	if (fdin<0)
	{
		fprintf (stderr, "Unable to open source file.\n");
		Cleanup();
		return 0;
	}

	blsize=(ParTable[np].xdpb.blm+1)*128;
	block=(u8 *)malloc(blsize);
	offsblk=OffsHeader +
		    ParTable[np].FirstLBA * SectorSize + 	/* Offset to the beginning of partition */
			ParTable[np].xdpb.spt * ParTable[np].xdpb.off * 128; 	/* Offset to beginning of blocks (directory starts at block 0). */

	filesize=lseek (fdin, 0, SEEK_END);  /* get filesize for source file */
	lseek (fdin, 0, SEEK_SET);

	if (filesize>blsize*FreeSpace())
	{
		fprintf (stderr, "Insuficient disk space.\n");
		close (fdin);
		Cleanup();
		free (block);
		return 0;
	}

	nextents = filesize/(blsize*8);  /* block number are 16 bits. There are room for 8 blocks in each extent. */
	if (filesize % (blsize*8))
		nextents++;
	
	blocks_last_extent = (filesize%(blsize*8))/blsize;
	if (blocks_last_extent * blsize < (filesize%(blsize*8)))
		blocks_last_extent++;

	bytes_last_lextent = filesize % 16384;
	if (bytes_last_lextent==0 && filesize>0)
		bytes_last_lextent=16384;

	records_last_lextent = bytes_last_lextent/128;
	if (bytes_last_lextent % 128)
		records_last_lextent++;

	bytes_last_record = filesize % 128;
	
	curext=0;
	curbl=0;
	blused=0;
	for (ex=0;ex<nextents;ex++)
	{
		for (;curext<=ParTable[np].xdpb.drm;curext++)
		{
			dirent=RawDirectory+curext*32;
			if (dirent[0]==0xe5)  /* E5 denotes a free/unused entry */
				break;
		}
		if (curext>=(ParTable[np].xdpb.drm))
		{
			fprintf (stderr, "No free directory entries.\n");
			close (fdin);
			free (block);
		}

		dirent[0]=0;  /* mark extent in use. */
		memcpy (dirent+1,filename,11);  /* put the file name (CP/M format) */
		if (ex!=(nextents-1))
		{
			dirent[13]=0; /* for fully covered extents. */
			dirent[15]=0x80;  /* for fully covered extents. (16384 / 128) */
			maxbl=8;
		}
		else
		{
			dirent[13]=bytes_last_record;
			dirent[15]=records_last_lextent;
			maxbl=blocks_last_extent;
		}

		for (bl=0;bl<maxbl;bl++)
		{
			/* Find a free block. */
			for (;curbl<blocks_filesystem;curbl++)
				if (!BlockMap[curbl])
					break;
			if (curbl>=blocks_filesystem)
			{
				fprintf (stderr, "FATAL: unable to find free blocks.\n");
				close (fdin);
				free (block);
			}
			BlockMap[curbl]=1;
			write16(dirent+16+bl*2,curbl);  /* Store block number in extent. */

			bc=read (fdin, block, blsize);
			lseek (fd, offsblk+curbl*blsize, SEEK_SET);
			write (fd, block, blsize);
			blused++;  /* count blocks used to compute logical extent */
		}
		logical_extent = (blused-1)*blsize/16384;  /* the logical extent is the number of 16K blocks used. */

		dirent[12]=logical_extent & 0x01F;  /* store low part of extent number (Xl) */
		dirent[14]=(logical_extent>>5)&0x3F;  /* store high part of extent number (Xh) */

		for (;bl<8;bl++)
			write16(dirent+16+bl*2,0);  /* No more block in this extent. */
	}
	close (fdin);

	/* Update directory to disk. */
	lseek (fd, offsblk, SEEK_SET);
	bc=write (fd, RawDirectory, (ParTable[np].xdpb.drm+1)*32);
	if (bc<(ParTable[np].xdpb.drm+1)*32)
	{
		fprintf (stderr, "Unable to write file. Disk update failed.\n");
		return 0;
	}
	free (block);
	Cleanup();
	return 1;
}


/*
The format of the header record is as follows:

	Bytes 0...7	- +3DOS signature - 'PLUS3DOS'
	Byte 8		- 1Ah (26) Soft-EOF (end of file)
	Byte 9		- Issue number
	Byte 10		- Version number
	Bytes 11...14	- Length of the file in bytes, 32 bit number,
			    least significant byte in lowest address
	Bytes 15...22	- +3 BASIC header data
	Bytes 23...126	- Reserved (set to 0)
	Byte 127	- Checksum (sum of bytes 0...126 modulo 256)
*/
int put_bin (int fd, char *inname, char *fname, u16 start)
{
	char innamet[512];
	u8 header[128];
	int fdin, fdout;
	int i,res,nblocks,ultbl;
	u32 filesize;
	u16 chks;
	char *buffer;

	strcpy (innamet, tempnam(NULL,"tmp_3e_"));  /* create temporal file name which will have the original file with the header. */
	memset (header, 0, 128);

	fdin = open (inname, O_RDONLY | O_BINARY);
	if (fdin<0)
	{
		fprintf (stderr, "Unable to open source file.\n");
		return 0;
	}
	filesize=lseek (fdin, 0, SEEK_END);
	lseek (fdin, 0, SEEK_SET);

	strncpy (header, "PLUS3DOS", 8);
	header[8]=0x1a;
	header[9]=1;
	header[10]=0;
	write32 (header+11, filesize+128);  /* filesize + header size */
	header[15]=3;  /* Bytes */
	if (filesize>65535)
		write16 (header+16, 65535);  /* Length param is 16 bits on +3DOS header. */
	else
		write16 (header+16, filesize);
	write16 (header+18, start);
	write16 (header+20, 32768);  /* standard value for Bytes type. */
	
	chks=0;
	for (i=0;i<127;i++)
		chks+=header[i];
	header[127]=chks&0xFF;

	fdout=open(innamet, O_CREAT | O_BINARY | O_WRONLY, 0644);
	if (fdout<0)
	{
		close (fdin);
		fprintf (stderr, "Unable to create temporary file. Please, check environment.\n");
		return 0;
	}

	write (fdout, header, 128);
	buffer=malloc(65536);
	nblocks = filesize/65536;
	ultbl = filesize%65536;

	for (i=0;i<nblocks;i++)
	{
		read (fdin, buffer, 65536);
		write (fdout, buffer, 65536);
	}
	if (ultbl)
	{
		read (fdin, buffer, ultbl);
		write (fdout, buffer, ultbl);
	}

	free (buffer);
	close (fdin);
	close (fdout);

	res=put_file (fd, innamet, fname);
	unlink (innamet);

	return res;
}

/*
HEADER
Byte 0..1   : Length of this block (19).
Byte 2      : Flag (00)
Byte 3      : Type (0:program, 1:num array, 2:char array, 3:bytes)
Byte 4..13  : Name (blank padded)
Byte 14..15 : Length
Byte 16..17 : Start (Autoexec)
Byte 18..19 : Vars (32768 for bytes)
Byte 20     : Checksum

DATA
Byte 0..1   : Length of this block (N).
Byte 2      : Flag (FF)
Byte 3..N   : Data
Byte N+1    : Checksum
*/
int put_tap (int fd, char *inname, char *part)
{
	char innamet[512];
	char fname[20], *outbasename;
	u8 header[128];
	int fdin, fdout;
	int i,res;
	u16 chks,sblock,datasize;
	int need_data=0;
	int block_num=1;
	u8 *tapblock;

	memset (header, 0, 128);
	fdin = open (inname, O_RDONLY | O_BINARY);
	if (fdin<0)
	{
		fprintf (stderr, "Unable to open source file.\n");
		return 0;
	}

	tapblock=(u8 *)malloc(65536);
	while(1)
	{
		res=read (fdin, &sblock, 2);
		if (res<2)
			break;
		
		sblock = read16((u8 *)&sblock);  /* Fix endian issues */
		read (fdin, tapblock, sblock);

		if (tapblock[0]==0)  /* standard header */
		{
			memset (header, 0, 128);
			strncpy (header, "PLUS3DOS", 8);
			header[8]=0x1a;
			header[9]=1;
			header[10]=0;
			datasize=read16(tapblock+12);
			write32 (header+11, datasize+128);  /* filesize + header size */
			header[15]=tapblock[1];  /* Type */
			memcpy (header+16, tapblock+12, 6);  /* Copy length, start address and vars. */
	
			/* Compute header checksum */
			chks=0;
			for (i=0;i<127;i++)
				chks+=header[i];
			header[127]=chks&0xFF;

			need_data=1;
			strncpy (innamet, tapblock+2, 8);
			innamet[8]='\0';
			if (!strcmp(innamet, "        "))
				strcpy (innamet, "VOIDNAME");  /* for fixing "blank" file names on tape. */
			sprintf (fname, "%s:%s.%d", part, innamet, block_num);  /* filename for put_file */
			continue;
		}

		if (tapblock[0]==255)
		{
			if (!need_data)  /* headerless data block! */
			{
				/* Create a suitable header for this block */
				memset (header, 0, 128);
				strncpy (header, "PLUS3DOS", 8);
				header[8]=0x1a;
				header[9]=1;
				header[10]=0;
				datasize=sblock-2;
				write32 (header+11, datasize+128);  /* filesize + header size */
				header[15]=3;  /* assume it's bytes */
				write16(header+16, datasize);  /* filesize */
				write16(header+18,0);  /* start address... just a guessing. */
				write16(header+20,32768); /* the same... */
	
				/* Compute header checksum */
				chks=0;
				for (i=0;i<127;i++)
					chks+=header[i];
				header[127]=chks&0xFF;

				strcpy (innamet, inname);
				outbasename=basename(innamet);
				outbasename[8]='\0';  /* truncate it to 8 chars. */
				sprintf (fname, "%s:%s.%d", part, outbasename, block_num);
			}

			need_data=0;
			strcpy (innamet, tempnam(NULL,"tmp_3e_"));  /* create temporal file name which will have the original file with the header. */
			fdout=open(innamet, O_CREAT | O_BINARY | O_WRONLY, 0644);
			if (fdout<0)
			{
				close (fdin);
				fprintf (stderr, "Unable to create temporary file. Please, check environment.\n");
				return 0;
			}

			write (fdout, header, 128);
			write (fdout, tapblock+1, datasize);  /* We won't store flag nor checksum */
			close (fdout);

			res=put_file (fd, innamet, fname);
			unlink (innamet);
			block_num++;

			if (!res)
				return 0;
		}
	}
	free (tapblock);

	return 1;
}


u16 get_hdf_parms (int fd)
{
	int bc;
	u8 hdf[11];
	u16 offs;

	bc=read (fd, hdf, 11);
	lseek (fd, 0, SEEK_SET);
	if (bc<11)
	{
		fprintf (stderr, "Error trying to read HDF header.\n");
		return -1;
	}

	if (strncmp (hdf, "RS-IDE", 6))
	{
		fprintf (stderr, "Warning: file does not seem to be a valid HDF file. Trying as RAW file.\n");
		return 0;  /* no offset for RAW files. */
	}

	if (hdf[8] & 1)  /* byte 8 contains a flag that indicates if a HDF file comes from a "halved sector size" disk. */
	{
		HalvedHDF=1;
		SectorSize=256;
	}

	offs=read16(hdf+9);
	return offs;
}


void scan_disks (void)
{
	int i,drive_letter;
	char fdiskname[20];
	int fd;
	u64 sizebytes=0;

	for (drive_letter='A';drive_letter<='Z';drive_letter++)
	{
#ifdef WIN32
		sprintf (fdiskname, "\\\\.\\PhysicalDrive%d", drive_letter-'A');
#elif defined(LINUX)
		sprintf (fdiskname, "/dev/sd%c", tolower(drive_letter));
#else
		sprintf (fdiskname, "/dev/null");
#endif
		fd = open (fdiskname, O_RDONLY | O_BINARY);
		if (fd<0)
			continue;
		if (get_partition_table(fd))
		{
			for (i=0;i<num_parts;i++)
				sizebytes+=(ParTable[i].LastLBA-ParTable[i].FirstLBA+1)*SectorSize;
			printf ("Detected PLUSIDEDOS partition table on %s . Size: %u MB\n", fdiskname, sizebytes/1048576);
		}
		Cleanup();
		close (fd);
	}
}


void scan_disks_backend (void)
{
	int i,drive_letter,num_disks=0;
	char fdiskname[20];
	int fd;
	u64 sizebytes;
	u64 sizes[26];

	for (drive_letter='A';drive_letter<='Z';drive_letter++)
	{
#ifdef WIN32
		sprintf (fdiskname, "\\\\.\\PhysicalDrive%d", drive_letter-'A');
#elif defined(LINUX)
		sprintf (fdiskname, "/dev/sd%c", tolower(drive_letter));
#else
		sprintf (fdiskname, "/dev/null");
#endif
		fd = open (fdiskname, O_RDONLY | O_BINARY);
		if (fd<0)
		{
			sizes[drive_letter-'A']=0;
			continue;
		}
		if (get_partition_table(fd))
		{
			sizebytes=0;
			num_disks++;
			for (i=0;i<num_parts;i++)
				sizebytes+=(ParTable[i].LastLBA-ParTable[i].FirstLBA+1)*SectorSize;
			sizes[drive_letter-'A']=sizebytes;
	
		}
		else
			sizes[drive_letter-'A']=0;
		Cleanup();
		close (fd);
	}

	printf ("%d\n", num_disks);
	for (drive_letter='A';drive_letter<='Z';drive_letter++)
		if (sizes[drive_letter-'A'])
#ifdef WIN32
			printf ("\\\\.\\PhysicalDrive%d%c%I64u\n", drive_letter-'A', sep, sizes[drive_letter-'A']);
#else
			printf ("/dev/sd%c%c%llu\n", tolower(drive_letter), sep, sizes[drive_letter-'A']);
#endif
}


int put_dsk (int fd, char *dskfilename, char *part)
{
	int fddsk, fdtmp;
	u8 *dsk, *dentry;
	int bc,i,pd,res,bl;
	int ntracks, tracksize;
	tDirEntry dir[64]; /* a standard DSK has 64 directory entries. */
	int max_dir;
	char tmpname[256];
	char dstname[128];

	fddsk=open (dskfilename, O_BINARY | O_RDONLY);
	if (fddsk<0)
	{
		fprintf (stderr, "Unable to open DSK file.\n");
		return 0;
	}

	dsk=malloc(65536);
	bc=read (fddsk, dsk, 256);
	if (bc<256)
	{
		fprintf (stderr, "Unable to read from DSK file.\n");
		free (dsk);
		close (fddsk);
		return 0;
	}
	if (strncmp (dsk, "MV - CPCEMU", 11))
	{
		fprintf (stderr, "This file is not a valid DSK file.\n");
		free (dsk);
		close (fddsk);
		return 0;
	}
	if (dsk[0x31]!=1)
	{
		fprintf (stderr, "Double sided DSK files not supported.\n");
		free (dsk);
		close (fddsk);
		return 0;
	}

	/* Copy the entire disk to memory. */
	ntracks=dsk[0x30];
	tracksize=read16(dsk+0x32)-256;  /* tracksize on DSK file includes the track info header. */

	free (dsk);
	dsk=malloc(ntracks * tracksize);

	for (i=0;i<ntracks;i++)
	{
		lseek (fddsk, 256, SEEK_CUR);  /* jump over the track info zone. */
		bc=read (fddsk, dsk+i*tracksize, tracksize);  /* read track */
		if (bc<tracksize)
		{
			fprintf (stderr, "DSK file truncated at track %d.\n", i);
			free (dsk);
			close (fddsk);
			return 0;
		}
	}
	close (fddsk);

	max_dir=0;
	for (i=0;i<64;i++)
	{
		/* jump over free directory entries. */
		if (dsk[i*32]==0xe5)
			continue;

		dentry=dsk+i*32; /* to easily read all fields on a entry. */
		/* Look if this entry is already in the list. */
		for (pd=0;pd<max_dir;pd++)
			if (!strncmp(dir[pd].fname, dentry+1, 11))
				break;

		/* If this is a new entry... */
		if (pd==max_dir)
		{
			strncpy (dir[pd].fname, dentry+1, 11);
			dir[pd].fname[11]='\0';
			dir[pd].blused=0;
			dir[pd].nbytes=0;
			for (bl=0;bl<16;bl++)
			{
				if (!dentry[16+bl])
				{
					dir[pd].nbytes = 16384*(dir[pd].nbytes/16384)+dentry[15]*128;  /* number of 128-records in last logical extent. */
					break;
				}
				dir[pd].blocks[dir[pd].blused++] = dentry[16+bl];
				dir[pd].nbytes+=1024;  /* size of a block */
			}
			max_dir++;
		}
		else
		{
			for (bl=0;bl<16;bl++)
			{
				if (!dentry[16+bl])
				{
					if (dir[pd].nbytes % 16384 == 0)
						dir[pd].nbytes -= 16384;
					else
						dir[pd].nbytes = 16384*(dir[pd].nbytes/16384);  /* number of 128-records in last logical extent. */
					dir[pd].nbytes+=((dentry[15]-1)*128+(dentry[13]==0?128:dentry[13]));
					break;
				}
				dir[pd].blocks[dir[pd].blused++] = dentry[16+bl];
				dir[pd].nbytes+=1024;  /* size of a block */
			}
		}
	}

	/* Directory read. Now, the transfer. */
	for (i=0;i<max_dir;i++)
	{
		strcpy (tmpname, tempnam(NULL,"tmp_3e_"));
		/* Copy file. */
		fdtmp = open (tmpname, O_CREAT | O_BINARY | O_WRONLY, 0644);
		if (fdtmp<0)
		{
			fprintf (stderr, "Unable to create temporary file. Please, check environment.\n");
			free (dsk);
			return 0;
		}
		for (bl=0;bl<dir[i].blused-1;bl++)
			write (fdtmp, dsk+dir[i].blocks[bl]*1024, 1024);
		write (fdtmp, dsk+dir[i].blocks[bl]*1024, dir[i].nbytes - 1024*(dir[i].blused-1) );  /* the last block may not be completed. */
		close (fdtmp);

		/* Prepare destination file name. */
		sprintf (dstname, "%s:%s", part, dir[i].fname);
		dstname[strlen(dstname)-3]='\0';
		strtrim(dstname);
		if (strncmp(dir[i].fname+8, "   ", 3))
		{
			strcat (dstname, ".");
			strcat (dstname, dir[i].fname+8);
			strtrim (dstname);
		}

		/* Do the transfer. */
		res=put_file (fd, tmpname, dstname);
		unlink (tmpname);
		if (!res)
		{
			free (dsk);
			return 0;
		}
	}
	return 1;
}


void usage (void)
{
	fprintf (stderr, "\n3e v%s: A commandline utility for transferring files from/to a +3E\n"
		             "formatted disk or HDF disk image file.\n", PROGRAM_VERSION);
	fprintf (stderr, "\nUsage: 3e image_file command [args...]\n");
	fprintf (stderr, "\nimage_file can be a HDF image file, a RAW image file, or a physicall device.\n"
		             "For the later, you must known by advance the block device file for your device.\n"
	                 "(Use the scan command to find out detected block device files).\n");
	fprintf (stderr, "\nOn Linux, block devices are named /dev/sda, /dev/sdb, etc. Issue a 'dmesg'\n"
		             "command right after inserting your memory card to find out which special file\n"
					 "the kernel assigns to your device. You must have read/write permissions on that\n"
					 "file.\n");
	fprintf (stderr, "\nOn Windows, block devices for installed drives can be accessed using this\n"
                     "notation: \\\\.\\D: where D: is the drive letter Windows assigns to your card\n"
					 "upon inserting it. Don't forget to CANCEL any attempt to format your memory\n"
					 "card, as Windows thinks it needs formatting. Remember, DON'T format.\n");
	fprintf (stderr, "\nCommands:\n");
	fprintf (stderr, " scan [-backend [-sep:N]]. Scans disks looking for PLUSIDEDOS partition tables.\n");
	fprintf (stderr, " showptable [-backend [-sep:N | -bare]. Shows PLUSIDEDOS partition table.\n");
	fprintf (stderr, " showpentry part [-backend]. Shows details about partition \"part\"\n"
	                 "           (only +3DOS filesystem supported.)\n");
	fprintf (stderr, " dir part [-backend -sep:N | -bare]. Shows directory for partition \"part\"\n"
	                 "           (only +3DOS filesystem supported.)\n");
	fprintf (stderr, " get part:file [dest_path] . Gets a file from a +3DOS filesystem and\n"
                     "           copies it to the current directory on your PC.\n"
	                 "           dest_path is optional path for destination file. Must end with %c\n",DIR_SEP);
	fprintf (stderr, " put file part:file . Writes a file from your PC to a file in\n"
	                 "           the selected partition.\n");
	fprintf (stderr, " putbin file part:file [start] . Writes a file from your PC to a file in\n"
	                 "           the selected partition, and adds a +3DOS header.\n");
	fprintf (stderr, " puttap tapfile part . Scans a TAP file and writes all files within it to the\n"
	                 "           specified partition with the correct +3DOS header for each one.\n"
	                 "           Headerless blocks are named after the original file name, and\n"
	                 "           adding 1,2,.. as extension.\n");
	fprintf (stderr, " putdsk dskfile part . Read the contents of a DSK image file (standard DSK)\n"
		             "           and transfers all its files to the selected partition.\n");
	fprintf (stderr, " del part:file . Deletes a file in the selected partition.\n");
	fprintf (stderr, "\nThe \"part\" argument may refer to a partition number (as numbered by the\n"
		             "showptable command) or a partition name. In this last case, the name is\n"
					 "case sensitive.\n");
	fprintf (stderr, "\nThe -backend option of some commands performs formatted output suitable for\n"
		             "reading it from a front-end application. See FRONTEND.TXT for details.\n");
	fprintf (stderr, "\nThe -bare option of some commands performs formatted output suitable\n"
		             "for processing it from a batch file or script.\n");
	fprintf (stderr, "\nExtra arguments in commands are ignored.\n");
	fprintf (stderr, "\n(C)2009-2012 Miguel Angel Rodriguez Jodar (mcleod_ideafix) http://www.zxprojects.com\nGPL Licensed.\n");
}


int main (int argc, char *argv[])
{
	int fd;
	char *image_name;
	char *command;
	char **args;
	int nargs;
	int np;
	int res=0;

	if (argc>=2 && !stricmp (argv[1], "scan"))
	{
		if (argc>2 && !stricmp (argv[2], "-backend"))
			scan_disks_backend();
		else
			scan_disks();
		return 1;
	}

	if (argc<3)
	{
		usage();
		return 1;
	}

	image_name=argv[1];
	command=argv[2];
	args=&argv[3];
	nargs=argc-3;

	fd=open(image_name, O_RDWR | O_BINARY);
	if (fd<0)
	{
		fprintf (stderr, "Error opening %s\n", image_name);
		return 0;
	}

	if (strstr(image_name,".hdf") || strstr(image_name,".HDF"))
	{
		OffsHeader=get_hdf_parms(fd);
		if (OffsHeader<0)
			return 0;
	}
	else
		OffsHeader=0;

    if (!get_partition_table(fd))
	{
		fprintf (stderr, "Error getting partition table.\n");
		close (fd);
		return 0;
	}

	if (!stricmp(command,"showptable"))
	{
		if (nargs>0 && !stricmp (args[0],"-backend"))
			show_partition_table_backend();
		else if (nargs>0 && !stricmp (args[0],"-bare"))
			show_partition_table_bare();
		else
			show_partition_table();
	}
	else if (!strcmp(command,"showpentry"))
	{
		if (nargs<1)
		{
			fprintf (stderr, "Missing partition.\n");
			close (fd);
			return 0;
		}
		if (nargs>1 && !stricmp (args[1],"-backend"))
			res=show_partition_entry_backend (args[0]);
		else
 			res=show_partition_entry (args[0]);
	}
	else if (!strcmp(command, "dir"))
	{
		if (nargs<1)
		{
			fprintf (stderr, "Missing partition.\n");
			close (fd);
			return 0;
		}
		if ((np=get_directory (fd, args[0]))>=0)
		{
			res=1;
			if (nargs>1 && !stricmp (args[1], "-backend"))
				show_directory_backend (fd, np);
			else if (nargs>1 && !stricmp (args[1], "-bare"))
				show_directory_bare (fd, np);
			else
				show_directory(fd, np);
		}
		else
			res=0;
	}
	else if (!strcmp(command, "get"))
	{
		if (nargs<1)
		{
			fprintf (stderr, "Missing filename.\n");
			close (fd);
			return 0;
		}
		if (nargs>1)
			res=get_file (fd, args[0], args[1]);
		else
			res=get_file(fd, args[0], "");
	}
	else if (!strcmp(command, "del"))
	{
		if (nargs<1)
		{
			fprintf (stderr, "Missing filename.\n");
			close (fd);
			return 0;
		}
		res=del_file(fd, args[0]);
	}
	else if (!strcmp(command, "put"))
	{
		if (argc<5)
		{
			fprintf (stderr, "Missing arguments.\n");
			close (fd);
			return 0;
		}
		res=put_file (fd, args[0], args[1]);
	}
	else if (!strcmp(command, "putbin"))
	{
		if (argc<5)
		{
			fprintf (stderr, "Missing arguments.\n");
			close (fd);
			return 0;
		}
		if (argc>=6)
			res=put_bin (fd, args[0], args[1], atoi(args[2]));
		else
			res=put_bin (fd, args[0], args[1], 0);
	}
	else if (!strcmp(command, "puttap"))
	{
		if (argc<5)
		{
			fprintf (stderr, "Missing arguments.\n");
			close (fd);
			return 0;
		}
		res=put_tap (fd, args[0], args[1]);
	}
	else if (!strcmp(command, "putdsk"))
	{
		if (argc<5)
		{
			fprintf (stderr, "Missing arguments.\n");
			close (fd);
			return 0;
		}
		res=put_dsk (fd, args[0], args[1]);
	}
	else
	{
		usage();
		res=1;
	}

	close (fd);
	return res;
}
