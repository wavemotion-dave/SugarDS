// SugarDS is Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
// 
// Bits of pieces of this emulator have been glued and attached from a large number of sources - along with a healthy amount of 
// code by this author to pull it all together. Although it was hard to trace everythign to the original sources, I believe all 
// sources have released their material under the GNU General Public License and, as such, the SugarDS emulator follows suit.
// 
// Previous contributions to this codebase:
//
// CrocoDS: CPC Emulator for the DS - Copyright (c) 2013 Miguel Vanhove (Kyuran)
// Win-CPC: Amstrad CPC Emulator - Copyright (c) 2012 Ludovic Deplanque.
// Caprice32: Amstrad CPC Emulator - Copyright (c) 1997-2004 Ulrich Doewich.
// Arnold: Amstrad CPC Emulator - Copyright (c) 1995-2002, 2007 Andreas Micklei and Kevin Thacker
//
// As far as I'm concerned, you can use this code in whatever way suits you provided you continue to release the sources under 
// the original copyright notice (see below) which appeared to be the intention of all the pioneers who came before me.
// 
// Original Copyright Notice
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef __AMSDOS_HEADER_INCLUDED__
#define __AMSDOS_HEADER_INCLUDED__

#ifndef _BOOL
#define _BOOL
typedef u8 BOOL;
#endif


/* BASIC parameters to describe an AMSDOS format. A CP/M based format */
typedef struct
{
	int nFirstSectorId;				/* id of first sector. it is assumed all sectors 
									are numbered sequentially */
	int nReservedTracks;			/* number of complete reserved tracks */
	int nSectorsPerTrack;			/* number of sectors per track. It is assumed that
									all tracks have the same number of sectors */
} AMSDOS_FORMAT;

typedef struct
{
	int nTrack;						/* physical track number */
	int nSector;					/* physical sector id */
	int nSide;						/* physical side */
} AMSDOS_TRACK_SECTOR_SIDE;

typedef struct 
{
	unsigned char UserNumber;		/* user number */
	unsigned char Filename[8];		/* name part of filename */
	unsigned char Extension[3];		/* extension part of filename */
	unsigned char Extent;			/* 16K extent of file */
	unsigned char unused[2];		/* not used */
	unsigned char LengthInRecords;	/* length of this extent in records */
	unsigned char Blocks[16];		/* blocks used by this directory entry; 8-bit or 16-bit values */
} amsdos_directory_entry;


typedef struct
{
	unsigned char Unused1;
	unsigned char Filename[8];
	unsigned char Extension[3];
	unsigned char Unused2[6];
	unsigned char FileType;
	unsigned char LengthLow;
	unsigned char LengthHigh;
	unsigned char LocationLow;
	unsigned char LocationHigh;
	unsigned char FirstBlockFlag;
	unsigned char LogicalLengthLow;
	unsigned char LogicalLengthHigh;
	unsigned char ExecutionAddressLow;
	unsigned char ExecutionAddressHigh;
	unsigned char DataLengthLow;
	unsigned char DataLengthMid;
	unsigned char DataLengthHigh;
	unsigned char ChecksumLow;
	unsigned char ChecksumHigh;
} AMSDOS_HEADER;

unsigned int AMSDOS_CalculateChecksum(const unsigned char *pHeader);
BOOL     AMSDOS_HasAmsdosHeader(const unsigned char *pHeader);

/* returns TRUE if the ch is a valid filename character, FALSE otherwise.
A valid filename character is a character which can be typed by the user and one
which, when used in a typed filename and is present on the disc, can then be loaded by
AMSDOS */
BOOL	AMSDOS_IsValidFilenameCharacter(char ch);


BOOL	AMSDOS_GetFilenameThatQualifiesForAutorun(amsdos_directory_entry *entry);

enum
{
	AUTORUN_OK,						/* found a valid autorun method */
	AUTORUN_NOT_POSSIBLE,			/* auto-run is not possible; no suitable files found and |CPM will not work */
	AUTORUN_TOO_MANY_POSSIBILITIES,	/* too many files qualify for auto-run and it is not possible to
									identify the correct one */
	AUTORUN_NO_FILES_QUALIFY,		/* no files qualify for auto-run */
	AUTORUN_FAILURE_READING_DIRECTORY,	/* failed to read directory */
};

int AMSDOS_GenerateAutorunCommand(char *AutorunCommand);


#endif
