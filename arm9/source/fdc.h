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

extern int DriveBusy;

void    ChangeCurrTrack( int newTrack );
void    ReadCHRN( void );
int     SeekSector( int newSect, int * pos );
int     ReadFDC( int port );
void    WriteFDC( int Port, int val );
void    ResetFDC( void );
void    EjectDiskFDC( void );
int     GetCurrTrack( void );
void    ReadDiskMem(u8 *rom, u32 romsize);

#pragma pack(1)
typedef struct
{
    char  debut[ 0x30 ]; // "MV - CPCEMU Disk-File\r\nDisk-Info\r\n"
    UBYTE NbTracks;
    UBYTE NbHeads;
    SHORT DataSize; // 0x1300 = 256 + ( 512 * nbsecteurs )
    UBYTE Unused[ 0xCC ];
} CPCEMUEnt;

typedef struct
{
    UBYTE C;                // track
    UBYTE H;                // head
    UBYTE R;                // sect
    UBYTE N;                // size
    UBYTE ST1;              // Valeur ST1
    UBYTE ST2;              // Valeur ST2
    SHORT SectSize;         // Taille du secteur en octets
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
    UBYTE       OctRemp;  // 0xE5
    CPCEMUSect  Sect[ 29 ];
} CPCEMUTrack;
#pragma pack()

#endif // FDC_H
