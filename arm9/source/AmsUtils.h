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
#ifndef _AMSUTILS_H_
#define _AMSUTILS_H_
#include <nds.h>
#include "SugarDS.h"
#include "cpu/z80/Z80_interface.h"
#include "cpu/ay38910/AY38910.h"

#define MAX_FILES                   1024
#define MAX_FILENAME_LEN            160
#define MAX_ROM_SIZE               (1024*1024) // 1024K is big enough for any disk / cart / snapshot

#define MAX_CONFIGS                 890
#define CONFIG_VERSION              0x0006

#define AMSTRAD_FILE                0x01
#define DIRECTORY                   0x02

#define ID_SHM_CANCEL               0x00
#define ID_SHM_YES                  0x01
#define ID_SHM_NO                   0x02

#define DPAD_NORMAL                 0
#define DPAD_DIAGONALS              1
#define DPAD_SLIDE_N_GLIDE          2

#define CRTC_DRV_STANDARD           0
#define CRTC_DRV_ADVANCED           1

extern char last_path[MAX_FILENAME_LEN];
extern char last_file[MAX_FILENAME_LEN];

extern u32 pre_inked_mode0[256];
extern u32 pre_inked_mode1[256];
extern u32 pre_inked_mode2a[256];
extern u32 pre_inked_mode2b[256];
extern u32 pre_inked_mode2c[256];

typedef struct
{
  char szName[MAX_FILENAME_LEN+1];
  u8 uType;
  u32 uCrc;
} FIAmstrad;

extern u32 file_size;

struct __attribute__((__packed__)) GlobalConfig_t
{
    u16 config_ver;
    u32 bios_checksums;
    char szLastFile[MAX_FILENAME_LEN+1];
    char szLastPath[MAX_FILENAME_LEN+1];
    char reserved1[MAX_FILENAME_LEN+1];
    char reserved2[MAX_FILENAME_LEN+1];
    u8  showFPS;
    u8  lastDir;
    u8  diskROM;
    u8  splashType;
    u8  keyboardDim;
    u8  global_04;
    u8  global_05;
    u8  global_06;
    u8  global_07;
    u8  global_08;
    u8  global_09;
    u8  global_10;
    u8  global_11;
    u8  global_12;
    u8  debugger;
    u32 config_checksum;
};

struct __attribute__((__packed__)) Config_t
{
    u32 game_crc;
    u8  keymap[10];
    u8  autoFire;
    u8  r52IntVsync;
    u8  jitter;    
    u8  dpad;
    u8  autoLoad;
    u8  gameSpeed;
    u8  autoSize;
    u8  cpuAdjust;
    u8  waveDirect;
    u8  screenTop;
    u8  mode2mode;
    u8  diskWrite;
    u8  crtcDriver;
    u8  reserved7;
    u8  reserved8;
    u8  reserved9;
    s8  offsetX;
    s8  offsetY;
    s16 scaleX;
    s16 scaleY;
};

extern struct Config_t       myConfig;
extern struct GlobalConfig_t myGlobalConfig;

extern u8 last_special_key;
extern u8 last_special_key_dampen;

extern u16 JoyState;                    // Joystick / Paddle management

extern u32 file_crc;
extern u8 bFirstTime;

extern u8 BufferedKeys[32];
extern u8 BufferedKeysWriteIdx;
extern u8 BufferedKeysReadIdx;

extern u8 portA, portB, portC, portDIR;

extern u8 ROM_Memory[MAX_ROM_SIZE];
extern u8 RAM_Memory[0x20000]; // 64K plus an expanded 512K

extern u8 *MemoryMapR[4];
extern u8 *MemoryMapW[4];
extern AY38910 myAY;

extern FIAmstrad gpFic[MAX_FILES];
extern int uNbRoms;
extern int ucGameAct;
extern int ucGameChoice;

extern void LoadConfig(void);
extern u8   showMessage(char *szCh1, char *szCh2);
extern void sugarDSFindFiles();
extern void sugarDSChangeOptions(void);
extern u8   sugarDSLoadFile(u8 bTapeOnly);
extern void DSPrint(int iX,int iY,int iScr,char *szMessage);
extern u32  crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);
extern void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait);
extern void DisplayFileName(void);
extern u32  ReadFileCarefully(char *filename, u8 *buf, u32 buf_size, u32 buf_offset);
extern u8   loadgame(const char *path);
extern u8   AmstradInit(char *szGame);
extern void amstrad_set_palette(void);
extern void amstrad_emulate(void);
extern u8   cpu_readport_ams(register unsigned short Port);
extern void amstrad_reset(void);
extern u32  amstrad_run(void);
extern void getfile_crc(const char *path);
extern void amstradLoadState();
extern void amstradSaveState();
extern void intro_logo(void);
extern void BufferKey(u8 key);
extern void BufferKeys(char *keys);
extern void ProcessBufferedKeys(void);
extern void SugarDSChangeKeymap(void);

#endif // _AMSUTILS_H_
