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
#include <nds/fifomessages.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fat.h>
#include <maxmod9.h>

#include "SugarDS.h"
#include "AmsUtils.h"
#include "cpc_kbd.h"
#include "debug_ovl.h"
#include "topscreen.h"
#include "mainmenu.h"
#include "soundbank.h"
#include "soundbank_bin.h"
#include "cpu/z80/Z80_interface.h"
#include "fdc.h"
#include "amsdos.h"
#include "printf.h"

// -----------------------------------------------------------------
// Most handy for development of the emulator is a set of 16 R/W
// registers and a couple of index vars... we show this when 
// global settings is set to show the 'Debugger'. It's amazing
// how handy these general registers are for emulation development.
// -----------------------------------------------------------------
u32 debug[0x10]={0};
u32 DX = 0;
u32 DY = 0;

u32 dampen_special_key = 0;
u8 floppy_sound=0;
u8 floppy_action=0;

u8 RAM_Memory[0x20000]      ALIGN(32) = {0};  // The Z80 Memory is 64K but the Amstrad CPC 6128 is 128K so we reserve the full amount
u8 ROM_Memory[MAX_ROM_SIZE] ALIGN(32) = {0};  // This is where we keep the raw untouched file as read from the SD card (.DSK, .SNA)

s16 temp_offset   __attribute__((section(".dtcm"))) = 0;
u16 slide_dampen  __attribute__((section(".dtcm"))) = 0;
u8 JITTER[]       __attribute__((section(".dtcm"))) = {0, 64, 128};
u8 debugger_pause __attribute__((section(".dtcm"))) = 0;

// ----------------------------------------------------------------------------
// We track the most recent directory and file loaded... both the initial one
// (for the CRC32) and subsequent additional tape loads (Side 2, Side B, etc)
// ----------------------------------------------------------------------------
static char cmd_line_file[256];
char initial_file[MAX_FILENAME_LEN] = "";
char initial_path[MAX_FILENAME_LEN] = "";
char last_path[MAX_FILENAME_LEN]    = "";
char last_file[MAX_FILENAME_LEN]    = "";

// --------------------------------------------------
// A few housekeeping vars to help with emulation...
// --------------------------------------------------
u8 bFirstTime        = 1;
u8 bottom_screen     = 0;
u8 bStartIn          = 0;

// ---------------------------------------------------------------------------
// Some timing and frame rate comutations to keep the emulation on pace...
// ---------------------------------------------------------------------------
u16 emuFps          __attribute__((section(".dtcm"))) = 0;
u16 emuActFrames    __attribute__((section(".dtcm"))) = 0;
u16 timingFrames    __attribute__((section(".dtcm"))) = 0;

// Set to 1 to pause (mute) sound, 0 is sound unmuted (sound channels active)
u8 soundEmuPause    __attribute__((section(".dtcm"))) = 1;

// -----------------------------------------------------------------------------------------------
// This set of critical vars is what determines the machine type -
// -----------------------------------------------------------------------------------------------
u8 amstrad_mode      __attribute__((section(".dtcm"))) = 0;       // See defines for the various modes...
u8 kbd_key           __attribute__((section(".dtcm"))) = 0;       // 0 if no key pressed, othewise the ASCII key (e.g. 'A', 'B', '3', etc)
u16 nds_key          __attribute__((section(".dtcm"))) = 0;       // 0 if no key pressed, othewise the NDS keys from keysCurrent() or similar
u8 last_mapped_key   __attribute__((section(".dtcm"))) = 0;       // The last mapped key which has been pressed - used for key click feedback
u8 kbd_keys_pressed  __attribute__((section(".dtcm"))) = 0;       // Each frame we check for keys pressed - since we can map keyboard keys to the NDS, there may be several pressed at once
u8 kbd_keys[12]      __attribute__((section(".dtcm")));           // Up to 12 possible keys pressed at the same time (we have 12 NDS physical buttons though it's unlikely that more than 2 or maybe 3 would be pressed)

u8 bStartSoundEngine = 0;      // Set to true to unmute sound after 1 frame of rendering...
int bg0, bg1, bg0b, bg1b;      // Some vars for NDS background screen handling
u16 vusCptVBL = 0;             // We use this as a basic timer for the Mario sprite... could be removed if another timer can be utilized
u8 touch_debounce = 0;         // A bit of touch-screen debounce
u8 key_debounce = 0;           // A bit of key debounce to ensure the key is held pressed for a minimum amount of time

// The DS/DSi has 10 keys that can be mapped
u16 NDS_keyMap[10] __attribute__((section(".dtcm"))) = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_A, KEY_B, KEY_X, KEY_Y, KEY_START, KEY_SELECT};

// ----------------------------------------------------------------------
// The key map for the Amstrad CPC... mapped into the NDS controller
// We allow mapping of the 5 joystick 'presses' (up, down, left, right
// and fire) along with most of the possible CPC keyboard keys.
// ----------------------------------------------------------------------
u16 keyCoresp[MAX_KEY_OPTIONS] __attribute__((section(".dtcm"))) = {
    JST_UP,     //0
    JST_DOWN,
    JST_LEFT,
    JST_RIGHT,
    JST_FIRE,
    JST_FIRE2,  //5 (alternate fire... not sure if used)
    JST_FIRE3,  //  (another alternate fire... not sure if used)

    META_KBD_A, //7
    META_KBD_B,
    META_KBD_C,
    META_KBD_D, //10
    META_KBD_E,
    META_KBD_F,
    META_KBD_G,
    META_KBD_H,
    META_KBD_I, //15
    META_KBD_J,
    META_KBD_K,
    META_KBD_L,
    META_KBD_M,
    META_KBD_N, //20
    META_KBD_O,
    META_KBD_P,
    META_KBD_Q,
    META_KBD_R,
    META_KBD_S, //25
    META_KBD_T,
    META_KBD_U,
    META_KBD_V,
    META_KBD_W,
    META_KBD_X, //30
    META_KBD_Y,
    META_KBD_Z,

    META_KBD_1, //33
    META_KBD_2,
    META_KBD_3,
    META_KBD_4,
    META_KBD_5,
    META_KBD_6,
    META_KBD_7,
    META_KBD_8,
    META_KBD_9,
    META_KBD_0, //42

    META_KBD_SHIFT,
    META_KBD_PERIOD,
    META_KBD_COMMA, //45
    META_KBD_COLON,
    META_KBD_SEMI,
    META_KBD_ATSIGN,
    META_KBD_SPACE,
    META_KBD_RETURN, //50
    
    META_KBD_PAN_UP16,
    META_KBD_PAN_UP24,
    META_KBD_PAN_UP32,
    META_KBD_PAN_DN16,
    META_KBD_PAN_DN24,
    META_KBD_PAN_DN32
};

static char tmp[64];    // For various sprintf() calls

// ------------------------------------------------------------
// Utility function to pause the sound...
// ------------------------------------------------------------
void SoundPause(void)
{
    soundEmuPause = 1;
}

// ------------------------------------------------------------
// Utility function to un pause the sound...
// ------------------------------------------------------------
void SoundUnPause(void)
{
    soundEmuPause = 0;
}

// --------------------------------------------------------------------------------------------
// MAXMOD streaming setup and handling...
// We were using the normal ARM7 sound core but it sounded "scratchy" and so with the help
// of FluBBa, we've swiched over to the maxmod sound core which performs much better.
// --------------------------------------------------------------------------------------------
#define sample_rate         (30800)    // To roughly match how many samples (4x per scanline x 312 scanlines x 50 frames)
#define buffer_size         (512+16)   // Enough buffer that we don't have to fill it too often. Must be multiple of 16.

mm_ds_system sys   __attribute__((section(".dtcm")));
mm_stream myStream __attribute__((section(".dtcm")));

#define WAVE_DIRECT_BUF_SIZE 4095
u16 mixer_read      __attribute__((section(".dtcm"))) = 0;
u16 mixer_write     __attribute__((section(".dtcm"))) = 0;
s16 mixer[WAVE_DIRECT_BUF_SIZE+1];


// The games normally run at the proper 100% speed, but user can override from 80% to 120%
u16 GAME_SPEED_PAL[]  __attribute__((section(".dtcm"))) = {655, 596, 547, 728, 818 };

// -------------------------------------------------------------------------------------------
// maxmod will call this routine when the buffer is half-empty and requests that
// we fill the sound buffer with more samples. They will request 'len' samples and
// we will fill exactly that many. If the sound is paused, we fill with 'mute' samples.
// -------------------------------------------------------------------------------------------
s16 last_sample __attribute__((section(".dtcm"))) = 0;
int breather    __attribute__((section(".dtcm"))) = 0;

ITCM_CODE mm_word OurSoundMixer(mm_word len, mm_addr dest, mm_stream_formats format)
{
    if (soundEmuPause)  // If paused, just "mix" in mute sound chip... all channels are OFF
    {
        s16 *p = (s16*)dest;
        for (int i=0; i<len; i++)
        {
           *p++ = last_sample;      // To prevent pops and clicks... just keep outputting the last sample
           *p++ = last_sample;      // To prevent pops and clicks... just keep outputting the last sample
        }
    }
    else
    {
        if (myConfig.waveDirect)
        {
            s16 *p = (s16*)dest;
            for (int i=0; i<len*2; i++)
            {
                if (mixer_read == mixer_write) {processDirectAudio();}
                else
                {
                    last_sample = mixer[mixer_read];
                    *p++ = last_sample;
                    mixer_read = (mixer_read + 1) & WAVE_DIRECT_BUF_SIZE;
                }
            }
            if (breather) {breather -= (len*2); if (breather < 0) breather = 0;}
        }
        else
        {
            ay38910Mixer(2*len, dest, &myAY);
        }
    }

    return len;
}

// --------------------------------------------------------------------------------------------
// This is called when we want to sample the audio directly - we grab 2x AY samples and mix
// them with the beeper tones. We do a little bit of edge smoothing on the audio  tones here
// to make the direct beeper sound a bit less harsh - but this really needs to be properly
// over-sampled and smoothed someday to make it really shine... good enough for now.
// --------------------------------------------------------------------------------------------
s16 mixbufAY[4]  __attribute__((section(".dtcm")));
s16 beeper_vol[4] __attribute__((section(".dtcm"))) = { 0x000, 0x200, 0x600, 0xA00 };
u32 vol __attribute__((section(".dtcm"))) = 0;
ITCM_CODE void processDirectAudio(void)
{
    ay38910Mixer(4, mixbufAY, &myAY);
    
    for (u8 i=0; i<4; i++)
    {
        if (breather) {return;}
        mixer[mixer_write] = mixbufAY[i];
        mixer_write++; mixer_write &= WAVE_DIRECT_BUF_SIZE;
        if (((mixer_write+1)&WAVE_DIRECT_BUF_SIZE) == mixer_read) {breather = 2048;}
    }
}

// -----------------------------------------------------------------------------------------------
// The user can override the core emulation speed from 80% to 120% to make games play faster/slow 
// than normal. We must adjust the MaxMode sample frequency to match or else we will not have the
// proper number of samples in our sound buffer... this isn't perfect but it's reasonably good!
// -----------------------------------------------------------------------------------------------
static u8 last_game_speed = 0;
static u32 sample_rate_adjust[] = {100, 110, 120, 90, 80};
void newStreamSampleRate(void)
{
    if (last_game_speed != myConfig.gameSpeed)
    {
        last_game_speed = myConfig.gameSpeed;
        mmStreamClose();

        // Adjust the sample rate to match the core emulation speed... user can override from 80% to 120%
        int new_sample_rate     = (sample_rate * sample_rate_adjust[myConfig.gameSpeed]) / 100;
        myStream.sampling_rate  = new_sample_rate;        // sample_rate for the CPC to match the AY/Beeper drivers
        myStream.buffer_length  = buffer_size;            // buffer length = (512+16)
        myStream.callback       = OurSoundMixer;          // set callback function
        myStream.format         = MM_STREAM_16BIT_STEREO; // format = stereo 16-bit
        myStream.timer          = MM_TIMER0;              // use hardware timer 0
        myStream.manual         = false;                  // use automatic filling
        mmStreamOpen(&myStream);
    }
}

// -----------------------------------------------------------------------
// Setup the maxmod audio stream - this will be a 16-bit Stereo PCM 
// output at 55KHz which sounds about right for the Amstrad CPC AY chip.
// -----------------------------------------------------------------------
void setupStream(void)
{
  //----------------------------------------------------------------
  //  initialize maxmod with our small 4-effect soundbank
  //----------------------------------------------------------------
  mmInitDefaultMem((mm_addr)soundbank_bin);

  mmLoadEffect(SFX_CLICKNOQUIT);
  mmLoadEffect(SFX_KEYCLICK);
  mmLoadEffect(SFX_MUS_INTRO);
  mmLoadEffect(SFX_FLOPPY3);

  //----------------------------------------------------------------
  //  open stream
  //----------------------------------------------------------------
  myStream.sampling_rate  = sample_rate;            // sample_rate for the CPC to match the AY/Beeper drivers
  myStream.buffer_length  = buffer_size;            // buffer length = (512+16)
  myStream.callback       = OurSoundMixer;          // set callback function
  myStream.format         = MM_STREAM_16BIT_STEREO; // format = stereo 16-bit
  myStream.timer          = MM_TIMER0;              // use hardware timer 0
  myStream.manual         = false;                  // use automatic filling
  mmStreamOpen(&myStream);

  //----------------------------------------------------------------
  //  when using 'automatic' filling, your callback will be triggered
  //  every time half of the wave buffer is processed.
  //
  //  so:
  //  25000 (rate)
  //  ----- = ~21 Hz for a full pass, and ~42hz for half pass
  //  1200  (length)
  //----------------------------------------------------------------
  //  with 'manual' filling, you must call mmStreamUpdate
  //  periodically (and often enough to avoid buffer underruns)
  //----------------------------------------------------------------
}

void sound_chip_reset()
{
  //  ----------------------------------------
  //  The AY sound chip for the Amstrad CPC 
  //  ----------------------------------------
  ay38910Reset(&myAY);             // Reset the "AY" sound chip
  ay38910IndexW(0x07, &myAY);      // Register 7 is ENABLE
  ay38910DataW(0x3F, &myAY);       // All OFF (negative logic)
  ay38910Mixer(4, mixbufAY, &myAY);// Do an initial mix conversion to clear the output
  
  floppy_sound = 0;
  floppy_action = 0;

  memset(mixbufAY, 0x00, sizeof(mixbufAY));
}

// -----------------------------------------------------------------------
// We setup the sound chips - disabling all volumes to start.
// -----------------------------------------------------------------------
void dsInstallSoundEmuFIFO(void)
{
  SoundPause();             // Pause any sound output
  sound_chip_reset();       // Reset the SN, AY and SCC chips
  setupStream();            // Setup maxmod stream...
  bStartSoundEngine = 5;    // Volume will 'unpause' after 5 frames in the main loop.
}

//*****************************************************************************
// Reset the Amstrad CPC - mostly CPU, Super Game Module and memory...
//*****************************************************************************

// --------------------------------------------------------------
// When we first load a ROM/CASSETTE or when the user presses
// the RESET button on the touch-screen...
// --------------------------------------------------------------
void ResetAmstrad(void)
{
  JoyState = 0x00000000;                // Nothing pressed to start

  sound_chip_reset();                   // Reset the AY chip
  ResetZ80(&CPU);                       // Reset the Z80 CPU core
  amstrad_reset();                      // Reset the Amstrad memory - will load .dsk or .sna 

  // -----------------------------------------------------------
  // Timer 1 is used to time frame-to-frame of actual emulation
  // -----------------------------------------------------------
  TIMER1_CR = 0;
  TIMER1_DATA=0;
  TIMER1_CR=TIMER_ENABLE  | TIMER_DIV_1024;

  // -----------------------------------------------------------
  // Timer 2 is used to time once per second events
  // -----------------------------------------------------------
  TIMER2_CR=0;
  TIMER2_DATA=0;
  TIMER2_CR=TIMER_ENABLE  | TIMER_DIV_1024;
  timingFrames  = 0;
  emuFps=0;

  bFirstTime = 1;
  bStartIn = 0;
  bottom_screen = 0;
  debugger_pause = 0;
}

//*********************************************************************************
// A mini Z80 debugger of sorts. Put out some Z80 and PORT information along
// with our ever-handy debug[] registers. This is enabled via global configuration.
//*********************************************************************************
extern u8 *fake_heap_end;     // current heap start
extern u8 *fake_heap_start;   // current heap end

u8* getHeapStart() {return fake_heap_start;}
u8* getHeapEnd()   {return (u8*)sbrk(0);}
u8* getHeapLimit() {return fake_heap_end;}

int getMemUsed() { // returns the amount of used memory in bytes
   struct mallinfo mi = mallinfo();
   return mi.uordblks;
}

int getMemFree() { // returns the amount of free memory in bytes
   struct mallinfo mi = mallinfo();
   return mi.fordblks + (getHeapLimit() - getHeapEnd());
}

void ShowDebugZ80(void)
{
    u8 idx=2;

    if (myGlobalConfig.debugger == 3)
    {
        sprintf(tmp, "PC %04X  SP %04X", CPU.PC.W, CPU.SP.W);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "AF %04X  AF'%04X", CPU.AF.W, CPU.AF1.W);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "BC %04X  BC'%04X", CPU.BC.W, CPU.BC1.W);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "DE %04X  DC'%04X", CPU.DE.W, CPU.DE1.W);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "HL %04X  HL'%04X", CPU.HL.W, CPU.HL1.W);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "IX %04X  IY %04X", CPU.IX.W, CPU.IY.W);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "I  %02X  %02X R %02X", CPU.I, CPU.IFF, (CPU.R & 0x7F) | CPU.R_HighBit);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "RMR %02X   MMR %02X", RMR, MMR);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "PORTS %02X %02X %02X", portA, portB, portC);
        DSPrint(0,idx++,7, tmp);

        idx++;

        sprintf(tmp, "AY %02X %02X %02X %02X", myAY.ayRegs[0], myAY.ayRegs[1], myAY.ayRegs[2], myAY.ayRegs[3]);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "AY %02X %02X %02X %02X", myAY.ayRegs[4], myAY.ayRegs[5], myAY.ayRegs[6], myAY.ayRegs[7]);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "AY %02X %02X %02X %02X", myAY.ayRegs[8], myAY.ayRegs[9], myAY.ayRegs[10], myAY.ayRegs[11]);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "AY %02X %02X %02X %02X", myAY.ayRegs[12], myAY.ayRegs[13], myAY.ayRegs[14], myAY.ayRegs[15]);
        DSPrint(0,idx++,7, tmp);

        idx++;

        sprintf(tmp, "CRT %02X %02X %02X %02X", CRTC[0],  CRTC[1],  CRTC[2],  CRTC[3]);  DSPrint(0,idx++, 7, tmp);
        sprintf(tmp, "CRT %02X %02X %02X %02X", CRTC[4],  CRTC[5],  CRTC[6],  CRTC[7]);  DSPrint(0,idx++, 7, tmp);
        sprintf(tmp, "CRT %02X %02X %02X %02X", CRTC[8],  CRTC[9],  CRTC[10], CRTC[11]); DSPrint(0,idx++, 7, tmp);
        sprintf(tmp, "CRT %02X %02X %02X %02X", CRTC[12], CRTC[13], CRTC[14], CRTC[15]); DSPrint(0,idx++, 7, tmp);

        // CPU Disassembly!

        // Put out the debug registers...
        idx = 2;
        for (u8 i=0; i<16; i++)
        {
            sprintf(tmp, "D%X %-7lu %04X", i, debug[i], (u16)debug[i]); DSPrint(17,idx++, 7, tmp);
        }
        sprintf(tmp, "DX %-7lu", DX); DSPrint(17,idx++, 7, tmp);
        sprintf(tmp, "DY %-7lu", DY); DSPrint(17,idx++, 7, tmp);
    }
    else
    {
        idx = 1;
        for (u8 i=0; i<4; i++)
        {
            sprintf(tmp, "D%d %-7ld %04lX  D%d %-7ld %04lX", i, (s32)debug[i], (debug[i] < 0xFFFF ? debug[i]:0xFFFF), 4+i, (s32)debug[4+i], (debug[4+i] < 0xFFFF ? debug[4+i]:0xFFFF));
            DSPrint(0,idx++,0, tmp);
        }
    }
    idx++;
}


// ------------------------------------------------------------
// The status line shows the status of the Amstrad System
// on the top line of the bottom DS display.
// ------------------------------------------------------------
void DisplayStatusLine(bool bForce)
{
    if (floppy_sound)
    {
        // If we are showing any sort of keyboard... update the floppy label
        if (myGlobalConfig.debugger != 3)
        {
            if (floppy_sound == 2) 
            {
                mmEffect(SFX_FLOPPY3);  // Play short floppy sound for feedback
            }

            if (--floppy_sound == 0)
            {
                DSPrint(25, 21, 2, (char*)"HI");   // White Idle Label
            }
            else
            {
                if (floppy_action == 1)
                {
                    DSPrint(25, 21, 2, (char*)"*+");   // Blue Write Label
                }
                else
                {
                    DSPrint(25, 21, 2, (char*)"()");   // Green Read Label
                }
            }
        }
    }
    
    if (last_special_key)
    {
        if (last_special_key == KBD_KEY_SFT)
        {
            DSPrint(1, 19, 6, (char*)"@");
        }
        if (last_special_key == KBD_KEY_CTL)
        {
            DSPrint(2, 23, 2, (char*)"@");
        }
    }
    else
    {
        DSPrint(1, 19, 6, (char*)" ");
        DSPrint(2, 23, 6, (char*)" ");
    }
}

// ---------------------------------
// Swap new disk into the drive...
// ---------------------------------
void DiskInsert(char *filename, u8 bForceRead)
{
    if (strstr(filename, ".dsk") != 0) amstrad_mode = MODE_DSK;
    if (strstr(filename, ".DSK") != 0) amstrad_mode = MODE_DSK;
    
    if (bForceRead)
    {
        last_file_size = ReadFileCarefully(filename, ROM_Memory, MAX_ROM_SIZE, 0);
    }
    
    if (last_file_size)
    {        
        strcpy(last_file, filename);
        getcwd(last_path, MAX_FILENAME_LEN);
        
        ReadDiskMem(ROM_Memory, last_file_size);
    }
}


// ----------------------------------------------------------------------
// The Cassette Menu can be called up directly from the keyboard graphic
// and allows the user to rewind the tape, swap in a new tape, etc.
// ----------------------------------------------------------------------
#define MENU_ACTION_END             255 // Always the last sentinal value
#define MENU_ACTION_EXIT            0   // Exit the menu
#define MENU_ACTION_PLAY            1   // Play Cassette
#define MENU_ACTION_STOP            2   // Stop Cassette
#define MENU_ACTION_SWAP            3   // Swap Cassette
#define MENU_ACTION_REWIND          4   // Rewind Cassette
#define MENU_ACTION_POSITION        5   // Position Cassette

#define MENU_ACTION_RESET           98  // Reset the machine
#define MENU_ACTION_SKIP            99  // Skip this MENU choice

typedef struct
{
    char *menu_string;
    u8    menu_action;
} MenuItem_t;

typedef struct
{
    char *title;
    u8   start_row;
    MenuItem_t menulist[15];
} CassetteDiskMenu_t;

CassetteDiskMenu_t generic_cassette_menu =
{
    "CASSETTE MENU", 3,
    {
        {" PLAY     CASSETTE  ",      MENU_ACTION_PLAY},
        {" STOP     CASSETTE  ",      MENU_ACTION_STOP},
        {" SWAP     CASSETTE  ",      MENU_ACTION_SWAP},
        {" REWIND   CASSETTE  ",      MENU_ACTION_REWIND},
        {" POSITION CASSETTE  ",      MENU_ACTION_POSITION},
        {" EXIT     MENU      ",      MENU_ACTION_EXIT},
        {" NULL               ",      MENU_ACTION_END},
    },
};


CassetteDiskMenu_t *menu = &generic_cassette_menu;

// ------------------------------------------------------------------------
// Show the Cassette/Disk Menu text - highlight the selected row.
// ------------------------------------------------------------------------
u8 cassette_menu_items = 0;
void CassetteMenuShow(bool bClearScreen, u8 sel)
{
}

// ------------------------------------------------------------------------
// Handle Cassette mini-menu interface... Allows rewind, swap tape, etc.
// ------------------------------------------------------------------------
void CassetteMenu(void)
{
  WAITVBL;WAITVBL;
  BottomScreenKeyboard();  // Could be generic or overlay...
  SoundUnPause();
}


// ------------------------------------------------------------------------
// Show the Mini Menu - highlight the selected row. This can be called
// up directly from the CPC Keyboard Graphic - allows the user to quit 
// the current game, set high scores, save/load game state, etc.
// ------------------------------------------------------------------------
u8 mini_menu_items = 0;
void MiniMenuShow(bool bClearScreen, u8 sel)
{
    mini_menu_items = 0;
    if (bClearScreen)
    {
      // ---------------------------------------------------
      // Put up a generic background for this mini-menu...
      // ---------------------------------------------------
      BottomScreenOptions();
    }

    DSPrint(8,7,6,                                           " DS MINI MENU  ");
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " RESET  GAME   ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " QUIT   GAME   ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " SAVE   STATE  ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " LOAD   STATE  ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " CONFIG GAME   ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " DEFINE KEYS   ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " EXIT   MENU   ");  mini_menu_items++;
}

// ------------------------------------------------------------------------
// Handle mini-menu interface...
// ------------------------------------------------------------------------
u8 MiniMenu(void)
{
  u8 retVal = MENU_CHOICE_NONE;
  u8 menuSelection = 0;

  SoundPause();
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  MiniMenuShow(true, menuSelection);

  while (true)
  {
    nds_key = keysCurrent();
    if (nds_key)
    {
        if (nds_key & KEY_UP)
        {
            menuSelection = (menuSelection > 0) ? (menuSelection-1):(mini_menu_items-1);
            MiniMenuShow(false, menuSelection);
        }
        if (nds_key & KEY_DOWN)
        {
            menuSelection = (menuSelection+1) % mini_menu_items;
            MiniMenuShow(false, menuSelection);
        }
        if (nds_key & KEY_A)
        {
            if      (menuSelection == 0) retVal = MENU_CHOICE_RESET_GAME;
            else if (menuSelection == 1) retVal = MENU_CHOICE_END_GAME;
            else if (menuSelection == 2) retVal = MENU_CHOICE_SAVE_GAME;
            else if (menuSelection == 3) retVal = MENU_CHOICE_LOAD_GAME;
            else if (menuSelection == 4) retVal = MENU_CHOICE_CONFIG_GAME;
            else if (menuSelection == 5) retVal = MENU_CHOICE_DEFINE_KEYS;
            else if (menuSelection == 6) retVal = MENU_CHOICE_NONE;
            else retVal = MENU_CHOICE_NONE;
            break;
        }
        if (nds_key & KEY_B)
        {
            retVal = MENU_CHOICE_NONE;
            break;
        }

        while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);
        WAITVBL;WAITVBL;
    }
  }

  while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);
  WAITVBL;WAITVBL;

  if (retVal == MENU_CHOICE_NONE)
  {
    BottomScreenKeyboard();  // Could be generic or overlay...
  }

  SoundUnPause();

  return retVal;
}


// ---------------------------------------------------------------------------------
// Keyboard handler - mapping DS touch screen virtual keys to keyboard keys that we
// can feed into the key processing handler in amstrad.c when the IO port is read.
// ---------------------------------------------------------------------------------

u8 last_special_key = 0;
u8 last_special_key_dampen = 0;
u8 last_kbd_key = 0;

u8 handle_cpc_keyboard_press(u16 iTx, u16 iTy)  // Amstrad CPC keyboard
{
    if ((iTy >= 40) && (iTy < 72))   // Row 1 (number row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   kbd_key = KBD_KEY_ESC;
        else if ((iTx >= 23)  && (iTx < 43))   kbd_key = '1';
        else if ((iTx >= 43)  && (iTx < 62))   kbd_key = '2';
        else if ((iTx >= 62)  && (iTx < 82))   kbd_key = '3';
        else if ((iTx >= 82)  && (iTx < 100))  kbd_key = '4';
        else if ((iTx >= 100) && (iTx < 119))  kbd_key = '5';
        else if ((iTx >= 119) && (iTx < 138))  kbd_key = '6';
        else if ((iTx >= 138) && (iTx < 157))  kbd_key = '7';
        else if ((iTx >= 157) && (iTx < 176))  kbd_key = '8';
        else if ((iTx >= 176) && (iTx < 195))  kbd_key = '9';
        else if ((iTx >= 195) && (iTx < 214))  kbd_key = '0';
        else if ((iTx >= 214) && (iTx < 233))  kbd_key = '-';
        else if ((iTx >= 233) && (iTx < 255))  kbd_key = KBD_KEY_CLR;
    }
    else if ((iTy >= 72) && (iTy < 102))  // Row 2 (QWERTY row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   kbd_key = KBD_KEY_TAB;
        else if ((iTx >= 23)  && (iTx < 43))   kbd_key = 'Q';
        else if ((iTx >= 43)  && (iTx < 62))   kbd_key = 'W';
        else if ((iTx >= 62)  && (iTx < 82))   kbd_key = 'E';
        else if ((iTx >= 82)  && (iTx < 100))  kbd_key = 'R';
        else if ((iTx >= 100) && (iTx < 119))  kbd_key = 'T';
        else if ((iTx >= 119) && (iTx < 138))  kbd_key = 'Y';
        else if ((iTx >= 138) && (iTx < 157))  kbd_key = 'U';
        else if ((iTx >= 157) && (iTx < 176))  kbd_key = 'I';
        else if ((iTx >= 176) && (iTx < 195))  kbd_key = 'O';
        else if ((iTx >= 195) && (iTx < 214))  kbd_key = 'P';
        else if ((iTx >= 214) && (iTx < 233))  kbd_key = '@';
        else if ((iTx >= 233) && (iTx < 255))  kbd_key = '[';
    }
    else if ((iTy >= 102) && (iTy < 132)) // Row 3 (ASDF row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   kbd_key = KBD_KEY_CAPS;
        else if ((iTx >= 23)  && (iTx < 43))   kbd_key = 'A';
        else if ((iTx >= 43)  && (iTx < 62))   kbd_key = 'S';
        else if ((iTx >= 62)  && (iTx < 82))   kbd_key = 'D';
        else if ((iTx >= 82)  && (iTx < 100))  kbd_key = 'F';
        else if ((iTx >= 100) && (iTx < 119))  kbd_key = 'G';
        else if ((iTx >= 119) && (iTx < 138))  kbd_key = 'H';
        else if ((iTx >= 138) && (iTx < 157))  kbd_key = 'J';
        else if ((iTx >= 157) && (iTx < 176))  kbd_key = 'K';
        else if ((iTx >= 176) && (iTx < 195))  kbd_key = 'L';
        else if ((iTx >= 195) && (iTx < 214))  kbd_key = ':';
        else if ((iTx >= 214) && (iTx < 233))  kbd_key = ';';
        else if ((iTx >= 233) && (iTx < 255))  kbd_key = ']';
    }
    else if ((iTy >= 132) && (iTy < 162)) // Row 4 (ZXCV row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   {kbd_key = KBD_KEY_SFT; last_special_key = KBD_KEY_SFT;}
        else if ((iTx >= 23)  && (iTx < 43))   kbd_key = 'Z';
        else if ((iTx >= 43)  && (iTx < 62))   kbd_key = 'X';
        else if ((iTx >= 62)  && (iTx < 82))   kbd_key = 'C';
        else if ((iTx >= 82)  && (iTx < 100))  kbd_key = 'V';
        else if ((iTx >= 100) && (iTx < 119))  kbd_key = 'B';
        else if ((iTx >= 119) && (iTx < 138))  kbd_key = 'N';
        else if ((iTx >= 138) && (iTx < 157))  kbd_key = 'M';
        else if ((iTx >= 157) && (iTx < 176))  kbd_key = ',';
        else if ((iTx >= 176) && (iTx < 195))  kbd_key = '.';
        else if ((iTx >= 195) && (iTx < 214))  kbd_key = '/';
        else if ((iTx >= 214) && (iTx < 233))  kbd_key = KBD_KEY_RET;
        else if ((iTx >= 233) && (iTx < 255))  kbd_key = KBD_KEY_RET;
    }
    else if ((iTy >= 162) && (iTy < 192)) // Row 5 (SPACE BAR and icons row)
    {
        if      ((iTx >= 1)   && (iTx < 43))   {kbd_key = KBD_KEY_CTL; last_special_key = KBD_KEY_CTL;}
        else if ((iTx >= 43)  && (iTx < 194))  kbd_key = ' ';
        else if ((iTx >= 194) && (iTx < 255))  return MENU_CHOICE_MENU;
    }

    DisplayStatusLine(false);

    return MENU_CHOICE_NONE;
}

// -----------------------------------------------------------------------------------
// Special version of the debugger overlay... handling just a small subset of keys...
// -----------------------------------------------------------------------------------
u8 handle_debugger_overlay(u16 iTx, u16 iTy)
{
    if ((iTy >= 165) && (iTy < 192)) // Bottom row is where the debugger keys are...
    {
        if      ((iTx >= 0)   && (iTx <  22))  kbd_key = 'Z';
        if      ((iTx >= 22)  && (iTx <  41))  kbd_key = 'X';
        if      ((iTx >= 41)  && (iTx <  60))  kbd_key = 'C';
        if      ((iTx >= 60)  && (iTx <  80))  kbd_key = 'V';
        if      ((iTx >= 80)  && (iTx <  98))  kbd_key = 'B';
        if      ((iTx >= 98)  && (iTx < 117))  kbd_key = 'N';
        if      ((iTx >= 117) && (iTx < 137))  kbd_key = '1';
        if      ((iTx >= 137) && (iTx < 168))  kbd_key = KBD_KEY_RET;
        if      ((iTx >= 168) && (iTx < 205))  return MENU_CHOICE_MENU;
        if      ((iTx >= 205) && (iTx < 230))  {debugger_pause = 2; WAITVBL;}
        if      ((iTx >= 230) && (iTx < 255))  debugger_pause = 0;

        DisplayStatusLine(false);
    }
    else {kbd_key = 0; last_kbd_key = 0;}

    return MENU_CHOICE_NONE;
}

u8 __attribute__((noinline)) handle_meta_key(u8 meta_key)
{
    switch (meta_key)
    {
        case MENU_CHOICE_RESET_GAME:
            SoundPause();
            // Ask for verification
            if (showMessage("DO YOU REALLY WANT TO", "RESET THE CURRENT GAME ?") == ID_SHM_YES)
            {
                ResetAmstrad();
            }
            BottomScreenKeyboard();
            SoundUnPause();
            break;

        case MENU_CHOICE_END_GAME:
              SoundPause();
              //  Ask for verification
              if  (showMessage("DO YOU REALLY WANT TO","QUIT THE CURRENT GAME ?") == ID_SHM_YES)
              {
                  memset((u8*)0x06000000, 0x00, 0x20000);    // Reset VRAM to 0x00 to clear any potential display garbage on way out
                  return 1;
              }
              BottomScreenKeyboard();
              DisplayStatusLine(true);
              SoundUnPause();
            break;

        case MENU_CHOICE_SAVE_GAME:
            SoundPause();
            if  (showMessage("DO YOU REALLY WANT TO","SAVE GAME STATE ?") == ID_SHM_YES)
            {
              amstradSaveState();
            }
            BottomScreenKeyboard();
            SoundUnPause();
            break;

        case MENU_CHOICE_LOAD_GAME:
            SoundPause();
            if (showMessage("DO YOU REALLY WANT TO","LOAD GAME STATE ?") == ID_SHM_YES)
            {
              amstradLoadState();
            }
            BottomScreenKeyboard();
            SoundUnPause();
            break;

        case MENU_CHOICE_CONFIG_GAME:
            SoundPause();
            SpeccySEGameOptions(false);
            BottomScreenKeyboard();
            SoundUnPause();
            break;
            
        case MENU_CHOICE_DEFINE_KEYS:
            SoundPause();
            SugarDSChangeKeymap();
            BottomScreenKeyboard();
            SoundUnPause();
            break;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// Slide-n-Glide D-pad keeps moving in the last known direction for a few more
// frames to help make those hairpin turns up and off ladders much easier...
// ----------------------------------------------------------------------------
u8 slide_n_glide_key_up = 0;
u8 slide_n_glide_key_down = 0;
u8 slide_n_glide_key_left = 0;
u8 slide_n_glide_key_right = 0;

static u8 dampen = 0;

// ------------------------------------------------------------------------
// The main emulation loop is here... call into the Z80 and render frame
// ------------------------------------------------------------------------
void SpeccySE_main(void)
{
  u16 iTx,  iTy;
  u32 ucDEUX;
  static u8 dampenClick = 0;
  u8 meta_key = 0;

  // Setup the debug buffer for DSi use
  debug_init();

  // Get the CPC Emulator ready
  AmstradInit(gpFic[ucGameAct].szName);

  amstrad_set_palette();
  amstrad_emulate();

  // Frame-to-frame timing...
  TIMER1_CR = 0;
  TIMER1_DATA=0;
  TIMER1_CR=TIMER_ENABLE  | TIMER_DIV_1024;

  // Once/second timing...
  TIMER2_CR=0;
  TIMER2_DATA=0;
  TIMER2_CR=TIMER_ENABLE  | TIMER_DIV_1024;
  timingFrames  = 0;
  emuFps=0;

  newStreamSampleRate();
  
  // Force the sound engine to turn on when we start emulation
  bStartSoundEngine = 10;

  bFirstTime = 1;

  // -----------------------------------------------------------------------
  // Stay in this loop running the Amstrad CPC game until the user exits...
  // -----------------------------------------------------------------------
  while(1)
  {
    u8 frame_complete = 0;
    
    // Take a tour of the Z80 and display the screen if necessary
    if (debugger_pause == 1)
    {
        frame_complete = 1;
    }
    else
    {
        frame_complete = !amstrad_run();
    }
    
    // -----------------------------------------------------
    // If we have completed 1 frame (vertical sync seen)...
    // This handles our keyboard polling, joystick polling
    // and handles the frame-to-frame timing of the system.
    // -----------------------------------------------------
    if (frame_complete)
    {
        if (debugger_pause == 2) debugger_pause = 1;
        
        // If we've been asked to start the sound engine, rock-and-roll!
        if (bStartSoundEngine)
        {
              if (--bStartSoundEngine == 0) SoundUnPause();
        }

        // -----------------------------------------------------
        // If asked to Auto-Size, we check the CRTC registers
        // and guestimate the proper screen horizontal size...
        // We don't touch the vertical offset/scale.
        // -----------------------------------------------------
        if (myConfig.autoSize)
        {
                 if (CRTC[1] <= 34)  myConfig.scaleX = 320;
            else if (CRTC[1] <= 37)  myConfig.scaleX = 288;
            else  myConfig.scaleX = 256;
        }
        
        // -------------------------------------------------------------
        // Stuff to do once/second such as FPS display and Debug Data
        // -------------------------------------------------------------
        if (TIMER1_DATA >= 32728)   //  1000MS (1 sec)
        {
            char szChai[4];

            TIMER1_CR = 0;
            TIMER1_DATA = 0;
            TIMER1_CR=TIMER_ENABLE | TIMER_DIV_1024;
            emuFps = emuActFrames;
            if (myGlobalConfig.showFPS)
            {
                // Due to minor sampling of the frame rate, 49,50 and 51
                // pretty much all represent full-speed so just show 50fps.
                if (emuFps == 51) emuFps=50;
                else if (emuFps == 49) emuFps=50;
                if (emuFps/100) szChai[0] = '0' + emuFps/100;
                else szChai[0] = ' ';
                szChai[1] = '0' + (emuFps%100) / 10;
                szChai[2] = '0' + (emuFps%100) % 10;
                szChai[3] = 0;
                DSPrint(28,0,6,szChai);
            }
            DisplayStatusLine(false);
            emuActFrames = 0;

            if (bFirstTime)
            {
                if (--bFirstTime == 0)
                {
                    // Tape Loader - Put the LOAD "" into the keyboard buffer
                    if (amstrad_mode == MODE_DSK)
                    {
                        if (myConfig.autoLoad)
                        {
                            char cmd[32];
                            if (AMSDOS_GenerateAutorunCommand(cmd) == AUTORUN_OK)
                            {
                                BufferKeys(cmd);
                                BufferKey(KBD_KEY_RET);
                            }
                            else
                            {
                                BufferKey('C');
                                BufferKey('A');
                                BufferKey('T');
                                BufferKey(KBD_KEY_RET);
                            }
                        }
                    }
                }
            }
        }
        emuActFrames++;

        // -------------------------------------------------------------------
        // We only support PAL 50 frames as this is a ZED-X Speccy!
        // -------------------------------------------------------------------
        if (++timingFrames == 50)
        {
            TIMER2_CR=0;
            TIMER2_DATA=0;
            TIMER2_CR=TIMER_ENABLE | TIMER_DIV_1024;
            timingFrames = 0;
        }

        // ----------------------------------------------------------------------
        // 32,728.5 ticks of TIMER2 = 1 second
        // 1 frame = 1/50 or 655 ticks of TIMER2
        //
        // This is how we time frame-to frame to keep the game running at 50FPS
        // ----------------------------------------------------------------------
        while (TIMER2_DATA < GAME_SPEED_PAL[myConfig.gameSpeed]*(timingFrames+1))
        {
            if (myGlobalConfig.showFPS == 2) break;   // If Full Speed, break out...
        }

      // If the Z80 Debugger is enabled, call it
      if (myGlobalConfig.debugger >= 2)
      {
          ShowDebugZ80();
      }

      // --------------------------------------------------------
      // Hold the key press for a brief instant... To allow the
      // emulated CPC to 'see' the key briefly... Good enough.
      // --------------------------------------------------------
      if (key_debounce > 0) key_debounce--;
      else
      {
          // -----------------------------------------------------------
          // This is where we accumualte the keys pressed... up to 12!
          // -----------------------------------------------------------
          kbd_keys_pressed = 0;
          memset(kbd_keys, 0x00, sizeof(kbd_keys));
          kbd_key = 0;

          // ------------------------------------------
          // Handle any screen touch events
          // ------------------------------------------
          if  (keysCurrent() & KEY_TOUCH)
          {
              // ------------------------------------------------------------------------------------------------
              // Just a tiny bit of touch debounce so ensure touch screen is pressed for a fraction of a second.
              // ------------------------------------------------------------------------------------------------
              if (++touch_debounce > 1)
              {
                touchPosition touch;
                touchRead(&touch);
                iTx = touch.px;
                iTy = touch.py;

                if (myGlobalConfig.debugger == 3)
                {
                    meta_key = handle_debugger_overlay(iTx, iTy);
                }
                // ------------------------------------------------------------
                // Test the touchscreen for various full keyboard handlers...
                // ------------------------------------------------------------
                else
                {
                    meta_key = handle_cpc_keyboard_press(iTx, iTy);
                }

                // If the special menu key indicates we should show the choice menu, do so here...
                if (meta_key == MENU_CHOICE_MENU)
                {
                    meta_key = MiniMenu();
                }

                // -------------------------------------------------------------------
                // If one of the special meta keys was picked, we handle that here...
                // -------------------------------------------------------------------
                if (handle_meta_key(meta_key)) return;

                if (++dampenClick > 0)  // Make sure the key is pressed for an appreciable amount of time...
                {
                    if (kbd_key != 0)
                    {
                        kbd_keys[kbd_keys_pressed++] = kbd_key;
                        key_debounce = 5;
                        if (last_kbd_key == 0)
                        {
                             mmEffect(SFX_KEYCLICK);  // Play short key click for feedback...
                        }
                        last_kbd_key = kbd_key;
                        if ((kbd_key != KBD_KEY_SFT) && (kbd_key != KBD_KEY_CTL))
                        {
                            dampen_special_key = 10;
                        }
                    }
                }
              }
          } //  SCR_TOUCH
          else
          {
            touch_debounce = 0;
            dampenClick = 0;
            last_kbd_key = 0;
            if (dampen_special_key)
            {
                if (--dampen_special_key == 0)
                {
                    last_special_key = 0;
                }
            }
            
          }
      }

      // -------------------------------------------------------------------
      //  Test DS keypresses (ABXY, L/R) and map to corresponding CPC keys
      // -------------------------------------------------------------------
      ucDEUX  = 0;
      nds_key  = keysCurrent();     // Get any current keys pressed on the NDS

      // -----------------------------------------
      // Check various key combinations first...
      // -----------------------------------------
      if ((nds_key & KEY_L) && (nds_key & KEY_R) && (nds_key & KEY_X))
      {
            lcdSwap();
            WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
      }
      else if (nds_key & (KEY_L | KEY_R))
      {
        if((nds_key & KEY_R) && !dampen)
        {
            if (nds_key & KEY_UP)
            {
                dampen = 2;
                myConfig.offsetY++;
            }
            if (nds_key & KEY_DOWN)
            {
                dampen = 2;
                if (myConfig.offsetY > -32) myConfig.offsetY--;
            }
            if (nds_key & KEY_LEFT)
            {
                dampen = 2;
                if (myConfig.offsetX < 96) myConfig.offsetX++;
            }
            if (nds_key & KEY_RIGHT)
            {
                dampen = 2;
                if (myConfig.offsetX > -32) myConfig.offsetX--;
            }
        }
        else
        if((nds_key & KEY_L) && !dampen)
        {
            if (nds_key & KEY_UP)
            {
                dampen = 2;
                if (myConfig.scaleY < 200) myConfig.scaleY++;
            }
            if (nds_key & KEY_DOWN)
            {
                dampen = 2;
                if (myConfig.scaleY > 140) myConfig.scaleY--;
            }
            if (nds_key & KEY_LEFT)
            {
                dampen = 2;
                if (myConfig.scaleX > 200) myConfig.scaleX--;
            }
            if (nds_key & KEY_RIGHT)
            {
                dampen = 2;
                if (myConfig.scaleX < 320) myConfig.scaleX++;
            }
        }                  
      }
      else if  (nds_key & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_START | KEY_SELECT | KEY_X | KEY_Y))
      {
          if (myConfig.dpad == DPAD_DIAGONALS) // Diagonals... map standard Left/Right/Up/Down to combinations
          {
              // TODO: add diagonal dpad support... not sure how often this is needed
          }

          if (myConfig.dpad == DPAD_SLIDE_N_GLIDE) // CHUCKIE-EGG Style... hold left/right or up/down for a few frames
          {
                if (nds_key & KEY_UP)
                {
                    slide_n_glide_key_up    = 12;
                    slide_n_glide_key_down  = 0;
                }
                if (nds_key & KEY_DOWN)
                {
                    slide_n_glide_key_down  = 12;
                    slide_n_glide_key_up    = 0;
                }
                if (nds_key & KEY_LEFT)
                {
                    slide_n_glide_key_left  = 12;
                    slide_n_glide_key_right = 0;
                }
                if (nds_key & KEY_RIGHT)
                {
                    slide_n_glide_key_right = 12;
                    slide_n_glide_key_left  = 0;
                }

                if (slide_n_glide_key_up)
                {
                    slide_n_glide_key_up--;
                    nds_key |= KEY_UP;
                }

                if (slide_n_glide_key_down)
                {
                    slide_n_glide_key_down--;
                    nds_key |= KEY_DOWN;
                }

                if (slide_n_glide_key_left)
                {
                    slide_n_glide_key_left--;
                    nds_key |= KEY_LEFT;
                }

                if (slide_n_glide_key_right)
                {
                    slide_n_glide_key_right--;
                    nds_key |= KEY_RIGHT;
                }
          }

          // --------------------------------------------------------------------------------------------------
          // There are 10 NDS buttons (D-Pad, XYAB, and Start+Select) - we allow mapping of any of these.
          // --------------------------------------------------------------------------------------------------
          for (u8 i=0; i<10; i++)
          {
              if (nds_key & NDS_keyMap[i])
              {
                  if (keyCoresp[myConfig.keymap[i]] < 0xF000)   // Normal key map
                  {
                      ucDEUX  |= keyCoresp[myConfig.keymap[i]];
                  }
                  else // This is a keyboard maping... handle that here... just set the appopriate kbd_key
                  {
                      if      ((keyCoresp[myConfig.keymap[i]] >= META_KBD_A) && (keyCoresp[myConfig.keymap[i]] <= META_KBD_Z))  kbd_key = ('A' + (keyCoresp[myConfig.keymap[i]] - META_KBD_A));
                      else if ((keyCoresp[myConfig.keymap[i]] >= META_KBD_0) && (keyCoresp[myConfig.keymap[i]] <= META_KBD_9))  kbd_key = ('0' + (keyCoresp[myConfig.keymap[i]] - META_KBD_0));
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_SPACE)     kbd_key  = ' ';
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_PERIOD)    kbd_key  = '.';
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_COMMA)     kbd_key  = ',';
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_COLON)     kbd_key  = ':';
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_SEMI)      kbd_key  = ';';
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_ATSIGN)    kbd_key  = '@';
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_SHIFT)     kbd_key  = KBD_KEY_SFT;
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_RETURN)    kbd_key  = KBD_KEY_RET;
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_PAN_UP16)  {temp_offset = -16;slide_dampen = 15;}
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_PAN_UP24)  {temp_offset = -24;slide_dampen = 15;}
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_PAN_UP32)  {temp_offset = -32;slide_dampen = 15;}
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_PAN_DN16)  {temp_offset =  16;slide_dampen = 15;}
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_PAN_DN24)  {temp_offset =  24;slide_dampen = 15;}
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_PAN_DN32)  {temp_offset =  32;slide_dampen = 15;}
                      
                      if (kbd_key != 0)
                      {
                          kbd_keys[kbd_keys_pressed++] = kbd_key;
                      }
                  }
              }
          }
      }
      else // No NDS keys pressed...
      {
          if (slide_n_glide_key_up)    slide_n_glide_key_up--;
          if (slide_n_glide_key_down)  slide_n_glide_key_down--;
          if (slide_n_glide_key_left)  slide_n_glide_key_left--;
          if (slide_n_glide_key_right) slide_n_glide_key_right--;
          last_mapped_key = 0;
      }

      if (dampen) dampen--;
      
      // ------------------------------------------------------------------------------------------
      // Finally, check if there are any buffered keys that need to go into the keyboard handling.
      // ------------------------------------------------------------------------------------------
      ProcessBufferedKeys();

      // ---------------------------------------------------------
      // Accumulate all bits above into the Joystick State var...
      // ---------------------------------------------------------
      JoyState = ucDEUX;

      // --------------------------------------------------
      // Handle Auto-Fire if enabled in configuration...
      // --------------------------------------------------
      static u8 autoFireTimer=0;
      if (myConfig.autoFire && (JoyState & JST_FIRE))  // Fire Button
      {
         if ((++autoFireTimer & 7) > 4)  JoyState &= ~JST_FIRE;
      }
    }
  }
}


// ----------------------------------------------------------------------------------------
// We steal 256K of the VRAM to hold a shadow copy of the ROM cart for fast swap...
// ----------------------------------------------------------------------------------------
void useVRAM(void)
{
  vramSetBankB(VRAM_B_LCD);        // 128K VRAM used for snapshot DCAP buffer - but could be repurposed during emulation ...
  vramSetBankD(VRAM_D_LCD);        // Not using this for video but 128K of faster RAM always useful!  Mapped at 0x06860000 -   256K Used for tape patch look-up
  vramSetBankE(VRAM_E_LCD);        // Not using this for video but 64K of faster RAM always useful!   Mapped at 0x06880000 -   ..
  vramSetBankF(VRAM_F_LCD);        // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x06890000 -   ..
  vramSetBankG(VRAM_G_LCD);        // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x06894000 -   ..
  vramSetBankH(VRAM_H_LCD);        // Not using this for video but 32K of faster RAM always useful!   Mapped at 0x06898000 -   ..
  vramSetBankI(VRAM_I_LCD);        // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x068A0000 -   Unused - reserved for future use
}

/*********************************************************************************
 * Init DS Emulator - setup VRAM banks and background screen rendering banks
 ********************************************************************************/
void speccySEInit(void)
{
  //  Init graphic mode (bitmap mode)
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE);
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE  | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankC(VRAM_C_SUB_BG);

  //  Stop blending effect of intro
  REG_BLDCNT=0; REG_BLDCNT_SUB=0; REG_BLDY=0; REG_BLDY_SUB=0;

  //  Render the top screen
  bg0 = bgInit(0, BgType_Text8bpp,  BgSize_T_256x512, 31,0);
  bg1 = bgInit(1, BgType_Text8bpp,  BgSize_T_256x512, 29,0);
  bgSetPriority(bg0,1);bgSetPriority(bg1,0);
  decompress(topscreenTiles,  bgGetGfxPtr(bg0), LZ77Vram);
  decompress(topscreenMap,  (void*) bgGetMapPtr(bg0), LZ77Vram);
  dmaCopy((void*) topscreenPal,(void*)  BG_PALETTE,256*2);
  unsigned  short dmaVal =*(bgGetMapPtr(bg0)+51*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1),32*24*2);

  // Put up the options screen
  BottomScreenOptions();

  //  Find the files
  sugarDSFindFiles(0);
}

void BottomScreenOptions(void)
{
    swiWaitForVBlank();

    if (bottom_screen != 1)
    {
        // ---------------------------------------------------
        // Put up the options select screen background...
        // ---------------------------------------------------
        bg0b = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);
        bg1b = bgInitSub(1, BgType_Text8bpp, BgSize_T_256x256, 29,0);
        bgSetPriority(bg0b,1);bgSetPriority(bg1b,0);

        decompress(mainmenuTiles, bgGetGfxPtr(bg0b), LZ77Vram);
        decompress(mainmenuMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
        dmaCopy((void*) mainmenuPal,(void*) BG_PALETTE_SUB,256*2);

        unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
        dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
    }
    else // Just clear the screen
    {
        for (u8 i=0; i<24; i++)  DSPrint(0,i,0,"                                ");
    }

    bottom_screen = 1;
}

// ---------------------------------------------------------------------------
// Setup the bottom screen - mostly for menu, high scores, options, etc.
// ---------------------------------------------------------------------------
void BottomScreenKeyboard(void)
{
    swiWaitForVBlank();

    if (myGlobalConfig.debugger == 3)  // Full Z80 Debug overrides things... put up the debugger overlay
    {
      //  Init bottom screen
      decompress(debug_ovlTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(debug_ovlMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) debug_ovlPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else
    {
      //  Init bottom screen for CPC Keyboard
      decompress(cpc_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(cpc_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) cpc_kbdPal,(void*) BG_PALETTE_SUB,256*2);
    }

    unsigned  short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1b),32*24*2);

    bottom_screen = 2;

    DisplayStatusLine(true);
}



/*********************************************************************************
 * Init CPU for the current game
 ********************************************************************************/
void speccySEInitCPU(void)
{
    //  -----------------------------------------
    //  Init Main Memory for the Amstrad CPC
    //  -----------------------------------------
    memset(RAM_Memory,    0x00, sizeof(RAM_Memory));

    // -----------------------------------------------
    // Init bottom screen do display the CPC Keyboard
    // -----------------------------------------------
    BottomScreenKeyboard();
}

ITCM_CODE void irqVBlank(void)
{
    // Manage time
    vusCptVBL++;
    
    int cxBG = ((s16)myConfig.offsetX << 8);
    int cyBG = ((s16)myConfig.offsetY+temp_offset) << 8;
    int xdxBG = ((320 / myConfig.scaleX) << 8) | (320 % myConfig.scaleX) ;
    int ydyBG = ((200 / myConfig.scaleY) << 8) | (200 % myConfig.scaleY);

    REG_BG2X = cxBG;
    REG_BG2Y = cyBG;
    REG_BG3X = cxBG+JITTER[myConfig.jitter];
    REG_BG3Y = cyBG;

    REG_BG2PA = xdxBG;
    REG_BG2PD = ydyBG;
    REG_BG3PA = xdxBG;
    REG_BG3PD = ydyBG;

    if (temp_offset)
    {
        if (slide_dampen == 0)
        {
            if (temp_offset > 0) temp_offset--;
            else temp_offset++;
        }
        else
        {
            slide_dampen--;
        }
    }
}

// ----------------------------------------------------------------------
// Look for the 48.rom and 128.rom bios in several possible locations...
// ----------------------------------------------------------------------
void LoadBIOSFiles(void)
{
    // We are using internal BIOS roms for now...
}

/************************************************************************************
 * Program entry point - check if an argument has been passed in probably from TWL++
 ***********************************************************************************/
int main(int argc, char **argv)
{
  //  Init sound
  consoleDemoInit();

  if  (!fatInitDefault()) {
     iprintf("Unable to initialize libfat!\n");
     return -1;
  }

  lcdMainOnTop();

  //  Init timer for frame management
  TIMER2_DATA=0;
  TIMER2_CR=TIMER_ENABLE|TIMER_DIV_1024;
  dsInstallSoundEmuFIFO();

  //  Show the fade-away intro logo...
  intro_logo();

  SetYtrigger(190); //trigger 2 lines before vblank

  irqSet(IRQ_VBLANK,  irqVBlank);
  irqEnable(IRQ_VBLANK);

  // -----------------------------------------------------------------
  // Grab the BIOS before we try to switch any directories around...
  // -----------------------------------------------------------------
  useVRAM();
  LoadBIOSFiles();

  // -----------------------------------------------------------------
  // And do an initial load of configuration... We'll match it up
  // with the game that was selected later...
  // -----------------------------------------------------------------
  LoadConfig();

  //  Handle command line argument... mostly for TWL++
  if  (argc > 1)
  {
      //  We want to start in the directory where the file is being launched...
      if  (strchr(argv[1], '/') != NULL)
      {
          static char  path[128];
          strcpy(path,  argv[1]);
          char  *ptr = &path[strlen(path)-1];
          while (*ptr !=  '/') ptr--;
          ptr++;
          strcpy(cmd_line_file,  ptr);
          *ptr=0;
          chdir(path);
      }
      else
      {
          strcpy(cmd_line_file,  argv[1]);
      }
  }
  else
  {
      cmd_line_file[0]=0; // No file passed on command line...

      if ((myGlobalConfig.lastDir == 2) && (strlen(myGlobalConfig.szLastPath) > 2))
      {
          chdir(myGlobalConfig.szLastPath);  // Try to start back where we last were...
      }
      else
      {
          chdir("/roms");       // Try to start in roms area... doesn't matter if it fails
          chdir("cpc");         // And try to start in the subdir /cpc... doesn't matter if it fails.
          chdir("amstrad");     // And try to start in the subdir /amstrad... doesn't matter if it fails.
      }
  }

  SoundPause();

  srand(time(NULL));

  //  ------------------------------------------------------------
  //  We run this loop forever until game exit is selected...
  //  ------------------------------------------------------------
  while(1)
  {
    speccySEInit();
    
    while(1)
    {
      SoundPause();
      //  Choose option
      if  (cmd_line_file[0] != 0)
      {
          ucGameChoice=0;
          ucGameAct=0;
          strcpy(gpFic[ucGameAct].szName, cmd_line_file);
          cmd_line_file[0] = 0;    // No more initial file...
          ReadFileCRCAndConfig(); // Get CRC32 of the file and read the config/keys
      }
      else
      {
          speccySEInit();
          sugarDSChangeOptions();
      }

      //  Run Machine
      speccySEInitCPU();
      SpeccySE_main();
    }
  }
  return(0);
}


// -----------------------------------------------------------------------
// The code below is a handy set of debug tools that allows us to
// write printf() like strings out to a file. Basically we accumulate
// the strings into a large RAM buffer and then when the L+R shoulder
// buttons are pressed and held, we will snapshot out the debug.log file.
// The DS-Lite only gets a small 16K debug buffer but the DSi gets 4MB!
// -----------------------------------------------------------------------

#define MAX_DPRINTF_STR_SIZE  256
u32     MAX_DEBUG_BUF_SIZE  = 0;

char *debug_buffer = 0;
u32  debug_len = 0;
extern char szName[]; // Reuse buffer which has no other in-game use

void debug_init()
{
    if (!debug_buffer)
    {
        if (isDSiMode())
        {
            MAX_DEBUG_BUF_SIZE = (1024*1024*2); // 2MB!!
            debug_buffer = malloc(MAX_DEBUG_BUF_SIZE);
        }
        else
        {
            MAX_DEBUG_BUF_SIZE = (1024*16);     // 16K only
            debug_buffer = malloc(MAX_DEBUG_BUF_SIZE);
        }
    }
    memset(debug_buffer, 0x00, MAX_DEBUG_BUF_SIZE);
    debug_len = 0;
}

void debug_printf(const char * str, ...)
{
    va_list ap = {0};

    va_start(ap, str);
    vsnprintf(szName, MAX_DPRINTF_STR_SIZE, str, ap);
    va_end(ap);

    strcat(debug_buffer, szName);
    debug_len += strlen(szName);
}

void debug_save()
{
    if (debug_len > 0) // Only if we have debug data to write...
    {
        FILE *fp = fopen("debug.log", "w");
        if (fp)
        {
            fwrite(debug_buffer, 1, debug_len, fp);
            fclose(fp);
        }
    }
}

// End of file
