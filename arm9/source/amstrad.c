// =====================================================================================
// =====================================================================================
// Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave and Marat
// Fayzullin (Z80 core) are thanked profusely.
//
// The SugarDS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================
#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fat.h>
#include <dirent.h>

#include "SugarDS.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "AmsUtils.h"
#include "fdc.h"
#include "printf.h"

u8  portA               __attribute__((section(".dtcm"))) = 0x00;
u8  portB               __attribute__((section(".dtcm"))) = 0xFF;
u8  portC               __attribute__((section(".dtcm"))) = 0x00;
u8  portDIR             __attribute__((section(".dtcm"))) = 0x00;

u32 last_file_size      __attribute__((section(".dtcm"))) = 0;
u32 scanline_count      __attribute__((section(".dtcm"))) = 1;
u8 ram_highwater        __attribute__((section(".dtcm"))) = 0;

u8 MMR                  __attribute__((section(".dtcm"))) = 0x00;
u8 RMR                  __attribute__((section(".dtcm"))) = 0x00;
u8 PENR                 __attribute__((section(".dtcm"))) = 0x00;
u8 UROM                 __attribute__((section(".dtcm"))) = 0x00;

u8 INK[17]              __attribute__((section(".dtcm"))) = {0};
u32 border_color        __attribute__((section(".dtcm"))) = 0;
u8 CRTC[0x20]           __attribute__((section(".dtcm"))) = {0};
u8 CRT_Idx              __attribute__((section(".dtcm"))) = 0;
u8 inks_changed         __attribute__((section(".dtcm"))) = 0;
u16 refresh_tstates     __attribute__((section(".dtcm"))) = 0;
u8 ink_map[256]         __attribute__((section(".dtcm"))) = {0};

u32  pre_inked_mode0[256] __attribute__((section(".dtcm"))) = {0};
u32  pre_inked_mode1[256] __attribute__((section(".dtcm"))) = {0};
u32  pre_inked_mode2a[256]  = {0};  // Not used often enough to soak up fast memory
u32  pre_inked_mode2b[256]  = {0};  // Not used often enough to soak up fast memory
u32  pre_inked_mode2c[256]  = {0};  // Not used often enough to soak up fast memory

u8  RAM_512k_bank = 0;      // Can be set to 1 on DSi for an additional 512K addressing
u8 *DSi_ExpandedRAM = 0;    // The DSi will malloc 1024K for extra emulated CPC memory

u8 CRTC_MASKS[0x20] = {0xFF, 0xFF, 0xFF, 0xFF,
                       0x7F, 0x1F, 0x7F, 0x7F,
                       0xF3, 0x1F, 0x7F, 0x1F,
                       0x3F, 0xFF, 0x3F, 0xFF,
                       0x3F, 0xFF, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00};

s16 CPU_ADJUST[] __attribute__((section(".dtcm"))) = {0, 1, 2, 0, -2, -1, 0}; // Extra 0 here for old configs...

u8 sna_last_motor = 0;
u8 sna_last_track = 0;


// ----------------------------------------------------
// Dandanator Mini support - mostly for Sword of IANNA
// and Los Amores de Brunilda.
// ----------------------------------------------------
u8  DAN_Zone0     __attribute__((section(".dtcm"))) = 0x00;   // Zone 0 enabled on slot0
u8  DAN_Zone1     __attribute__((section(".dtcm"))) = 0x20;   // Zone 1 disabled on slot0
u16 DAN_Config    __attribute__((section(".dtcm"))) = 0x00;   // Zones 0 and 1 high address bit and CE
u16 DAN_WaitCFG   __attribute__((section(".dtcm"))) = 0x00;   // When waiting for a RET to update configuration
u8  DAN_Follow    __attribute__((section(".dtcm"))) = 28;     // FollowRomEn bank for zone 0 (poor man 'ROMBOX')
u8  DAN_WaitRET   __attribute__((section(".dtcm"))) = 0;      // Set to '1' if we wait for a RET to configure memory

ITCM_CODE void compute_pre_inked(u8 mode)
{
    if (mode == 0) // Mode 0
    {
        for (int pixel=0; pixel < 256; pixel++)
        {
            u8 pixel0 = INK[((pixel & 0x80) >> 7) | ((pixel & 0x20) >> 3) | ((pixel & 0x08) >> 2) | ((pixel & 0x02) << 2)];
            u8 pixel1 = INK[((pixel & 0x40) >> 6) | ((pixel & 0x10) >> 2) | ((pixel & 0x04) >> 1) | ((pixel & 0x01) << 3)];
            pre_inked_mode0[pixel] = (pixel1 << 24) | (pixel1 << 16) | (pixel0 << 8) | (pixel0 << 0);
        }
    }
    if (mode == 1) // Mode 1
    {
        for (int pixel=0; pixel < 256; pixel++)
        {
            u8 pixel0 = INK[((pixel & 0x80) >> 7) | ((pixel & 0x08) >> 2)];
            u8 pixel1 = INK[((pixel & 0x40) >> 6) | ((pixel & 0x04) >> 1)];
            u8 pixel2 = INK[((pixel & 0x20) >> 5) | ((pixel & 0x02) >> 0)];
            u8 pixel3 = INK[((pixel & 0x10) >> 4) | ((pixel & 0x01) << 1)];
            pre_inked_mode1[pixel] = (pixel3 << 24) | (pixel2 << 16) | (pixel1 << 8) | (pixel0 << 0);
        }
    }
    else if (mode == 2) // Mode 2
    {
        for (int pixel=0; pixel < 256; pixel++)
        {
            u8 pixel0 = INK[((pixel & 0x80) >> 7)];
            u8 pixel1 = INK[((pixel & 0x40) >> 6)];
            u8 pixel2 = INK[((pixel & 0x20) >> 5)];
            u8 pixel3 = INK[((pixel & 0x10) >> 4)];
            u8 pixel4 = INK[((pixel & 0x08) >> 3)];
            u8 pixel5 = INK[((pixel & 0x04) >> 2)];
            u8 pixel6 = INK[((pixel & 0x02) >> 1)];
            u8 pixel7 = INK[((pixel & 0x01) >> 0)];

            pre_inked_mode2b[pixel] = (pixel7 << 24) | (pixel6 << 16) | (pixel5 << 8) | (pixel4 << 0);
            pre_inked_mode2a[pixel] = (pixel3 << 24) | (pixel2 << 16) | (pixel1 << 8) | (pixel0 << 0);
            pre_inked_mode2c[pixel] = (pixel7 << 24) | (pixel5 << 16) | (pixel3 << 8) | (pixel1 << 0);
        }
    }
}

// --------------------------------------------------------------------
// We use the back-end of the DISK IMAGE BUFFER as the place to store
// our 32 banks (0..31) of 16K each. This buffer is otherwise unused
// when we have a cartridge inserted. In theory, we could still keep
// the disk active for loading and limit ourselves to 400K worth of
// .dsk space which is plenty for this emulation.
// --------------------------------------------------------------------
u8 *CartBankPtr[32] = {0}; // The 32 banks of 16K ROM cart chunks
void CartLoad(void)
{
    u8 *cart_buffer = (u8 *) (DISK_IMAGE_BUFFER + (400 * 1024));

    memset(cart_buffer, 0x00, (512 * 1024)); // Nothing out there to start...

    if ((ROM_Memory[0] == 'R') && (ROM_Memory[1] == 'I') && (ROM_Memory[2] == 'F') && (ROM_Memory[3] == 'F') &&
        (ROM_Memory[8] == 'A') && (ROM_Memory[9] == 'M') && (ROM_Memory[10] == 'S') && (ROM_Memory[11] == '!'))
    {
        u32 total_len = (ROM_Memory[7] << 24) | (ROM_Memory[6] << 16) | (ROM_Memory[5] << 8) | (ROM_Memory[4] << 0);

        // ---------------------------------------------------------------------------
        // Set the 32 banks of CART memory. Not all banks need to be present and will
        // contain 0x00 values for any part of the cart not present in the .CPR file
        // ---------------------------------------------------------------------------
        for (u8 bank=0; bank<32; bank++)
        {
            CartBankPtr[bank] = (u8 *) (cart_buffer + (0x4000 * bank));
        }

        u32 idx = 0x0C; // The first chunk is here...

        while (idx < (total_len-4))
        {
            if ((ROM_Memory[idx] == 'c') && (ROM_Memory[idx+1] == 'b'))
            {
                u8  bank        = ((ROM_Memory[idx+2] - '0') * 10) + (ROM_Memory[idx+3] - '0');
                u16 chunk_size  = (ROM_Memory[idx+5] << 8) | (ROM_Memory[idx+4] << 0);
                for (int i=0; i<chunk_size; i++)
                {
                    CartBankPtr[bank][i] = ROM_Memory[idx+8+i];
                }
                idx += 8 + chunk_size;
            }
            else break;
        }
    }
}


// -------------------------------------------------------------------
// Dandanator support is limited to cart banking into zones only.
// There is no writing back of the EEPROM (really it's Flash) for
// games that might make use of save states. For that, we can just
// use our Save/Load state handling for the emulator.
// -------------------------------------------------------------------
void DandanatorLoad(void)
{
    // -------------------------------------------------------------------
    // Not much to do really. The Dandanator file is laid out in sequential
    // blocks of 16K from 0..31 for a total of 512K of cart file memory.
    // This is now contained in our ROM_Memory[] buffer ready to use...
    // -------------------------------------------------------------------
    DAN_Zone0     = 0x00;   // Zone 0 enabled on slot0
    DAN_Zone1     = 0x20;   // Zone 1 disabled on slot0
    DAN_Config    = 0x00;   // Zones 0 and 1 high address bit
    DAN_WaitCFG   = 0x00;   // When waiting for a RET opcode
    DAN_Follow    = 28;     // FollowRomEn bank for zone 0
    DAN_WaitRET   = 0;      // Flag for when we wait to configure memory
}


// -Address-     0      1      2      3      4      5      6      7
// 0000-3FFF   RAM_0  RAM_0  RAM_4  RAM_0  RAM_0  RAM_0  RAM_0  RAM_0
// 4000-7FFF   RAM_1  RAM_1  RAM_5  RAM_3  RAM_4  RAM_5  RAM_6  RAM_7
// 8000-BFFF   RAM_2  RAM_2  RAM_6  RAM_2  RAM_2  RAM_2  RAM_2  RAM_2
// C000-FFFF   RAM_3  RAM_7  RAM_7  RAM_7  RAM_3  RAM_3  RAM_3  RAM_3
ITCM_CODE void ConfigureMemory(void)
{
    u8 *LROM_Ptr = (u8 *) OS_6128;
    u8 *UROM_Ptr = (u8 *) BASIC_6128;
    if (UROM == 7) // AMSDOS or PARADOS (Disk ROM)
    {
        if (myGlobalConfig.diskROM)
            UROM_Ptr = (u8 *)PARADOS;
        else
            UROM_Ptr = (u8 *)AMSDOS;
    }

    // ------------------------------------------------------------
    // Cartridge Support... This is not a full CPC+ implementation
    // but is good enough for using CPR as a storage medium.
    // ------------------------------------------------------------
    if (amstrad_mode == MODE_CPR)
    {
        // ------------------------------------------------------
        // CPC+ map their logical cart banks starting at 128...
        // There are, at most 32 banks supported for 512K total.
        // ------------------------------------------------------
        if (UROM & 0x80)
        {
            UROM_Ptr = CartBankPtr[UROM & 0x1F];
        }
        else
        {
            if (UROM == 3) UROM_Ptr = (u8 *)AMSDOS;
            else UROM_Ptr = (u8 *) BASIC_6128;
        }

        LROM_Ptr = CartBankPtr[0];
    }

    u8 *upper_ram_block = RAM_Memory+0x10000;

    // ------------------------------------------------------------------
    // For the DS-Lite/Phat, if a game calls for more than the standard
    // 128K of RAM found in a CPC 6128, we start to steal blocks of 64K
    // from the back-end of the ROM_Memory[] buffer. The hope is that
    // we would not need to use both that much ROM_Memory[] and extended
    // RAM_Memory[] at the same time... but this is a compromise as we
    // have limited RAM. Steal the 64K blocks starting at the far (back)
    // end of the buffer so there is less chance of the two memory ends
    // meeting and causing a catastrophe of biblical proportions.
    // ------------------------------------------------------------------
    u8 bank = RAM_512k_bank | ((MMR >> 3) & 0x7);
    if (bank > ram_highwater) ram_highwater = bank;

    if (isDSiMode()) // DSi has plenty of memory so we don't need to steal from the ROM_Memory[]
    {
        switch (bank)
        {
            case 0:  upper_ram_block = RAM_Memory+0x10000; break;
            case 1:  upper_ram_block = DSi_ExpandedRAM+0x00000; break;
            case 2:  upper_ram_block = DSi_ExpandedRAM+0x10000; break;
            case 3:  upper_ram_block = DSi_ExpandedRAM+0x20000; break;
            case 4:  upper_ram_block = DSi_ExpandedRAM+0x30000; break;
            case 5:  upper_ram_block = DSi_ExpandedRAM+0x40000; break;
            case 6:  upper_ram_block = DSi_ExpandedRAM+0x50000; break;
            case 7:  upper_ram_block = DSi_ExpandedRAM+0x60000; break;
            case 8:  upper_ram_block = DSi_ExpandedRAM+0x70000; break;
            case 9:  upper_ram_block = DSi_ExpandedRAM+0x80000; break;
            case 10: upper_ram_block = DSi_ExpandedRAM+0x90000; break;
            case 11: upper_ram_block = DSi_ExpandedRAM+0xA0000; break;
            case 12: upper_ram_block = DSi_ExpandedRAM+0xB0000; break;
            case 13: upper_ram_block = DSi_ExpandedRAM+0xC0000; break;
            case 14: upper_ram_block = DSi_ExpandedRAM+0xD0000; break;
            case 15: upper_ram_block = DSi_ExpandedRAM+0xE0000; break;
        }
    }
    else // DS-Lite/Phat we steal memory from the ROM_Memory[]
    {
        switch (bank & 7)
        {
            case 0: upper_ram_block = RAM_Memory+0x10000; break;
            case 1: upper_ram_block = ROM_Memory+0xF0000; break;
            case 2: upper_ram_block = ROM_Memory+0xE0000; break;
            case 3: upper_ram_block = ROM_Memory+0xD0000; break;
            case 4: upper_ram_block = ROM_Memory+0xC0000; break;
            case 5: upper_ram_block = ROM_Memory+0xB0000; break;
            case 6: upper_ram_block = ROM_Memory+0xA0000; break;
            case 7: upper_ram_block = ROM_Memory+0x90000; break;
        }
    }

    // ----------------------------------------------------------------------
    // The heart of the memory management system utilizes MMR to tell us
    // what blocks get mapped where in the 64K Z80 memory space.  One
    // neat thing about the Amstrad is that the RAM is always write-thru
    // so even if ROM is mapped into a 16K block, the RAM under it can
    // always be written (which is convenient since ROM can't be written to)
    // ----------------------------------------------------------------------
    switch (MMR & 0x7)
    {
      case 0x00: // 0-1-2-3
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : LROM_Ptr;   // Lower ROM
        MemoryMapR[1] = RAM_Memory + 0x4000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (RAM_Memory + 0xC000) : UROM_Ptr;   // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = RAM_Memory + 0x4000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = RAM_Memory + 0xC000;
        break;

      case 0x01: // 0-1-2-7
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : LROM_Ptr;   // Lower ROM
        MemoryMapR[1] = RAM_Memory + 0x4000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (upper_ram_block + 0xC000) : UROM_Ptr;  // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = RAM_Memory + 0x4000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = upper_ram_block + 0xC000;
        break;

      case 0x02: // 4-5-6-7
        MemoryMapR[0] = (RMR & 0x04) ? (upper_ram_block + 0x0000) : LROM_Ptr;   // Lower ROM
        MemoryMapR[1] = upper_ram_block + 0x4000 ;
        MemoryMapR[2] = upper_ram_block + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (upper_ram_block + 0xC000) : UROM_Ptr;   // Upper ROM

        MemoryMapW[0] = upper_ram_block + 0x0000;
        MemoryMapW[1] = upper_ram_block + 0x4000;
        MemoryMapW[2] = upper_ram_block + 0x8000;
        MemoryMapW[3] = upper_ram_block + 0xC000;
        break;

      case 0x03: // 0-3-2-7
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : LROM_Ptr;   // Lower ROM
        MemoryMapR[1] = RAM_Memory + 0xC000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (upper_ram_block + 0xC000) : UROM_Ptr;  // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = RAM_Memory + 0xC000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = upper_ram_block + 0xC000;
        break;

      case 0x04: // 0-4-2-3
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : LROM_Ptr;   // Lower ROM
        MemoryMapR[1] = upper_ram_block + 0x0000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (RAM_Memory + 0xC000) : UROM_Ptr;   // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = upper_ram_block + 0x0000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = RAM_Memory + 0xC000;
        break;

      case 0x05: // 0-5-2-3
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : LROM_Ptr;   // Lower ROM
        MemoryMapR[1] = upper_ram_block + 0x4000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (RAM_Memory + 0xC000) : UROM_Ptr;   // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = upper_ram_block + 0x4000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = RAM_Memory + 0xC000;
        break;

      case 0x06: // 0-6-2-3
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : LROM_Ptr;   // Lower ROM
        MemoryMapR[1] = upper_ram_block + 0x8000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (RAM_Memory + 0xC000) : UROM_Ptr;   // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = upper_ram_block + 0x8000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = RAM_Memory + 0xC000;
        break;

      case 0x07: // 0-7-2-3
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : LROM_Ptr;   // Lower ROM
        MemoryMapR[1] = upper_ram_block + 0xC000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (RAM_Memory + 0xC000) : UROM_Ptr;   // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = upper_ram_block + 0xC000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = RAM_Memory + 0xC000;
        break;
    }


    // ------------------------------------------------------------------------
    // And after all is said and done above, the last thing we do is check if
    // any Dandanator Mini cart memory is mapped in which overrides everything
    // else... but only for memory reads (writes always go to RAM).
    //
    // Registers B and C are read by the CPLD as follows:
    // Bits 4-0:  Slot to assign to zone 0-31
    // Bit  5:    ‘0’ zone enabled, ‘1’ zone disabled
    // Other bits unused
    //
    // Dandanator Configuration Register:
    //   bit 7: Config Function 1 vs Function 0
    //
    //   Function 1:
    //   bit 6: Delay configuration until RET instruction seen
    //   bit 5: Disable Further Dandanator commands until reset
    //   bit 4: Enable FollowRomEn on RET (only read if bit 6 = 1)
    //   b3-b2: A15 values for zone 1 and zone 0.
    //   Zone 0 Can be at 0x0000 or 0x8000, zone 1 can be at 0x4000 or 0xC000
    //   b1-b0: EEPROM_CE for zone 1 and zone 0. “0”: Enabled, “1” Disabled.
    //
    //   Function 0:
    //   b5-b6: Unused
    //   b4-b3: Slot to use for Zone 0 in 'RollowRomEn' mode... 28 + these bits
    //   bit 2: Bit bang output to USB Serial Port (not emulated)
    //   bit 1: EEPROM Write Enable/Disable (not emulated)
    //   bit 0: Serial Port Enable/Disable (not emulated)
    //
    // Note: I found the Dandanator Programmer Guide a little confusing and
    // it's not at all clear how the CE for Zone0/1 should be utilized. In
    // the end, I mostly followed the way CPCEC emulator handles it (and if
    // you look at that codebase, you can see they had trouble getting all of
    // the various Dandanator carts to run and had to revert to doing checks
    // in very specific orders or omitting checks to get some compilation packs
    // to run). Until more exact details can be found, this will do...
    // ------------------------------------------------------------------------
    if (amstrad_mode == MODE_DAN)
    {
        if (!(DAN_Config & 0x20)) // Is the Dandanator mapped in?
        {
            if ((DAN_Zone0 & 0x20) == 0) // Is Zone 0 Enabled?
            {
                if (DAN_Config & 0x04) // High bit A15 enabled for Zone 0?
                {
                    MemoryMapR[2] = &ROM_Memory[(DAN_Zone0 & 0x1F) * 0x4000];
                }
                else if (!(DAN_Config & 0x01)) // Make sure the Flash is CE (Chip Enabled) for Zone 0
                {
                    MemoryMapR[0] = &ROM_Memory[(DAN_Zone0 & 0x1F) * 0x4000];
                }
            }

            if ((DAN_Zone1 & 0x20) == 0) // Is Zone 1 Enabled?
            {
                if (DAN_Config & 0x08) // High bit A15 enabled for Zone 1?
                {
                    MemoryMapR[3] = &ROM_Memory[(DAN_Zone1 & 0x1F) * 0x4000];
                }
                else if (!(DAN_Config & 0x02)) // Make sure the Flash is CE (Chip Enabled) for Zone 1
                {
                    MemoryMapR[1] = &ROM_Memory[(DAN_Zone1 & 0x1F) * 0x4000];
                }
            }
        }
        else if (DAN_Config & 0x10) // RollowRomEn: This is the "poor mans" ROMBOX support
        {
            if (!(RMR & 0x04)) MemoryMapR[0] = &ROM_Memory[DAN_Follow * 0x4000];
            if (!(RMR & 0x08)) MemoryMapR[3] = &ROM_Memory[(DAN_Zone1 & 0x1F) * 0x4000];
        }
    }

    // -------------------------------------------------------------------------------
    // Offset so lookup is faster in Z80 core - this way we don't have to use a mask.
    // -------------------------------------------------------------------------------
    MemoryMapR[0] -= 0x0000;
    MemoryMapR[1] -= 0x4000;
    MemoryMapR[2] -= 0x8000;
    MemoryMapR[3] -= 0xC000;

    MemoryMapW[0] -= 0x0000;
    MemoryMapW[1] -= 0x4000;
    MemoryMapW[2] -= 0x8000;
    MemoryMapW[3] -= 0xC000;
}

// Keyboard Matrix... Scan row is PortC and read returned on PortA (low bits active)
//Bit:      7       6       5       4       3       2       1       0
//&40       F Dot   ENTER   F3      F6      F9      CURDN   CURRT   CURUP
//&41       F0      F2      F1      F5      F8      F7      COPY    CURLEFT
//&42       CTL     \`      SHIFT   F4      ] }     RETURN  [ {     CLR
//&43       .>      /?      : *     ; +     P       @ ¦     - =     ^ £
//&44       ,<      M       K       L       I       O       9 )     0 _
//&45       SPACE   N       J       H       Y       U       7 '     8 (
//&46       V       B       F       G       T       R       5 %     6 &
//&47       X       C       D       S       W       E       3 #     4 $
//&48       Z       CAPLK   A       TAB     Q       ESC     2 "     1 !
//&49       DEL     J1-F3   J1-F2   J1-F1   J1-RT   J1-LF   J1-DN   J1-UP

ITCM_CODE unsigned char cpu_readport_ams(register unsigned short Port)
{
    if (!(Port & 0x0480)) // FDC Controller
    {
        return ReadFDC(Port);
    }

    // -------------------------------------
    // We are loosely emulating CRTC Type 3
    // -------------------------------------
    if ((Port & 0x4200) == 0x0200) // CRTC Read
    {
        if (0==1) // CRTC 0 - Not Ready Yet....
        {
            if ((Port & 0x0100) == 0) // Status
            {
                return 0xFF;    // No status register on CRTC 0
            }
            else // Register
            {
                u8 reg_val = 0x00;

                // Registers 12-17 can be read-back on CRTC 0
                if (CRT_Idx == 12) reg_val = CRTC[CRT_Idx] & 0x3F;
                if (CRT_Idx == 13) reg_val = CRTC[CRT_Idx] & 0xFF;
                if (CRT_Idx == 14) reg_val = CRTC[CRT_Idx] & 0x3F;
                if (CRT_Idx == 15) reg_val = CRTC[CRT_Idx] & 0xFF;
                if (CRT_Idx == 16) reg_val = CRTC[CRT_Idx] & 0x3F;
                if (CRT_Idx == 17) reg_val = CRTC[CRT_Idx] & 0xFF;

                // For CRTC 0 all other registers return 0x00
                return reg_val;
            }
        }
        else // CRTC 3 - both Status and Register reads return the Registers
        {
            // CRTC 3 uses an odd mapping of Index to Registers on read-back
            switch (CRT_Idx & 0x07)
            {
                case 0x00:  return CRTC[16];
                case 0x01:  return CRTC[17];
                case 0x02:  return CRTC[10];
                case 0x03:  return CRTC[11];
                case 0x04:  return CRTC[12];
                case 0x05:  return CRTC[13];
                case 0x06:  return CRTC[14];
                case 0x07:  return CRTC[15];
            }
        }
    }

    if (!(Port & 0x0800)) // PPI - PSG
    {
        switch ((Port >> 8) & 0x03)
        {
            case 0: // 0xF4xx (Port A)
                if (myAY.ayRegIndex == 0x0E) // Keyboard/Joystick on PSG reg14
                {
                    // Otherwise return Keyboard bits...
                    u8 keyBits = 0x00;
                    for (u8 i=0; i< (kbd_keys_pressed ? kbd_keys_pressed:1); i++) // We may have more than one key pressed...
                    {
                        if (kbd_keys_pressed) kbd_key = kbd_keys[i]; else kbd_key = 0;

                        switch (portC & 0xF)
                        {
                            case 0x00:
                                if (kbd_key == KBD_KEY_CUP)  keyBits |= 0x01;
                                if (kbd_key == KBD_KEY_CRT)  keyBits |= 0x02;
                                if (kbd_key == KBD_KEY_CDN)  keyBits |= 0x04;
                                if (kbd_key == KBD_KEY_F9)   keyBits |= 0x08;
                                if (kbd_key == KBD_KEY_F6)   keyBits |= 0x10;
                                if (kbd_key == KBD_KEY_F3)   keyBits |= 0x20;
                                if (kbd_key == KBD_KEY_FENT) keyBits |= 0x40;
                                if (kbd_key == KBD_KEY_FDOT) keyBits |= 0x80;
                                break;

                            case 0x01:
                                if (kbd_key == KBD_KEY_CLT) keyBits |= 0x01;
                                if (kbd_key == KBD_KEY_CPY) keyBits |= 0x02;
                                if (kbd_key == KBD_KEY_F7)  keyBits |= 0x04;
                                if (kbd_key == KBD_KEY_F8)  keyBits |= 0x08;
                                if (kbd_key == KBD_KEY_F5)  keyBits |= 0x10;
                                if (kbd_key == KBD_KEY_F1)  keyBits |= 0x20;
                                if (kbd_key == KBD_KEY_F2)  keyBits |= 0x40;
                                if (kbd_key == KBD_KEY_F0)  keyBits |= 0x80;
                                break;

                            case 0x02:
                                if (kbd_key == KBD_KEY_CTL) keyBits |= 0x80;
                                if (kbd_key == KBD_KEY_BSL) keyBits |= 0x40;
                                if (kbd_key == KBD_KEY_SFT) keyBits |= 0x20;
                                if (kbd_key == KBD_KEY_F4)  keyBits |= 0x10;
                                if (kbd_key == ']')         keyBits |= 0x08;
                                if (kbd_key == KBD_KEY_RET) keyBits |= 0x04;
                                if (kbd_key == '[')         keyBits |= 0x02;
                                if (kbd_key == KBD_KEY_CLR) keyBits |= 0x01;

                                // And handle the special modifier keys
                                if (last_special_key == KBD_KEY_SFT) keyBits |= 0x20;
                                if (last_special_key == KBD_KEY_CTL) keyBits |= 0x80;
                                break;

                            case 0x03:
                                if (kbd_key == '.')         keyBits |= 0x80;
                                if (kbd_key == '/')         keyBits |= 0x40;
                                if (kbd_key == ':')         keyBits |= 0x20;
                                if (kbd_key == ';')         keyBits |= 0x10;
                                if (kbd_key == 'P')         keyBits |= 0x08;
                                if (kbd_key == '@')         keyBits |= 0x04;
                                if (kbd_key == '-')         keyBits |= 0x02;
                                if (kbd_key == '^')         keyBits |= 0x01;
                                break;

                            case 0x04:
                                if (kbd_key == ',')         keyBits |= 0x80;
                                if (kbd_key == 'M')         keyBits |= 0x40;
                                if (kbd_key == 'K')         keyBits |= 0x20;
                                if (kbd_key == 'L')         keyBits |= 0x10;
                                if (kbd_key == 'I')         keyBits |= 0x08;
                                if (kbd_key == 'O')         keyBits |= 0x04;
                                if (kbd_key == '9')         keyBits |= 0x02;
                                if (kbd_key == '0')         keyBits |= 0x01;
                                break;

                            case 0x05:
                                if (kbd_key == ' ')         keyBits |= 0x80;
                                if (kbd_key == 'N')         keyBits |= 0x40;
                                if (kbd_key == 'J')         keyBits |= 0x20;
                                if (kbd_key == 'H')         keyBits |= 0x10;
                                if (kbd_key == 'Y')         keyBits |= 0x08;
                                if (kbd_key == 'U')         keyBits |= 0x04;
                                if (kbd_key == '7')         keyBits |= 0x02;
                                if (kbd_key == '8')         keyBits |= 0x01;
                                break;

                            case 0x06:
                                if (kbd_key == 'V')         keyBits |= 0x80;
                                if (kbd_key == 'B')         keyBits |= 0x40;
                                if (kbd_key == 'F')         keyBits |= 0x20;
                                if (kbd_key == 'G')         keyBits |= 0x10;
                                if (kbd_key == 'T')         keyBits |= 0x08;
                                if (kbd_key == 'R')         keyBits |= 0x04;
                                if (kbd_key == '5')         keyBits |= 0x02;
                                if (kbd_key == '6')         keyBits |= 0x01;
                                break;

                            case 0x07:
                                if (kbd_key == 'X')         keyBits |= 0x80;
                                if (kbd_key == 'C')         keyBits |= 0x40;
                                if (kbd_key == 'D')         keyBits |= 0x20;
                                if (kbd_key == 'S')         keyBits |= 0x10;
                                if (kbd_key == 'W')         keyBits |= 0x08;
                                if (kbd_key == 'E')         keyBits |= 0x04;
                                if (kbd_key == '3')         keyBits |= 0x02;
                                if (kbd_key == '4')         keyBits |= 0x01;
                                break;

                            case 0x08:
                                if (kbd_key == 'Z')         keyBits |= 0x80;
                                if (kbd_key == KBD_KEY_CAPS)keyBits |= 0x40;
                                if (kbd_key == 'A')         keyBits |= 0x20;
                                if (kbd_key == KBD_KEY_TAB) keyBits |= 0x10;
                                if (kbd_key == 'Q')         keyBits |= 0x08;
                                if (kbd_key == KBD_KEY_ESC) keyBits |= 0x04;
                                if (kbd_key == '2')         keyBits |= 0x02;
                                if (kbd_key == '1')         keyBits |= 0x01;
                                break;

                            case 0x09:
                                if (kbd_key == KBD_KEY_DEL) keyBits |= 0x80;
                                if (JoyState & JST_FIRE3)   keyBits |= 0x40;
                                if (JoyState & JST_FIRE2)   keyBits |= 0x20;
                                if (JoyState & JST_FIRE)    keyBits |= 0x10;
                                if (JoyState & JST_RIGHT)   keyBits |= 0x08;
                                if (JoyState & JST_LEFT)    keyBits |= 0x04;
                                if (JoyState & JST_DOWN)    keyBits |= 0x02;
                                if (JoyState & JST_UP)      keyBits |= 0x01;
                                break;
                        }
                    }

                    return ~keyBits;
                }
                else // Normal register
                {
                    return ay38910DataR(&myAY);
                }
                break;
            case 1: // 0xF5xx (Port B)
                return portB;
                break;
            case 2: // 0xF6xx (Port C)
                return portC;
                break;
            case 3: // 0xF7xx (Control)
                return 0xFF;
                break;
        }
    }

    return 0xFF;  // Unused port returns 0xFF
}

// -----------------------------------------------------------------------------
// #BCXX    %x0xxxx00 xxxxxxxx  6845 CRTC Index - Write
// #BDXX    %x0xxxx01 xxxxxxxx  6845 CRTC Data Out - Write
// #BEXX    %x0xxxx10 xxxxxxxx  6845 CRTC Status (as far as supported) - Read
// #BFXX    %x0xxxx11 xxxxxxxx  6845 CRTC Data In (as far as supported) - Read
// -----------------------------------------------------------------------------

ITCM_CODE void cpu_writeport_ams(register unsigned short Port,register unsigned char Value)
{
    if (!(Port & 0x0800)) // PPI / PSG
    {
        switch ((Port >> 8) & 0x03)
        {
            case 0x00:
                portA = Value;
                if ((portC & 0xC0) == 0x80) // AY Data Write into Register
                {
                    ay38910DataW(portA, &myAY);
                }

                if ((portC & 0xC0) == 0xC0) // AY Register Select
                {
                    if (portA < 16) ay38910IndexW(portA & 0xF, &myAY);
                }
                break;

            case 0x02:
                portC = Value;

                // PortC high bits:
                // 0 0   Inactive
                // 0 1   Read from selected PSG register.When function is set, the PSG will make the register data available to PPI Port A
                // 1 0   Write to selected PSG register. When set, the PSG will take the data at PPI Port A and write it into the selected PSG register
                // 1 1   Select PSG register. When set, the PSG will take the data at PPI Port A and select a register
                if (portC & 0xC0) // Is this a PSG access?
                {
                    if ((portC & 0xC0) == 0x80) // AY Data Write into Register
                    {
                        ay38910DataW(portA, &myAY);
                    }

                    if ((portC & 0xC0) == 0xC0) // AY Register Select
                    {
                        if (portA < 16) ay38910IndexW(portA & 0xF, &myAY);
                    }
                }
                break;

            case 0x03:  // PPI Control
                if ((Value & 0x80) == 0) // Bit Control
                {
                    u8 bit = (Value & 0x0E) >> 1;
                    if (Value & 1) // Set
                    {
                        portC |= (1 << bit);
                    }
                    else // Clear
                    {
                        portC &= ~(1 << bit);
                    }
                }
                else // Port Direction...
                {
                    // CPC will clear the latch on a direction change of PortA
                    if ((portDIR & 0x10) != (Value & 0x10))
                    {
                        portC = 0x00;
                    }
                    portDIR = Value;
                }
                break;
            }
    }

    if (!(Port & 0x4000)) // CRTC
    {
        switch ((Port >> 8) & 0x03)
        {
            case 0x00:
                CRT_Idx = Value & 0x1F;
                break;

            case 0x01:
                CRTC[CRT_Idx] = Value & CRTC_MASKS[CRT_Idx];
                if (CRT_Idx == 9)
                {
                    // On CRTC 0/3 if we write R[9] lower than VLC, then VLC resets to zero
                    if (CRTC[9] < VLC)  VLC = 0;
                }
                break;
        }
    }

    if ((Port & 0xC000 ) == 0x4000) // Video Gate Array
    {
        switch ((Value >> 6) & 0x03)
        {
            case 0x00:  // PENR - Pen Register
                PENR = Value;
                break;

            case 0x01:  // INKR - Ink Register
                if (PENR & 0x10)
                {
                    INK[16] = ink_map[Value & 0x1F];
                    border_color = (INK[16] << 24) | (INK[16] << 16) | (INK[16] << 8) | (INK[16] << 0);
                }
                else
                {
                    if (INK[PENR & 0xF] != ink_map[Value & 0x1F])
                    {
                        INK[PENR & 0xF] = ink_map[Value & 0x1F];
                        inks_changed = 1;
                    }
                }
                break;

            case 0x02:  // RMR - ROM Map and Graphics Mode
                // Are we changing graphic modes?
                if ((RMR & 0x03) != (Value & 0x03))
                {
                    inks_changed = 1; // Force ink change
                }

                RMR = Value;
                if (RMR & 0x10)
                {
                    R52 = 0; // Force R52 counter to zero...
                }
                ConfigureMemory();
                break;

            case 0x03:  // MMR - RAM Memory Mapping
                MMR = Value;
                if (isDSiMode()) // DS-Lite/Phat only supports the 512K expansion
                {
                    // -----------------------------------------------------------------------------
                    // For memory above 512K, we follow the "RAM7/Yarek-style" of expansion which
                    // uses the Port address bit 8 as the selector for which 512K chunk is mapped
                    // in. Basically if the programmer hits port &7Fxx they are using the standard
                    // lower 512K chunk and if they hit port &7Exx they map in the upper 512k bank.
                    // -----------------------------------------------------------------------------
                    RAM_512k_bank = (Port & 0x100) ? 0x00:0x08; // Bit 8 is inverted here
                }
                ConfigureMemory();
                break;
        }
    }

    if (!(Port & 0x0480))  // FDC
    {
        WriteFDC(Port, Value);
    }

    if (!(Port & 0x2000))   //Upper ROM Number
    {
        UROM = Value;
        ConfigureMemory();
    }

    return;
}


// ----------------------------------------------------------------------
// Reset the emulation. Freshly decompress the contents of RAM memory
// and setup the CPU registers exactly as the snapshot indicates. Then
// we can start the emulation at exactly the point it left off... This
// works fine so long as the game does not need to go back out to the
// tape to load another segment of the game.
// ----------------------------------------------------------------------
void amstrad_reset(void)
{
    if (!DSi_ExpandedRAM && isDSiMode())
    {
        DSi_ExpandedRAM = malloc(1024*1024); // Grab an extra 1024K of RAM for the DSi which will support 1024K+64K of emulated CPC memory
    }
    memset(DSi_ExpandedRAM, 0x00, 1024*1024); // Clear all expanded RAM

    ResetFDC();

    MMR = 0x00;
    RMR = 0x00;
    UROM = 0x00;
    RAM_512k_bank = 0;
    ConfigureMemory();

    memset(INK, 0x00, sizeof(INK));

    border_color = 0;

    memset(pre_inked_mode0, 0x00, sizeof(pre_inked_mode0));
    memset(pre_inked_mode1, 0x00, sizeof(pre_inked_mode1));

    memset(ink_map, 0x00, sizeof(ink_map));
    ink_map[0x14] = 0;
    ink_map[0x04] = 1;
    ink_map[0x15] = 2;
    ink_map[0x1C] = 3;
    ink_map[0x18] = 4;
    ink_map[0x1D] = 5;
    ink_map[0x0C] = 6;
    ink_map[0x05] = 7;

    ink_map[0x0D] = 8;
    ink_map[0x16] = 9;
    ink_map[0x06] = 10;
    ink_map[0x17] = 11;
    ink_map[0x1E] = 12;
    ink_map[0x00] = 13;
    ink_map[0x1F] = 14;
    ink_map[0x0E] = 15;

    ink_map[0x07] = 16;
    ink_map[0x0F] = 17;
    ink_map[0x12] = 18;
    ink_map[0x02] = 19;
    ink_map[0x13] = 20;
    ink_map[0x1A] = 21;
    ink_map[0x19] = 22;
    ink_map[0x1B] = 23;

    ink_map[0x0A] = 24;
    ink_map[0x03] = 25;
    ink_map[0x0B] = 26;
    ink_map[0x10] = 27;
    ink_map[0x08] = 28;
    ink_map[0x01] = 29;
    ink_map[0x11] = 30;
    ink_map[0x09] = 31;

    for (int i=0; i<32; i++) CartBankPtr[i] = (u8*)0;

    crtc_reset();

    memset(RAM_Memory, 0x00, sizeof(RAM_Memory));

    ram_highwater = 0;

    CPU.PC.W            = 0x0000;   // Z80 entry point
    CPU.SP.W            = 0x0000;   // Z80 entry point
    portA               = 0x00;
    portB               = 0xFF;     // 50Hz, Amstrad, VSync set
    portC               = 0x00;
    portDIR             = 0x00;

    scanline_count = 1;

    sna_last_motor = 0;
    sna_last_track = 0;

    // The current Color Palette
    for (int ink=0; ink<17; ink++)
    {
       INK[ink] = ink_map[0];
    }

    border_color = (INK[16] << 24) | (INK[16] << 16) | (INK[16] << 8) | (INK[16] << 0);

    compute_pre_inked(0);
    compute_pre_inked(1);
    compute_pre_inked(2);

    if (amstrad_mode == MODE_DSK)
    {
        // ---------------------------------------------------
        // Check if this is MEGABLASTERS 2020 Re-Release
        // This needs a patch to get it past the CRCT checks
        // since this emulation is not accurate enough...
        // ---------------------------------------------------
        if (getCRC32(ROM_Memory+0x100, 0x1000) == 0xC7119924)
        {
            amstrad_mode = MODE_MEG;
        }
        else
        {
            DiskInsert(initial_file, false);
        }
    }

    if (amstrad_mode == MODE_CPR)
    {
        CartLoad();
        ConfigureMemory();
    }
    else if (amstrad_mode == MODE_DAN)
    {
        DandanatorLoad();
        ConfigureMemory();
    }
    else if ((amstrad_mode == MODE_SNA) || (amstrad_mode == MODE_MEG))
    {
        if (amstrad_mode == MODE_MEG)
        {
            memcpy(ROM_Memory, MEGALOAD, sizeof(MEGALOAD));
        }

        // ----------------------------------
        // The memory .SNA snapshot format.
        // ----------------------------------
        u8 snap_ver = ROM_Memory[0x10];
        (void)snap_ver;

        CPU.AF.B.l = ROM_Memory[0x11]; //F
        CPU.AF.B.h = ROM_Memory[0x12]; //A

        CPU.BC.B.l = ROM_Memory[0x13]; //C
        CPU.BC.B.h = ROM_Memory[0x14]; //B

        CPU.DE.B.l = ROM_Memory[0x15]; //E
        CPU.DE.B.h = ROM_Memory[0x16]; //D

        CPU.HL.B.l = ROM_Memory[0x17]; //L
        CPU.HL.B.h = ROM_Memory[0x18]; //H

        CPU.R      = ROM_Memory[0x19]; // Low 7-bits of Refresh
        CPU.I      = ROM_Memory[0x1A]; // Interrupt register
        CPU.R_HighBit = (CPU.R & 0x80);// High bit of refresh

        CPU.IFF     = ((ROM_Memory[0x1B] & 1) ? IFF_1 : 0x00);
        CPU.IFF    |= ((ROM_Memory[0x1C] & 1) ? IFF_2 : 0x00);

        CPU.IX.B.l  = ROM_Memory[0x1D]; // IX low
        CPU.IX.B.h  = ROM_Memory[0x1E]; // IX high

        CPU.IY.B.l  = ROM_Memory[0x1F]; // IY low
        CPU.IY.B.h  = ROM_Memory[0x20]; // IY high

        CPU.SP.B.l = ROM_Memory[0x21]; // SP low byte
        CPU.SP.B.h = ROM_Memory[0x22]; // SP high byte

        CPU.PC.B.l = ROM_Memory[0x23]; // PC low byte
        CPU.PC.B.h = ROM_Memory[0x24]; // PC high byte

        CPU.IFF   |= ((ROM_Memory[0x25] & 3) == 1 ? IFF_IM1 : (((ROM_Memory[0x25] & 3) == 2 ? IFF_IM2 : 0)));

        CPU.AF1.B.l = ROM_Memory[0x26]; // AF'
        CPU.AF1.B.h = ROM_Memory[0x27];

        CPU.BC1.B.l = ROM_Memory[0x28]; // BC'
        CPU.BC1.B.h = ROM_Memory[0x29];

        CPU.DE1.B.l = ROM_Memory[0x2A]; // DE'
        CPU.DE1.B.h = ROM_Memory[0x2B];

        CPU.HL1.B.l = ROM_Memory[0x2C]; // HL'
        CPU.HL1.B.h = ROM_Memory[0x2D];

        PENR = ROM_Memory[0x2E]; // Selected Pen

        // The current Color Palette
        for (int ink=0; ink<17; ink++)
        {
           INK[ink] = ink_map[ROM_Memory[0x2F+ink]];
        }

        border_color = (INK[16] << 24) | (INK[16] << 16) | (INK[16] << 8) | (INK[16] << 0);

        compute_pre_inked(0);
        compute_pre_inked(1);
        compute_pre_inked(2);

        RMR = ROM_Memory[0x40]; // ROM configuration
        MMR = ROM_Memory[0x41]; // RAM configuration
        ConfigureMemory();

        extern u32 VCC, VLC;
        VCC = ROM_Memory[0xAB];
        VCC = (VCC/8)*8;
        VLC = ROM_Memory[0xAC];

        CRT_Idx = ROM_Memory[0x42];
        for (int i=0; i<18; i++)
        {
            CRTC[i] = ROM_Memory[0x43 + i];
        }

        //portA = ROM_Memory[0x56];
        //portB = ROM_Memory[0x57];
        portC = ROM_Memory[0x58];

        // Some disk status if available
        sna_last_motor = ROM_Memory[0x9C];
        sna_last_track = ROM_Memory[0x9D];

        for (u8 i=0; i<16; i++)
        {
            ay38910IndexW(i, &myAY);
            ay38910DataW(ROM_Memory[0x5B], &myAY);
        }
        ay38910IndexW(ROM_Memory[0x5A], &myAY);

        u32 dump_size = ROM_Memory[0x6B] * 1024;

        if (dump_size > 0)
        {
            memcpy(RAM_Memory, ROM_Memory+0x100, dump_size);
        }
        else // Compressed MEM0, MEM1, etc...
        {
            // -----------------------------------------------
            // We always have MEM0 so we process that now...
            // -----------------------------------------------
            u32 ram_idx = 0;
            u8 *romPtr = ROM_Memory+0x104;
            u32 comp_size = romPtr[0] | (romPtr[1] << 8);

            romPtr += 4;
            for (int i=0; i<comp_size; i++)
            {
                if (romPtr[i] == 0xE5)
                {
                    i++;
                    u16 repeat = romPtr[i];
                    if (repeat == 0) RAM_Memory[ram_idx++] = 0xe5;
                    else
                    {
                        i++;
                        for (int j=0; j<repeat; j++)
                        {
                            RAM_Memory[ram_idx++] = romPtr[i];
                        }
                    }
                }
                else
                {
                    RAM_Memory[ram_idx++] = romPtr[i];
                }
            }

            // -----------------------------------------------------------------------
            // And now look for MEM1 in case it's present... That brings us to 128K
            // -----------------------------------------------------------------------
            romPtr += comp_size;
            if ((romPtr[0] == 'M') && (romPtr[1] == 'E') && (romPtr[2] == 'M') && (romPtr[3] == '1'))
            {
                romPtr += 4;
                ram_idx = 0x10000;
                comp_size = romPtr[0] | (romPtr[1] << 8);
                romPtr += 4;
                for (int i=0; i<comp_size; i++)
                {
                    if (romPtr[i] == 0xE5)
                    {
                        i++;
                        u16 repeat = romPtr[i];
                        if (repeat == 0) RAM_Memory[ram_idx++] = 0xe5;
                        else
                        {
                            i++;
                            for (int j=0; j<repeat; j++)
                            {
                                RAM_Memory[ram_idx++] = romPtr[i];
                            }
                        }
                    }
                    else
                    {
                        RAM_Memory[ram_idx++] = romPtr[i];
                    }
                }
            }

            // ------------------------------------------------------------------------------------
            // Finally, check for any extended memory blocks 2-8... For possible 512K expanded SNA
            // ------------------------------------------------------------------------------------
            for (int ex_mem = 0; ex_mem < 7; ex_mem++)
            {
                romPtr += comp_size;
                if ((romPtr[0] == 'M') && (romPtr[1] == 'E') && (romPtr[2] == 'M') && (romPtr[3] == '2'+ex_mem))
                {
                    romPtr += 4;
                    u32 rom_idx = 0xF0000 - (0x10000 * ex_mem);
                    comp_size = romPtr[0] | (romPtr[1] << 8);
                    romPtr += 4;
                    for (int i=0; i<comp_size; i++)
                    {
                        if (romPtr[i] == 0xE5)
                        {
                            i++;
                            u16 repeat = romPtr[i];
                            if (repeat == 0) ROM_Memory[rom_idx++] = 0xe5;
                            else
                            {
                                i++;
                                for (int j=0; j<repeat; j++)
                                {
                                    ROM_Memory[rom_idx++] = romPtr[i];
                                }
                            }
                        }
                        else
                        {
                            ROM_Memory[rom_idx++] = romPtr[i];
                        }
                    }
                }
            }
        }

        // If MEGABLASTERS, switch back the original diskette
        if (amstrad_mode == MODE_MEG)
        {
            DiskInsert(initial_file, true);
        }
    }

    return;
}


// -----------------------------------------------------------------------------
// Run the emulation for exactly 1 scanline and handle the VDP interrupt if
// the emulation has executed the last line of the frame.
//
// We also process 1 line worth of Audio and render one screen line for
// the CRTC display controller. This is a carefully choreographed dance
// between the CRTC controller, audio processor and CPU - and as with most
// emulation - it's not perfect. We have a few tweaks/tricks that can be
// configured to help keep things in alignment and keep the game running.
// -----------------------------------------------------------------------------
ITCM_CODE u32 amstrad_run(void)
{
    // Process 1 scanline worth of AY audio
    if (myConfig.waveDirect) processDirectAudio();

    // -----------------------------------------------------------------------
    // Execute half of the current scanline - we do this to get as close
    // to accurate as possible - half the CPU line, then the CRTC line
    // then the back-half of the CPU line. This way we're no more than half
    // a line "wrong" at any time which is good enough for 98% of CPC games.
    // -----------------------------------------------------------------------
    CPU.Target += 128;
    ExecZ80(CPU.Target);

    // Process 1 scanline for the mighty CRTC controller chip
    u8 vsync = crtc_render_screen_line();

    // Finish the scanline plus any user-called for adjustment...
    CPU.Target += 128;
    ExecZ80(CPU.Target);

    if (vsync) // Will return non-zero if VSYNC started
    {
        if (++refresh_tstates & 0x10) // Every 16 Frames, reset counters to prevent overflow
        {
            refresh_tstates = 0;
            CPU.TStates = CPU.TStates - CPU.Target;
            CPU.Target = 0;

            // ---------------------------------------------------------------------------
            // If we came up significantly short we make an attempt to adjust the VCC to
            // put the CPU and CRTC back into alignment. Due to the line-based emulation,
            // if things are not firing perfectly on games that have very tight timings,
            // we can end up with a mess. This is a last ditch effort to get the system
            // running by skewing the CRTC VCC counter and the CPU line-based emulation.
            // The one game that definitely is improved by this is Galactic Tomb 128K.
            // ---------------------------------------------------------------------------
            if (scanline_count < (250 * 16))
            {
                VCC = 0; // Shock the monkey!
                VLC = 0;
                R52 = 0;
            }
            scanline_count = 0; // Will be incremented to 1 directly below
        }
    }
    scanline_count++;

    return vsync; // Return '1' if end of frame.. '0' if not end of frame
}

// End of file
