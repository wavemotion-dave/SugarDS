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

#ifndef FDC_H
#define FDC_H

typedef unsigned short          USHORT;
typedef signed short            SHORT;
typedef unsigned char           UBYTE;
typedef unsigned long           ULONG;

#ifndef _BOOL
#define _BOOL
typedef u8 BOOL;
#endif

void    ChangeCurrTrack( int newTrack );
void    ReadCHRN( void );
int     SeekSector( int newSect, int * pos );
int     ReadFDC( int port );
void    WriteFDC( int Port, int val );
void    ResetFDC( void );
void    EjectDiskFDC( void );
void    ReadDiskMem(u8 *rom, u32 romsize);

#pragma pack(1)
typedef struct
{
    char  debut[ 0x30 ];        // "MV - CPCEMU Disk-File\r\nDisk-Info\r\n"
    UBYTE NumTracks;
    UBYTE NumHeads;
    SHORT TrackSize;            // For non-Extended disks. 0x1300 = 256 + ( 512 * NumberOfSectors )
    UBYTE TrackSizes[ 0xCC ];   // For Extended Disks
} CPCEMUHeader;

typedef struct
{
    UBYTE C;                // track
    UBYTE H;                // head (side)
    UBYTE R;                // sect ID
    UBYTE N;                // sect size
    UBYTE ST1;              // FDC Status ST1
    UBYTE ST2;              // FDC Status ST2
    SHORT SectSize;         // Size of sectors
} CPCEMUSect;

typedef struct
{
    char        ID[ 0x10 ];   // "Track-Info\r\n"
    UBYTE       Track;
    UBYTE       Head;
    SHORT       Unused;
    UBYTE       SectSize; // 2
    UBYTE       NbSect;   // 9
    UBYTE       Gap3;     // 0x4E
    UBYTE       PadByte;  // 0xE5
    CPCEMUSect  Sect[ 29 ];
} CPCEMUTrack;
#pragma pack()


typedef struct
{
    int disk_size;
    int state;
    CPCEMUHeader DiskInfo;
    CPCEMUTrack  CurrTrackDatasDSK[2]; // For 2 heads/sides
    int PosData[2];
    int FlagWrite;
    int Image;
    int DriveBusy;
    int Status;
    int ST0;
    int ST1;
    int ST2;
    int ST3;
    int C;
    int H;
    int R;
    int N;
    int Drive;
    int Side;
    int EOT;
    int Busy;
    int Inter;
    int Motor;
    int sector_index;
    int function;
    int rd_sect;
    int rd_cntdata;
    int rd_newPos;
    int rd_SectorSize;
    int wr_sect;
    int wr_cntdata;
    int wr_newPos;
    int wr_SectorSize;
    int dirty_counter;
    u8  bDirtyFlags[256]; // one flag for each of 256 possible 4K SD flash blocks (1024K max)
    u8 *ImgDsk;
} FDC_t;

extern FDC_t fdc;

#endif // FDC_H
