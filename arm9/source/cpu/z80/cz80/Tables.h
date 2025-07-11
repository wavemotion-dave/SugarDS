/******************************************************************************
*  SugarDS Z80 CPU 
*
* Note: Most of this file is from the ColEm emulator core by Marat Fayzullin
*       but heavily modified for specific NDS use. If you want to use this
*       code, you are advised to seek out the much more portable ColEm core
*       and contact Marat.       
*
******************************************************************************/

/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                          Tables.h                       **/
/**                                                         **/
/** This file contains tables of used by Z80 emulation to   **/
/** compute SIGN,ZERO, PARITY flags, and decimal correction **/
/** There are also timing tables for Z80 opcodes. This file **/
/** is included from Z80.c.                                 **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/   
/**     changes to this file.                               **/
/*************************************************************/

// -----------------------------------------------------------------
// Note: These are all compensated for by the waits incurred by 
// the Amstrad CPC for Gate Access to memory. Basically everything
// in this system is a multiple of 4 cycles (one NOP) in length.
//
// These timings were taken from the "Z80 CPC Timings Cheat Sheet"
//
// Note: This table builds in the assumption that the jump is taken
// and we will compensate the cycles if the jump is not taken. 
// -----------------------------------------------------------------
static const byte Cycles[256] __attribute__((section(".dtcm"))) =
{
  //00 01  02  03  04  05  06  07       08  09  0A  0B   0C  0D  0E  0F
    4, 12,  8,  8,  4,  4,  8,  4,       4, 12,  8,  8,  4,  4,  8,  4,  // 0x00
   16, 12,  8,  8,  4,  4,  8,  4,      12, 12,  8,  8,  4,  4,  8,  4,  // 0x10
   12, 12, 20,  8,  4,  4,  8,  4,      12, 12, 20,  8,  4,  4,  8,  4,  // 0x20
   12, 12, 16,  8, 12, 12, 12,  4,      12, 12, 16,  8,  4,  4,  8,  4,  // 0x30
    4,  4,  4,  4,  4,  4,  8,  4,       4,  4,  4,  4,  4,  4,  8,  4,  // 0x40
    4,  4,  4,  4,  4,  4,  8,  4,       4,  4,  4,  4,  4,  4,  8,  4,  // 0x50
    4,  4,  4,  4,  4,  4,  8,  4,       4,  4,  4,  4,  4,  4,  8,  4,  // 0x60
    8,  8,  8,  8,  8,  8,  4,  8,       4,  4,  4,  4,  4,  4,  8,  4,  // 0x70
    4,  4,  4,  4,  4,  4,  8,  4,       4,  4,  4,  4,  4,  4,  8,  4,  // 0x80
    4,  4,  4,  4,  4,  4,  8,  4,       4,  4,  4,  4,  4,  4,  8,  4,  // 0x90
    4,  4,  4,  4,  4,  4,  8,  4,       4,  4,  4,  4,  4,  4,  8,  4,  // 0xA0
    4,  4,  4,  4,  4,  4,  8,  4,       4,  4,  4,  4,  4,  4,  8,  4,  // 0xB0
    8, 12, 12, 12, 12, 12,  8, 16,       8, 12, 12,  0, 12, 20,  8, 16,  // 0xC0
    8, 12, 12, 12, 12, 12,  8, 16,       8,  4, 12, 12, 12,  0,  8, 16,  // 0xD0
    8, 12, 12, 24, 12, 12,  8, 16,       8,  4, 12,  4, 12,  0,  8, 16,  // 0xE0
    8, 12, 12,  4, 12, 12,  8, 16,       8,  8, 12,  4, 12,  0,  8, 16   // 0xF0
};


static const byte CyclesCB[256] __attribute__((section(".dtcm"))) =
{
   8,  8,  8,  8,  8,  8, 16, 8,          8,  8,  8,  8,  8,  8, 16, 8,   // 0x00
   8,  8,  8,  8,  8,  8, 16, 8,          8,  8,  8,  8,  8,  8, 16, 8,   // 0x10
   8,  8,  8,  8,  8,  8, 16, 8,          8,  8,  8,  8,  8,  8, 16, 8,   // 0x20
   8,  8,  8,  8,  8,  8, 16, 8,          8,  8,  8,  8,  8,  8, 16, 8,   // 0x30
   8,  8,  8,  8,  8,  8, 12, 8,          8,  8,  8,  8,  8,  8, 12, 8,   // 0x40
   8,  8,  8,  8,  8,  8, 12, 8,          8,  8,  8,  8,  8,  8, 12, 8,   // 0x50
   8,  8,  8,  8,  8,  8, 12, 8,          8,  8,  8,  8,  8,  8, 12, 8,   // 0x60
   8,  8,  8,  8,  8,  8, 12, 8,          8,  8,  8,  8,  8,  8, 12, 8,   // 0x70
   8,  8,  8,  8,  8,  8, 16, 8,          8,  8,  8,  8,  8,  8, 16, 8,   // 0x80
   8,  8,  8,  8,  8,  8, 16, 8,          8,  8,  8,  8,  8,  8, 16, 8,   // 0x90
   8,  8,  8,  8,  8,  8, 16, 8,          8,  8,  8,  8,  8,  8, 16, 8,   // 0xA0
   8,  8,  8,  8,  8,  8, 16, 8,          8,  8,  8,  8,  8,  8, 16, 8,   // 0xB0
   8,  8,  8,  8,  8,  8, 16, 8,          8,  8,  8,  8,  8,  8, 16, 8,   // 0xC0
   8,  8,  8,  8,  8,  8, 16, 8,          8,  8,  8,  8,  8,  8, 16, 8,   // 0xD0
   8,  8,  8,  8,  8,  8, 16, 8,          8,  8,  8,  8,  8,  8, 16, 8,   // 0xE0
   8,  8,  8,  8,  8,  8, 16, 8,          8,  8,  8,  8,  8,  8, 16, 8    // 0xF0
};


static const byte CyclesED[256] __attribute__((section(".dtcm"))) =
{
   8,  8,  8,  8,  8,  8,  8,   8,        8,  8,  8,  8,  8,  8,  8,  8,   // 0x00
   8,  8,  8,  8,  8,  8,  8,   8,        8,  8,  8,  8,  8,  8,  8,  8,   // 0x10
   8,  8,  8,  8,  8,  8,  8,   8,        8,  8,  8,  8,  8,  8,  8,  8,   // 0x20
   8,  8,  8,  8,  8,  8,  8,   8,        8,  8,  8,  8,  8,  8,  8,  8,   // 0x30
  16, 16, 20, 24,  8, 16,  8,   12,      16, 16, 16, 24,  0, 16,  0, 12,   // 0x40
  16, 16, 20, 24,  0,  0,  8,   12,      16, 16, 16, 24,  0,  0,  8, 12,   // 0x50
  16, 16, 20, 24,  0,  0,  0,   20,      16, 16, 16, 24,  0,  0,  0, 20,   // 0x60
  16, 16, 20, 24,  0,  0,  0,   0,       16, 16, 16, 24,  0,  0,  0,  0,   // 0x70
   0,  0,  0,  0,  0,  0,  0,   0,        0,  0,  0,  0,  0,  0,  0,  0,   // 0x80
   0,  0,  0,  0,  0,  0,  0,   0,        0,  0,  0,  0,  0,  0,  0,  0,   // 0x90
  20, 20, 20, 20,  0,  0,  0,   0,       20, 20, 20, 20,  0,  0,  0,  0,   // 0xA0
  24, 24, 24, 24,  0,  0,  0,   0,       24, 24, 24, 24,  0,  0,  0,  0,   // 0xB0 - These cycles assume the block transfer continues... will compensate in the CodesED handler
  0,   0,  0,  0,  0,  0,  0,   0,        0,  0,  0,  0,  0,  0,  0,  0,   // 0xC0
  0,   0,  0,  0,  0,  0,  0,   0,        0,  0,  0,  0,  0,  0,  0,  0,   // 0xD0
  0,   0,  0,  0,  0,  0,  0,   0,        0,  0,  0,  0,  0,  0,  0,  0,   // 0xE0
  0,   0,  0,  0,  0,  0,  0,   0,        0,  0,  0,  0,  0,  0,  0,  0    // 0xF0
};

static const byte CyclesXX[256] __attribute__((section(".dtcm"))) =
{
   0,  0,  0,  0,  8,  8,  12,  0,        0, 16,  0,  0, 8,  8, 12,  0,   // 0x00
   0,  0,  0,  0,  8,  8,  12,  0,        0, 16,  0,  0, 8,  8, 12,  0,   // 0x10
   0, 16, 24, 12,  8,  8,  12,  0,        0, 16, 24, 12, 8,  8, 12,  0,   // 0x20
   0,  0,  0,  0, 24, 24,  24,  0,        0, 16,  0,  0, 8,  8, 12,  0,   // 0x30
   8,  8,  8,  8,  8,  8,  20,  8,        8,  8,  8,  8, 8,  8, 20,  8,   // 0x40
   8,  8,  8,  8,  8,  8,  20,  8,        8,  8,  8,  8, 8,  8, 20,  8,   // 0x50
   8,  8,  8,  8,  8,  8,  20,  8,        8,  8,  8,  8, 8,  8, 20,  8,   // 0x60
  20, 20, 20, 20, 20, 20,   8, 20,        8,  8,  8,  8, 8,  8, 20,  8,   // 0x70
   8,  8,  8,  8,  8,  8,  20,  8,        8,  8,  8,  8, 8,  8, 20,  8,   // 0x80
   8,  8,  8,  8,  8,  8,  20,  8,        8,  8,  8,  8, 8,  8, 20,  8,   // 0x90
   8,  8,  8,  8,  8,  8,  20,  8,        8,  8,  8,  8, 8,  8, 20,  8,   // 0xA0
   8,  8,  8,  8,  8,  8,  20,  8,        8,  8,  8,  8, 8,  8, 20,  8,   // 0xB0
   0,  0,  0,  0,  0,  0,   0,  0,        0,  0,  0,  0, 0,  0,  0,  0,   // 0xC0
   0,  0,  0,  0,  0,  0,   0,  0,        0,  0,  0,  0, 0,  4,  0,  0,   // 0xD0
   0, 20,  0, 28,  0, 20,   0,  0,        0,  8,  0,  0, 0,  0,  0,  0,   // 0xE0
   0,  0,  0,  0,  0,  0,   0,  0,        0, 12,  0,  0, 0,  4,  0,  0    // 0xF0
};


static const byte CyclesXXCB[256] __attribute__((section(".dtcm"))) =
{
   0,  0,  0,  0,  0,  0, 28,  0,        0,  0,  0,  0,  0,  0, 28,  0,   // 0x00
   0,  0,  0,  0,  0,  0, 28,  0,        0,  0,  0,  0,  0,  0, 28,  0,   // 0x10
   0,  0,  0,  0,  0,  0, 28,  0,        0,  0,  0,  0,  0,  0, 28,  0,   // 0x20
   0,  0,  0,  0,  0,  0, 28,  0,        0,  0,  0,  0,  0,  0, 28,  0,   // 0x30
  24, 24, 24, 24, 24, 24, 24, 24,       24, 24, 24, 24, 24, 24, 24, 24,   // 0x40
  24, 24, 24, 24, 24, 24, 24, 24,       24, 24, 24, 24, 24, 24, 24, 24,   // 0x50
  24, 24, 24, 24, 24, 24, 24, 24,       24, 24, 24, 24, 24, 24, 24, 24,   // 0x60
  24, 24, 24, 24, 24, 24, 24, 24,       24, 24, 24, 24, 24, 24, 24, 24,   // 0x70
   0,  0,  0,  0,  0,  0, 28,  0,        0,  0,  0,  0,  0,  0, 28,  0,   // 0x80
   0,  0,  0,  0,  0,  0, 28,  0,        0,  0,  0,  0,  0,  0, 28,  0,   // 0x90
   0,  0,  0,  0,  0,  0, 28,  0,        0,  0,  0,  0,  0,  0, 28,  0,   // 0xA0
   0,  0,  0,  0,  0,  0, 28,  0,        0,  0,  0,  0,  0,  0, 28,  0,   // 0xB0
   0,  0,  0,  0,  0,  0, 28,  0,        0,  0,  0,  0,  0,  0, 28,  0,   // 0xC0
   0,  0,  0,  0,  0,  0, 28,  0,        0,  0,  0,  0,  0,  0, 28,  0,   // 0xD0
   0,  0,  0,  0,  0,  0, 28,  0,        0,  0,  0,  0,  0,  0, 28,  0,   // 0xE0
   0,  0,  0,  0,  0,  0, 28,  0,        0,  0,  0,  0,  0,  0, 28,  0    // 0xF0
};

static const byte ZSTable[256] __attribute__((section(".dtcm"))) =
{
  Z_FLAG,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG
};

static const byte ZSTable_INC[256] __attribute__((section(".dtcm"))) =
{
  Z_FLAG|H_FLAG,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  H_FLAG,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  H_FLAG,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  H_FLAG,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  H_FLAG,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  H_FLAG,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  H_FLAG,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  H_FLAG,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  H_FLAG|V_FLAG|S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,   S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  H_FLAG|S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,          S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  H_FLAG|S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,          S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  H_FLAG|S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,          S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  H_FLAG|S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,          S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  H_FLAG|S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,          S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  H_FLAG|S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,          S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,
  H_FLAG|S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,          S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG,S_FLAG
};


static const byte ZSTable_DEC[256] __attribute__((section(".dtcm"))) =
{
  Z_FLAG|N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,H_FLAG|N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,H_FLAG|N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,H_FLAG|N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,H_FLAG|N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,H_FLAG|N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,H_FLAG|N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,H_FLAG|N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,V_FLAG|H_FLAG|N_FLAG,
  
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|H_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|H_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|H_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|H_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|H_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|H_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|H_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,
  S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|N_FLAG,S_FLAG|H_FLAG|N_FLAG
};

static const byte PZSTable[256] __attribute__((section(".dtcm"))) =
{
  Z_FLAG|P_FLAG,0,0,P_FLAG,0,P_FLAG,P_FLAG,0,
  0,P_FLAG,P_FLAG,0,P_FLAG,0,0,P_FLAG,
  0,P_FLAG,P_FLAG,0,P_FLAG,0,0,P_FLAG,P_FLAG,0,0,P_FLAG,0,P_FLAG,P_FLAG,0,
  0,P_FLAG,P_FLAG,0,P_FLAG,0,0,P_FLAG,P_FLAG,0,0,P_FLAG,0,P_FLAG,P_FLAG,0,
  P_FLAG,0,0,P_FLAG,0,P_FLAG,P_FLAG,0,0,P_FLAG,P_FLAG,0,P_FLAG,0,0,P_FLAG,
  0,P_FLAG,P_FLAG,0,P_FLAG,0,0,P_FLAG,P_FLAG,0,0,P_FLAG,0,P_FLAG,P_FLAG,0,
  P_FLAG,0,0,P_FLAG,0,P_FLAG,P_FLAG,0,0,P_FLAG,P_FLAG,0,P_FLAG,0,0,P_FLAG,
  P_FLAG,0,0,P_FLAG,0,P_FLAG,P_FLAG,0,0,P_FLAG,P_FLAG,0,P_FLAG,0,0,P_FLAG,
  0,P_FLAG,P_FLAG,0,P_FLAG,0,0,P_FLAG,P_FLAG,0,0,P_FLAG,0,P_FLAG,P_FLAG,0,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG,S_FLAG|P_FLAG,S_FLAG|P_FLAG,S_FLAG,
  S_FLAG|P_FLAG,S_FLAG,S_FLAG,S_FLAG|P_FLAG
};

static const byte PZSHTable_BIT[129] __attribute__((section(".dtcm"))) =
{
  Z_FLAG|P_FLAG|H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,
  H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,
  H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,
  H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,
  H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,
  H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,
  H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,
  H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,H_FLAG,
  S_FLAG|H_FLAG
};

static const word DAATable[2048] __attribute__((section(".dtcm"))) =
{
  0x0044,0x0100,0x0200,0x0304,0x0400,0x0504,0x0604,0x0700,
  0x0808,0x090C,0x1010,0x1114,0x1214,0x1310,0x1414,0x1510,
  0x1000,0x1104,0x1204,0x1300,0x1404,0x1500,0x1600,0x1704,
  0x180C,0x1908,0x2030,0x2134,0x2234,0x2330,0x2434,0x2530,
  0x2020,0x2124,0x2224,0x2320,0x2424,0x2520,0x2620,0x2724,
  0x282C,0x2928,0x3034,0x3130,0x3230,0x3334,0x3430,0x3534,
  0x3024,0x3120,0x3220,0x3324,0x3420,0x3524,0x3624,0x3720,
  0x3828,0x392C,0x4010,0x4114,0x4214,0x4310,0x4414,0x4510,
  0x4000,0x4104,0x4204,0x4300,0x4404,0x4500,0x4600,0x4704,
  0x480C,0x4908,0x5014,0x5110,0x5210,0x5314,0x5410,0x5514,
  0x5004,0x5100,0x5200,0x5304,0x5400,0x5504,0x5604,0x5700,
  0x5808,0x590C,0x6034,0x6130,0x6230,0x6334,0x6430,0x6534,
  0x6024,0x6120,0x6220,0x6324,0x6420,0x6524,0x6624,0x6720,
  0x6828,0x692C,0x7030,0x7134,0x7234,0x7330,0x7434,0x7530,
  0x7020,0x7124,0x7224,0x7320,0x7424,0x7520,0x7620,0x7724,
  0x782C,0x7928,0x8090,0x8194,0x8294,0x8390,0x8494,0x8590,
  0x8080,0x8184,0x8284,0x8380,0x8484,0x8580,0x8680,0x8784,
  0x888C,0x8988,0x9094,0x9190,0x9290,0x9394,0x9490,0x9594,
  0x9084,0x9180,0x9280,0x9384,0x9480,0x9584,0x9684,0x9780,
  0x9888,0x998C,0x0055,0x0111,0x0211,0x0315,0x0411,0x0515,
  0x0045,0x0101,0x0201,0x0305,0x0401,0x0505,0x0605,0x0701,
  0x0809,0x090D,0x1011,0x1115,0x1215,0x1311,0x1415,0x1511,
  0x1001,0x1105,0x1205,0x1301,0x1405,0x1501,0x1601,0x1705,
  0x180D,0x1909,0x2031,0x2135,0x2235,0x2331,0x2435,0x2531,
  0x2021,0x2125,0x2225,0x2321,0x2425,0x2521,0x2621,0x2725,
  0x282D,0x2929,0x3035,0x3131,0x3231,0x3335,0x3431,0x3535,
  0x3025,0x3121,0x3221,0x3325,0x3421,0x3525,0x3625,0x3721,
  0x3829,0x392D,0x4011,0x4115,0x4215,0x4311,0x4415,0x4511,
  0x4001,0x4105,0x4205,0x4301,0x4405,0x4501,0x4601,0x4705,
  0x480D,0x4909,0x5015,0x5111,0x5211,0x5315,0x5411,0x5515,
  0x5005,0x5101,0x5201,0x5305,0x5401,0x5505,0x5605,0x5701,
  0x5809,0x590D,0x6035,0x6131,0x6231,0x6335,0x6431,0x6535,
  0x6025,0x6121,0x6221,0x6325,0x6421,0x6525,0x6625,0x6721,
  0x6829,0x692D,0x7031,0x7135,0x7235,0x7331,0x7435,0x7531,
  0x7021,0x7125,0x7225,0x7321,0x7425,0x7521,0x7621,0x7725,
  0x782D,0x7929,0x8091,0x8195,0x8295,0x8391,0x8495,0x8591,
  0x8081,0x8185,0x8285,0x8381,0x8485,0x8581,0x8681,0x8785,
  0x888D,0x8989,0x9095,0x9191,0x9291,0x9395,0x9491,0x9595,
  0x9085,0x9181,0x9281,0x9385,0x9481,0x9585,0x9685,0x9781,
  0x9889,0x998D,0xA0B5,0xA1B1,0xA2B1,0xA3B5,0xA4B1,0xA5B5,
  0xA0A5,0xA1A1,0xA2A1,0xA3A5,0xA4A1,0xA5A5,0xA6A5,0xA7A1,
  0xA8A9,0xA9AD,0xB0B1,0xB1B5,0xB2B5,0xB3B1,0xB4B5,0xB5B1,
  0xB0A1,0xB1A5,0xB2A5,0xB3A1,0xB4A5,0xB5A1,0xB6A1,0xB7A5,
  0xB8AD,0xB9A9,0xC095,0xC191,0xC291,0xC395,0xC491,0xC595,
  0xC085,0xC181,0xC281,0xC385,0xC481,0xC585,0xC685,0xC781,
  0xC889,0xC98D,0xD091,0xD195,0xD295,0xD391,0xD495,0xD591,
  0xD081,0xD185,0xD285,0xD381,0xD485,0xD581,0xD681,0xD785,
  0xD88D,0xD989,0xE0B1,0xE1B5,0xE2B5,0xE3B1,0xE4B5,0xE5B1,
  0xE0A1,0xE1A5,0xE2A5,0xE3A1,0xE4A5,0xE5A1,0xE6A1,0xE7A5,
  0xE8AD,0xE9A9,0xF0B5,0xF1B1,0xF2B1,0xF3B5,0xF4B1,0xF5B5,
  0xF0A5,0xF1A1,0xF2A1,0xF3A5,0xF4A1,0xF5A5,0xF6A5,0xF7A1,
  0xF8A9,0xF9AD,0x0055,0x0111,0x0211,0x0315,0x0411,0x0515,
  0x0045,0x0101,0x0201,0x0305,0x0401,0x0505,0x0605,0x0701,
  0x0809,0x090D,0x1011,0x1115,0x1215,0x1311,0x1415,0x1511,
  0x1001,0x1105,0x1205,0x1301,0x1405,0x1501,0x1601,0x1705,
  0x180D,0x1909,0x2031,0x2135,0x2235,0x2331,0x2435,0x2531,
  0x2021,0x2125,0x2225,0x2321,0x2425,0x2521,0x2621,0x2725,
  0x282D,0x2929,0x3035,0x3131,0x3231,0x3335,0x3431,0x3535,
  0x3025,0x3121,0x3221,0x3325,0x3421,0x3525,0x3625,0x3721,
  0x3829,0x392D,0x4011,0x4115,0x4215,0x4311,0x4415,0x4511,
  0x4001,0x4105,0x4205,0x4301,0x4405,0x4501,0x4601,0x4705,
  0x480D,0x4909,0x5015,0x5111,0x5211,0x5315,0x5411,0x5515,
  0x5005,0x5101,0x5201,0x5305,0x5401,0x5505,0x5605,0x5701,
  0x5809,0x590D,0x6035,0x6131,0x6231,0x6335,0x6431,0x6535,
  0x0604,0x0700,0x0808,0x090C,0x0A0C,0x0B08,0x0C0C,0x0D08,
  0x0E08,0x0F0C,0x1010,0x1114,0x1214,0x1310,0x1414,0x1510,
  0x1600,0x1704,0x180C,0x1908,0x1A08,0x1B0C,0x1C08,0x1D0C,
  0x1E0C,0x1F08,0x2030,0x2134,0x2234,0x2330,0x2434,0x2530,
  0x2620,0x2724,0x282C,0x2928,0x2A28,0x2B2C,0x2C28,0x2D2C,
  0x2E2C,0x2F28,0x3034,0x3130,0x3230,0x3334,0x3430,0x3534,
  0x3624,0x3720,0x3828,0x392C,0x3A2C,0x3B28,0x3C2C,0x3D28,
  0x3E28,0x3F2C,0x4010,0x4114,0x4214,0x4310,0x4414,0x4510,
  0x4600,0x4704,0x480C,0x4908,0x4A08,0x4B0C,0x4C08,0x4D0C,
  0x4E0C,0x4F08,0x5014,0x5110,0x5210,0x5314,0x5410,0x5514,
  0x5604,0x5700,0x5808,0x590C,0x5A0C,0x5B08,0x5C0C,0x5D08,
  0x5E08,0x5F0C,0x6034,0x6130,0x6230,0x6334,0x6430,0x6534,
  0x6624,0x6720,0x6828,0x692C,0x6A2C,0x6B28,0x6C2C,0x6D28,
  0x6E28,0x6F2C,0x7030,0x7134,0x7234,0x7330,0x7434,0x7530,
  0x7620,0x7724,0x782C,0x7928,0x7A28,0x7B2C,0x7C28,0x7D2C,
  0x7E2C,0x7F28,0x8090,0x8194,0x8294,0x8390,0x8494,0x8590,
  0x8680,0x8784,0x888C,0x8988,0x8A88,0x8B8C,0x8C88,0x8D8C,
  0x8E8C,0x8F88,0x9094,0x9190,0x9290,0x9394,0x9490,0x9594,
  0x9684,0x9780,0x9888,0x998C,0x9A8C,0x9B88,0x9C8C,0x9D88,
  0x9E88,0x9F8C,0x0055,0x0111,0x0211,0x0315,0x0411,0x0515,
  0x0605,0x0701,0x0809,0x090D,0x0A0D,0x0B09,0x0C0D,0x0D09,
  0x0E09,0x0F0D,0x1011,0x1115,0x1215,0x1311,0x1415,0x1511,
  0x1601,0x1705,0x180D,0x1909,0x1A09,0x1B0D,0x1C09,0x1D0D,
  0x1E0D,0x1F09,0x2031,0x2135,0x2235,0x2331,0x2435,0x2531,
  0x2621,0x2725,0x282D,0x2929,0x2A29,0x2B2D,0x2C29,0x2D2D,
  0x2E2D,0x2F29,0x3035,0x3131,0x3231,0x3335,0x3431,0x3535,
  0x3625,0x3721,0x3829,0x392D,0x3A2D,0x3B29,0x3C2D,0x3D29,
  0x3E29,0x3F2D,0x4011,0x4115,0x4215,0x4311,0x4415,0x4511,
  0x4601,0x4705,0x480D,0x4909,0x4A09,0x4B0D,0x4C09,0x4D0D,
  0x4E0D,0x4F09,0x5015,0x5111,0x5211,0x5315,0x5411,0x5515,
  0x5605,0x5701,0x5809,0x590D,0x5A0D,0x5B09,0x5C0D,0x5D09,
  0x5E09,0x5F0D,0x6035,0x6131,0x6231,0x6335,0x6431,0x6535,
  0x6625,0x6721,0x6829,0x692D,0x6A2D,0x6B29,0x6C2D,0x6D29,
  0x6E29,0x6F2D,0x7031,0x7135,0x7235,0x7331,0x7435,0x7531,
  0x7621,0x7725,0x782D,0x7929,0x7A29,0x7B2D,0x7C29,0x7D2D,
  0x7E2D,0x7F29,0x8091,0x8195,0x8295,0x8391,0x8495,0x8591,
  0x8681,0x8785,0x888D,0x8989,0x8A89,0x8B8D,0x8C89,0x8D8D,
  0x8E8D,0x8F89,0x9095,0x9191,0x9291,0x9395,0x9491,0x9595,
  0x9685,0x9781,0x9889,0x998D,0x9A8D,0x9B89,0x9C8D,0x9D89,
  0x9E89,0x9F8D,0xA0B5,0xA1B1,0xA2B1,0xA3B5,0xA4B1,0xA5B5,
  0xA6A5,0xA7A1,0xA8A9,0xA9AD,0xAAAD,0xABA9,0xACAD,0xADA9,
  0xAEA9,0xAFAD,0xB0B1,0xB1B5,0xB2B5,0xB3B1,0xB4B5,0xB5B1,
  0xB6A1,0xB7A5,0xB8AD,0xB9A9,0xBAA9,0xBBAD,0xBCA9,0xBDAD,
  0xBEAD,0xBFA9,0xC095,0xC191,0xC291,0xC395,0xC491,0xC595,
  0xC685,0xC781,0xC889,0xC98D,0xCA8D,0xCB89,0xCC8D,0xCD89,
  0xCE89,0xCF8D,0xD091,0xD195,0xD295,0xD391,0xD495,0xD591,
  0xD681,0xD785,0xD88D,0xD989,0xDA89,0xDB8D,0xDC89,0xDD8D,
  0xDE8D,0xDF89,0xE0B1,0xE1B5,0xE2B5,0xE3B1,0xE4B5,0xE5B1,
  0xE6A1,0xE7A5,0xE8AD,0xE9A9,0xEAA9,0xEBAD,0xECA9,0xEDAD,
  0xEEAD,0xEFA9,0xF0B5,0xF1B1,0xF2B1,0xF3B5,0xF4B1,0xF5B5,
  0xF6A5,0xF7A1,0xF8A9,0xF9AD,0xFAAD,0xFBA9,0xFCAD,0xFDA9,
  0xFEA9,0xFFAD,0x0055,0x0111,0x0211,0x0315,0x0411,0x0515,
  0x0605,0x0701,0x0809,0x090D,0x0A0D,0x0B09,0x0C0D,0x0D09,
  0x0E09,0x0F0D,0x1011,0x1115,0x1215,0x1311,0x1415,0x1511,
  0x1601,0x1705,0x180D,0x1909,0x1A09,0x1B0D,0x1C09,0x1D0D,
  0x1E0D,0x1F09,0x2031,0x2135,0x2235,0x2331,0x2435,0x2531,
  0x2621,0x2725,0x282D,0x2929,0x2A29,0x2B2D,0x2C29,0x2D2D,
  0x2E2D,0x2F29,0x3035,0x3131,0x3231,0x3335,0x3431,0x3535,
  0x3625,0x3721,0x3829,0x392D,0x3A2D,0x3B29,0x3C2D,0x3D29,
  0x3E29,0x3F2D,0x4011,0x4115,0x4215,0x4311,0x4415,0x4511,
  0x4601,0x4705,0x480D,0x4909,0x4A09,0x4B0D,0x4C09,0x4D0D,
  0x4E0D,0x4F09,0x5015,0x5111,0x5211,0x5315,0x5411,0x5515,
  0x5605,0x5701,0x5809,0x590D,0x5A0D,0x5B09,0x5C0D,0x5D09,
  0x5E09,0x5F0D,0x6035,0x6131,0x6231,0x6335,0x6431,0x6535,
  0x0046,0x0102,0x0202,0x0306,0x0402,0x0506,0x0606,0x0702,
  0x080A,0x090E,0x0402,0x0506,0x0606,0x0702,0x080A,0x090E,
  0x1002,0x1106,0x1206,0x1302,0x1406,0x1502,0x1602,0x1706,
  0x180E,0x190A,0x1406,0x1502,0x1602,0x1706,0x180E,0x190A,
  0x2022,0x2126,0x2226,0x2322,0x2426,0x2522,0x2622,0x2726,
  0x282E,0x292A,0x2426,0x2522,0x2622,0x2726,0x282E,0x292A,
  0x3026,0x3122,0x3222,0x3326,0x3422,0x3526,0x3626,0x3722,
  0x382A,0x392E,0x3422,0x3526,0x3626,0x3722,0x382A,0x392E,
  0x4002,0x4106,0x4206,0x4302,0x4406,0x4502,0x4602,0x4706,
  0x480E,0x490A,0x4406,0x4502,0x4602,0x4706,0x480E,0x490A,
  0x5006,0x5102,0x5202,0x5306,0x5402,0x5506,0x5606,0x5702,
  0x580A,0x590E,0x5402,0x5506,0x5606,0x5702,0x580A,0x590E,
  0x6026,0x6122,0x6222,0x6326,0x6422,0x6526,0x6626,0x6722,
  0x682A,0x692E,0x6422,0x6526,0x6626,0x6722,0x682A,0x692E,
  0x7022,0x7126,0x7226,0x7322,0x7426,0x7522,0x7622,0x7726,
  0x782E,0x792A,0x7426,0x7522,0x7622,0x7726,0x782E,0x792A,
  0x8082,0x8186,0x8286,0x8382,0x8486,0x8582,0x8682,0x8786,
  0x888E,0x898A,0x8486,0x8582,0x8682,0x8786,0x888E,0x898A,
  0x9086,0x9182,0x9282,0x9386,0x9482,0x9586,0x9686,0x9782,
  0x988A,0x998E,0x3423,0x3527,0x3627,0x3723,0x382B,0x392F,
  0x4003,0x4107,0x4207,0x4303,0x4407,0x4503,0x4603,0x4707,
  0x480F,0x490B,0x4407,0x4503,0x4603,0x4707,0x480F,0x490B,
  0x5007,0x5103,0x5203,0x5307,0x5403,0x5507,0x5607,0x5703,
  0x580B,0x590F,0x5403,0x5507,0x5607,0x5703,0x580B,0x590F,
  0x6027,0x6123,0x6223,0x6327,0x6423,0x6527,0x6627,0x6723,
  0x682B,0x692F,0x6423,0x6527,0x6627,0x6723,0x682B,0x692F,
  0x7023,0x7127,0x7227,0x7323,0x7427,0x7523,0x7623,0x7727,
  0x782F,0x792B,0x7427,0x7523,0x7623,0x7727,0x782F,0x792B,
  0x8083,0x8187,0x8287,0x8383,0x8487,0x8583,0x8683,0x8787,
  0x888F,0x898B,0x8487,0x8583,0x8683,0x8787,0x888F,0x898B,
  0x9087,0x9183,0x9283,0x9387,0x9483,0x9587,0x9687,0x9783,
  0x988B,0x998F,0x9483,0x9587,0x9687,0x9783,0x988B,0x998F,
  0xA0A7,0xA1A3,0xA2A3,0xA3A7,0xA4A3,0xA5A7,0xA6A7,0xA7A3,
  0xA8AB,0xA9AF,0xA4A3,0xA5A7,0xA6A7,0xA7A3,0xA8AB,0xA9AF,
  0xB0A3,0xB1A7,0xB2A7,0xB3A3,0xB4A7,0xB5A3,0xB6A3,0xB7A7,
  0xB8AF,0xB9AB,0xB4A7,0xB5A3,0xB6A3,0xB7A7,0xB8AF,0xB9AB,
  0xC087,0xC183,0xC283,0xC387,0xC483,0xC587,0xC687,0xC783,
  0xC88B,0xC98F,0xC483,0xC587,0xC687,0xC783,0xC88B,0xC98F,
  0xD083,0xD187,0xD287,0xD383,0xD487,0xD583,0xD683,0xD787,
  0xD88F,0xD98B,0xD487,0xD583,0xD683,0xD787,0xD88F,0xD98B,
  0xE0A3,0xE1A7,0xE2A7,0xE3A3,0xE4A7,0xE5A3,0xE6A3,0xE7A7,
  0xE8AF,0xE9AB,0xE4A7,0xE5A3,0xE6A3,0xE7A7,0xE8AF,0xE9AB,
  0xF0A7,0xF1A3,0xF2A3,0xF3A7,0xF4A3,0xF5A7,0xF6A7,0xF7A3,
  0xF8AB,0xF9AF,0xF4A3,0xF5A7,0xF6A7,0xF7A3,0xF8AB,0xF9AF,
  0x0047,0x0103,0x0203,0x0307,0x0403,0x0507,0x0607,0x0703,
  0x080B,0x090F,0x0403,0x0507,0x0607,0x0703,0x080B,0x090F,
  0x1003,0x1107,0x1207,0x1303,0x1407,0x1503,0x1603,0x1707,
  0x180F,0x190B,0x1407,0x1503,0x1603,0x1707,0x180F,0x190B,
  0x2023,0x2127,0x2227,0x2323,0x2427,0x2523,0x2623,0x2727,
  0x282F,0x292B,0x2427,0x2523,0x2623,0x2727,0x282F,0x292B,
  0x3027,0x3123,0x3223,0x3327,0x3423,0x3527,0x3627,0x3723,
  0x382B,0x392F,0x3423,0x3527,0x3627,0x3723,0x382B,0x392F,
  0x4003,0x4107,0x4207,0x4303,0x4407,0x4503,0x4603,0x4707,
  0x480F,0x490B,0x4407,0x4503,0x4603,0x4707,0x480F,0x490B,
  0x5007,0x5103,0x5203,0x5307,0x5403,0x5507,0x5607,0x5703,
  0x580B,0x590F,0x5403,0x5507,0x5607,0x5703,0x580B,0x590F,
  0x6027,0x6123,0x6223,0x6327,0x6423,0x6527,0x6627,0x6723,
  0x682B,0x692F,0x6423,0x6527,0x6627,0x6723,0x682B,0x692F,
  0x7023,0x7127,0x7227,0x7323,0x7427,0x7523,0x7623,0x7727,
  0x782F,0x792B,0x7427,0x7523,0x7623,0x7727,0x782F,0x792B,
  0x8083,0x8187,0x8287,0x8383,0x8487,0x8583,0x8683,0x8787,
  0x888F,0x898B,0x8487,0x8583,0x8683,0x8787,0x888F,0x898B,
  0x9087,0x9183,0x9283,0x9387,0x9483,0x9587,0x9687,0x9783,
  0x988B,0x998F,0x9483,0x9587,0x9687,0x9783,0x988B,0x998F,
  0xFABE,0xFBBA,0xFCBE,0xFDBA,0xFEBA,0xFFBE,0x0046,0x0102,
  0x0202,0x0306,0x0402,0x0506,0x0606,0x0702,0x080A,0x090E,
  0x0A1E,0x0B1A,0x0C1E,0x0D1A,0x0E1A,0x0F1E,0x1002,0x1106,
  0x1206,0x1302,0x1406,0x1502,0x1602,0x1706,0x180E,0x190A,
  0x1A1A,0x1B1E,0x1C1A,0x1D1E,0x1E1E,0x1F1A,0x2022,0x2126,
  0x2226,0x2322,0x2426,0x2522,0x2622,0x2726,0x282E,0x292A,
  0x2A3A,0x2B3E,0x2C3A,0x2D3E,0x2E3E,0x2F3A,0x3026,0x3122,
  0x3222,0x3326,0x3422,0x3526,0x3626,0x3722,0x382A,0x392E,
  0x3A3E,0x3B3A,0x3C3E,0x3D3A,0x3E3A,0x3F3E,0x4002,0x4106,
  0x4206,0x4302,0x4406,0x4502,0x4602,0x4706,0x480E,0x490A,
  0x4A1A,0x4B1E,0x4C1A,0x4D1E,0x4E1E,0x4F1A,0x5006,0x5102,
  0x5202,0x5306,0x5402,0x5506,0x5606,0x5702,0x580A,0x590E,
  0x5A1E,0x5B1A,0x5C1E,0x5D1A,0x5E1A,0x5F1E,0x6026,0x6122,
  0x6222,0x6326,0x6422,0x6526,0x6626,0x6722,0x682A,0x692E,
  0x6A3E,0x6B3A,0x6C3E,0x6D3A,0x6E3A,0x6F3E,0x7022,0x7126,
  0x7226,0x7322,0x7426,0x7522,0x7622,0x7726,0x782E,0x792A,
  0x7A3A,0x7B3E,0x7C3A,0x7D3E,0x7E3E,0x7F3A,0x8082,0x8186,
  0x8286,0x8382,0x8486,0x8582,0x8682,0x8786,0x888E,0x898A,
  0x8A9A,0x8B9E,0x8C9A,0x8D9E,0x8E9E,0x8F9A,0x9086,0x9182,
  0x9282,0x9386,0x3423,0x3527,0x3627,0x3723,0x382B,0x392F,
  0x3A3F,0x3B3B,0x3C3F,0x3D3B,0x3E3B,0x3F3F,0x4003,0x4107,
  0x4207,0x4303,0x4407,0x4503,0x4603,0x4707,0x480F,0x490B,
  0x4A1B,0x4B1F,0x4C1B,0x4D1F,0x4E1F,0x4F1B,0x5007,0x5103,
  0x5203,0x5307,0x5403,0x5507,0x5607,0x5703,0x580B,0x590F,
  0x5A1F,0x5B1B,0x5C1F,0x5D1B,0x5E1B,0x5F1F,0x6027,0x6123,
  0x6223,0x6327,0x6423,0x6527,0x6627,0x6723,0x682B,0x692F,
  0x6A3F,0x6B3B,0x6C3F,0x6D3B,0x6E3B,0x6F3F,0x7023,0x7127,
  0x7227,0x7323,0x7427,0x7523,0x7623,0x7727,0x782F,0x792B,
  0x7A3B,0x7B3F,0x7C3B,0x7D3F,0x7E3F,0x7F3B,0x8083,0x8187,
  0x8287,0x8383,0x8487,0x8583,0x8683,0x8787,0x888F,0x898B,
  0x8A9B,0x8B9F,0x8C9B,0x8D9F,0x8E9F,0x8F9B,0x9087,0x9183,
  0x9283,0x9387,0x9483,0x9587,0x9687,0x9783,0x988B,0x998F,
  0x9A9F,0x9B9B,0x9C9F,0x9D9B,0x9E9B,0x9F9F,0xA0A7,0xA1A3,
  0xA2A3,0xA3A7,0xA4A3,0xA5A7,0xA6A7,0xA7A3,0xA8AB,0xA9AF,
  0xAABF,0xABBB,0xACBF,0xADBB,0xAEBB,0xAFBF,0xB0A3,0xB1A7,
  0xB2A7,0xB3A3,0xB4A7,0xB5A3,0xB6A3,0xB7A7,0xB8AF,0xB9AB,
  0xBABB,0xBBBF,0xBCBB,0xBDBF,0xBEBF,0xBFBB,0xC087,0xC183,
  0xC283,0xC387,0xC483,0xC587,0xC687,0xC783,0xC88B,0xC98F,
  0xCA9F,0xCB9B,0xCC9F,0xCD9B,0xCE9B,0xCF9F,0xD083,0xD187,
  0xD287,0xD383,0xD487,0xD583,0xD683,0xD787,0xD88F,0xD98B,
  0xDA9B,0xDB9F,0xDC9B,0xDD9F,0xDE9F,0xDF9B,0xE0A3,0xE1A7,
  0xE2A7,0xE3A3,0xE4A7,0xE5A3,0xE6A3,0xE7A7,0xE8AF,0xE9AB,
  0xEABB,0xEBBF,0xECBB,0xEDBF,0xEEBF,0xEFBB,0xF0A7,0xF1A3,
  0xF2A3,0xF3A7,0xF4A3,0xF5A7,0xF6A7,0xF7A3,0xF8AB,0xF9AF,
  0xFABF,0xFBBB,0xFCBF,0xFDBB,0xFEBB,0xFFBF,0x0047,0x0103,
  0x0203,0x0307,0x0403,0x0507,0x0607,0x0703,0x080B,0x090F,
  0x0A1F,0x0B1B,0x0C1F,0x0D1B,0x0E1B,0x0F1F,0x1003,0x1107,
  0x1207,0x1303,0x1407,0x1503,0x1603,0x1707,0x180F,0x190B,
  0x1A1B,0x1B1F,0x1C1B,0x1D1F,0x1E1F,0x1F1B,0x2023,0x2127,
  0x2227,0x2323,0x2427,0x2523,0x2623,0x2727,0x282F,0x292B,
  0x2A3B,0x2B3F,0x2C3B,0x2D3F,0x2E3F,0x2F3B,0x3027,0x3123,
  0x3223,0x3327,0x3423,0x3527,0x3627,0x3723,0x382B,0x392F,
  0x3A3F,0x3B3B,0x3C3F,0x3D3B,0x3E3B,0x3F3F,0x4003,0x4107,
  0x4207,0x4303,0x4407,0x4503,0x4603,0x4707,0x480F,0x490B,
  0x4A1B,0x4B1F,0x4C1B,0x4D1F,0x4E1F,0x4F1B,0x5007,0x5103,
  0x5203,0x5307,0x5403,0x5507,0x5607,0x5703,0x580B,0x590F,
  0x5A1F,0x5B1B,0x5C1F,0x5D1B,0x5E1B,0x5F1F,0x6027,0x6123,
  0x6223,0x6327,0x6423,0x6527,0x6627,0x6723,0x682B,0x692F,
  0x6A3F,0x6B3B,0x6C3F,0x6D3B,0x6E3B,0x6F3F,0x7023,0x7127,
  0x7227,0x7323,0x7427,0x7523,0x7623,0x7727,0x782F,0x792B,
  0x7A3B,0x7B3F,0x7C3B,0x7D3F,0x7E3F,0x7F3B,0x8083,0x8187,
  0x8287,0x8383,0x8487,0x8583,0x8683,0x8787,0x888F,0x898B,
  0x8A9B,0x8B9F,0x8C9B,0x8D9F,0x8E9F,0x8F9B,0x9087,0x9183,
  0x9283,0x9387,0x9483,0x9587,0x9687,0x9783,0x988B,0x998F
};
