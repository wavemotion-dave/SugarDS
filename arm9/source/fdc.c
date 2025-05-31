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

#include  <nds.h>
#include "fdc.h"

extern u32 debug[];
extern u8 floppy_sound, floppy_action;

// General Disk Defines
#define SECTSIZE        512
#define NBSECT          9

// Status Bits
#define STATUS_CB       0x10
#define STATUS_NDM      0x20
#define STATUS_DIO      0x40
#define STATUS_RQM      0x80

// ST0
#define ST0_NR          0x08
#define ST0_SE          0x20
#define ST0_IC1         0x40
#define ST0_IC2         0x80

// ST1
#define ST1_ND          0x04
#define ST1_EN          0x80

// ST3
#define ST3_TS          0x08
#define ST3_T0          0x10
#define ST3_RY          0x20

u8 *ImgDsk=NULL;
int LongFic;

typedef int ( * pfctFDC )( int );

static int Nothing( int );

static pfctFDC fct = Nothing;
static int state;
CPCEMUTrack CurrTrackDatasDSK;
static CPCEMUEnt Infos;
static int FlagWrite;
int Image;
int PosData;
int DriveBusy;
static int Status;
static int ST0;
static int ST1;
static int ST2;
static int ST3;
int C;
int H;
int R;
int N;
static int Drive;
static int EOT;
static int Busy;
static int Inter;
static int Motor;
int IndexSecteur = 0;

u8 DISK_IMAGE_BUFFER[512*1024]; // Big enough for any 3" disk format


int SeekSector( int newSect, int * pos )
{
	int i;

    floppy_sound = 2;
    floppy_action = 0;
    
	* pos = 0;
	for ( i = 0; i < CurrTrackDatasDSK.NbSect; i++ )
	if ( CurrTrackDatasDSK.Sect[ i ].R == newSect )
	return( ( UBYTE )i );
	else
	* pos += CurrTrackDatasDSK.Sect[ i ].SectSize;

	ST0 |= ST0_IC1;
	ST1 |= ST1_ND;
	return( -1 );
}


void ReadCHRN( void )
{
	C = CurrTrackDatasDSK.Sect[ IndexSecteur ].C;
	H = CurrTrackDatasDSK.Sect[ IndexSecteur ].H;
	R = CurrTrackDatasDSK.Sect[ IndexSecteur ].R;
	N = CurrTrackDatasDSK.Sect[ IndexSecteur ].N;
	if ( ++IndexSecteur == CurrTrackDatasDSK.NbSect )
	IndexSecteur = 0;
}


static void SetST0( void )
{
	ST0 = 0; // drive A
	if ( ! Motor || Drive || ! Image )
	ST0 |= ST0_IC1 | ST0_NR;

	ST1 = 0;
	ST2 = 0;
}

static int Nothing( int val )
{
	Status &= ~STATUS_CB & ~STATUS_DIO;
	state = 0;
	ST0 = ST0_IC2;
	return( Status );
}

static int ReadST0( int val )
{
	if ( ! Inter )
	ST0 = ST0_IC2;
	else
	{
		Inter = 0;
		if ( Busy )
		{
			ST0 = ST0_SE;
			Busy = 0;
		}
		else
		ST0 |= ST0_IC1 | ST0_IC2;
	}
	if ( Motor && Image && ! Drive )
	ST0 &= ~ST0_NR;
	else
	{
		ST0 |= ST0_NR;
		if ( ! Image )
		ST0 |= ( ST0_IC1 | ST0_IC2 );
	}

	if ( state++ == 1 )
	{
		Status |= STATUS_DIO;
		return( ST0 );
	}

	state = val = 0;
	Status &= ~STATUS_CB & ~STATUS_DIO;
	ST0 &= ~ST0_IC1 & ~ST0_IC2;
	ST1 &= ~ST1_ND;
	return( C );
}

static int ReadST3( int val )
{
	if ( state++ == 1 )
	{
		Drive = val;
		Status |= STATUS_DIO;
		return( 0 );
	}
	state = 0;
	Status &= ~STATUS_CB & ~STATUS_DIO;
	if ( Motor && ! Drive )
	ST3 |= ST3_RY;
	else
	ST3 &= ~ST3_RY;

	return( ST3 );
}


static int Specify( int val )
{
	if ( state++ == 1 )
	return( 0 );

	state = val = 0;
	Status &= ~STATUS_CB & ~STATUS_DIO;
	return( 0 );
}



static int ReadID( int val )
{
	switch( state++ )  {
	case 1 :
		Drive = val;
		Status |= STATUS_DIO;
		Inter = 1;
		break;

	case 2 :
		return( 0 /*ST0*/ );

	case 3 :
		return( ST1 );

	case 4 :
		return( ST2 );

	case 5 :
		ReadCHRN();
		return( C );

	case 6 :
		return( H );

	case 7 :
		return( R );

	case 8 :
		state = 0;
		Status &= ~STATUS_CB & ~STATUS_DIO;
		return( N );
	}
	return( 0 );
}



static int FormatTrack( int val )
{
    floppy_sound = 2;
    floppy_action = 1;

	state = val = 0;
	Status &= ~STATUS_CB & ~STATUS_DIO;
	return( 0 );
}


static int Scan( int val )
{
	state = val = 0;
	return( 0 );
}

void ChangeCurrTrack( int newTrack )
{
	ULONG Pos = 0;
	int t, s;

	if ( ! Infos.DataSize )
	{
		memcpy( &CurrTrackDatasDSK, ImgDsk, sizeof( CurrTrackDatasDSK ) );
		for ( t = 0; t < newTrack; t++ )
		{
			for ( s = 0; s < CurrTrackDatasDSK.NbSect; s++ )
			Pos += CurrTrackDatasDSK.Sect[ s ].SectSize;

			Pos += sizeof( CurrTrackDatasDSK );
			memcpy( &CurrTrackDatasDSK, &ImgDsk[ Pos ], sizeof( CurrTrackDatasDSK ) );
		}
	}
	else
	Pos += Infos.DataSize * newTrack;

	memcpy( &CurrTrackDatasDSK, &ImgDsk[ Pos ], sizeof( CurrTrackDatasDSK ) );

	PosData = Pos + sizeof( CurrTrackDatasDSK );
	IndexSecteur = 0;
	ReadCHRN();

	if ( ! newTrack )
	ST3 |= ST3_T0;
	else
	ST3 &= ~ST3_T0;
}


static int MoveTrack( int val )
{
	switch( state++ )
	{
	case 1 :
		Drive = val;
		SetST0();            
		Status |= STATUS_NDM;
		break;

	case 2 :
		ChangeCurrTrack( C = val );
		state = 0;
		Status &= ~STATUS_CB & ~STATUS_DIO & ~STATUS_NDM;
		Busy = 1;
		Inter = 1;
		break;
	}
	return( 0 );
}


static int MoveTrack0( int val )
{
	Drive = val;
	ChangeCurrTrack( C = 0 );
	state = 0;
	Status &= ~STATUS_CB & ~STATUS_DIO & ~STATUS_NDM;
	SetST0();
	Busy = 1;
	Inter = 1;
	return( 0 );
}

static int ReadData( int val )
{
	static int sect = 0, cntdata = 0, newPos;
	static signed int TailleSect;

	switch( state++ )
	{
	case 1 :
		Drive = val;
		SetST0();            
		break;

	case 2 :
		C = val;
		break;

	case 3 :
		H = val;
		break;

	case 4 :
		R = val;
		break;

	case 5 :
		N = val;
		break;

	case 6 :
		EOT = val;
		break;

	case 7 :
		sect = SeekSector( R, &newPos );
		if (sect != -1) {
			TailleSect = 128 << CurrTrackDatasDSK.Sect[ sect ].N;
			if ( ! newPos )
			cntdata = ( sect * CurrTrackDatasDSK.SectSize ) << 8;
			else
			cntdata = newPos;
		}
		break;

	case 8 :
		Status |= STATUS_DIO | STATUS_NDM;
		break;

	case 9 :
		if ( ! ( ST0 & ST0_IC1 ) )
		{
			if ( --TailleSect )
			state--;
			else
			{
				if ( R++ < EOT )
				state = 7;
				else
				Status &= ~STATUS_NDM;
			}
			return( ImgDsk[ PosData + cntdata++ ] );
		}
		Status &= ~STATUS_NDM;
		return( 0 );

	case 10 :
		return( ST0 );

	case 11 :
		return( ST1 | ST1_EN );

	case 12 :
		return( ST2 );

	case 13 :
		return( C );

	case 14 :
		return( H );

	case 15 :
		return( R );

	case 16 :
		state = 0;
		Status &= ~STATUS_CB & ~STATUS_DIO;
		return( N );
	}
	return( 0 );
}

static int WriteData( int val )
{
	static int sect = 0, cntdata = 0, newPos;
	static signed int TailleSect;

	switch( state++ )
	{
	case 1 :
		Drive = val;
		SetST0();            
		break;

	case 2 :
		C = val;
		break;

	case 3 :
		H = val;
		break;

	case 4 :
		R = val;
		break;

	case 5 :
		N = val;
		break;

	case 6 :
		EOT = val;
		break;

	case 7 :
		sect = SeekSector( R, &newPos );
		if (sect != -1) {
			TailleSect = 128 << CurrTrackDatasDSK.Sect[ sect ].N;
			if ( ! newPos )
			cntdata = ( sect * CurrTrackDatasDSK.SectSize ) << 8;
			else
			cntdata = newPos;
		}
		break;

	case 8 :
		Status |= STATUS_DIO | STATUS_NDM;
		break;

	case 9 :
		if ( ! ( ST0 & ST0_IC1 ) )
		{
            floppy_sound = 2;
            floppy_action = 1;

			ImgDsk[ PosData + cntdata++ ] = ( UBYTE )val;
			if ( --TailleSect )
			state--;
			else
			{
				if ( R++ < EOT )
				state = 7;
				else
				Status &= ~STATUS_NDM;
			}
			return( 0 );
		}
		Status &= ~STATUS_NDM;
		return( 0 );

	case 10 :
		if ( ! ( ST0 & ST0_IC1 ) )
		FlagWrite = 1;

		return( ST0 );

	case 11 :
		return( ST1 );

	case 12 :
		return( ST2 );

	case 13 :
		return( C );

	case 14 :
		return( H );

	case 15 :
		return( R );

	case 16 :
		state = 0;
		Status &= ~STATUS_CB & ~STATUS_DIO;
		return( N );
	}
	return( 0 );
}

int ReadFDC( int port )
{
	if ( port & 1 )
	return( fct( port ) );

	return( Status );
}

void WriteFDC( int port, int val )
{
	DriveBusy=1;
	
	if ( port == 0xFB7F ) {     
		if (state) {    
			fct(val);
		} else {
			Status |= STATUS_CB;
			state = 1;
			switch( val & 0x1F )
			{
			case 0x03 :
				// Specify
				fct = Specify;
				break;

			case 0x04 :
				// Read ST3
				fct = ReadST3;
				break;

			case 0x05 :
				// Write data
				fct = WriteData;
				break;

			case 0x06 :
				// Reading data
				fct = ReadData;
				break;

			case 0x07 :
				// Track head movement 0
				// Status |= STATUS_NDM;
				fct = MoveTrack0;
				break;

			case 0x08 :
				// Read ST0, track
				Status |= STATUS_DIO;
				fct = ReadST0;
				break;

			case 0x09 :
				// Write data
				fct = WriteData;
				break;

			case 0x0A :
				// Read Field ID
				fct = ReadID;
				break;

			case 0x0C :
				// Read Data
				fct = ReadData;
				break;

			case 0x0D :
				// Track formatting
				fct = FormatTrack;
				break;

			case 0x0F :
				// Head movement
				fct = MoveTrack;
				break;

			case 0x11 :
				fct = Scan;
				break;

				default :
				Status |= STATUS_DIO;
				fct = Nothing;
			}
		}
	} else {
		if ( port == 0xFA7E ) {
			Motor = val & 1;
		}
	}
}

void ResetFDC( void )
{
	Status = STATUS_RQM;
	ST0 = ST0_SE;
	ST1 = 0;
	ST2 = 0;
	ST3 = ST3_RY | ST3_TS;
	Busy = 0;
	Inter = 0;
	state = 0;
    Motor = 0;
}

void EjectDiskFDC( void )
{
	if (Image!=0) Image = 0;
}

int GetCurrTrack( void )
{
	return( C );
}

void ReadDiskMem(u8 *rom, u32 romsize)
{
	EjectDiskFDC();

	LongFic=romsize-sizeof(Infos);

	ImgDsk=(u8*)DISK_IMAGE_BUFFER;
	
	memcpy(&Infos, rom, sizeof(Infos));
	memcpy(ImgDsk, rom+sizeof(Infos), LongFic);
	Image=1;
	FlagWrite=0;
	
	ChangeCurrTrack(0);
}
