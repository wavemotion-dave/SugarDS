// =====================================================================================
// Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
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
#include "fdc.h"
#include "lzav.h"

#define SUGAR_SAVE_VER   0x0004     // Change this if the basic format of the .SAV file changes. Invalidates older .sav files.

/*********************************************************************************
 * Save the current state - save everything we need to a single .sav file.
 ********************************************************************************/
u8  spare[256] = {0x00};            // We keep some spare bytes so we can use them in the future without changing the structure

static char szLoadFile[256];        // We build the filename out of the base filename and tack on .sav, .ee, etc.
static char tmpStr[32];             // For various screen status strings

u8 CompressBuffer[150*1024];        // Big enough to handle compression of even full 128K games

void amstradSaveState()
{
  size_t retVal;

  // Return to the original path
  chdir(initial_path);

  // Init filename = romname and SAV in place of ROM
  DIR* dir = opendir("sav");
  if (dir) closedir(dir);    // Directory exists... close it out and move on.
  else mkdir("sav", 0777);   // Otherwise create the directory...
  sprintf(szLoadFile,"sav/%s", initial_file);

  int len = strlen(szLoadFile);
  szLoadFile[len-3] = 's';
  szLoadFile[len-2] = 'a';
  szLoadFile[len-1] = 'v';

  strcpy(tmpStr,"SAVING...");
  DSPrint(18,0,0,tmpStr);

  FILE *handle = fopen(szLoadFile, "wb+");
  if (handle != NULL)
  {
    // Write Version
    u16 save_ver = SUGAR_SAVE_VER;
    retVal = fwrite(&save_ver, sizeof(u16), 1, handle);

    // Write Last Directory Path / Disk File
    if (retVal) retVal = fwrite(last_path, sizeof(last_path), 1, handle);
    if (retVal) retVal = fwrite(last_file, sizeof(last_file), 1, handle);

    // Write CZ80 CPU
    if (retVal) retVal = fwrite(&CPU, sizeof(CPU), 1, handle);

    // Write AY Chip info
    if (retVal) retVal = fwrite(&myAY, sizeof(myAY), 1, handle);
    
    // Write the FDC floppy struct
    if (retVal) retVal = fwrite(&fdc, sizeof(fdc), 1, handle);
    
    // Write CRTC info
    if (retVal) retVal = fwrite(CRTC,  sizeof(CRTC), 1, handle);
    if (retVal) retVal = fwrite(&CRT_Idx, sizeof(CRT_Idx), 1, handle);
    
    // And a bunch more CRTC and Amstrad misc stuff...
    if (retVal) retVal = fwrite(&HCC,               sizeof(HCC),                1, handle);
    if (retVal) retVal = fwrite(&HSC,               sizeof(HSC),                1, handle);
    if (retVal) retVal = fwrite(&VCC,               sizeof(VCC),                1, handle);
    if (retVal) retVal = fwrite(&VSC,               sizeof(VSC),                1, handle);
    if (retVal) retVal = fwrite(&VLC,               sizeof(VLC),                1, handle);
    if (retVal) retVal = fwrite(&R52,               sizeof(R52),                1, handle);
    if (retVal) retVal = fwrite(&R52,               sizeof(R52),                1, handle);
    if (retVal) retVal = fwrite(&VTAC,              sizeof(VTAC),               1, handle);
    if (retVal) retVal = fwrite(&DISPEN,            sizeof(DISPEN),             1, handle);

    if (retVal) retVal = fwrite(&current_ds_line,   sizeof(current_ds_line),    1, handle);
    if (retVal) retVal = fwrite(&vsync_plus_two,    sizeof(vsync_plus_two),     1, handle);
    if (retVal) retVal = fwrite(&r12_screen_offset, sizeof(r12_screen_offset),  1, handle);
    if (retVal) retVal = fwrite(&crtc_force_vsync,  sizeof(crtc_force_vsync),   1, handle);
    if (retVal) retVal = fwrite(&vsync_off_count,   sizeof(vsync_off_count),    1, handle);
    if (retVal) retVal = fwrite(&escapeClause,      sizeof(escapeClause),       1, handle);
    if (retVal) retVal = fwrite(&vSyncSeen,         sizeof(vSyncSeen),          1, handle);
    if (retVal) retVal = fwrite(&display_disable_in,sizeof(display_disable_in), 1, handle);
    if (retVal) retVal = fwrite(&scanline_count,    sizeof(scanline_count),     1, handle);
    if (retVal) retVal = fwrite(&b32K_Mode,         sizeof(b32K_Mode),          1, handle);
    
    if (retVal) retVal = fwrite(&MMR,               sizeof(MMR),                1, handle);
    if (retVal) retVal = fwrite(&RMR,               sizeof(RMR),                1, handle);
    if (retVal) retVal = fwrite(&PENR,              sizeof(PENR),               1, handle);
    if (retVal) retVal = fwrite(&UROM,              sizeof(UROM),               1, handle);
    
    if (retVal) retVal = fwrite(&border_color,      sizeof(border_color),       1, handle);
    if (retVal) retVal = fwrite(INK,                sizeof(INK),                1, handle);
    if (retVal) retVal = fwrite(ink_map,            sizeof(ink_map),            1, handle);
    if (retVal) retVal = fwrite(&inks_changed,      sizeof(inks_changed),       1, handle);
    if (retVal) retVal = fwrite(&refresh_tstates,   sizeof(refresh_tstates),    1, handle);

    if (retVal) retVal = fwrite(&portA,             sizeof(portA),              1, handle);
    if (retVal) retVal = fwrite(&portB,             sizeof(portB),              1, handle);
    if (retVal) retVal = fwrite(&portC,             sizeof(portC),              1, handle);
    
    if (retVal) retVal = fwrite(&DAN_Zone0,         sizeof(DAN_Zone0),          1, handle);
    if (retVal) retVal = fwrite(&DAN_Zone1,         sizeof(DAN_Zone1),          1, handle);
    if (retVal) retVal = fwrite(&DAN_Config,        sizeof(DAN_Config),         1, handle);

    if (retVal) retVal = fwrite(spare,              256,                        1, handle);

    // The RAM highwater tells us how many extra RAM banks were utilized
    if (retVal) retVal = fwrite(&ram_highwater,     sizeof(ram_highwater),      1, handle);

    // -------------------------------------------------------------------
    // Save Z80 Memory Map... All 128K of it!
    //
    // Compress the RAM data using 'high' compression ratio... it's
    // still quite fast for such small memory buffers and often shrinks
    // 128K of memory down to less than 32K (or 64K at worst).
    // -------------------------------------------------------------------
    int max_len = lzav_compress_bound_hi( 0x20000 );
    int comp_len = lzav_compress_hi( RAM_Memory, CompressBuffer, 0x20000, max_len );

    if (retVal) retVal = fwrite(&comp_len,          sizeof(comp_len), 1, handle);
    if (retVal) retVal = fwrite(&CompressBuffer,    comp_len,         1, handle);
    
    if (ram_highwater) // If we used more than one extra 64K bank... we need to save those
    {
        u8 *upper_ram_block = 0;
        for (u8 block=1; block<=ram_highwater; block++)
        {
            switch (block)
            {
                case 1: upper_ram_block = ROM_Memory+0xF0000; break;
                case 2: upper_ram_block = ROM_Memory+0xE0000; break;
                case 3: upper_ram_block = ROM_Memory+0xD0000; break;
                case 4: upper_ram_block = ROM_Memory+0xC0000; break;
                case 5: upper_ram_block = ROM_Memory+0xB0000; break;
                case 6: upper_ram_block = ROM_Memory+0xA0000; break;
                case 7: upper_ram_block = ROM_Memory+0x90000; break;
            }
    
            int max_len = lzav_compress_bound_hi( 0x10000 );        
            int comp_len = lzav_compress_hi( upper_ram_block, CompressBuffer, 0x10000, max_len );

            if (retVal) retVal = fwrite(&comp_len,          sizeof(comp_len), 1, handle);
            if (retVal) retVal = fwrite(&CompressBuffer,    comp_len,         1, handle);
        }
    }

    strcpy(tmpStr, (retVal ? "OK ":"ERR"));
    DSPrint(27,0,0,tmpStr);
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    DSPrint(18,0,0,"             ");
    DisplayStatusLine(true);
  }
  else {
    strcpy(tmpStr,"Error opening SAV file ...");
  }
  fclose(handle);
}


/*********************************************************************************
 * Load the current state - read everything back from the .sav file.
 ********************************************************************************/
void amstradLoadState()
{
    size_t retVal;

    // Return to the original path
    chdir(initial_path);

    // Init filename = romname and .SAV in place of ROM
    sprintf(szLoadFile,"sav/%s", initial_file);
    int len = strlen(szLoadFile);

    szLoadFile[len-3] = 's';
    szLoadFile[len-2] = 'a';
    szLoadFile[len-1] = 'v';

    FILE* handle = fopen(szLoadFile, "rb");
    if (handle != NULL)
    {
         strcpy(tmpStr,"LOADING...");
         DSPrint(18,0,0,tmpStr);

        // Read Version
        u16 save_ver = 0xBEEF;
        retVal = fread(&save_ver, sizeof(u16), 1, handle);

        if (save_ver == SUGAR_SAVE_VER)
        {
            // Read Last Directory Path / Disk File
            if (retVal) retVal = fread(last_path, sizeof(last_path), 1, handle);
            if (retVal) retVal = fread(last_file, sizeof(last_file), 1, handle);
            
            // ----------------------------------------------------------------
            // If the last known file was a disk file we want to reload it.
            // ----------------------------------------------------------------
            if ( (strcasecmp(strrchr(last_file, '.'), ".dsk") == 0) )
            {
                chdir(last_path);
                DiskInsert(last_file, true);
            }

            // Read CZ80 CPU
            if (retVal) retVal = fread(&CPU, sizeof(CPU), 1, handle);

            // Read AY Chip info
            if (retVal) retVal = fread(&myAY, sizeof(myAY), 1, handle);
            
            // Read the FDC floppy struct
            if (retVal) retVal = fread(&fdc, sizeof(fdc), 1, handle);
            if (fdc.ImgDsk) fdc.ImgDsk = DISK_IMAGE_BUFFER;
            
            // Read CRTC info
            if (retVal) retVal = fread(CRTC,  sizeof(CRTC), 1, handle);
            if (retVal) retVal = fread(&CRT_Idx, sizeof(CRT_Idx), 1, handle);
            
            // And a bunch more CRTC and Amstrad misc stuff...
            if (retVal) retVal = fread(&HCC,               sizeof(HCC),                1, handle);
            if (retVal) retVal = fread(&HSC,               sizeof(HSC),                1, handle);
            if (retVal) retVal = fread(&VCC,               sizeof(VCC),                1, handle);
            if (retVal) retVal = fread(&VSC,               sizeof(VSC),                1, handle);
            if (retVal) retVal = fread(&VLC,               sizeof(VLC),                1, handle);
            if (retVal) retVal = fread(&R52,               sizeof(R52),                1, handle);
            if (retVal) retVal = fread(&R52,               sizeof(R52),                1, handle);
            if (retVal) retVal = fread(&VTAC,              sizeof(VTAC),               1, handle);
            if (retVal) retVal = fread(&DISPEN,            sizeof(DISPEN),             1, handle);

            if (retVal) retVal = fread(&current_ds_line,   sizeof(current_ds_line),    1, handle);
            if (retVal) retVal = fread(&vsync_plus_two,    sizeof(vsync_plus_two),     1, handle);
            if (retVal) retVal = fread(&r12_screen_offset, sizeof(r12_screen_offset),  1, handle);
            if (retVal) retVal = fread(&crtc_force_vsync,  sizeof(crtc_force_vsync),   1, handle);
            if (retVal) retVal = fread(&vsync_off_count,   sizeof(vsync_off_count),    1, handle);
            if (retVal) retVal = fread(&escapeClause,      sizeof(escapeClause),       1, handle);
            if (retVal) retVal = fread(&vSyncSeen,         sizeof(vSyncSeen),          1, handle);
            if (retVal) retVal = fread(&display_disable_in,sizeof(display_disable_in), 1, handle);
            if (retVal) retVal = fread(&scanline_count,    sizeof(scanline_count),     1, handle);
            if (retVal) retVal = fread(&b32K_Mode,         sizeof(b32K_Mode),          1, handle);            
            
            if (retVal) retVal = fread(&MMR,               sizeof(MMR),                1, handle);
            if (retVal) retVal = fread(&RMR,               sizeof(RMR),                1, handle);
            if (retVal) retVal = fread(&PENR,              sizeof(PENR),               1, handle);
            if (retVal) retVal = fread(&UROM,              sizeof(UROM),               1, handle);
            
            if (retVal) retVal = fread(&border_color,      sizeof(border_color),       1, handle);
            if (retVal) retVal = fread(INK,                sizeof(INK),                1, handle);
            if (retVal) retVal = fread(ink_map,            sizeof(ink_map),            1, handle);
            if (retVal) retVal = fread(&inks_changed,      sizeof(inks_changed),       1, handle);
            if (retVal) retVal = fread(&refresh_tstates,   sizeof(refresh_tstates),    1, handle);

            if (retVal) retVal = fread(&portA,             sizeof(portA),              1, handle);
            if (retVal) retVal = fread(&portB,             sizeof(portB),              1, handle);
            if (retVal) retVal = fread(&portC,             sizeof(portC),              1, handle);
            
            if (retVal) retVal = fread(&DAN_Zone0,         sizeof(DAN_Zone0),          1, handle);
            if (retVal) retVal = fread(&DAN_Zone1,         sizeof(DAN_Zone1),          1, handle);
            if (retVal) retVal = fread(&DAN_Config,        sizeof(DAN_Config),         1, handle);            
            
            if (retVal) retVal = fread(spare,              256,                        1, handle);
            
            // The RAM highwater tells us how many extra RAM banks were utilized
            if (retVal) retVal = fread(&ram_highwater,     sizeof(ram_highwater),      1, handle);            
    
            // Load Z80 Memory Map... all 128K of it!
            int comp_len = 0;
            if (retVal) retVal = fread(&comp_len,          sizeof(comp_len), 1, handle);
            if (retVal) retVal = fread(&CompressBuffer,    comp_len,         1, handle);
            
            // ------------------------------------------------------------------
            // Decompress the previously compressed RAM and put it back into the
            // right memory location... this is quite fast all things considered.
            // ------------------------------------------------------------------
            (void)lzav_decompress( CompressBuffer, RAM_Memory, comp_len, 0x20000 );
            
            if (ram_highwater) // If we used more than one extra 64K bank... we need to save those
            {
                u8 *upper_ram_block = 0;
                for (u8 block=1; block<=ram_highwater; block++)
                {
                    int comp_len = 0;
                    if (retVal) retVal = fread(&comp_len,          sizeof(comp_len), 1, handle);
                    if (retVal) retVal = fread(&CompressBuffer,    comp_len,         1, handle);
                    
                    switch (block)
                    {
                        case 1: upper_ram_block = ROM_Memory+0xF0000; break;
                        case 2: upper_ram_block = ROM_Memory+0xE0000; break;
                        case 3: upper_ram_block = ROM_Memory+0xD0000; break;
                        case 4: upper_ram_block = ROM_Memory+0xC0000; break;
                        case 5: upper_ram_block = ROM_Memory+0xB0000; break;
                        case 6: upper_ram_block = ROM_Memory+0xA0000; break;
                        case 7: upper_ram_block = ROM_Memory+0x90000; break;
                    }
                    
                    (void)lzav_decompress( CompressBuffer, upper_ram_block, comp_len, 0x10000 );
                }
            }
            

            // And put the memory pointers back in place...
            ConfigureMemory();
            compute_pre_inked(0);
            compute_pre_inked(1);
            compute_pre_inked(2);

            strcpy(tmpStr, (retVal ? "OK ":"ERR"));
            DSPrint(27,0,0,tmpStr);

            WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
            DSPrint(18,0,0,"             ");
            DisplayStatusLine(true);
        }
        
        fclose(handle);
      }
      else
      {
        DSPrint(18,0,0,"NO SAVED GAME");
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(18,0,0,"             ");
      }
}

// End of file
