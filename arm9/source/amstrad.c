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

u32 last_file_size      __attribute__((section(".dtcm"))) = 0;
u32 scanline_count      __attribute__((section(".dtcm"))) = 1;

u8 MMR                  __attribute__((section(".dtcm"))) = 0x00;
u8 RMR                  __attribute__((section(".dtcm"))) = 0x00;
u8 PENR                 __attribute__((section(".dtcm"))) = 0x00;
u8 UROM                 __attribute__((section(".dtcm"))) = 0x00;

u8 INK[17]              __attribute__((section(".dtcm"))) = {0};
u32 border_color        __attribute__((section(".dtcm"))) = 0;
u8 CRTC[0x20]           __attribute__((section(".dtcm"))) = {0};
u8 CRT_Idx              __attribute__((section(".dtcm"))) = 0;
u8 inks_changed         __attribute__((section(".dtcm"))) = 0;

u8 ink_map[256]         __attribute__((section(".dtcm"))) = {0};

u32  pre_inked_mode0[256] __attribute__((section(".dtcm"))) = {0};
u32  pre_inked_mode1[256] __attribute__((section(".dtcm"))) = {0};
u32  pre_inked_mode2a[256]  = {0};
u32  pre_inked_mode2b[256]  = {0};

extern u8 last_special_key;

ITCM_CODE void compute_pre_inked(u8 mode)
{
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
            
            // We don't have enough DS LCD display resolution anyway... so render the best we can...
            pre_inked_mode2a[pixel] = (pixel7 << 24) | (pixel5 << 16) | (pixel3 << 8) | (pixel1 << 0);
            pre_inked_mode2b[pixel] = (pixel6 << 24) | (pixel4 << 16) | (pixel2 << 8) | (pixel0 << 0);
        }
    }
    else // Mode 0
    {
        for (int pixel=0; pixel < 256; pixel++)
        {
            u8 pixel0 = INK[((pixel & 0x80) >> 7) | ((pixel & 0x20) >> 3) | ((pixel & 0x08) >> 2) | ((pixel & 0x02) << 2)];
            u8 pixel1 = INK[((pixel & 0x40) >> 6) | ((pixel & 0x10) >> 2) | ((pixel & 0x04) >> 1) | ((pixel & 0x01) << 3)];
            pre_inked_mode0[pixel] = (pixel1 << 24) | (pixel1 << 16) | (pixel0 << 8) | (pixel0 << 0);
        }
    }
}

// -Address-     0      1      2      3      4      5      6      7
// 0000-3FFF   RAM_0  RAM_0  RAM_4  RAM_0  RAM_0  RAM_0  RAM_0  RAM_0
// 4000-7FFF   RAM_1  RAM_1  RAM_5  RAM_3  RAM_4  RAM_5  RAM_6  RAM_7
// 8000-BFFF   RAM_2  RAM_2  RAM_6  RAM_2  RAM_2  RAM_2  RAM_2  RAM_2
// C000-FFFF   RAM_3  RAM_7  RAM_7  RAM_7  RAM_3  RAM_3  RAM_3  RAM_3
void ConfigureMemory(void)
{
    u8 *UROM_Ptr = (u8 *)BASIC_6128;
    if (UROM == 7) // AMSDOS or PARADOS (Disk ROM)
    {
        if (myGlobalConfig.diskROM)
            UROM_Ptr = (u8 *)PARADOS;
        else 
            UROM_Ptr = (u8 *)AMSDOS;
    }
    
    switch (MMR & 0x7)
    {
      case 0x00: // 0-1-2-3
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : OS_6128;    // Lower ROM
        MemoryMapR[1] = RAM_Memory + 0x4000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (RAM_Memory + 0xC000) : UROM_Ptr;   // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = RAM_Memory + 0x4000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = RAM_Memory + 0xC000;
        break;

      case 0x01: // 0-1-2-7
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : OS_6128;    // Lower ROM
        MemoryMapR[1] = RAM_Memory + 0x4000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (RAM_Memory + 0x1C000) : UROM_Ptr;  // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = RAM_Memory + 0x4000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = RAM_Memory + 0x1C000;
        break;

      case 0x02: // 4-5-6-7
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x10000) : OS_6128;    // Lower ROM
        MemoryMapR[1] = RAM_Memory + 0x14000;
        MemoryMapR[2] = RAM_Memory + 0x18000;
        MemoryMapR[3] = (RMR & 0x08) ? (RAM_Memory + 0x1C000) : UROM_Ptr;   // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x10000;
        MemoryMapW[1] = RAM_Memory + 0x14000;
        MemoryMapW[2] = RAM_Memory + 0x18000;
        MemoryMapW[3] = RAM_Memory + 0x1C000;
        break;

      case 0x03: // 0-3-2-7
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : OS_6128;    // Lower ROM
        MemoryMapR[1] = RAM_Memory + 0xC000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (RAM_Memory + 0x1C000) : UROM_Ptr;  // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = RAM_Memory + 0xC000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = RAM_Memory + 0x1C000;
        break;

      case 0x04: // 0-4-2-3
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : OS_6128;    // Lower ROM
        MemoryMapR[1] = RAM_Memory + 0x10000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (RAM_Memory + 0xC000) : UROM_Ptr;   // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = RAM_Memory + 0x10000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = RAM_Memory + 0xC000;
        break;

      case 0x05: // 0-5-2-3
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : OS_6128;    // Lower ROM
        MemoryMapR[1] = RAM_Memory + 0x14000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (RAM_Memory + 0xC000) : UROM_Ptr;   // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = RAM_Memory + 0x14000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = RAM_Memory + 0xC000;
        break;
        
      case 0x06: // 0-6-2-3
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : OS_6128;    // Lower ROM
        MemoryMapR[1] = RAM_Memory + 0x18000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (RAM_Memory + 0xC000) : UROM_Ptr;   // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = RAM_Memory + 0x18000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = RAM_Memory + 0xC000;
        break;

      case 0x07: // 0-7-2-3
        MemoryMapR[0] = (RMR & 0x04) ? (RAM_Memory + 0x0000) : OS_6128;    // Lower ROM
        MemoryMapR[1] = RAM_Memory + 0x1C000;
        MemoryMapR[2] = RAM_Memory + 0x8000;
        MemoryMapR[3] = (RMR & 0x08) ? (RAM_Memory + 0xC000) : UROM_Ptr;   // Upper ROM

        MemoryMapW[0] = RAM_Memory + 0x0000;
        MemoryMapW[1] = RAM_Memory + 0x1C000;
        MemoryMapW[2] = RAM_Memory + 0x8000;
        MemoryMapW[3] = RAM_Memory + 0xC000;
        break;
    }
    
    // Offset so lookup is faster in Z80 core
    MemoryMapR[1] -= 0x4000;
    MemoryMapR[2] -= 0x8000;
    MemoryMapR[3] -= 0xC000;

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
                                if (kbd_key == KBD_KEY_F3)  keyBits |= 0x20;
                                break;

                            case 0x01:
                                if (kbd_key == KBD_KEY_F1)  keyBits |= 0x20;
                                if (kbd_key == KBD_KEY_F2)  keyBits |= 0x40;
                                break;
                                
                            case 0x02:
                                if (kbd_key == KBD_KEY_CTL) keyBits |= 0x80;
                                if (kbd_key == KBD_KEY_SFT) keyBits |= 0x20;
                                if (kbd_key == ']')         keyBits |= 0x08;
                                if (kbd_key == KBD_KEY_RET) keyBits |= 0x04;
                                if (kbd_key == '[')         keyBits |= 0x02;
                                
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
                                if (kbd_key == KBD_KEY_CLR) keyBits |= 0x80;
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


//#BCXX	%x0xxxx00 xxxxxxxx	6845 CRTC Index - Write
//#BDXX	%x0xxxx01 xxxxxxxx	6845 CRTC Data Out - Write
//#BEXX	%x0xxxx10 xxxxxxxx	6845 CRTC Status (as far as supported) - Read
//#BFXX	%x0xxxx11 xxxxxxxx	6845 CRTC Data In (as far as supported) - Read

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
                }                break;
                
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
                break;
            }
    }
    
    if (!(Port & 0x4000)) // CRTC
    {
        switch ((Port >> 8) & 0x03)
        {
            case 0x00:
                CRT_Idx = Value & 0xF;
                break;
                
            case 0x01:
                CRTC[CRT_Idx] = Value;
                if ((CRT_Idx == 7) && (Value == 0))
                {
                    crtc_rupture = 1; // RUPTURE!
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
                    debug[14] = 0x3333;
                }
                ConfigureMemory();
                break;
                
            case 0x03:  // MMR - RAM Memory Mapping
                MMR = Value;
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
    ResetFDC();

    MMR = 0x00;
    RMR = 0x00;
    UROM = 0x00;
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
    
    crtc_reset();
    
    memset(RAM_Memory, 0x00, sizeof(RAM_Memory));
    
    CPU.PC.W            = 0x0000;   // Z80 entry point
    CPU.SP.W            = 0x0000;   // Z80 entry point
    portA               = 0x00;
    portB               = 0xFF;     // 50Hz, Amstrad, VSync set
    portC               = 0x00;
    
    scanline_count = 1;
    
    if (amstrad_mode == MODE_DSK)
    {
        // The current Color Palette
        for (int ink=0; ink<17; ink++)
        {
           INK[ink] = ink_map[0];
        }
        
        border_color = (INK[16] << 24) | (INK[16] << 16) | (INK[16] << 8) | (INK[16] << 0);
        
        compute_pre_inked(0);
        compute_pre_inked(1);
        compute_pre_inked(2);
        
        DiskInsert(initial_file, false);
    }
    else // Must be SNA snapshot 
    {   
        // ------------------------------------------
        // For now, just assume .SNA snapshot format.
        // ------------------------------------------
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
            
            // And now look for MEM1 in case it's present...
            romPtr += comp_size;
            if ((romPtr[0] == 'M') && (romPtr[1] == 'E') && (romPtr[2] == 'M') && (romPtr[3] == '1'))
            {
                romPtr += 4;
                u32 ram_idx = 0x10000;
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
            }        
        }
    }
    
    return;
}


// -----------------------------------------------------------------------------
// Run the emulation for exactly 1 scanline and handle the VDP interrupt if
// the emulation has executed the last line of the frame.
//
// We also process 1 line worth of Audio and render one screen line for
// the CRTC display controller.
// -----------------------------------------------------------------------------
s16 CPU_ADJUST[] __attribute__((section(".dtcm"))) = {0, 4, 8, -4, -8};
ITCM_CODE u32 amstrad_run(void)
{
    // Process 1 scanline worth of AY audio
    if (myConfig.waveDirect) processDirectAudio();
    
    // Execute 1 scanline worth of CPU instructions ... Z80 at 4MHz
    ExecZ80((256+CPU_ADJUST[myConfig.cpuAdjust]) * scanline_count);

    if (inks_changed)
    {
        compute_pre_inked(RMR & 0x03);
        inks_changed = 0;
    }

    // Process 1 scanline for the mighty CRTC controller chip
    if (crtc_render_screen_line()) // Will return non-zero if VSYNC started
    {
        static int refresh_tstates=0;
        
        if (++refresh_tstates & 0x100) // Every 256 Frames
        {
            CPU.TStates = CPU.TStates - ((256+CPU_ADJUST[myConfig.cpuAdjust]) * scanline_count);
            scanline_count = 1;
        }
        return 0; // End of frame (start of VSYNC)
    }
    else
    {
        scanline_count++;
    }

    return 1; // Not end of frame
}

// End of file
