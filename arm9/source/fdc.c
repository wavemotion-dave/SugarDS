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
#include "SugarDS.h"
#include "fdc.h"

// Status Bits
#define STATUS_CB       0x10
#define STATUS_EXM      0x20
#define STATUS_DIO      0x40
#define STATUS_RQM      0x80

// ST0
#define ST0_HD          0x04
#define ST0_NR          0x08
#define ST0_SE          0x20
#define ST0_IC1         0x40
#define ST0_IC2         0x80

// ST1
#define ST1_ND          0x04
#define ST1_EN          0x80

// ST3
#define ST3_HD          0x04
#define ST3_TS          0x08
#define ST3_T0          0x10
#define ST3_RY          0x20

typedef int ( * pfctFDC )( int );

FDC_t fdc;

u8 DISK_IMAGE_BUFFER[896*1024]; // Big enough for any 3" or 3.5" disk format

int SeekSector( int *pos )
{
    floppy_sound = 2;
    floppy_action = 0;

    *pos = 0;
    for ( int i = 0; i < fdc.CurrTrackDatasDSK[fdc.Side].NbSect; i++ )
    {
        if ( (fdc.CurrTrackDatasDSK[fdc.Side].Sect[ i ].C == fdc.C) &&
             (fdc.CurrTrackDatasDSK[fdc.Side].Sect[ i ].H == fdc.H) &&
             (fdc.CurrTrackDatasDSK[fdc.Side].Sect[ i ].R == fdc.R) &&
             (fdc.CurrTrackDatasDSK[fdc.Side].Sect[ i ].N == fdc.N) )
        {
            fdc.sector_index = (i + 1) % fdc.CurrTrackDatasDSK[fdc.Side].NbSect;
            return( ( UBYTE )i );
        }
        else
        {
            *pos += fdc.CurrTrackDatasDSK[fdc.Side].Sect[ i ].SectSize;
        }
    }

    fdc.ST0 |= ST0_IC1;
    fdc.ST1 |= ST1_ND;
    return( -1 );
}


void ReadCHRN( void )
{
    fdc.C = fdc.CurrTrackDatasDSK[fdc.Side].Sect[ fdc.sector_index ].C;
    fdc.H = fdc.CurrTrackDatasDSK[fdc.Side].Sect[ fdc.sector_index ].H;
    fdc.R = fdc.CurrTrackDatasDSK[fdc.Side].Sect[ fdc.sector_index ].R;
    fdc.N = fdc.CurrTrackDatasDSK[fdc.Side].Sect[ fdc.sector_index ].N;
    if ( ++fdc.sector_index == fdc.CurrTrackDatasDSK[fdc.Side].NbSect )
    {
        fdc.sector_index = 0;
    }
}


static void SetST0( void )
{
    fdc.ST0 = 0; // drive 00 (A:)
    
    if ( !fdc.Motor || fdc.Drive || !fdc.Image )
    {
        fdc.ST0 |= ST0_IC1 | ST0_NR; // Not ready... No Motor, or No Drive or No Disk Image
    }

    if (fdc.Side)
    {
        fdc.ST0 |= ST0_HD;
    }
    else
    {
        fdc.ST0 &= ~ST0_HD;
    }
    
    // Status 1 and 2 will get filled in as we progress
    fdc.ST1 = 0;
    fdc.ST2 = 0;
}

static int Nothing( int val )
{
    fdc.Status &= ~STATUS_CB & ~STATUS_DIO;
    fdc.state = 0;
    fdc.ST0 = ST0_IC2;
    return( fdc.ST0 );
}

static int ReadST0( int val )
{
    if (!fdc.Inter)
    {
        fdc.ST0 = ST0_IC2;
    }
    else
    {
        fdc.Inter = 0;
        if (fdc.Busy)
        {
            fdc.ST0 = ST0_SE;
            fdc.Busy = 0;
        }
        else
        {
            fdc.ST0 |= ST0_IC1 | ST0_IC2;
        }

        if (fdc.Side)
        {
            fdc.ST0 |= ST0_HD;
        }
        else
        {
            fdc.ST0 &= ~ST0_HD;
        }
    }

    if (fdc.Motor && fdc.Image && !fdc.Drive)
    {
        fdc.ST0 &= ~ST0_NR;
    }
    else
    {
        fdc.ST0 |= ST0_NR;
        if ( !fdc.Image )
        {
            fdc.ST0 |= ( ST0_IC1 | ST0_IC2 );
        }
    }

    if (fdc.state++ == 1)
    {
        fdc.Status |= STATUS_DIO;
        return( fdc.ST0 );
    }

    fdc.state = 0;
    fdc.Status &= ~STATUS_CB & ~STATUS_DIO;
    fdc.ST0 &= ~ST0_IC1 & ~ST0_IC2;
    fdc.ST1 &= ~ST1_ND;
    return( fdc.C );
}

static int ReadST3( int val )
{
    if ( fdc.state++ == 1 )
    {
        fdc.Drive = val & 3;
        fdc.Side = (val >> 2) & 1;
        fdc.Status |= STATUS_DIO;
        return( 0 );
    }

    fdc.state = 0;
    fdc.Status &= ~STATUS_CB & ~STATUS_DIO;

    if ( fdc.Motor && !fdc.Drive && fdc.Image)
    {
        fdc.ST3 |= ST3_RY; // Drive ready
    }
    else
    {
        fdc.ST3 &= ~ST3_RY; // Drive not ready
    }

    if ( fdc.Side )
    {
        fdc.ST3 |= ST3_HD;
    }
    else
    {
        fdc.ST3 &= ~ST3_HD;
    }

    return( fdc.ST3 );
}


static int Specify( int val )
{
    if ( fdc.state++ == 1 )
    {
        return( 0 );
    }

    fdc.state = 0;
    fdc.Status &= ~STATUS_CB & ~STATUS_DIO;
    return( 0 );
}



static int ReadID( int val )
{
    switch( fdc.state++ )
    {
    case 1 :
        fdc.Drive = val & 3;
        fdc.Side = (val >> 2) & 1;
        fdc.Status |= STATUS_DIO;
        fdc.Inter = 1;
        break;

    case 2 :
        return( fdc.ST0 );

    case 3 :
        return( fdc.ST1 );

    case 4 :
        return( fdc.ST2 );

    case 5 :
        ReadCHRN();
        return( fdc.C );

    case 6 :
        return( fdc.H );

    case 7 :
        return( fdc.R );

    case 8 :
        fdc.state = 0;
        fdc.Status &= ~STATUS_CB & ~STATUS_DIO;
        return( fdc.N );
    }
    return( 0 );
}



static int FormatTrack( int val )
{
    floppy_sound = 2;
    floppy_action = 1;

    fdc.state = 0;
    fdc.Status &= ~STATUS_CB & ~STATUS_DIO;
    return( 0 );
}


static int Scan( int val )
{
    fdc.state = 0;
    return( 0 );
}

void ChangeCurrTrack( int newTrack )
{
    ULONG Pos = 0;

    // Set status before we modify newTrack below
    if (newTrack == 0)
    {
        fdc.ST3 |= ST3_T0;      // We are on track 0
    }
    else
    {
        fdc.ST3 &= ~ST3_T0;     // We are not on track 0
    }

    if ( fdc.Side )
    {
        fdc.ST3 |= ST3_HD;      // Current Head is 1 (side 1)
    }
    else
    {
        fdc.ST3 &= ~ST3_HD;     // Current Head is 0 (side 0)
    }

    // ---------------------------------------------------------------------
    // If we are double-sided... handle the math for tracks. This gets us
    // to the right track for side 0 and we might read in side 1 as well.
    // ---------------------------------------------------------------------
    newTrack = (newTrack * fdc.DiskInfo.NumHeads);

    // -----------------------------------------------------------------------------
    // We cache the track info for both sides of the disk if we are double sided...
    // -----------------------------------------------------------------------------
    for (int head=0;head<fdc.DiskInfo.NumHeads;head++)
    {
        Pos = 0;
        if (fdc.DiskInfo.TrackSize == 0) // If no Data Size set... Extended Disk
        {
            for (int t = 0; t < newTrack; t++ )
            {
                Pos += fdc.DiskInfo.TrackSizes[t] * 256;
            }
        }
        else // Standard disk
        {
            Pos += fdc.DiskInfo.TrackSize * newTrack;
        }

        // Read in Side 0/1 (based on head)
        memcpy( &fdc.CurrTrackDatasDSK[head], &fdc.ImgDsk[ Pos ], sizeof( CPCEMUTrack ) );
        fdc.PosData[head] = Pos + sizeof( CPCEMUTrack );

        newTrack++; // Side 1 always follows side 0 in the .DSK layout
    }

    fdc.sector_index = 0;   // Just one sector index pulse no matter how many sides
    ReadCHRN();             // Read the CHRN data from the current side of the disk
}


static int MoveTrack( int val )
{
    switch( fdc.state++ )
    {
    case 1 :
        fdc.Drive = val & 3;
        fdc.Side = (val >> 2) & 1;
        SetST0();
        fdc.Status |= STATUS_EXM;
        break;

    case 2 :
        ChangeCurrTrack( fdc.C = val );
        fdc.state = 0;
        fdc.Status &= ~STATUS_CB & ~STATUS_DIO & ~STATUS_EXM;
        fdc.Busy = 1;
        fdc.Inter = 1;
        break;
    }
    return( 0 );
}


static int MoveTrack0( int val )
{
    fdc.Drive = val & 3;
    fdc.Side = (val >> 2) & 1;
    ChangeCurrTrack( fdc.C = 0 );
    fdc.state = 0;
    fdc.Status &= ~STATUS_CB & ~STATUS_DIO & ~STATUS_EXM;
    SetST0();
    fdc.Busy = 1;
    fdc.Inter = 1;
    return( 0 );
}

static int ReadData( int val )
{
    switch( fdc.state++ )
    {
    case 1 :
        fdc.Drive = val & 3;
        fdc.Side = (val >> 2) & 1;
        SetST0();
        break;

    case 2 :
        fdc.C = val;
        break;

    case 3 :
        fdc.H = val;
        break;

    case 4 :
        fdc.R = val;
        break;

    case 5 :
        fdc.N = val;
        break;

    case 6 :
        fdc.EOT = val;
        break;

    case 7 :
        fdc.rd_sect = SeekSector( &fdc.rd_newPos );
        if (fdc.rd_sect != -1)
        {
            fdc.ST1 = fdc.CurrTrackDatasDSK[fdc.Side].Sect[fdc.rd_sect].ST1 & 0x25;  // Grab the ST1 field from the sector info 
            fdc.ST2 = fdc.CurrTrackDatasDSK[fdc.Side].Sect[fdc.rd_sect].ST1 & 0x61;  // Grab the ST2 field from the sector info 
            
            if (fdc.CurrTrackDatasDSK[fdc.Side].Sect[fdc.rd_sect].N)
            {
                fdc.rd_SectorSize = 128 << fdc.CurrTrackDatasDSK[fdc.Side].Sect[fdc.rd_sect].N;
            }
            else
            {
                fdc.rd_SectorSize = fdc.CurrTrackDatasDSK[fdc.Side].Sect[fdc.rd_sect].SectSize;
            }

            if (fdc.DiskInfo.TrackSize != 0) // If we are a standard (non-Extended) disk...
                fdc.rd_cntdata = ( fdc.rd_sect * fdc.CurrTrackDatasDSK[fdc.Side].SectSize ) << 8;
            else
                fdc.rd_cntdata = fdc.rd_newPos;
        }
        break;

    case 8 :
        fdc.Status |= STATUS_DIO | STATUS_EXM;
        break;

    case 9 :
        if ( ! ( fdc.ST0 & ST0_IC1 ) )
        {
            if ( --fdc.rd_SectorSize )
            {
                fdc.state--;
            }
            else
            {
                if ( fdc.R++ < fdc.EOT )
                  fdc.state = 7;
                else
                  fdc.Status &= ~STATUS_EXM;
            }
            return( fdc.ImgDsk[ fdc.PosData[fdc.Side] + fdc.rd_cntdata++ ] );
        }
        fdc.Status &= ~STATUS_EXM;
        return( 0 );

    case 10 :
        return( fdc.ST0 );

    case 11 :
        return( fdc.ST1 | ST1_EN );

    case 12 :
        return( fdc.ST2 );

    case 13 :
        return( fdc.C );

    case 14 :
        return( fdc.H );

    case 15 :
        return( fdc.R );

    case 16 :
        fdc.state = 0;
        fdc.Status &= ~STATUS_CB & ~STATUS_DIO;
        return( fdc.N );
    }
    return( 0 );
}

static int WriteData( int val )
{
    switch( fdc.state++ )
    {
    case 1 :
        fdc.Drive = val & 3;
        fdc.Side = (val >> 2) & 1;
        SetST0();
        break;

    case 2 :
        fdc.C = val;
        break;

    case 3 :
        fdc.H = val;
        break;

    case 4 :
        fdc.R = val;
        break;

    case 5 :
        fdc.N = val;
        break;

    case 6 :
        fdc.EOT = val;
        break;

    case 7 :
        fdc.wr_sect = SeekSector( &fdc.wr_newPos );
        if (fdc.wr_sect != -1)
        {
            if (fdc.CurrTrackDatasDSK[fdc.Side].Sect[ fdc.wr_sect ].N)
            {
                fdc.wr_SectorSize = 128 << fdc.CurrTrackDatasDSK[fdc.Side].Sect[ fdc.wr_sect ].N;
            }
            else
            {
                fdc.wr_SectorSize = fdc.CurrTrackDatasDSK[fdc.Side].Sect[fdc.wr_sect].SectSize;
            }
            

            if (fdc.DiskInfo.TrackSize != 0) // If we are a standard (non-Extended) disk...
                fdc.wr_cntdata = ( fdc.wr_sect * fdc.CurrTrackDatasDSK[fdc.Side].SectSize ) << 8;
            else
                fdc.wr_cntdata = fdc.wr_newPos;

        }
        break;

    case 8 :
        fdc.Status |= STATUS_DIO | STATUS_EXM;
        break;

    case 9 :
        if ( ! ( fdc.ST0 & ST0_IC1 ) )
        {
            floppy_sound = 2;
            floppy_action = 1;

            fdc.dirty_counter = 2;
            fdc.bDirtyFlags[(fdc.PosData[fdc.Side] + fdc.wr_cntdata) / 4096] = 1;

            fdc.ImgDsk[ fdc.PosData[fdc.Side] + fdc.wr_cntdata++ ] = ( UBYTE )val;
            if ( --fdc.wr_SectorSize )
            {
                fdc.state--;
            }
            else
            {
                if ( fdc.R++ < fdc.EOT )
                  fdc.state = 7;
                else
                  fdc.Status &= ~STATUS_EXM;
            }
            return( 0 );
        }
        fdc.Status &= ~STATUS_EXM;
        return( 0 );

    case 10 :
        if ( ! ( fdc.ST0 & ST0_IC1 ) )
        {
            fdc.FlagWrite = 1;
        }
        return( fdc.ST0 );

    case 11 :
        return( fdc.ST1 );

    case 12 :
        return( fdc.ST2 );

    case 13 :
        return( fdc.C );

    case 14 :
        return( fdc.H );

    case 15 :
        return( fdc.R );

    case 16 :
        fdc.state = 0;
        fdc.Status &= ~STATUS_CB & ~STATUS_DIO;
        return( fdc.N );
    }
    return( 0 );
}

pfctFDC fdc_func_lookup[] =
{
    Nothing,    // 0x00
    Nothing,    // 0x01
    Nothing,    // 0x02
    Specify,    // 0x03
    ReadST3,    // 0x04
    WriteData,  // 0x05
    ReadData,   // 0x06
    MoveTrack0, // 0x07
    ReadST0,    // 0x08
    WriteData,  // 0x09
    ReadID,     // 0x0A
    Nothing,    // 0x0B
    ReadData,   // 0x0C
    FormatTrack,// 0x0D
    Nothing,    // 0x0E
    MoveTrack,  // 0x0F
    Nothing,    // 0x10
    Scan,       // 0x11
    Nothing,    // 0x12
    Nothing,    // 0x13
    Nothing,    // 0x14
    Nothing,    // 0x15
    Nothing,    // 0x16
    Nothing,    // 0x17
    Nothing,    // 0x18
    Nothing,    // 0x19
    Nothing,    // 0x1A
    Nothing,    // 0x1B
    Nothing,    // 0x1C
    Nothing,    // 0x1D
    Nothing,    // 0x1E
    Nothing,    // 0x1F
};

void FDC_frame(void)
{
    if (fdc.ReadyIn)
    {
        if (--fdc.ReadyIn == 0) 
        {
            fdc.Image=1;
        }

        if (fdc.Motor && fdc.Image && !fdc.Drive)
        {
            fdc.ST0 &= ~ST0_NR;
            fdc.ST0 &= ~ST0_IC1 & ~ST0_IC2;
        }
        else
        {
            fdc.ST0 |= ST0_NR;
            if ( !fdc.Image )
            {
                fdc.ST0 |= ( ST0_IC1 | ST0_IC2 );
            }
        }
        
    }
}

int ReadFDC( int port )
{
    if (port & 1)
    {
        return(  fdc_func_lookup[fdc.function](port) );
    }

    return( fdc.Status );
}

void WriteFDC( int port, int val )
{
    if (port == 0xFB7F)
    {
        if (fdc.state)
        {
            fdc_func_lookup[fdc.function](val);
        }
        else
        {
            fdc.Status |= STATUS_CB;
            fdc.state = 1;
            fdc.function = (val & 0x1F);

            switch( fdc.function )
            {
            case 0x03 :
                // Specify
                break;

            case 0x04 :
                // Read ST3
                break;

            case 0x05 :
                // Write data
                break;

            case 0x06 :
                // Reading data
                break;

            case 0x07 :
                // Track head movement 0
                break;

            case 0x08 :
                // Read ST0, track
                fdc.Status |= STATUS_DIO;
                break;

            case 0x09 :
                // Write data
                break;

            case 0x0A :
                // Read Field ID
                break;

            case 0x0C :
                // Read Data
                break;

            case 0x0D :
                // Track formatting
                break;

            case 0x0F :
                // Head movement
                break;

            case 0x11 :
                // Scan
                break;

            default :
                fdc.Status |= STATUS_DIO;
            }
        }
    }
    else
    {
        if (port == 0xFA7E)
        {
            fdc.Motor = val & 1;
        }
    }
}

void ResetFDC( void )
{
    // Start with a blank slate...
    memset(&fdc, 0x00, sizeof(fdc));

    fdc.Drive = 0;
    fdc.Side = 0;
    fdc.sector_index = 0;
    fdc.Status = STATUS_RQM;
    fdc.ST0 = ST0_SE;
    fdc.ST1 = 0;
    fdc.ST2 = 0;
    fdc.ST3 = 0;
    fdc.Busy = 0;
    fdc.Inter = 0;
    fdc.state = 0;
    fdc.Motor = 0;
    fdc.ReadyIn = 0;
}

// --------------------------------------------------------------
// This is called when a disk is inserted into the emulated
// CPC machine. Only one disk drive (A: or Drive 0) is emulated.
// --------------------------------------------------------------
void ReadDiskMem(u8 *rom, u32 romsize)
{
    // --------------------------
    // Eject current disk image
    // --------------------------
    if (fdc.Image!=0) fdc.Image = 0;

    // ----------------------------------------------------
    // And read in the new disk image into the FDC buffers
    // ----------------------------------------------------
    fdc.disk_size=romsize-sizeof(fdc.DiskInfo);

    fdc.ImgDsk=(u8*)DISK_IMAGE_BUFFER;

    memcpy(&fdc.DiskInfo, rom, sizeof(fdc.DiskInfo));
    memcpy(fdc.ImgDsk, rom+sizeof(fdc.DiskInfo), fdc.disk_size);

    // ---------------------------------------------------------------------------
    // Setting ReadyIn here will mark the status as 'Not Ready' for 12 frames
    // as there are some games that monitor the status to see if the disk has
    // actually been ejected and swapped for another disk. Orion Prime does this.
    // ---------------------------------------------------------------------------
    fdc.ReadyIn = 12;
    fdc.FlagWrite=0;

    // --------------------------------------------------------------------------------------------
    // Note: there is some confusion as to whether two sides should have the bit set or reset.
    // The CPC wiki says this signal is inverted and so does the French Floppy Guide so that's
    // what we're going with here. Basically we clear the TWO SIDES signal in ST3 if the disk
    // we have loaded indicates it has more than one side.
    // --------------------------------------------------------------------------------------------
    if (fdc.DiskInfo.NumHeads > 1)
    {
        fdc.ST3 &= ~ST3_TS; // Clear Two Sides signal (inverted means two-sided drive)
    }
    else
    {
        fdc.ST3 |= ST3_TS;  // Set Two Sides signal (inverted means one-sided drive)
    }

    // ------------------------------------------------------
    // A new disk so seek to track zero to get us started...
    // ------------------------------------------------------
    ChangeCurrTrack(0);
}
