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
/**                           Z80.c                         **/
/**                                                         **/
/** This file contains implementation for Z80 CPU. Don't    **/
/** forget to provide RdZ80(), WrZ80(), InZ80(), OutZ80(),  **/
/** LoopZ80(), and PatchZ80() functions to accomodate the   **/
/** emulated machine's architecture.                        **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#include <nds.h>
#include "Z80.h"
#include "Tables.h"
#include <stdio.h>
#include "../../../printf.h"

extern Z80 CPU;

extern u32 debug[];
extern u32 DX,DY;
extern void EI_Enable(void);

extern u8  DAN_Zone0;
extern u8  DAN_Zone1;
extern u16 DAN_Config;

extern void ConfigureMemory(void);

#define INLINE static inline

/** System-Dependent Stuff ***********************************/
/** This is system-dependent code put here to speed things  **/
/** up. It has to stay inlined to be fast.                  **/
/*************************************************************/
extern u8 *MemoryMapR[4];
extern u8 *MemoryMapW[4];

typedef u8 (*patchFunc)(void);
#define PatchLookup ((patchFunc*)0x06860000)

// ------------------------------------------------------
// These defines and inline functions are to map maximum
// speed/efficiency onto the memory system we have.
// ------------------------------------------------------
extern unsigned char cpu_readport_ams(register unsigned short Port);
extern void cpu_writeport_ams(register unsigned short Port,register unsigned char Value);

// ------------------------------------------------------------------------------
// This is how we access the Z80 memory. We indirect through the MemoryMapR[] to 
// allow for easy mapping by the 128K machines. It's slightly slower than direct
// RAM access but much faster than having to move around chunks of bank memory.
// ------------------------------------------------------------------------------
inline byte OpZ80(word A)
{
    return MemoryMapR[(A)>>14][A];
}

#define RdZ80 OpZ80 // Nothing unique about a memory read - same as an OpZ80 opcode fetch

// -------------------------------------------------------------------------------------------
// The Amstrad CPC allows write-through to RAM even if a ROM/Cart/OS page is mapped in for
// reading... this means that any write will always make it to RAM and so this is trivial.
// -------------------------------------------------------------------------------------------
inline void WrZ80(word A, byte value)   {MemoryMapW[(A)>>14][A] = value;}

// -------------------------------------------------------------------
// And these two macros will give us access to the Z80 I/O ports...
// -------------------------------------------------------------------
#define OutZ80(P,V)     cpu_writeport_ams(P,V)
#define InZ80(P)        cpu_readport_ams(P)


/** Macros for use through the CPU subsystem */
#define S(Fl)        CPU.AF.B.l|=Fl
#define R(Fl)        CPU.AF.B.l&=~(Fl)
#define FLAGS(Rg,Fl) CPU.AF.B.l=Fl|ZSTable[Rg]
#define INCR(N)      CPU.R++       // Faster to just increment this odd 7-bit RAM Refresh counter here and mask off and OR the high bit back in when asked for in CodesED.h

#define M_RLC(Rg)      \
  CPU.AF.B.l=Rg>>7;Rg=(Rg<<1)|CPU.AF.B.l;CPU.AF.B.l|=PZSTable[Rg]
#define M_RRC(Rg)      \
  CPU.AF.B.l=Rg&0x01;Rg=(Rg>>1)|(CPU.AF.B.l<<7);CPU.AF.B.l|=PZSTable[Rg]
#define M_RL(Rg)       \
  if(Rg&0x80)          \
  {                    \
    Rg=(Rg<<1)|(CPU.AF.B.l&C_FLAG); \
    CPU.AF.B.l=PZSTable[Rg]|C_FLAG; \
  }                    \
  else                 \
  {                    \
    Rg=(Rg<<1)|(CPU.AF.B.l&C_FLAG); \
    CPU.AF.B.l=PZSTable[Rg];        \
  }
#define M_RR(Rg)       \
  if(Rg&0x01)          \
  {                    \
    Rg=(Rg>>1)|(CPU.AF.B.l<<7);     \
    CPU.AF.B.l=PZSTable[Rg]|C_FLAG; \
  }                    \
  else                 \
  {                    \
    Rg=(Rg>>1)|(CPU.AF.B.l<<7);     \
    CPU.AF.B.l=PZSTable[Rg];        \
  }

#define M_SLA(Rg)      \
  CPU.AF.B.l=Rg>>7;Rg<<=1;CPU.AF.B.l|=PZSTable[Rg]
#define M_SRA(Rg)      \
  CPU.AF.B.l=Rg&C_FLAG;Rg=(Rg>>1)|(Rg&0x80);CPU.AF.B.l|=PZSTable[Rg]

#define M_SLL(Rg)      \
  CPU.AF.B.l=Rg>>7;Rg=(Rg<<1)|0x01;CPU.AF.B.l|=PZSTable[Rg]
#define M_SRL(Rg)      \
  CPU.AF.B.l=Rg&0x01;Rg>>=1;CPU.AF.B.l|=PZSTable[Rg]

#define M_BIT(Bit,Rg)  \
  CPU.AF.B.l=(CPU.AF.B.l&C_FLAG)|PZSHTable_BIT[Rg&(1<<Bit)]

#define M_SET(Bit,Rg) Rg|=1<<Bit
#define M_RES(Bit,Rg) Rg&=~(1<<Bit)

#define M_POP(Rg)      \
  CPU.Rg.B.l=OpZ80(CPU.SP.W++);CPU.Rg.B.h=OpZ80(CPU.SP.W++)
#define M_PUSH(Rg)     \
  WrZ80(--CPU.SP.W,CPU.Rg.B.h);WrZ80(--CPU.SP.W,CPU.Rg.B.l)

#define M_CALL         \
  J.B.l=OpZ80(CPU.PC.W++);J.B.h=OpZ80(CPU.PC.W++);         \
  WrZ80(--CPU.SP.W,CPU.PC.B.h);WrZ80(--CPU.SP.W,CPU.PC.B.l); \
  CPU.PC.W=J.W; \
  JumpZ80(J.W)

#define M_JP  CPU.PC.W = (u32)OpZ80(CPU.PC.W) | ((u32)OpZ80(CPU.PC.W+1) << 8);
#define M_JR  CPU.PC.W+=(offset)OpZ80(CPU.PC.W)+1;JumpZ80(CPU.PC.W)
#define M_RET CPU.PC.B.l=OpZ80(CPU.SP.W++);CPU.PC.B.h=OpZ80(CPU.SP.W++);JumpZ80(CPU.PC.W)

#define M_RST(Ad)      \
  WrZ80(--CPU.SP.W,CPU.PC.B.h);WrZ80(--CPU.SP.W,CPU.PC.B.l);CPU.PC.W=Ad;JumpZ80(Ad)

#define M_LDWORD(Rg)   \
  CPU.Rg.B.l=OpZ80(CPU.PC.W++);CPU.Rg.B.h=OpZ80(CPU.PC.W++)

#define M_ADD(Rg)      \
  J.W=CPU.AF.B.h+Rg;    \
  CPU.AF.B.l=           \
    (~(CPU.AF.B.h^Rg)&(Rg^J.B.l)&0x80? V_FLAG:0)| \
    J.B.h|ZSTable[J.B.l]|                        \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG);               \
  CPU.AF.B.h=J.B.l

#define M_SUB(Rg)      \
  J.W=CPU.AF.B.h-Rg;    \
  CPU.AF.B.l=           \
    ((CPU.AF.B.h^Rg)&(CPU.AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|                      \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG);                     \
  CPU.AF.B.h=J.B.l

#define M_ADC(Rg)      \
  J.W=CPU.AF.B.h+Rg+(CPU.AF.B.l&C_FLAG); \
  CPU.AF.B.l=                           \
    (~(CPU.AF.B.h^Rg)&(Rg^J.B.l)&0x80? V_FLAG:0)| \
    J.B.h|ZSTable[J.B.l]|              \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG);     \
  CPU.AF.B.h=J.B.l

#define M_SBC(Rg)      \
  J.W=CPU.AF.B.h-Rg-(CPU.AF.B.l&C_FLAG); \
  CPU.AF.B.l=                           \
    ((CPU.AF.B.h^Rg)&(CPU.AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|      \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG);     \
  CPU.AF.B.h=J.B.l

#define M_CP(Rg)       \
  J.W=CPU.AF.B.h-Rg;    \
  CPU.AF.B.l=           \
    ((CPU.AF.B.h^Rg)&(CPU.AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|                      \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG)

#define M_AND(Rg) CPU.AF.B.h&=Rg;CPU.AF.B.l=H_FLAG|PZSTable[CPU.AF.B.h]
#define M_OR(Rg)  CPU.AF.B.h|=Rg;CPU.AF.B.l=PZSTable[CPU.AF.B.h]
#define M_XOR(Rg) CPU.AF.B.h^=Rg;CPU.AF.B.l=PZSTable[CPU.AF.B.h]

#define M_IN(Rg)        \
  Rg=InZ80(CPU.BC.W);  \
  CPU.AF.B.l=PZSTable[Rg]|(CPU.AF.B.l&C_FLAG)

#define M_INC(Rg)       \
  Rg++;                 \
  CPU.AF.B.l=(CPU.AF.B.l&C_FLAG)|ZSTable_INC[Rg];

#define M_DEC(Rg)       \
  Rg--;                 \
  CPU.AF.B.l= (CPU.AF.B.l&C_FLAG)|ZSTable_DEC[Rg];

#define M_ADDW(Rg1,Rg2) \
  J.W=(CPU.Rg1.W+CPU.Rg2.W)&0xFFFF;                        \
  CPU.AF.B.l=                                             \
    (CPU.AF.B.l&~(H_FLAG|N_FLAG|C_FLAG))|                 \
    ((CPU.Rg1.W^CPU.Rg2.W^J.W)&0x1000? H_FLAG:0)|          \
    (((long)CPU.Rg1.W+(long)CPU.Rg2.W)&0x10000? C_FLAG:0); \
  CPU.Rg1.W=J.W

#define M_ADCW(Rg)      \
  I=CPU.AF.B.l&C_FLAG;J.W=(CPU.HL.W+CPU.Rg.W+I)&0xFFFF;           \
  CPU.AF.B.l=                                                   \
    (((long)CPU.HL.W+(long)CPU.Rg.W+(long)I)&0x10000? C_FLAG:0)| \
    (~(CPU.HL.W^CPU.Rg.W)&(CPU.Rg.W^J.W)&0x8000? V_FLAG:0)|       \
    ((CPU.HL.W^CPU.Rg.W^J.W)&0x1000? H_FLAG:0)|                  \
    (J.W? 0:Z_FLAG)|(J.B.h&S_FLAG);                            \
  CPU.HL.W=J.W

#define M_SBCW(Rg)      \
  I=CPU.AF.B.l&C_FLAG;J.W=(CPU.HL.W-CPU.Rg.W-I)&0xFFFF;           \
  CPU.AF.B.l=                                                   \
    N_FLAG|                                                    \
    (((long)CPU.HL.W-(long)CPU.Rg.W-(long)I)&0x10000? C_FLAG:0)| \
    ((CPU.HL.W^CPU.Rg.W)&(CPU.HL.W^J.W)&0x8000? V_FLAG:0)|        \
    ((CPU.HL.W^CPU.Rg.W^J.W)&0x1000? H_FLAG:0)|                  \
    (J.W? 0:Z_FLAG)|(J.B.h&S_FLAG);                            \
  CPU.HL.W=J.W


enum Codes
{
  NOP,      LD_BC_WORD, LD_xBC_A,    INC_BC,     INC_B,      DEC_B,      LD_B_BYTE,   RLCA,       // 0x00
  EX_AF_AF, ADD_HL_BC,  LD_A_xBC,    DEC_BC,     INC_C,      DEC_C,      LD_C_BYTE,   RRCA,       // 0x08
  DJNZ,     LD_DE_WORD, LD_xDE_A,    INC_DE,     INC_D,      DEC_D,      LD_D_BYTE,   RLA,        // 0x10
  JR,       ADD_HL_DE,  LD_A_xDE,    DEC_DE,     INC_E,      DEC_E,      LD_E_BYTE,   RRA,        // 0x18
  JR_NZ,    LD_HL_WORD, LD_xWORD_HL, INC_HL,     INC_H,      DEC_H,      LD_H_BYTE,   DAA,        // 0x20
  JR_Z,     ADD_HL_HL,  LD_HL_xWORD, DEC_HL,     INC_L,      DEC_L,      LD_L_BYTE,   CPL,        // 0x28
  JR_NC,    LD_SP_WORD, LD_xWORD_A,  INC_SP,     INC_xHL,    DEC_xHL,    LD_xHL_BYTE, SCF,        // 0x30
  JR_C,     ADD_HL_SP,  LD_A_xWORD,  DEC_SP,     INC_A,      DEC_A,      LD_A_BYTE,   CCF,        // 0x38
  LD_B_B,   LD_B_C,     LD_B_D,      LD_B_E,     LD_B_H,     LD_B_L,     LD_B_xHL,    LD_B_A,     // 0x40
  LD_C_B,   LD_C_C,     LD_C_D,      LD_C_E,     LD_C_H,     LD_C_L,     LD_C_xHL,    LD_C_A,     // 0x48
  LD_D_B,   LD_D_C,     LD_D_D,      LD_D_E,     LD_D_H,     LD_D_L,     LD_D_xHL,    LD_D_A,     // 0x50
  LD_E_B,   LD_E_C,     LD_E_D,      LD_E_E,     LD_E_H,     LD_E_L,     LD_E_xHL,    LD_E_A,     // 0x58
  LD_H_B,   LD_H_C,     LD_H_D,      LD_H_E,     LD_H_H,     LD_H_L,     LD_H_xHL,    LD_H_A,     // 0x60
  LD_L_B,   LD_L_C,     LD_L_D,      LD_L_E,     LD_L_H,     LD_L_L,     LD_L_xHL,    LD_L_A,     // 0x68
  LD_xHL_B, LD_xHL_C,   LD_xHL_D,    LD_xHL_E,   LD_xHL_H,   LD_xHL_L,   HALT,        LD_xHL_A,   // 0x70
  LD_A_B,   LD_A_C,     LD_A_D,      LD_A_E,     LD_A_H,     LD_A_L,     LD_A_xHL,    LD_A_A,     // 0x78
  ADD_B,    ADD_C,      ADD_D,       ADD_E,      ADD_H,      ADD_L,      ADD_xHL,     ADD_A,      // 0x80
  ADC_B,    ADC_C,      ADC_D,       ADC_E,      ADC_H,      ADC_L,      ADC_xHL,     ADC_A,      // 0x88
  SUB_B,    SUB_C,      SUB_D,       SUB_E,      SUB_H,      SUB_L,      SUB_xHL,     SUB_A,      // 0x90
  SBC_B,    SBC_C,      SBC_D,       SBC_E,      SBC_H,      SBC_L,      SBC_xHL,     SBC_A,      // 0x98
  AND_B,    AND_C,      AND_D,       AND_E,      AND_H,      AND_L,      AND_xHL,     AND_A,      // 0xA0
  XOR_B,    XOR_C,      XOR_D,       XOR_E,      XOR_H,      XOR_L,      XOR_xHL,     XOR_A,      // 0xA8
  OR_B,     OR_C,       OR_D,        OR_E,       OR_H,       OR_L,       OR_xHL,      OR_A,       // 0xB0
  CP_B,     CP_C,       CP_D,        CP_E,       CP_H,       CP_L,       CP_xHL,      CP_A,       // 0xB8
  RET_NZ,   POP_BC,     JP_NZ,       JP,         CALL_NZ,    PUSH_BC,    ADD_BYTE,    RST00,      // 0xC0
  RET_Z,    RET,        JP_Z,        PFX_CB,     CALL_Z,     CALL,       ADC_BYTE,    RST08,      // 0xC8
  RET_NC,   POP_DE,     JP_NC,       OUTA,       CALL_NC,    PUSH_DE,    SUB_BYTE,    RST10,      // 0xD0
  RET_C,    EXX,        JP_C,        INA,        CALL_C,     PFX_DD,     SBC_BYTE,    RST18,      // 0xD8
  RET_PO,   POP_HL,     JP_PO,       EX_HL_xSP,  CALL_PO,    PUSH_HL,    AND_BYTE,    RST20,      // 0xE0
  RET_PE,   LD_PC_HL,   JP_PE,       EX_DE_HL,   CALL_PE,    PFX_ED,     XOR_BYTE,    RST28,      // 0xE8
  RET_P,    POP_AF,     JP_P,        DI,         CALL_P,     PUSH_AF,    OR_BYTE,     RST30,      // 0xF0
  RET_M,    LD_SP_HL,   JP_M,        EI,         CALL_M,     PFX_FD,     CP_BYTE,     RST38       // 0xF8
};

enum CodesCB
{
  RLC_B,    RLC_C,      RLC_D,      RLC_E,      RLC_H,      RLC_L,      RLC_xHL,    RLC_A,      // 0x00
  RRC_B,    RRC_C,      RRC_D,      RRC_E,      RRC_H,      RRC_L,      RRC_xHL,    RRC_A,
  RL_B,     RL_C,       RL_D,       RL_E,       RL_H,       RL_L,       RL_xHL,     RL_A,       // 0x10
  RR_B,     RR_C,       RR_D,       RR_E,       RR_H,       RR_L,       RR_xHL,     RR_A,
  SLA_B,    SLA_C,      SLA_D,      SLA_E,      SLA_H,      SLA_L,      SLA_xHL,    SLA_A,      // 0x20
  SRA_B,    SRA_C,      SRA_D,      SRA_E,      SRA_H,      SRA_L,      SRA_xHL,    SRA_A,
  SLL_B,    SLL_C,      SLL_D,      SLL_E,      SLL_H,      SLL_L,      SLL_xHL,    SLL_A,      // 0x30
  SRL_B,    SRL_C,      SRL_D,      SRL_E,      SRL_H,      SRL_L,      SRL_xHL,    SRL_A,
  BIT0_B,   BIT0_C,     BIT0_D,     BIT0_E,     BIT0_H,     BIT0_L,     BIT0_xHL,   BIT0_A,     // 0x40
  BIT1_B,   BIT1_C,     BIT1_D,     BIT1_E,     BIT1_H,     BIT1_L,     BIT1_xHL,   BIT1_A,
  BIT2_B,   BIT2_C,     BIT2_D,     BIT2_E,     BIT2_H,     BIT2_L,     BIT2_xHL,   BIT2_A,     // 0x50
  BIT3_B,   BIT3_C,     BIT3_D,     BIT3_E,     BIT3_H,     BIT3_L,     BIT3_xHL,   BIT3_A,
  BIT4_B,   BIT4_C,     BIT4_D,     BIT4_E,     BIT4_H,     BIT4_L,     BIT4_xHL,   BIT4_A,     // 0x60
  BIT5_B,   BIT5_C,     BIT5_D,     BIT5_E,     BIT5_H,     BIT5_L,     BIT5_xHL,   BIT5_A,
  BIT6_B,   BIT6_C,     BIT6_D,     BIT6_E,     BIT6_H,     BIT6_L,     BIT6_xHL,   BIT6_A,     // 0x70
  BIT7_B,   BIT7_C,     BIT7_D,     BIT7_E,     BIT7_H,     BIT7_L,     BIT7_xHL,   BIT7_A,
  RES0_B,   RES0_C,     RES0_D,     RES0_E,     RES0_H,     RES0_L,     RES0_xHL,   RES0_A,     // 0x80
  RES1_B,   RES1_C,     RES1_D,     RES1_E,     RES1_H,     RES1_L,     RES1_xHL,   RES1_A,
  RES2_B,   RES2_C,     RES2_D,     RES2_E,     RES2_H,     RES2_L,     RES2_xHL,   RES2_A,     // 0x90
  RES3_B,   RES3_C,     RES3_D,     RES3_E,     RES3_H,     RES3_L,     RES3_xHL,   RES3_A,
  RES4_B,   RES4_C,     RES4_D,     RES4_E,     RES4_H,     RES4_L,     RES4_xHL,   RES4_A,     // 0xA0
  RES5_B,   RES5_C,     RES5_D,     RES5_E,     RES5_H,     RES5_L,     RES5_xHL,   RES5_A,
  RES6_B,   RES6_C,     RES6_D,     RES6_E,     RES6_H,     RES6_L,     RES6_xHL,   RES6_A,     // 0xB0
  RES7_B,   RES7_C,     RES7_D,     RES7_E,     RES7_H,     RES7_L,     RES7_xHL,   RES7_A,
  SET0_B,   SET0_C,     SET0_D,     SET0_E,     SET0_H,     SET0_L,     SET0_xHL,   SET0_A,     // 0xC0
  SET1_B,   SET1_C,     SET1_D,     SET1_E,     SET1_H,     SET1_L,     SET1_xHL,   SET1_A,
  SET2_B,   SET2_C,     SET2_D,     SET2_E,     SET2_H,     SET2_L,     SET2_xHL,   SET2_A,     // 0xD0
  SET3_B,   SET3_C,     SET3_D,     SET3_E,     SET3_H,     SET3_L,     SET3_xHL,   SET3_A,
  SET4_B,   SET4_C,     SET4_D,     SET4_E,     SET4_H,     SET4_L,     SET4_xHL,   SET4_A,     // 0xE0
  SET5_B,   SET5_C,     SET5_D,     SET5_E,     SET5_H,     SET5_L,     SET5_xHL,   SET5_A,
  SET6_B,   SET6_C,     SET6_D,     SET6_E,     SET6_H,     SET6_L,     SET6_xHL,   SET6_A,     // 0xF0
  SET7_B,   SET7_C,     SET7_D,     SET7_E,     SET7_H,     SET7_L,     SET7_xHL,   SET7_A
};

enum CodesED
{
  DB_00,    DB_01,      DB_02,      DB_03,          DB_04,      DB_05,  DB_06,  DB_07,  // 0x00
  DB_08,    DB_09,      DB_0A,      DB_0B,          DB_0C,      DB_0D,  DB_0E,  DB_0F,
  DB_10,    DB_11,      DB_12,      DB_13,          DB_14,      DB_15,  DB_16,  DB_17,  // 0x10
  DB_18,    DB_19,      DB_1A,      DB_1B,          DB_1C,      DB_1D,  DB_1E,  DB_1F,
  DB_20,    DB_21,      DB_22,      DB_23,          DB_24,      DB_25,  DB_26,  DB_27,  // 0x20
  DB_28,    DB_29,      DB_2A,      DB_2B,          DB_2C,      DB_2D,  DB_2E,  DB_2F,
  DB_30,    DB_31,      DB_32,      DB_33,          DB_34,      DB_35,  DB_36,  DB_37,  // 0x30
  DB_38,    DB_39,      DB_3A,      DB_3B,          DB_3C,      DB_3D,  DB_3E,  DB_3F,
  IN_B_xC,  OUT_xC_B,   SBC_HL_BC,  LD_xWORDe_BC,   NEG,        RETN,   IM_0,   LD_I_A, // 0x40
  IN_C_xC,  OUT_xC_C,   ADC_HL_BC,  LD_BC_xWORDe,   DB_4C,      RETI,   DB_,    LD_R_A,
  IN_D_xC,  OUT_xC_D,   SBC_HL_DE,  LD_xWORDe_DE,   DB_54,      DB_55,  IM_1,   LD_A_I, // 0x50
  IN_E_xC,  OUT_xC_E,   ADC_HL_DE,  LD_DE_xWORDe,   DB_5C,      DB_5D,  IM_2,   LD_A_R,
  IN_H_xC,  OUT_xC_H,   SBC_HL_HL,  LD_xWORDe_HL,   DB_64,      DB_65,  DB_66,  RRD,    // 0x60
  IN_L_xC,  OUT_xC_L,   ADC_HL_HL,  LD_HL_xWORDe,   DB_6C,      DB_6D,  DB_6E,  RLD,
  IN_F_xC,  OUT_xC_F,   SBC_HL_SP,  LD_xWORDe_SP,   DB_74,      DB_75,  DB_76,  DB_77,  // 0x70
  IN_A_xC,  OUT_xC_A,   ADC_HL_SP,  LD_SP_xWORDe,   DB_7C,      DB_7D,  DB_7E,  DB_7F,
  DB_80,    DB_81,      DB_82,      DB_83,          DB_84,      DB_85,  DB_86,  DB_87,  // 0x80
  DB_88,    DB_89,      DB_8A,      DB_8B,          DB_8C,      DB_8D,  DB_8E,  DB_8F,
  DB_90,    DB_91,      DB_92,      DB_93,          DB_94,      DB_95,  DB_96,  DB_97,  // 0x90
  DB_98,    DB_99,      DB_9A,      DB_9B,          DB_9C,      DB_9D,  DB_9E,  DB_9F,
  LDI,      CPI,        INI,        OUTI,           DB_A4,      DB_A5,  DB_A6,  DB_A7,  // 0xA0
  LDD,      CPD,        IND,        OUTD,           DB_AC,      DB_AD,  DB_AE,  DB_AF,
  LDIR,     CPIR,       INIR,       OTIR,           DB_B4,      DB_B5,  DB_B6,  DB_B7,  // 0xB0
  LDDR,     CPDR,       INDR,       OTDR,           DB_BC,      DB_BD,  DB_BE,  DB_BF,
  DB_C0,    DB_C1,      DB_C2,      DB_C3,          DB_C4,      DB_C5,  DB_C6,  DB_C7,  // 0xC0
  DB_C8,    DB_C9,      DB_CA,      DB_CB,          DB_CC,      DB_CD,  DB_CE,  DB_CF,
  DB_D0,    DB_D1,      DB_D2,      DB_D3,          DB_D4,      DB_D5,  DB_D6,  DB_D7,  // 0xD0
  DB_D8,    DB_D9,      DB_DA,      DB_DB,          DB_DC,      DB_DD,  DB_DE,  DB_DF,
  DB_E0,    DB_E1,      DB_E2,      DB_E3,          DB_E4,      DB_E5,  DB_E6,  DB_E7,  // 0xE0
  DB_E8,    DB_E9,      DB_EA,      DB_EB,          DB_EC,      DB_ED,  DB_EE,  DB_EF,
  DB_F0,    DB_F1,      DB_F2,      DB_F3,          DB_F4,      DB_F5,  DB_F6,  DB_F7,  // 0xF0
  DB_F8,    DB_F9,      DB_FA,      DB_FB,          DB_FC,      DB_FD,  DB_FE,  DB_FF
};

extern void Trap_Bad_Ops(char *, byte, word);

/** ResetZ80() ***********************************************/
/** This function can be used to reset the register struct  **/
/** before starting execution with Z80(). It sets the       **/
/** registers to their supposed initial values.             **/
/*************************************************************/
void ResetZ80(Z80 *R)
{
  CPU.PC.W       = 0x0000;
  CPU.SP.W       = 0xF000;
  CPU.AF.W       = 0x0000;
  CPU.BC.W       = 0x0000;
  CPU.DE.W       = 0x0000;
  CPU.HL.W       = 0x0000;
  CPU.AF1.W      = 0x0000;
  CPU.BC1.W      = 0x0000;
  CPU.DE1.W      = 0x0000;
  CPU.HL1.W      = 0x0000;
  CPU.IX.W       = 0x0000;
  CPU.IY.W       = 0x0000;
  CPU.I          = 0x00;
  CPU.R          = 0x00;
  CPU.R_HighBit  = 0x00;
  CPU.IFF        = 0x00;
  CPU.IRequest   = INT_NONE;
  CPU.EI_Delay   = 0;
  CPU.Trace      = 0;
  CPU.TrapBadOps = 1;
  CPU.IAutoReset = 1;
  CPU.TStates    = 0;

  JumpZ80(CPU.PC.W);
}


/** IntZ80() *************************************************/
/** This function will generate interrupt of given vector.  **/
/*************************************************************/
ITCM_CODE void IntZ80(Z80 *R,word Vector)
{
    /* If HALTed, take CPU off HALT instruction */
    if(CPU.IFF&IFF_HALT) { CPU.PC.W++;CPU.IFF&=~IFF_HALT; }

    if((CPU.IFF&IFF_1)||(Vector==INT_NMI))
    {
      CPU.TStates += (CPU.IFF&IFF_IM2 ? 20:8); // Time to acknowledge interrupt. 

      /* Save PC on stack */
      M_PUSH(PC);

      /* Automatically reset IRequest */
      CPU.IRequest = INT_NONE;

      /* If it is NMI... */
      if(Vector==INT_NMI)
      {
          /* Clear IFF1 */
          CPU.IFF&=~(IFF_1|IFF_EI);
          /* Jump to hardwired NMI vector */
          CPU.PC.W=0x0066;
          JumpZ80(0x0066);
          /* Done */
          return;
      }

      /* Further interrupts off */
      CPU.IFF&=~(IFF_1|IFF_2|IFF_EI);

      /* If in IM2 mode... */
      if(CPU.IFF&IFF_IM2)
      {
          /* Make up the vector address */
          Vector=(Vector&0xFF)|((word)(CPU.I)<<8);
          /* Read the vector */
          CPU.PC.B.l=RdZ80(Vector++);
          CPU.PC.B.h=RdZ80(Vector);
          
          JumpZ80(CPU.PC.W);

          /* Done */
          return;
      }

      /* If in IM1 mode, just jump to hardwired IRQ vector */
      if(CPU.IFF&IFF_IM1) { CPU.PC.W=0x0038; JumpZ80(0x0038); return; }

      /* If in IM0 mode... */

      /* Jump to a vector */
      switch(Vector)
      {
          case INT_RST00: CPU.PC.W=0x0000;JumpZ80(0x0000);break;
          case INT_RST08: CPU.PC.W=0x0008;JumpZ80(0x0008);break;
          case INT_RST10: CPU.PC.W=0x0010;JumpZ80(0x0010);break;
          case INT_RST18: CPU.PC.W=0x0018;JumpZ80(0x0018);break;
          case INT_RST20: CPU.PC.W=0x0020;JumpZ80(0x0020);break;
          case INT_RST28: CPU.PC.W=0x0028;JumpZ80(0x0028);break;
          case INT_RST30: CPU.PC.W=0x0030;JumpZ80(0x0030);break;
          case INT_RST38: CPU.PC.W=0x0038;JumpZ80(0x0038);break;
      }
    }
}


static void CodesCB(void)
{
  register byte I;

  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);
  CPU.TStates += CyclesCB[I];
  
  /* R register incremented on each M1 cycle */
  INCR(1);

  switch(I)
  {
#include "CodesCB.h"
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops(" CB ", I, CPU.PC.W-2);
  }
}

ITCM_CODE static void CodesDDCB(void)
{
  register pair J;
  register byte I;

#define XX IX
  /* Get offset, read opcode and count cycles */
  J.W=CPU.XX.W+(offset)OpZ80(CPU.PC.W++);
  I=OpZ80(CPU.PC.W++);
  CPU.TStates += CyclesXXCB[I];

  switch(I)
  {
#include "CodesXCB.h"
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops("DDCB", I, CPU.PC.W-4);
  }
#undef XX
}

ITCM_CODE static void CodesFDCB(void)
{
  register pair J;
  register byte I;

#define XX IY
  /* Get offset, read opcode and count cycles */
  J.W=CPU.XX.W+(offset)OpZ80(CPU.PC.W++);
  I=OpZ80(CPU.PC.W++);
  CPU.TStates += CyclesXXCB[I];

  switch(I)
  {
#include "CodesXCB.h"
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops("FDCB", I, CPU.PC.W-4);
  }
#undef XX
}

ITCM_CODE static void CodesED(void)
{
  register byte I;
  register pair J;

  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);
  CPU.TStates += CyclesED[I];

  /* R register incremented on each M1 cycle */
  INCR(1);

  switch(I)
  {
#include "CodesED.h"
    case PFX_ED:
      CPU.TStates += 4;
      CPU.PC.W--;break;
    default:
      if(CPU.TrapBadOps) Trap_Bad_Ops(" ED ", I, CPU.PC.W-4);
  }
}

static void CodesDD(void)
{
  register byte I;
  register pair J;

#define XX IX
  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);
  CPU.TStates += CyclesXX[I];

  /* R register incremented on each M1 cycle */
  INCR(1);

  switch(I)
  {
#include "CodesXX.h"
    case PFX_FD:
    case PFX_DD:
      CPU.TStates += 4;
      CPU.PC.W--;break;
    case PFX_CB:
      CodesDDCB();break;
    default:
      if(CPU.TrapBadOps)  Trap_Bad_Ops(" DD ", I, CPU.PC.W-2);
  }
#undef XX
}

static void CodesFD(void)
{
  register byte I;
  register pair J;

#define XX IY
  /* Read opcode and count cycles */
  I=OpZ80(CPU.PC.W++);
  CPU.TStates += CyclesXX[I];

  /* R register incremented on each M1 cycle */
  INCR(1);
  
  switch(I)
  {
#include "CodesXX.h"
    case PFX_FD:
        CPU.TStates += 4;
        if (RdZ80(CPU.PC.W) == 0x70) // LD (IY+nn),B - Zone 0 command
        {
            DAN_Zone0 = CPU.BC.B.h;
            ConfigureMemory();
        }        
        if (RdZ80(CPU.PC.W) == 0x71) // LD (IY+nn),C - Zone 1 command
        {
            DAN_Zone1 = CPU.BC.B.l;
            ConfigureMemory();
        }        
        if (RdZ80(CPU.PC.W) == 0x77) // LD (IY+nn),A - Config command
        {
            if (CPU.AF.B.h & 0x80) // High bit is the Dandanator Configuration
            {
                DAN_Config = CPU.AF.B.h;
                ConfigureMemory();
            }
        }        
        CPU.PC.W--;break;
    case PFX_DD:
      CPU.TStates += 4;
      CPU.PC.W--;break;
    case PFX_CB:
      CodesFDCB();break;
    default:
        if(CPU.TrapBadOps)  Trap_Bad_Ops(" FD ", I, CPU.PC.W-2);
  }
#undef XX
}

// ------------------------------------------------------------------------------
// Almost 15K wasted space... but needed so we can keep the Enable Interrupt
// instruction out of the main fast Z80 instruction loop. When the EI instruction
// is issued, the interrupts are not enabled until one instruction later. This
// function let's us execute that one instruction - somewhat more slowly but 
// interrupts are enabled very infrequently (often enabled and left that way).
// ------------------------------------------------------------------------------
void ExecOneInstruction(void)
{
  register byte I;
  register pair J;

  I=OpZ80(CPU.PC.W++);
  CPU.TStates += Cycles[I];

  /* R register incremented on each M1 cycle */
  INCR(1);

  /* Interpret opcode */
  switch(I)
  {
#include "Codes.h"
    case PFX_CB: CodesCB();break;
    case PFX_ED: CodesED();break;
    case PFX_FD: CodesFD();break;
    case PFX_DD: CodesDD();break;
  }
}
  
// ------------------------------------------------------------------------
// The Enable Interrupt is delayed 1 M1 instruction. The Amstrad CPC Gate
// Array will hold onto the interrupt indefinitely so we can fire it here...
// ------------------------------------------------------------------------
void EI_Enable(void)
{
   ExecOneInstruction(); 
   CPU.IFF=(CPU.IFF&~IFF_EI)|IFF_1;
   if (CPU.IRequest != INT_NONE)
   {
       extern u32 R52;
       R52 &= 0xDF;                 // Clear bit 5 of R52
       IntZ80(&CPU, CPU.IRequest);  // Fire the interrupt
   }
}

// --------------------------------------------------------------------------------------------
// The main Z80 instruction loop. We put this 15K chunk into fast memory as we want to make
// the Z80 run as quickly as possible - this, along with the CRTC, is the heart of the system.
// --------------------------------------------------------------------------------------------
ITCM_CODE void ExecZ80(u32 RunToCycles)
{
  register byte I;
  register pair J;

  while (CPU.TStates < RunToCycles)
  {
      I=OpZ80(CPU.PC.W++);
      CPU.TStates += Cycles[I];

      /* R register incremented on each M1 cycle */
      INCR(1);

      /* Interpret opcode */
      switch(I)
      {
#include "Codes.h"
        case PFX_CB: CodesCB();break;
        case PFX_ED: CodesED();break;
        case PFX_FD: CodesFD();break;
        case PFX_DD: CodesDD();break;
      }
  }
}
