// =====================================================================================
// Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave and Marat
// Fayzullin (ColEM core) are thanked profusely.
//
// The SugarDS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================
#ifndef _SUGARDS_H_
#define _SUGARDS_H_

#include <nds.h>
#include <string.h>
#include "printf.h"

#define PING_A (0x06860000)     // For DSi this is LCD buffer A for double-buffer action
#define PING_B (0x06880000)     // For DSi this is LCD buffer B for double-buffer action

extern u32 debug[0x10];
extern u32 DX, DY;

extern u8 MMR;
extern u8 RMR;
extern u8 PENR;
extern u8 INK[17];
extern u8 CRTC[0x20];
extern u8 CRT_Idx;
extern u32 R52;
extern u32 border_color;
extern u32 last_frame_mode1;
extern u32 last_frame_mode2;
extern u8 last_frame_crtc1;
extern int8 currentBrightness;
extern u8 ram_highwater;
extern u8 sna_last_motor;
extern u8 sna_last_track;
extern u8 backgroundRender;
extern u8  mode2_offset;
extern u16 mode2_scale;
extern u8  mode1_offset;
extern u16 mode1_scale;

// These are the various special icons/menu operations
#define MENU_CHOICE_NONE        0x00        // No menu choice
#define MENU_CHOICE_RESET_GAME  0x01        // Reset current game (will prompt YES/NO)
#define MENU_CHOICE_END_GAME    0x02        // Quit current game (will prompt YES/NO)
#define MENU_CHOICE_SAVE_GAME   0x03        // Save Game state
#define MENU_CHOICE_LOAD_GAME   0x04        // Load Game state
#define MENU_CHOICE_DEFINE_KEYS 0x05        // Define Key sub-menu
#define MENU_CHOICE_CONFIG_GAME 0x06        // Config Game sub-menu
#define MENU_CHOICE_SWAP_DISK   0x07        // Swap Disk sub-menu
#define MENU_CHOICE_TOGGLE_KBD  0xFE        // Toggle Keyboard for Keypad
#define MENU_CHOICE_MENU        0xFF        // Special brings up a mini-menu of choices

// ------------------------------------------------------------------------------
// Joystick UP, RIGHT, LEFT, DOWN and FIRE 1+2+3 buttons for the Amstrad Joystick.
// Designed specifically so each has its own bit so we can press more than one
// direction/fire at the same time.  Keyboard keys are grafted onto this below.
// Most games only use Fire button #1 but some of the later homebrews use more.
// ------------------------------------------------------------------------------
#define JST_UP              0x0001
#define JST_RIGHT           0x0002
#define JST_DOWN            0x0004
#define JST_LEFT            0x0008
#define JST_FIRE            0x0010
#define JST_FIRE2           0x0020
#define JST_FIRE3           0x0040

// -----------------------------------------------------------------------------------
// And these are meta keys for mapping NDS keys to keyboard keys (many of the computer
// games don't use joystick inputs and so need to map to keyboard keys...)
// -----------------------------------------------------------------------------------
#define META_KBD_A          0xF001
#define META_KBD_B          0xF002
#define META_KBD_C          0xF003
#define META_KBD_D          0xF004
#define META_KBD_E          0xF005
#define META_KBD_F          0xF006
#define META_KBD_G          0xF007
#define META_KBD_H          0xF008
#define META_KBD_I          0xF009
#define META_KBD_J          0xF00A
#define META_KBD_K          0xF00B
#define META_KBD_L          0xF00C
#define META_KBD_M          0xF00D
#define META_KBD_N          0xF00E
#define META_KBD_O          0xF00F
#define META_KBD_P          0xF010
#define META_KBD_Q          0xF011
#define META_KBD_R          0xF012
#define META_KBD_S          0xF013
#define META_KBD_T          0xF014
#define META_KBD_U          0xF015
#define META_KBD_V          0xF016
#define META_KBD_W          0xF017
#define META_KBD_X          0xF018
#define META_KBD_Y          0xF019
#define META_KBD_Z          0xF01A

#define META_KBD_0          0xF01B
#define META_KBD_1          0xF01C
#define META_KBD_2          0xF01D
#define META_KBD_3          0xF01E
#define META_KBD_4          0xF01F
#define META_KBD_5          0xF020
#define META_KBD_6          0xF021
#define META_KBD_7          0xF022
#define META_KBD_8          0xF023
#define META_KBD_9          0xF024

#define META_KBD_SHIFT      0xF025
#define META_KBD_CONTROL    0xF026
#define META_KBD_CAPSLOCK   0xF027
#define META_KBD_PERIOD     0xF028
#define META_KBD_COMMA      0xF029
#define META_KBD_COLON      0xF02A
#define META_KBD_SEMI       0xF02B
#define META_KBD_ATSIGN     0xF02C
#define META_KBD_SLASH      0xF02D
#define META_KBD_SPACE      0xF02E
#define META_KBD_RETURN     0xF02F
#define META_KBD_BACKSLASH  0xF030
#define META_KBD_ESCAPE     0xF031
#define META_KBD_LBRACKET   0xF032
#define META_KBD_RBRACKET   0xF033
#define META_KBD_DASH       0xF034
#define META_KBD_CARRET     0xF035
#define META_KBD_RESERVED1  0xF036
#define META_KBD_RESERVED2  0xF037

#define META_KBD_F0         0xF038
#define META_KBD_F1         0xF039
#define META_KBD_F2         0xF03A
#define META_KBD_F3         0xF03B
#define META_KBD_F4         0xF03C
#define META_KBD_F5         0xF03D
#define META_KBD_F6         0xF03E
#define META_KBD_F7         0xF03F
#define META_KBD_F8         0xF040
#define META_KBD_F9         0xF041
#define META_KBD_FDOT       0xF042
#define META_KBD_FENT       0xF043

#define META_KBD_CURS_UP    0xF044
#define META_KBD_CURS_DN    0xF045
#define META_KBD_CURS_LF    0xF046
#define META_KBD_CURS_RT    0xF047
#define META_KBD_CURS_CPY   0xF048

#define META_KBD_PAN_UP16   0xF049
#define META_KBD_PAN_UP24   0xF04A
#define META_KBD_PAN_UP32   0xF04B
#define META_KBD_PAN_UP48   0xF04C
#define META_KBD_PAN_UP64   0xF04D

#define META_KBD_PAN_DN16   0xF04E
#define META_KBD_PAN_DN24   0xF04F
#define META_KBD_PAN_DN32   0xF050
#define META_KBD_PAN_DN48   0xF051
#define META_KBD_PAN_DN64   0xF052

#define META_KBD_OFFSET16   0xF053
#define META_KBD_OFFSET32   0xF054
#define META_KBD_OFFSET48   0xF055
#define META_KBD_OFFSET64   0xF056

#define MAX_KEY_OPTIONS     93

// -----------------------------
// For the Full Keyboard...
// -----------------------------
#define KBD_KEY_SFT         1
#define KBD_KEY_DEL         2
#define KBD_KEY_CAPS        3
#define KBD_KEY_CTL         4
#define KBD_KEY_TAB         5
#define KBD_KEY_CUP         6
#define KBD_KEY_CRT         7
#define KBD_KEY_CDN         8
#define KBD_KEY_CLT         9
#define KBD_KEY_CPY         10
#define KBD_KEY_BSL         11
#define KBD_KEY_RET         13

#define KBD_KEY_F0          14
#define KBD_KEY_F1          15
#define KBD_KEY_F2          16
#define KBD_KEY_F3          17
#define KBD_KEY_F4          18
#define KBD_KEY_F5          19
#define KBD_KEY_F6          20
#define KBD_KEY_F7          21
#define KBD_KEY_F8          22
#define KBD_KEY_F9          23
#define KBD_KEY_FDOT        24
#define KBD_KEY_FENT        25
#define KBD_KEY_CLR         26

#define KBD_KEY_ESC         27

// What format is the input file?
#define MODE_DSK            1
#define MODE_CPR            2
#define MODE_DAN            3
#define MODE_SNA            4
#define MODE_MEG            5

#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

extern unsigned char BASIC_6128[16384];
extern unsigned char OS_6128[16384];
extern unsigned char AMSDOS[16384];
extern unsigned char PARADOS[16384];
extern unsigned char MEGALOAD[40377];

extern u8 DISK_IMAGE_BUFFER[];

extern u8 CRTC[];
extern u8 CRT_Idx;

extern u32 scanline_count;

extern u8 MMR;
extern u8 RMR;
extern u8 PENR;
extern u8 UROM;

extern u8  DAN_Zone0;
extern u8  DAN_Zone1;
extern u16 DAN_Config;
extern u8  DAN_Follow;
extern u8  DAN_WaitRET;

extern u8 INK[17];
extern u32 border_color;
extern u8 CRTC[0x20];
extern u8 CRT_Idx;
extern u8 inks_changed;
extern u16 refresh_tstates;
extern u8 ink_map[256];

extern u32 HCC;
extern u32 HSC;
extern u32 VCC;
extern u32 VSC;
extern u32 VLC;
extern u32 R52;
extern u32 VTAC;
extern u8  DISPEN;

extern s16 temp_offset;
extern s16 perm_offset;
extern u16 slide_dampen;

extern int current_ds_line;
extern u32 vsync_plus_two;
extern u32 r12_screen_offset;
extern u8  vsync_off_count;
extern u8  *cpc_ScreenPage;
extern u16 escapeClause;
extern u32 raster_counter;
extern u8  vSyncSeen;
extern u8  display_disable_in;

extern u8 floppy_sound;
extern u8 floppy_action;

extern char initial_file[];
extern char initial_path[];

extern u8 amstrad_mode;
extern u8 kbd_keys_pressed;
extern u8 kbd_keys[12];
extern u16 emuFps;
extern u16 emuActFrames;
extern u16 timingFrames;
extern u32 emuTotFrames;
extern u16 nds_key;
extern u8  kbd_key;
extern u16 vusCptVBL;
extern u16 keyCoresp[MAX_KEY_OPTIONS];
extern u16 NDS_keyMap[];
extern u8  soundEmuPause;
extern int bg0, bg1, bg0b, bg1b;
extern u32 last_file_size;
extern u8  b32K_Mode;

extern u8 SugarDSChooseGame(u8 bDiskOnly);
extern void CartLoad(void);
extern void ConfigureMemory(void);
extern void compute_pre_inked(u8 mode);
extern void SugarDSGameOptions(bool bIsGlobal);
extern void processDirectAudio(void);
extern u8 crtc_render_screen_line(void);
extern void crtc_reset(void);
extern void crtc_r52_int(void);
extern void BottomScreenOptions(void);
extern void TopScreenOptions(void);
extern void TopScreenImage(void);
extern void BottomScreenKeyboard(void);
extern void ReadFileCRCAndConfig(void);
extern void DisplayStatusLine(bool bForce);
extern void DiskInsert(char *filename, u8 bForceRead);
extern void ResetAmstrad(void);
extern void MaxBrightness(void);
extern void debug_init();
extern void debug_save();
extern void debug_printf(const char * str, ...);

#endif // _SUGARDS_H_
