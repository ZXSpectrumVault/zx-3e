'3e': Utility for handing files in a memory card formatted for a +3E
enhanced Spectrum. Version 0.5alpha.

(C)2009-2012 Miguel Angel Rodriguez Jodar (mcleod_ideafix). GPL Licensed.
For support, please check http://www.zxprojects.com


CHANGELOG

January, 14, 2012
- Added a small utility, hdf2hdf256 to assist converting 8-bit HDF files
into HDF256 format, so "3e" can handle it correctly. It's a single C file
you can comple with: gcc -O hdf2hdf256 hdf2hdf256.c . Windows executable
is included.

January, 8, 2012
- Fixed an inconsistency with return values for the backend mode. Now the
front-end won't treat as an error if the image file is not a HDF file, but
RAW.
- Changed the scheme for device file names for Windows. Instead of using
the \\.\letter: scheme, which is valid for memory cards with no 
partitions, the "scan" command now returns the device file name of the 
physical disk, which is \\.\PhysicalDriveN with N being 0,1,2,etc.
- Initial support for shared disks (disks with a standard IBM partition table
where the first one is a "unused/unkown" partition, where the PLUSIDEDOS 
volume resides). USE WITH CAUTION. The partition type used for detecting
the presence of an IDEPLUSDOS disk inside a partition is type 0x7F, as 
suggested by Garry Lancaster at:
http://www.worldofspectrum.org/zxplus3e/sharingdisks.html
- Initial support for halved size sector disks. It can now handle an HDF image
file with halved sectors. USE WITH CAUTION

August, 2, 2009.
- Fixed an issue with get_partition_table. When the number of partitions * 64
is not a integer multiple of 512, the read fails, if issued against a physical
device.

July, 30, 2009.
- Fixed an issue with full length names in get_file.
- PC filename is now always lowercase.
- NEW! Added "scan" support for Linux.

July, 27, 2009. v0.3alpha
- Fixed a subtle error when calculating file size for storing a file
and displaying the directory.
- NEW! Added support for transferring DSK images to a partition. Just
like it's currently being done with TAP files. See "USAGE". Standard
DSK files are supported (1 side, 1024 bytes per block, 40 tracks, all
the tracks the same size.)

July, 26, 2009. v0.2alpha
- Added more conditional compiling.
- Added -bare and -backend command line options to some commands
so output is suitable for post-processing, through scripts or
front-end applications.
- Added partition name support. Now, a file within the memory
card / disk file can be refered using the notation partname:filename
where partname is the name (not the number) of the partition the
file resides in.
- Added the "scan" command (only Windows at the moment). Scans through
all driver letters looking for disks with a PLUSIDEDOS partition table
and shows the results. Useful for front-end applications and users who
cannot figure which special file to work with.

July, 23, 2009. 
- Added a little of conditional compiling to better
support Linux. Now should compile straightforward with:
gcc -o 3e 3e.c
- Compiles flawesly on Windows with MinGW32 (tested on C-Free 4)
Forget about the issue with tempnam... ;)
- Fixed an issue with big partition tables that led to a GPF.
- Better HDF image file support. Now I can detect whether halved size
sectors are being used or not, and calculate the correct offset to
the raw data (uses to be 128, but with HDF specification 1.1 this
changes. (thanks Velesoft for providing this information :)

July, 22, 2009.
Version 0.1 alpha. First publicy available version. 
Works on Windows 7. Should work on previous versions (Vista & XP).
Should also work on Linux, but not yet tested.



USAGE

Usage: 3e image_file command [args...]

image_file can be a HDF image file, a RAW image file, or a physicall device.
For the later, you must known by advance the block device file for your device.
(Use the scan command to find out detected block device files).

On Linux, block devices are named /dev/sda, /dev/sdb, etc. Issue a 'dmesg'
command right after inserting your memory card to find out which special file
the kernel assigns to your device. You must have read/write permissions on that
file.

On Windows, block devices for installed drives can be accessed using this
notation (*): \\.\D: where D: is the drive letter Windows assigns to your card
upon inserting it. Don't forget to CANCEL any attempt to format your memory
card, as Windows thinks it needs formatting. Remember, DON'T format.

* Use the "scan" command to obtain the newer, recommended, device file name 
for accessing the memory card from Windows.

Commands:
 scan [-backend [-sep:N]]. Scans disks looking for PLUSIDEDOS partition tables.
 showptable [-backend [-sep:N | -bare]. Shows PLUSIDEDOS partition table.
 showpentry part [-backend]. Shows details about partition "part"
           (only +3DOS filesystem supported.)
 dir part [-backend -sep:N | -bare]. Shows directory for partition "part"
           (only +3DOS filesystem supported.)
 get part:file [dest_path] . Gets a file from a +3DOS filesystem and
           copies it to the current directory on your PC.
           dest_path is optional path for destination file. Must end with \
 put file part:file . Writes a file from your PC to a file in
           the selected partition.
 putbin file part:file [start] . Writes a file from your PC to a file in
           the selected partition, and adds a +3DOS header.
 puttap tapfile part . Scans a TAP file and writes all files within it to the
           specified partition with the correct +3DOS header for each one.
           Headerless blocks are named after the original file name, and
           adding 1,2,.. as extension.
 putdsk dskfile part . Read the contents of a DSK image file (standard DSK)
           and transfers all its files to the selected partition.
 del part:file . Deletes a file in the selected partition.
The "part" argument may refer to a partition number (as numbered by the
showptable command) or a partition name. In this last case, the name is
case sensitive.

The -backend option of some commands performs formatted output suitable for
reading it from a front-end application. See FRONTEND.TXT for details.

The -bare option of some commands performs formatted output suitable
for processing it from a batch file or script.

Extra arguments in commands are ignored.

(C)2009 Miguel Angel Rodriguez Jodar (mcleod_ideafix) http://www.zxprojects.com
GPL Licensed.

Filenames to be used within the +3DOS filesystem are somewhat "sanitized" so
any extraneous character is changed to a '_'.


BUGS

The sep:N command option is not yet implemented (see FRONTEND.TXT).

Being alpha software, it's expected that a lot of bugs are floating around. 
At this moment, it does not do any consistency check about the filesystem, 
so any command that reads or writes to a corrupt filesystem is likely to fail 
with some random error.

Support for HDF image files is very limited. The program uses an HDF file just
like a RAW image file or a physical device. It only takes into account that HDF
files has a 128 byte header, which is completly ignored.

Support for physical devices is left upon the operating system. There's no 
special provision to assure that all disk access are made on a sector frontier,
nor there's any checks to ensure all disk operations read or write whole 
sectors. In addition, support for these devices are limited to those that 
allows LBA addressing, and has a sector size of 512.

There's no support for physical devices with uses only 8 bits when should use
16 (some cheap interfaces) nor there's support for the so-called HDF256.


EXAMPLES (Windows, see examples for Linux below)

- Find out what PLUSIDEDOS drives are available on the system:

C:\>3e scan
Detected PLUSIDEDOS partition table on \\.\M: . Size: 61 MB


- Shows the partition table on a memory card detected as drive M:

C:\>3e \\.\m: showptable

Disk geometry (C/H/S): 491/2/128

PARTITION TABLE

#  Name             Type      C/H begin   C/H end   LBA Begin LBA End   Size
------------------------------------------------------------------------------
0  PLUSIDEDOS       System      0/  0       0/  0   0         127       0   MB
1  juegos           +3DOS C:    0/  1     128/  0   128       32895     16  MB
2  utils            +3DOS D:  128/  1     256/  0   32896     65663     16  MB
3  swap             Swap      256/  1     272/  0   65664     69759     2   MB
4  chica            +3DOS     272/  1     280/  0   69760     71807     1   MB
5  ---------------- FREE      280/  1     490/  1   71808     125695    26  MB

2 partition entries remain unassigned.

The drive letter next to +3DOS in some partitions means that the partition is
permanently mapped to that drive letter. This mapping can be controlled by the
command MOVE drive_letter IN partition, and MOVE drive_letter OUT (see +3E 
manual).


- Shows details about partition #1 (named "juegos")

C:\>3e \\.\m: showpentry 1

Also this command can be issued as:
C:\>3e \\.\m: showpentry juegos

DETAILS FOR PARTITION: juegos
Type: +3DOS mapped to C:
First LBA address: 128
Last LBA address: 32895
Size (MBytes): 16 MB
Reserved bytes from begining of partition: 0
Block size: 8192 bytes
Directory entries: 512
Offset to data area (directory size): 4000h
Data size for this filesystem: 16711680 bytes.

XDBP parms:
 SPT : 512               BSH : 6
 BLM : 63                EXM : 3
 DSM : 2039              DRM : 511
 AL0 : 192               AL1 : 0
 CKS : 32768             OFF : 0
 PSH : 2                 PHM : 3
 Sideness : 0                    Tracks per side : 255
 Sectors per track : 128         First sector # : 0
 Sector size : 512               GAP length R/W : 0
 GAP length fmt : 0              Multitrack : 0
 Freeze flag : 0

This information is usually not necessary to the end user. It is presented 
for completeness and to aid debugging the program itself.


- List directory for partition "juegos" (partition #1)

C:\>3e \\.\m: dir 1

Also this command can be issued as:
C:\>3e \\.\m: dir juegos

Directory for juegos

Name          Disksize  Att  Ver  HdSize    Type        RSize  Start  Vars
---------------------------------------------------------------------------
ROM     .     16512          1.0  16512     Bytes       16384  0      32860
CIRCULO .SCR  7040           1.0  7040      Bytes       6912   16384  32903
CHCODE  .     2560           1.0  2458      Bytes       2330   49406  32768
EDL     .Z80  131200         Headerless file.
CHAIN   .     256            1.0  205       Program     77     1      77
CHAIN   .SNA  49280          Headerless file.
EDL     .SCR  7040           1.0  7040      Bytes       6912   16384  32768

Roughly speaking, there are two kind of files: +3DOS compilant files, with 
header, and headerless files. The later ones are verbatim copies from the
original file on the PC. For example, .SNA and .Z80 files for the SPECTRUM
command must be naked binary files, without any header.
On the other way, +3DOS files have a header, the same way tape blocks have
headers: to identify the type of file, its length, start address, etc.


- Copy a file called EDL.SCR from the partition 1 of the memory card to the 
current working directory on the PC.

C:\>3e \\.\m: get 1:edl.scr

Also, this command can be issued as:
C:\>3e \\.\m: get juegos:edl.scr

A file called "edl.scr" is created on C:\ containing both the header and
data for this file. The header, if any, is copied along with the file.


- Copy a file called EDL.SCR from the partition 1 of the memory card to the parent directory related to the current working directory:

C:\>3e \\.\m: get 1:edl.scr ..\

Also, this command can be issued as:
C:\>3e \\.\m: get juegos:edl.scr ..\


- Copy a file called "screen.bin" located on the root directory of the PC, 
which is a screen dump, to the memory card, but with an added header to make 
it work under +3DOS with the LOAD command. The +3DOS file name for this file
will be "screen" and it will be stored in partition "utils" (#2).
As it's a screen, we will provide the start address for this file to be 16384.

C:\>3e \\.\m: putbin screen.bin 2:screen 16384

Also, this command can be issued as:
C:\>3e \\.\m: putbin screen.bin utils:screen 16384

This file can be loaded from +3DOS with:
LOAD "d:screen" CODE
(provided that partition "utils" is mapped to drive D:)

If the start address is not provided for the putbin command, it is assumed 
to be 0.

It's also possible to write a file to the memory card without adding a header.
Use the "put" command instead of "putbin".


- Delete a file named "screen" from partition #2
C:\>3e \\.\m: del 2:screen

Also, this command can be issued as:
C:\>3e \\.\m: del utils:screen


- Dump all the files on a TAP archive to the partition 4 of memory card 
and list the directory of that partition.

C:>3e \\.\m: puttap "c:\SPECTRUM\Games\h\HORIZONT.TAP" 4

Also, this command can be issued as:
C:>3e \\.\m: puttap "c:\SPECTRUM\Games\h\HORIZONT.TAP" chica

(after a couple of seconds...)

C:>3e \\.\m: dir chica
Directory for chica

Name          Disksize  Att  Ver  HdSize    Type        RSize  Start  Vars
---------------------------------------------------------------------------
CARAA   .     256            1.0  249       Program     121    10     121
ARCOIRIS.     33024          1.0  32993     Bytes       6912   16384  32865
LOGO    .     33024          1.0  33007     Bytes       6912   16384  32879
HARDWARE.     4224           1.0  4188      Program     4060   9100   4060
        .     33152          1.0  33095     Bytes       300    32256  32967
HWS     .     33024          1.0  33000     Bytes       6912   16384  32872
LECCION1.     8064           1.0  7982      Program     7854   9100   7854
MCODE   .     33024          1.0  32909     Bytes       299    32000  32781
LECCION2.     7552           1.0  7513      Program     7385   9100   7385
LECCION3.     7936           1.0  7859      Program     7731   9100   7731
LECCION4.     7808           1.0  7777      Program     7649   9100   7649
DICCIONA.     3072           1.0  2984      Program     2856   9980   2856
D       .     33024          1.0  32936     Bytes       400    32256  32808
FILE    .     33024          1.0  33001     Char array  5103   50690  32873
CARAB   .     3072           1.0  2989      Program     2861   9100   2861
MURO    .     4480           1.0  4370      Program     4317   300    4242
WALLG   .     33152          1.0  33042     Bytes       168    65368  32914
C       .     33152          1.0  33093     Bytes       300    32256  32965
CLASIFIC.     7680           1.0  7621      Program     7493   9050   7493
CHAR    .     33024          1.0  32965     Bytes       168    65368  32837
EVOLUCIO.     6656           1.0  6541      Program     6413   9050   6413
BITS    .     33024          1.0  32909     Bytes       32     65368  32781
LIFE    .     4992           1.0  4914      Program     4786   9010   4786
P       .     33152          1.0  33092     Bytes       300    32196  32964
DIBUJAR .     4992           1.0  4933      Program     4805   8000   4805
MONTECAR.     5504           1.0  5444      Program     5316   9110   5316
GENERADO.     5760           1.0  5703      Program     5575   9100   5575
ONDAS   .     4480           1.0  4423      Program     4295   9100   4295
M       .     33152          1.0  33095     Bytes       300    32256  32967

Take into account however, that the copy does not altere program contents. This
is important because the usual way to load "the next block" is LOAD "" , and 
this will issue a "F Invalid file name" error. You will have to modify by hand 
each LOAD sentence to include proper name on it.


- Transfer a DSK image file to a partition named "utils"

C:\>3e \\.\m: putdsk c:\Spectrum\disks\g\devpac3.dsk utils

(after doing this, a directory listing can be show for the "utils" partition to
reveal the new files)

C:\>3e \\.\m: dir utils

Directory for utils

Name          Disksize  Att  Ver  HdSize    Type        RSize  Start  Vars
---------------------------------------------------------------------------
GENP3   .     27264          1.0  10880     Bytes       10752  24700  0
GENP351 .     27776          1.0  11392     Bytes       11264  24700  0
GENS451 .     27904          1.0  11520     Bytes       11392  26000  32768
MONP3   .     25088          1.0  8704      Bytes       8576   24700  0
MONS4   .     6784           1.0  6784      Bytes       6656   40000  32984


Usefull shell scripts for using 3e with Linux
---------------------------------------------
The -bare option allows shell scripts to use '3e' in order to perform
complex tasks not available as standalone commands within '3e'. Here are
some examples:

- Retrieving all files from a partition, to a directory of the same name as
the partition:

getdir.sh
-------------------------------
#!/bin/sh
mkdir $2
for f in `3e $1 dir $2 -bare`
do
  3e $1 get $2:$f $2/
done
-------------------------------
Usage: getdir.sh disk_file_name partition_to_retrieve
Example: getdir.sh /dev/sdh utils

- Retrieving all the files from all partitions, each partition on a directory
of the same name:

getall.sh
-------------------------------
#!/bin/sh
for p in `3e $1 showptable -bare`
do 
  mkdir $p
  for f in `3e $1 dir $p -bare`
  do
   3e $1 get $p:$f $p/
  done
done
-------------------------------
Usage: getall.sh disk_file_name
Example: getall.sh mydisk.hdf

You can use egrep on the dir command to allow for wildcards. For
example, get all .BIN files from the "games" partition on /dev/sdh to the
"backup" directory on your PC:

for f in `3e /dev/sdh dir games -bare | egrep "*.BIN"`
do
  3e /dev/sdh get games:$f backup/
done


- Write all files from a directory full of SCR files (binary files with a ZX Spectrum
screen dump) to the "scr" partition on /dev/sdh, adding a header so you can 
use LOAD "somefile.scr" SCREEN$ from +3 BASIC to load one of them.

for f in myscreens/*.scr
do
  3e /dev/sdh putbin $f scr 16384
done
