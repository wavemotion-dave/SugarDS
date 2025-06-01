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
#include "printf.h"

//CRTC Register set:
//========================================================================================================================
//R0    Horizontal total character number           63     Define the width of a scanline (including borders)
//R1    Horizontal displayed character number       40     Define when DISPEN goes OFF on the scanline (in CRTC-Char)
//R2    Position of horizontal sync. pulse          46     Define when the HSync goes ON on the scanline
//R3    Width of horizontal/vertical sync. pulses   &8E    VSync width can only be changed on type 3 and 4
//R4    Vertical total Line character number        38     Define the height of a screen
//R5    Vertical raster adjust                      0      Define additionnal scanlines at the end of a screen
//R6    Vertical displayed character number         25     Define when DISPEN remains OFF until a new screen starts
//R7    Position of vertical sync. pulse            30     Define when the VSync goes ON on a screen
//R8    Interlaced mode                             0      Configure various stuff such as interlaced mode, skews, Results greatly differs between CRTC types
//R9    Maximum raster                              7      Define the height of a CRTC-Char in scanline
//R10   Cursor start raster                         0      Useless on the Amstrad CPC/Plus (text-mode is not wired)
//R11   Cursor end                                  0      Useless on the Amstrad CPC/Plus (text-mode is not wired)
//R12   Display Start Address (High)                &20    Define the MSB of MA when a CRTC-screen starts
//R13   Display Start Address (Low)                 &00    Define the LSB of MA when a CRTC-screen starts
//R14   Cursor Address (High)                       0      Useless on the Amstrad CPC/Plus (text-mode is not wired)
//R15   Cursor Address (Low)                        0      Useless on the Amstrad CPC/Plus (text-mode is not wired)
//R16   Light Pen Address (High)                    0      Hold the MSB of the cursor position when the lightpen was ON
//R17   Light Pen Address (Low)                     0      Hold the LSB of the cursor position when the lightpen was ON

u32 HCC     __attribute__((section(".dtcm"))) = 0;                // Horizontal Character Counter
u32 HSC     __attribute__((section(".dtcm"))) = 0;                // Horizontal Sync Counter
u32 VCC     __attribute__((section(".dtcm"))) = 0;                // Vertical Character Counter
u32 VSC     __attribute__((section(".dtcm"))) = 0;                // Vertical Sync Counter
u32 VLC     __attribute__((section(".dtcm"))) = 0;                // Vertical Line Counter
u32 R52     __attribute__((section(".dtcm"))) = 0;                // Raster flip-flop counter for interrupt generation every 52 scanlines
u32 VTAC    __attribute__((section(".dtcm"))) = 0;                // Vertical Total Adjust Counter (we count down instead of up)
u8  DISPEN  __attribute__((section(".dtcm"))) = 0;                // Set to 1 when display is visible and drawing

int current_ds_line      __attribute__((section(".dtcm"))) = 0;    // Used to track where we are drawing on the DS/DSi LCD
u32 vsync_plus_two       __attribute__((section(".dtcm"))) = 0;    // Generate interrupt 2 raster lines after VSync
u32 r12_screen_offset    __attribute__((section(".dtcm"))) = 0;    // Latched R12/R13 at VSync
u8  crtc_force_vsync     __attribute__((section(".dtcm"))) = 0;    // Set to 1 to force rupture to occur by writing R7 to zero
u8  vsync_off_count      __attribute__((section(".dtcm"))) = 0;    // Set to 16 when VSYNC occurs to count down to turning it off
u8  *cpc_ScreenPage      __attribute__((section(".dtcm"))) = 0;    // Screen Memory address to pull video pixel data
u16 escapeClause         __attribute__((section(".dtcm"))) = 0;    // To ensure we never let the DS get stuck 
u32 cpc_scanline_counter __attribute__((section(".dtcm"))) = 0;    // This is where we are in the CPC graphics memory 

int R52_INT_ON_VSYNC[]   __attribute__((section(".dtcm"))) = {20, 10, 32};
u8 vSyncSeen             __attribute__((section(".dtcm"))) = 0;
u8 display_disable_in    __attribute__((section(".dtcm"))) = 0;
u8 b32K_Mode             __attribute__((section(".dtcm"))) = 0;

void crtc_reset(void)
{
    HCC = 0;
    HSC = 0;
    VCC = 0;
    VSC = 0;
    VLC = 0;
    R52 = 0;
    VTAC = 0;
    DISPEN = 0;
    b32K_Mode = 0;

    current_ds_line = 0;
    vsync_plus_two = 0;
    r12_screen_offset = 0;
    crtc_force_vsync = 0;
    vsync_off_count = 0;
    escapeClause = 320;
    vSyncSeen = 0;
    display_disable_in = 0;
    
    CRTC[0] = 63;
    CRTC[1] = 40;
    CRTC[2] = 46;
    CRTC[4] = 38;
    CRTC[5] = 0;
    CRTC[6] = 25;
    CRTC[7] = 30;
    CRTC[8] = 0;
    CRTC[9] = 7;
    CRTC[12] = 0;
    CRTC[13] = 0;
}


//R52 will return to 0 and the Gate Array will send an interrupt request on any of these conditions:
//  - When it exceeds 51
//  - By setting bit4 of the RMR register of the Gate Array to 1
//  - At the end of the 2nd HSYNC after the start of the VSYNC
//
//When the Gate Array sends an interrupt request:
//  If the interrupts were authorized at the time of the request, then bit5 of R52 is cleared (but R52 was reset to 0 anyway) and the interrupt takes place
//  If interrupts are not authorized, then the R52 counter continues to increment and the interrupt remains armed (the Gate Array then maintains its INT signal). 
//  When interrupts are enabled (using the EI instruction), bit5 of R52 is cleared and the interrupt takes place. This happens only after the instruction that
//  follows EI as this Z80 instruction has a 1-instruction delay.

void crtc_r52_int(void)
{
    CPU.IRequest = INT_RST38;
    IntZ80(&CPU, CPU.IRequest);
}

// ------------------------------------------------------------------------------------
// Only when we start to render a new frame do we latch the screen memory and offsets
//
// From CRTC[12]. It's always memory in the lower 64K RAM however.
// B13   B12    Video Page          B11  B10     Page Size
// 0     0      0000 - 3FFF         0    0       16KB
// 0     1      4000 - 7FFF         0    1       16KB
// 1     0      8000 - BFFF         1    0       16KB
// 1     1      C000 - FFFF         1    1       32KB
// ------------------------------------------------------------------------------------
void crtc_new_frame_coming_up(void)
{
    DISPEN = 1;                 // Next frame coming up... Display On
    cpc_scanline_counter = 0;   // And we're at the top of the raster memory
    cpc_ScreenPage = RAM_Memory + (((CRTC[12] & 0x30) >> 4) * 0x4000);
    r12_screen_offset = (((u16)(CRTC[12] & 3) << 8) | CRTC[13]) << 1;
    
    b32K_Mode = ((CRTC[12] & 0xC) == 0xC) ? 1:0;
}

// ----------------------------------------------------------------------------
// Render one screen line of pixels. This is called on every visible scanline
// and is heavily optimized to draw as fast as possible. Since the screen is
// often drawing background (paper vs ink), that's handled via look-up table.
//
// RMR lower 2 bits:
// 0    0   Mode 0, 160x200 resolution, 16 colors
// 0    1   Mode 1, 320x200 resolution, 4 colors
// 1    0   Mode 2, 640x200 resolution, 2 colors
// 1    1   Mode 3, 160x200 resolution, 4 colors (undocumented)
// ----------------------------------------------------------------------------
ITCM_CODE u8 crtc_render_screen_line(void)
{
    u8 vSyncDS = 0;             // To inform caller if we are at DS VSync
    u32 *vidBufDS;              // This is where we draw to on the DS LCD

    cpc_scanline_counter++;
    
    // -------------------------------------------------
    // See if the inks have changed since the last call.
    // -------------------------------------------------
    if (inks_changed)
    {
        compute_pre_inked(RMR & 0x03);
        inks_changed = 0;
    }

    // -------------------------------------------
    // Increment R52 every horizontal flyback and
    // see if we should fire the raster interrupt.
    // -------------------------------------------
    if ((++R52 & 0x3F) >= 52)
    {
        crtc_r52_int();     // Our ~300Hz timer
        R52 = 0;            // And reset the R52 counter
    }

    // ----------------------------------------------
    // Two HSYNC lines after VSYNC we fire interrupt
    // ----------------------------------------------
    if (vsync_plus_two)
    {
        if (--vsync_plus_two == 0)
        {
            // --------------------------------------------------------------------
            // According to spec, must be above 31 to fire interrupt here
            // But as the timing of SugarDS isn't perfect, we are more forgiving...
            // --------------------------------------------------------------------
            if (R52 >= R52_INT_ON_VSYNC[myConfig.r52IntVsync]) crtc_r52_int();
            R52 = 0;                // Always reset the R52 counter on VSync+2
            vSyncDS = 1;            // Inform caller of frame end - our DS 'vSync' if you will...
            escapeClause = 320;     // Reset watchdog
            vSyncSeen = 1;          // We've seen a vertical sync this frame
        }
    }

    // -----------------------------------------------
    // The VSYNC width fixed at 16 scanlines long...
    // -----------------------------------------------
    if (vsync_off_count)
    {
        if (--vsync_off_count == 0)
        {
            portB &= ~0x01;         // Clear VSYNC
        }
    }
    
    if (crtc_force_vsync)
    {
        crtc_force_vsync = 0; // Only one force per frame
        portB |= 0x01;        // Set VSYNC
        vsync_plus_two = 2;   // Interrupt in 2 raster lines
        vsync_off_count = 16; // And turn off in 16 scanlines
    }

    // -------------------------------------------------------------------------
    // The heart of the CRTC system we use the internal counters to 'time' the
    // vertical character generation, increment line counters and handle VSYNC.
    // -------------------------------------------------------------------------
    VLC &= 0x1F;
    if (++VLC >= (CRTC[9]+1))
    {
        VLC = 0;    // Reset counter - build up the next character line
        VCC++;      // We have finished one vertical character

        if (VCC == CRTC[7]) // Vertical Sync reached?
        {
            portB |= 0x01;        // Set VSYNC
            vsync_plus_two = 2;   // Interrupt in 2 raster lines
            vsync_off_count = 16; // And turn off in 16 scanlines
        }

        if (VCC >= (CRTC[4]+1)) // Full "Screen" rendered?
        {
            VTAC = 1;   // Assume no extra lines... may adjust below
            VCC = 0;    // Start counting characters again
            
            if (vSyncSeen)
            {
                vSyncSeen = 0;                         // Setup for next VSYNC
                current_ds_line = -myConfig.screenTop; // Top of LCD screen
                
                if (((CRTC[5] & 0x1F) > 0)) // Vertical Total Adjustment?
                {
                    // -------------------------------------------
                    // Extra scanlines before we enable display.
                    // Add 1 as we will process immediately below.
                    // -------------------------------------------                
                    VTAC = (CRTC[5] & 0x1F) + 1;
                }
            }
        }
        
        if (VCC == CRTC[6]) // End of visible display?
        {
            if ((CRTC[5] & 0x1F) > 0) // Vertical Total Adjustment?
            {
                display_disable_in = (CRTC[5] & 0x1F);
            }
            else
            {
                DISPEN = 0; // Turn off display (render border)
            }
        }
    }

    // -------------------------------------------------------
    // Counting extra vertical lines (total line adjustment)?
    // -------------------------------------------------------
    if (VTAC)
    {
        if (--VTAC == 0)
        {
            crtc_new_frame_coming_up();
        }
    }
    
    // ---------------------------------------------------------------
    // Check watchdog... Ensure we never get into a no-sync situation.
    // ---------------------------------------------------------------
    if (--escapeClause == 0)
    {
        escapeClause = 320;     // Reset the watchdog
        VCC = 0;                // Start again... something went wrong
        vSyncDS = 1;            // Inform caller of frame end - our DS 'vSync' if you will...
    }

    // ----------------------------------------------------------
    // Video buffer... write 32-bits at a time for maximum speed
    // and render directly to the video memory. This can cause
    // some slight tearing effects as we're dealing with 50Hz
    // emulation on a 60Hz DSi LCD refresh... but good enough!
    // ----------------------------------------------------------
    vidBufDS = (u32*)(0x06000000 + (current_ds_line * 512));
    
    // ------------------------------------------------------------------------
    // This is a poor-mans horizontal sync "scroll trick". Some CPC games
    // will modify the HSync width to produce smoother scrolling horizontal
    // effects... we do a very simplified version of that by shifting 4 pixels
    // when we are 'odd' and normal display when we are 'even'. Games like
    // Super Edge Grinder will now scroll perfectly smoothly... 
    // ------------------------------------------------------------------------
    if (CRTC[3] & 1) vidBufDS++;

    // If the display is enabled... render screen.
    if ((current_ds_line & 0xFFFFFF00) == 0) // If we are in the 256 visible lines...
    {
        if (DISPEN)
        {
            // ------------------------------------------------------------------------------------------------
            // The display memory normally at 0xC000 but can be modified by CRTC[12]. Always in lower 64K RAM.
            // The pixel data is stored in 2K chunks of memory within a 16K block of memory. This is a bit
            // unusual, but we follow the addressing formula completely including wrapping at 2K blocks.
            // ------------------------------------------------------------------------------------------------
            u8 *pixelPtr2K = (cpc_ScreenPage + (((cpc_scanline_counter) % (CRTC[9]+1)) * 2048));

            u32 offset = (((cpc_scanline_counter)/(CRTC[9]+1)) * (CRTC[1]<<1));  // Base offset is based on current scanline
            offset += r12_screen_offset;                                         // As defined in R12/R13 - offset is in WORDs (2 bytes)
            
            // -------------------------------------------------------------
            // For the rare '32K video buffer' we handle this but only at
            // the point of computing where in memory this scanline starts.
            // -------------------------------------------------------------
            if (((offset&0xFFF) >= 0x800) && b32K_Mode)
            {
                offset += 0x4000; // We address into the next bank of memory
            }
            
            offset &= 0x47FF;                                               // Wrap is on 2K boundary

            // -------------------------------------------------------------------------------------
            // With 2, 4 or 8 pixels per byte, there are always 80 bytes of horizontal screen data
            // -------------------------------------------------------------------------------------
            if ((RMR & 0x03) == 0x00) // Mode 0  (160x256)
            {
                for (int x=0; x<CRTC[1]; x++)
                {
                    *vidBufDS++ = pre_inked_mode0[pixelPtr2K[offset++]];    // Fast draw pixel with pre-rendered lookup
                    *vidBufDS++ = pre_inked_mode0[pixelPtr2K[offset++]];    // Fast draw pixel with pre-rendered lookup
                    offset &= 0x47FF;                                       // Wrap is always at the 2K boundary
                }

                // Draw border out to at least 320 pixels
                for (u16 i=0; i<16;i++)
                {
                    *vidBufDS++ = border_color;
                }
            }
            else if ((RMR & 0x03) == 0x01) // Mode 1 (320x256)
            {
                for (int x=0; x<(CRTC[1]); x++)
                {
                    *vidBufDS++ = pre_inked_mode1[pixelPtr2K[offset++]];  // Fast draw pixel with pre-rendered lookup
                    *vidBufDS++ = pre_inked_mode1[pixelPtr2K[offset++]];  // Fast draw pixel with pre-rendered lookup
                    offset &= 0x47FF;                                     // Wrap is always at the 2K boundary
                }

                // Draw border out to at least 320 pixels
                for (u16 i=0; i<16;i++)
                {
                    *vidBufDS++ = border_color;
                }
            }
            else if ((RMR & 0x03) == 0x02) // Mode 2 (640x256)
            {
                for (int x=0; x<(CRTC[1]); x++)
                {
                    *vidBufDS++ = pre_inked_mode2a[pixelPtr2K[offset++]];  // Fast draw pixel with pre-rendered lookup
                    *vidBufDS++ = pre_inked_mode2b[pixelPtr2K[offset++]];  // Fast draw pixel with pre-rendered lookup
                    offset &= 0x47FF;                                      // Wrap is always at the 2K boundary
                }
            }
            else // Mode 3 never used... hopefully!
            {
                DY++;
            }
        }
        else // Display not enabled - render border
        {
            for (int x=0; x<(CRTC[1]<<1)+16; x++)
            {
                *vidBufDS++ = border_color;
            }
        }
    }

    if (display_disable_in)
    {
        if (--display_disable_in == 0)
        {
            DISPEN = 0; // No visible lines until next frame
        }
    }

    current_ds_line++;

    return vSyncDS;
}
