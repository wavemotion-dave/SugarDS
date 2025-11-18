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

#include <stdlib.h>
#include <stdio.h>
#include <fat.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <maxmod9.h>

#include "SugarDS.h"
#include "AmsUtils.h"
#include "topscreen.h"
#include "mainmenu.h"
#include "soundbank.h"
#include "splash.h"
#include "splash_top.h"
#include "printf.h"

#include "CRC32.h"
#include "printf.h"

int         countFiles=0;
int         ucGameAct=0;
int         ucGameChoice = -1;
FIAmstrad   gpFic[MAX_FILES];
char        szName[256];
char        szFile[256];
u32         file_size = 0;
char        strBuf[40];
u8          bShowInstructions = 0;

struct Config_t AllConfigs[MAX_CONFIGS];
struct Config_t myConfig __attribute((aligned(4))) __attribute__((section(".dtcm")));
struct GlobalConfig_t myGlobalConfig;
extern u32 file_crc;

#define MAX_FAVS  1024

typedef struct
{
  u32   name_hash;  // Repurpose the lower bit for love vs like
} Favorites_t;

Favorites_t myFavs[MAX_FAVS]; // Total of 4K of space with 32 bit hash


// -----------------------------------------------------------------------
// Used by our system to map into 8K memory chunks which allows for very
// rapid banking of memory - very useful for the CPC with 128K of memory.
// -----------------------------------------------------------------------
u8 *MemoryMapR[4]        __attribute__((section(".dtcm"))) = {0,0,0,0};
u8 *MemoryMapW[4]        __attribute__((section(".dtcm"))) = {0,0,0,0};

// ------------------------------------------------------------------------
// The Z80 Processor! Put the entire CPU state into fast memory for speed!
// ------------------------------------------------------------------------
Z80 CPU __attribute__((section(".dtcm")));

u32 file_crc __attribute__((section(".dtcm")))  = 0x00000000;  // Our global file CRC32 to uniquiely identify this game

// -----------------------------------------------------------
// The AY sound chip is used for the Amstrad CPC machines
// -----------------------------------------------------------
AY38910 myAY   __attribute__((section(".dtcm")));

u16 JoyState   __attribute__((section(".dtcm"))) = 0;           // Joystick State and Key Bits

u8 option_table=0;

const char szKeyName[MAX_KEY_OPTIONS][17] = {
  "JOY UP",
  "JOY DOWN",
  "JOY LEFT",
  "JOY RIGHT",
  "JOY FIRE",
  "JOY FIRE 2",
  "JOY FIRE 3",

  "KEYBOARD A", //7
  "KEYBOARD B",
  "KEYBOARD C",
  "KEYBOARD D",
  "KEYBOARD E",
  "KEYBOARD F",
  "KEYBOARD G",
  "KEYBOARD H",
  "KEYBOARD I",
  "KEYBOARD J",
  "KEYBOARD K",
  "KEYBOARD L",
  "KEYBOARD M",
  "KEYBOARD N",
  "KEYBOARD O",
  "KEYBOARD P",
  "KEYBOARD Q",
  "KEYBOARD R",
  "KEYBOARD S",
  "KEYBOARD T",
  "KEYBOARD U",
  "KEYBOARD V",
  "KEYBOARD W",
  "KEYBOARD X",
  "KEYBOARD Y",
  "KEYBOARD Z", // 32

  "KEYBOARD 1", // 33
  "KEYBOARD 2",
  "KEYBOARD 3",
  "KEYBOARD 4",
  "KEYBOARD 5",
  "KEYBOARD 6",
  "KEYBOARD 7",
  "KEYBOARD 8",
  "KEYBOARD 9",
  "KEYBOARD 0", // 42

  "KEYBOARD SHIFT",
  "KEYBOARD CONTROL",
  "KEYBOARD CAPLOCK",
  "KEYBOARD SPACE",
  "KEYBOARD RETURN",
  "KEYBOARD PERIOD",
  "KEYBOARD COMMA",
  "KEYBOARD COLON",
  "KEYBOARD SEMI",
  "KEYBOARD ATSIGN",
  "KEYBOARD SLASH",
  "KEYBOARD BACKSL",
  "KEYBOARD ESC",
  "KEYBOARD [",
  "KEYBOARD ]",
  "KEYBOARD -",
  "KEYBOARD ^",
  "KEYBOARD RES1",
  "KEYBOARD RES2",
  "KEYBOARD F0",
  "KEYBOARD F1",
  "KEYBOARD F2",
  "KEYBOARD F3",
  "KEYBOARD F4",
  "KEYBOARD F5",
  "KEYBOARD F6",
  "KEYBOARD F7",
  "KEYBOARD F8",
  "KEYBOARD F9",
  "KEYBOARD F-DOT",
  "KEYBOARD F-ENTER",

  "CURSOR UP",
  "CURSOR DOWN",
  "CURSOR LEFT",
  "CURSOR RIGHT",
  "CURSOR COPY",

  "PAN UP 16",
  "PAN UP 24",
  "PAN UP 32",
  "PAN UP 48",
  "PAN UP 64",

  "PAN DN 16",
  "PAN DN 24",
  "PAN DN 32",
  "PAN DN 48",
  "PAN DN 64",

  "OFFSET 16",
  "OFFSET 32",
  "OFFSET 48",
  "OFFSET 64",
};


/*********************************************************************************
 * Show A message with YES / NO
 ********************************************************************************/
u8 showMessage(char *szCh1, char *szCh2)
{
  u16 iTx, iTy;
  u8 uRet=ID_SHM_CANCEL;
  u8 ucGau=0x00, ucDro=0x00,ucGauS=0x00, ucDroS=0x00, ucCho = ID_SHM_YES;

  BottomScreenOptions();

  DSPrint(16-strlen(szCh1)/2,10,6,szCh1);
  DSPrint(16-strlen(szCh2)/2,12,6,szCh2);
  DSPrint(8,14,6,("> YES <"));
  DSPrint(20,14,6,("  NO   "));
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  while (uRet == ID_SHM_CANCEL)
  {
    WAITVBL;
    if (keysCurrent() & KEY_TOUCH) {
      touchPosition touch;
      touchRead(&touch);
      iTx = touch.px;
      iTy = touch.py;
      if ( (iTx>8*8) && (iTx<8*8+7*8) && (iTy>14*8-4) && (iTy<15*8+4) ) {
        if (!ucGauS) {
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
          ucGauS = 1;
          if (ucCho == ID_SHM_YES) {
            uRet = ucCho;
          }
          else {
            ucCho  = ID_SHM_YES;
          }
        }
      }
      else
        ucGauS = 0;
      if ( (iTx>20*8) && (iTx<20*8+7*8) && (iTy>14*8-4) && (iTy<15*8+4) ) {
        if (!ucDroS) {
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
          ucDroS = 1;
          if (ucCho == ID_SHM_NO) {
            uRet = ucCho;
          }
          else {
            ucCho = ID_SHM_NO;
          }
        }
      }
      else
        ucDroS = 0;
    }
    else {
      ucDroS = 0;
      ucGauS = 0;
    }

    if (keysCurrent() & KEY_LEFT){
      if (!ucGau) {
        ucGau = 1;
        if (ucCho == ID_SHM_YES) {
          ucCho = ID_SHM_NO;
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
        }
        WAITVBL;
      }
    }
    else {
      ucGau = 0;
    }
    if (keysCurrent() & KEY_RIGHT) {
      if (!ucDro) {
        ucDro = 1;
        if (ucCho == ID_SHM_YES) {
          ucCho  = ID_SHM_NO;
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
        }
        WAITVBL;
      }
    }
    else {
      ucDro = 0;
    }
    if (keysCurrent() & KEY_A) {
      uRet = ucCho;
    }
  }
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  BottomScreenKeyboard();

  return uRet;
}

// --------------------------------------------------------------
// Provide an array of filename hashes to store game "Favorites"
// --------------------------------------------------------------
void LoadFavorites(void)
{
    memset(myFavs, 0x00, sizeof(myFavs));
    FILE *fp = fopen("/data/SugarDS.fav", "rb");
    if (fp)
    {
        fread(&myFavs, sizeof(myFavs), 1, fp);
        fclose(fp);
    }
}

void SaveFavorites(void)
{
    // --------------------------------------------------
    // Now save the config file out o the SD card...
    // --------------------------------------------------
    DIR* dir = opendir("/data");
    if (dir)
    {
        closedir(dir);  // directory exists.
    }
    else
    {
        mkdir("/data", 0777);   // Doesn't exist - make it...
    }

    FILE *fp = fopen("/data/SugarDS.fav", "wb");
    if (fp)
    {
        fwrite(&myFavs, sizeof(myFavs), 1, fp);
        fclose(fp);
    }
}

u8 IsFavorite(char *name)
{
    u32 filename_crc32 = getCRC32((u8 *)name, strlen(name));

    for (int i=0; i<MAX_FAVS; i++)
    {
        if ((myFavs[i].name_hash & 0xFFFFFFFE) == (filename_crc32 & 0xFFFFFFFE)) return (1 + (myFavs[i].name_hash&1));
    }
    return 0;
}

void ToggleFavorite(char *name)
{
    int firstZero = 0;
    u32 filename_crc32 = getCRC32((u8 *)name, strlen(name));

    for (int i=0; i<MAX_FAVS; i++)
    {
        // We use the lower bit of the filename hash (CRC32) as the flag for 'like' vs 'love'
        // Basically there are 3 states:
        //    - No hash found... not a favorite
        //    - Hash found with lower bit 0... Love
        //    - Hash found with lower bit 1... Like
        if ((myFavs[i].name_hash & 0xFFFFFFFE) == (filename_crc32 & 0xFFFFFFFE))
        {
            if ((myFavs[i].name_hash & 1) == 0)
            {
                myFavs[i].name_hash |= 1;
                return;
            }
            else
            {
                myFavs[i].name_hash = 0x00000000;
                return;
            }
        }

        if (myFavs[i].name_hash == 0x00000000)
        {
            if (!firstZero) firstZero = i;
        }
    }

    myFavs[firstZero].name_hash = (filename_crc32 & 0xFFFFFFFE);
}

/*********************************************************************************
 * Show The 14 games on the list to allow the user to choose a new game.
 ********************************************************************************/
static char szName2[40];
void dsDisplayFiles(u16 NoDebGame, u8 ucSel)
{
  u16 ucBcl,ucGame;
  u8 maxLen;

  DSPrint(31,6,0,(NoDebGame>0 ? "<" : " "));
  DSPrint(31,23,0,(NoDebGame+14<countFiles ? ">" : " "));

  for (ucBcl=0;ucBcl<18; ucBcl++)
  {
    ucGame= ucBcl+NoDebGame;
    if (ucGame < countFiles)
    {
      maxLen=strlen(gpFic[ucGame].szName);
      strcpy(szName,gpFic[ucGame].szName);
      if (maxLen>30) szName[30]='\0';
      if (gpFic[ucGame].uType == DIRECTORY)
      {
        szName[28] = 0; // Needs to be 2 chars shorter with brackets
        sprintf(szName2, "[%s]",szName);
        sprintf(szName,"%-30s",szName2);
        DSPrint(1,6+ucBcl,(ucSel == ucBcl ? 2 :  0),szName);
      }
      else
      {
        sprintf(szName,"%-30s",strupr(szName));
        DSPrint(1,6+ucBcl,(ucSel == ucBcl ? 2 : 0 ),szName);
        if (IsFavorite(gpFic[ucGame].szName))
        {
            DSPrint(0,6+ucBcl,(IsFavorite(gpFic[ucGame].szName) == 1) ? 0:2,(char*)"@");
        }
        else
        {
            DSPrint(0,6+ucBcl,0,(char*)" ");
        }
      }
    }
    else
    {
        DSPrint(0,6+ucBcl,(ucSel == ucBcl ? 2 : 0 ),"                               ");
    }
  }
}


// -------------------------------------------------------------------------
// Standard qsort routine for the games - we sort all directory
// listings first and then a case-insenstive sort of all games.
// -------------------------------------------------------------------------
int Filescmp (const void *c1, const void *c2)
{
  FIAmstrad *p1 = (FIAmstrad *) c1;
  FIAmstrad *p2 = (FIAmstrad *) c2;

  if (p1->szName[0] == '.' && p2->szName[0] != '.')
      return -1;
  if (p2->szName[0] == '.' && p1->szName[0] != '.')
      return 1;
  if ((p1->uType == DIRECTORY) && !(p2->uType == DIRECTORY))
      return -1;
  if ((p2->uType == DIRECTORY) && !(p1->uType == DIRECTORY))
      return 1;
  return strcasecmp (p1->szName, p2->szName);
}

/*********************************************************************************
 * Find files (DSK/CPR/SNA) available - sort them for display.
 ********************************************************************************/
void sugarDSFindFiles(void)
{
  u32 uNbFile;
  DIR *dir;
  struct dirent *pent;

  uNbFile=0;
  countFiles=0;

  dir = opendir(".");
  while (((pent=readdir(dir))!=NULL) && (uNbFile<MAX_FILES))
  {
    strcpy(szFile,pent->d_name);

    if(pent->d_type == DT_DIR)
    {
      if (!((szFile[0] == '.') && (strlen(szFile) == 1)))
      {
        // Do not include the [sav] and [pok] directories
        if ((strcasecmp(szFile, "sav") != 0) && (strcasecmp(szFile, "pok") != 0))
        {
            strcpy(gpFic[uNbFile].szName,szFile);
            gpFic[uNbFile].uType = DIRECTORY;
            uNbFile++;
            countFiles++;
        }
      }
    }
    else {
      if ((strlen(szFile)>4) && (strlen(szFile)<(MAX_FILENAME_LEN-4)) && (szFile[0] != '.') && (szFile[0] != '_'))  // For MAC don't allow files starting with an underscore
      {
        if ( (strcasecmp(strrchr(szFile, '.'), ".sna") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = AMSTRAD_FILE;
          uNbFile++;
          countFiles++;
        }

        if ( (strcasecmp(strrchr(szFile, '.'), ".cpr") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = AMSTRAD_FILE;
          uNbFile++;
          countFiles++;
        }

        if ( (strcasecmp(strrchr(szFile, '.'), ".dan") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = AMSTRAD_FILE;
          uNbFile++;
          countFiles++;
        }

        if ( (strcasecmp(strrchr(szFile, '.'), ".dsk") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = AMSTRAD_FILE;
          uNbFile++;
          countFiles++;
        }
      }
    }
  }
  closedir(dir);

  // ----------------------------------------------
  // If we found any files, go sort the list...
  // ----------------------------------------------
  if (countFiles)
  {
    qsort (gpFic, countFiles, sizeof(FIAmstrad), Filescmp);
  }
}

// ----------------------------------------------------------------------
// Let the user select a new game (.dsk, .cpr, etc) file and load it up!
// ----------------------------------------------------------------------
u8 SugarDSChooseGame(u8 bDiskOnly)
{
  bool bDone=false;
  u16 ucHaut=0x00, ucBas=0x00,ucSHaut=0x00, ucSBas=0x00, romSelected= 0, firstRomDisplay=0,nbRomPerPage, uNbRSPage;
  s16 uLenFic=0, ucFlip=0, ucFlop=0;

  // Show the menu...
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B))!=0);

  BottomScreenOptions();

  sugarDSFindFiles();

  ucGameChoice = -1;

  nbRomPerPage = (countFiles>=18 ? 18 : countFiles);
  uNbRSPage = (countFiles>=5 ? 5 : countFiles);

  if (ucGameAct>countFiles-nbRomPerPage)
  {
    firstRomDisplay=countFiles-nbRomPerPage;
    romSelected=ucGameAct-countFiles+nbRomPerPage;
  }
  else
  {
    firstRomDisplay=ucGameAct;
    romSelected=0;
  }

  if (romSelected >= countFiles) romSelected = 0; // Just start at the top

  dsDisplayFiles(firstRomDisplay,romSelected);

  // -----------------------------------------------------
  // Until the user selects a file or exits the menu...
  // -----------------------------------------------------
  while (!bDone)
  {
    MaxBrightness();
    if (keysCurrent() & KEY_UP)
    {
      if (!ucHaut)
      {
        ucGameAct = (ucGameAct>0 ? ucGameAct-1 : countFiles-1);
        if (romSelected>uNbRSPage) { romSelected -= 1; }
        else {
          if (firstRomDisplay>0) { firstRomDisplay -= 1; }
          else {
            if (romSelected>0) { romSelected -= 1; }
            else {
              firstRomDisplay=countFiles-nbRomPerPage;
              romSelected=nbRomPerPage-1;
            }
          }
        }
        ucHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {

        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else
    {
      ucHaut = 0;
    }
    if (keysCurrent() & KEY_DOWN)
    {
      if (!ucBas) {
        ucGameAct = (ucGameAct< countFiles-1 ? ucGameAct+1 : 0);
        if (romSelected<uNbRSPage-1) { romSelected += 1; }
        else {
          if (firstRomDisplay<countFiles-nbRomPerPage) { firstRomDisplay += 1; }
          else {
            if (romSelected<nbRomPerPage-1) { romSelected += 1; }
            else {
              firstRomDisplay=0;
              romSelected=0;
            }
          }
        }
        ucBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else {
      ucBas = 0;
    }

    // -------------------------------------------------------------
    // Left and Right on the D-Pad will scroll 1 page at a time...
    // -------------------------------------------------------------
    if (keysCurrent() & KEY_RIGHT)
    {
      if (!ucSBas)
      {
        ucGameAct = (ucGameAct< countFiles-nbRomPerPage ? ucGameAct+nbRomPerPage : countFiles-nbRomPerPage);
        if (firstRomDisplay<countFiles-nbRomPerPage) { firstRomDisplay += nbRomPerPage; }
        else { firstRomDisplay = countFiles-nbRomPerPage; }
        if (ucGameAct == countFiles-nbRomPerPage) romSelected = 0;
        ucSBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucSBas++;
        if (ucSBas>10) ucSBas=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else {
      ucSBas = 0;
    }

    // -------------------------------------------------------------
    // Left and Right on the D-Pad will scroll 1 page at a time...
    // -------------------------------------------------------------
    if (keysCurrent() & KEY_LEFT)
    {
      if (!ucSHaut)
      {
        ucGameAct = (ucGameAct> nbRomPerPage ? ucGameAct-nbRomPerPage : 0);
        if (firstRomDisplay>nbRomPerPage) { firstRomDisplay -= nbRomPerPage; }
        else { firstRomDisplay = 0; }
        if (ucGameAct == 0) romSelected = 0;
        if (romSelected > ucGameAct) romSelected = ucGameAct;
        ucSHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucSHaut++;
        if (ucSHaut>10) ucSHaut=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else {
      ucSHaut = 0;
    }

    // The SELECT key will toggle favorites
    if (keysCurrent() & KEY_SELECT)
    {
        if (gpFic[ucGameAct].uType != DIRECTORY)
        {
            ToggleFavorite(gpFic[ucGameAct].szName);
            dsDisplayFiles(firstRomDisplay,romSelected);
            SaveFavorites();
            while (keysCurrent() & KEY_SELECT)
            {
                WAITVBL;
            }
        }
    }

    // -------------------------------------------------------------------------
    // They B key will exit out of the ROM selection without picking a new game
    // -------------------------------------------------------------------------
    if ( keysCurrent() & KEY_B )
    {
      bDone=true;
      while (keysCurrent() & KEY_B);
    }

    // -------------------------------------------------------------------
    // Any of these keys will pick the current ROM and try to load it...
    // -------------------------------------------------------------------
    if (keysCurrent() & KEY_A || keysCurrent() & KEY_Y || keysCurrent() & KEY_X)
    {
      if (gpFic[ucGameAct].uType != DIRECTORY)
      {
          // If we are disk-only fetching, only allow a .dsk choice!
          if (bDiskOnly && ((strcasecmp(strrchr(gpFic[ucGameAct].szName, '.'), ".dsk") != 0)))
          {
              continue;
          }
          bDone=true;
          ucGameChoice = ucGameAct;
          WAITVBL;
      }
      else
      {
        chdir(gpFic[ucGameAct].szName);
        sugarDSFindFiles();
        ucGameAct = 0;
        nbRomPerPage = (countFiles>=14 ? 14 : countFiles);
        uNbRSPage = (countFiles>=5 ? 5 : countFiles);
        if (ucGameAct>countFiles-nbRomPerPage) {
          firstRomDisplay=countFiles-nbRomPerPage;
          romSelected=ucGameAct-countFiles+nbRomPerPage;
        }
        else {
          firstRomDisplay=ucGameAct;
          romSelected=0;
        }
        dsDisplayFiles(firstRomDisplay,romSelected);
        while (keysCurrent() & KEY_A);
      }
    }

    // --------------------------------------------
    // If the filename is too long... scroll it.
    // --------------------------------------------
    if (strlen(gpFic[ucGameAct].szName) > 30)
    {
      ucFlip++;
      if (ucFlip >= 25)
      {
        ucFlip = 0;
        uLenFic++;
        if ((uLenFic+30)>strlen(gpFic[ucGameAct].szName))
        {
          ucFlop++;
          if (ucFlop >= 15)
          {
            uLenFic=0;
            ucFlop = 0;
          }
          else
            uLenFic--;
        }
        strncpy(szName,gpFic[ucGameAct].szName+uLenFic,30);
        szName[30] = '\0';
        DSPrint(1,6+romSelected,2,szName);
      }
    }
    swiWaitForVBlank();
  }

  // Wait for some key to be pressed before returning
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B | KEY_R | KEY_L | KEY_UP | KEY_DOWN))!=0);

  return 0x01;
}


// ---------------------------------------------------------------------------
// Write out the SugarDS.DAT configuration file to capture the settings for
// each game.  This one file contains global settings ~1000 game settings.
// ---------------------------------------------------------------------------
void SaveConfig(bool bShow)
{
    FILE *fp;
    int slot = 0;

    if (bShow) DSPrint(6,23,0, (char*)"SAVING CONFIGURATION");

    // Set the global configuration version number...
    myGlobalConfig.config_ver = CONFIG_VERSION;

    // If there is a game loaded, save that into a slot... re-use the same slot if it exists
    myConfig.game_crc = file_crc;

    // Find the slot we should save into...
    for (slot=0; slot<MAX_CONFIGS; slot++)
    {
        if (AllConfigs[slot].game_crc == myConfig.game_crc)  // Got a match?!
        {
            break;
        }
        if (AllConfigs[slot].game_crc == 0x00000000)  // Didn't find it... use a blank slot...
        {
            break;
        }
    }

    // --------------------------------------------------------------------------
    // Copy our current game configuration to the main configuration database...
    // --------------------------------------------------------------------------
    if (myConfig.game_crc != 0x00000000)
    {
        memcpy(&AllConfigs[slot], &myConfig, sizeof(struct Config_t));
    }

    // Grab the directory we are currently in so we can restore it
    getcwd(myGlobalConfig.szLastPath, MAX_FILENAME_LEN);

    // --------------------------------------------------
    // Now save the config file out o the SD card...
    // --------------------------------------------------
    DIR* dir = opendir("/data");
    if (dir)
    {
        closedir(dir);  // directory exists.
    }
    else
    {
        mkdir("/data", 0777);   // Doesn't exist - make it...
    }
    fp = fopen("/data/SugarDS.DAT", "wb+");
    if (fp != NULL)
    {
        fwrite(&myGlobalConfig, sizeof(myGlobalConfig), 1, fp); // Write the global config
        fwrite(&AllConfigs, sizeof(AllConfigs), 1, fp);         // Write the array of all configurations
        fclose(fp);
    } else DSPrint(4,23,0, (char*)"ERROR SAVING CONFIG FILE");

    if (bShow)
    {
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(4,23,0, (char*)"                        ");
    }
}

void MapPlayer1(void)
{
    myConfig.keymap[0]   = 0;    // NDS D-Pad mapped to Joystick UP
    myConfig.keymap[1]   = 1;    // NDS D-Pad mapped to Joystick DOWN
    myConfig.keymap[2]   = 2;    // NDS D-Pad mapped to Joystick LEFT
    myConfig.keymap[3]   = 3;    // NDS D-Pad mapped to Joystick RIGHT
    myConfig.keymap[4]   = 4;    // NDS A Button mapped to Joystick Fire

    myConfig.keymap[5]   = 0;    // NDS B Button mapped to Joystick UP (jump)
    myConfig.keymap[6]   = 46;   // NDS X Button mapped to SPACE
    myConfig.keymap[7]   = 5;    // NDS Y Button mapped to Joystick Fire 2
    myConfig.keymap[8]   = 47;   // NDS START mapped to RETURN
    myConfig.keymap[9]   = 46;   // NDS SELECT mapped to SPACE
}

void MapAllJoy(void)
{
    myConfig.keymap[0]   = 0;    // NDS D-Pad mapped to Joystick UP
    myConfig.keymap[1]   = 1;    // NDS D-Pad mapped to Joystick DOWN
    myConfig.keymap[2]   = 2;    // NDS D-Pad mapped to Joystick LEFT
    myConfig.keymap[3]   = 3;    // NDS D-Pad mapped to Joystick RIGHT
    myConfig.keymap[4]   = 4;    // NDS A Button mapped to Joystick Fire 1
    myConfig.keymap[5]   = 0;    // NDS B Button mapped to Joystick Up
    myConfig.keymap[6]   = 5;    // NDS X Button mapped to Joystick Fire 2
    myConfig.keymap[7]   = 6;    // NDS Y Button mapped to Joystick Fire 3
    
    myConfig.keymap[8]   = 47;   // NDS START mapped to RETURN
    myConfig.keymap[9]   = 46;   // NDS SELECT mapped to SPACE
}

void MapQAOP(void)
{
    myConfig.keymap[0]   = 23;   // Q
    myConfig.keymap[1]   =  7;   // A
    myConfig.keymap[2]   = 21;   // O
    myConfig.keymap[3]   = 22;   // P
    myConfig.keymap[4]   = 46;   // Space
    myConfig.keymap[5]   = 23;   // Q (up)
    myConfig.keymap[6]   = 19;   // M
    myConfig.keymap[7]   = 32;   // Z
    myConfig.keymap[8]   = 47;   // NDS START mapped to RETURN
    myConfig.keymap[9]   = 46;   // NDS SELECT mapped to SPACE
}

void MapCursors(void)
{
    myConfig.keymap[0]   = 74;   // Cursor UP
    myConfig.keymap[1]   = 75;   // Cursor DOWN
    myConfig.keymap[2]   = 76;   // Cursor LEFT
    myConfig.keymap[3]   = 77;   // Cursor RIGHT
    myConfig.keymap[4]   = 46;   // Space
    myConfig.keymap[5]   = 46;   // Space
    myConfig.keymap[6]   = 47;   // RETURN
    myConfig.keymap[7]   = 73;   // F-ENTER
    myConfig.keymap[8]   = 42;   // NDS START mapped to '0'
    myConfig.keymap[9]   = 33;   // NDS SELECT mapped to '1'
}

void SetDefaultGlobalConfig(void)
{
    // A few global defaults...
    memset(&myGlobalConfig, 0x00, sizeof(myGlobalConfig));
    myGlobalConfig.showFPS        = 0;    // Don't show FPS counter by default
    myGlobalConfig.lastDir        = 0;    // Default is to start in /roms/cpc
    myGlobalConfig.debugger       = 0;    // Debugger is not shown by default
    myGlobalConfig.splashType     = 0;    // Show the Amstrad Croc by default
}

void SetDefaultGameConfig(void)
{
    myConfig.game_crc    = 0;    // No game in this slot yet

    MapPlayer1();                // Default to Player 1 Joystick mapping

    myConfig.r52IntVsync = 0;                           // Normal Interrupt timing
    myConfig.autoFire    = 0;                           // Default to no auto-fire on either button
    myConfig.dpad        = DPAD_NORMAL;                 // Normal DPAD use - mapped to joystick
    myConfig.autoLoad    = 1;                           // Default is to to auto-load Disk Games
    myConfig.gameSpeed   = 0;                           // Default is 100% game speed
    myConfig.jitter      = 1;                           // Light Jitter
    myConfig.offsetX     = 0;                           // Push the side border off the main display
    myConfig.offsetY     = 0;                           // Push the top border off the main display
    myConfig.scaleX      = 256;                         // Scale the 320 pixels of display to the DS 256 pixels (squashed... booo!)
    myConfig.scaleY      = 200;                         // Scale the 200 pixels of display to the DS 200 (yes, there is only 192 so this will cut... use PAN UP/DN)

    myConfig.autoSize    = 1;                           // Default to Auto-Size the screen
    myConfig.cpuAdjust   = 0;                           // No CPU adjustment by default
    myConfig.waveDirect  = 0;                           // Default is normal sound driver
    myConfig.screenTop   = 0;                           // Normal screen top position
    myConfig.panAndScan  = 0;                           // Default to compressed 320/640 to fit on LCD
    myConfig.diskWrite   = 1;                           // Default is to allow write back to SD
    myConfig.crtcDriver  = CRTC_DRV_STANDARD;           // Default is standard driver
    myConfig.reserved1   = 0;
    myConfig.reserved2   = 0;
    myConfig.reserved3   = 0;
}

// ----------------------------------------------------------
// Load configuration into memory where we can use it.
// The configuration is stored in SugarDS.DAT
// ----------------------------------------------------------
void LoadConfig(void)
{
    u8 bWipeConfig = 0;
    
    // -----------------------------------------------------------------
    // Start with defaults.. if we find a match in our config database
    // below, we will fill in the config with data read from the file.
    // -----------------------------------------------------------------
    SetDefaultGameConfig();

    if (ReadFileCarefully("/data/SugarDS.DAT", (u8*)&myGlobalConfig, sizeof(myGlobalConfig), 0))  // Read Global Config
    {
        ReadFileCarefully("/data/SugarDS.DAT", (u8*)&AllConfigs, sizeof(AllConfigs), sizeof(myGlobalConfig)); // Read the full game array of configs

        if (myGlobalConfig.config_ver != CONFIG_VERSION)
        {
            bWipeConfig = 1;
        }
    }
    else    // Not found... init the entire database...
    {
        bWipeConfig = 1;
    }
    
    if (bWipeConfig)
    {
        memset(&AllConfigs, 0x00, sizeof(AllConfigs));
        SetDefaultGameConfig();
        SetDefaultGlobalConfig();
        SaveConfig(FALSE);
        bShowInstructions = 1;
    }
    else
    {
        bShowInstructions = 0;
    }
}

// -------------------------------------------------------------------------
// Try to match our loaded game to a configuration my matching CRCs
// -------------------------------------------------------------------------
void FindConfig(char *filename)
{
    // -----------------------------------------------------------------
    // Start with defaults.. if we find a match in our config database
    // below, we will fill in the config with data read from the file.
    // -----------------------------------------------------------------
    SetDefaultGameConfig();

    // ---------------------------------------------------------------------
    // And now some special cases for games that require different settings
    // ---------------------------------------------------------------------
    static char szName[MAX_FILENAME_LEN+1];
    strcpy(szName, filename);
    for (int j=0; j<strlen(szName); j++) szName[j] = toupper(szName[j]);

    if (strstr(szName, "PINBALL")       != 0)   myConfig.crtcDriver = CRTC_DRV_STANDARD;    // Pinball Dreams doesn't handle the advanced driver well
    if (strstr(szName, "PD.")           != 0)   myConfig.crtcDriver = CRTC_DRV_STANDARD;    // Pinball Dreams doesn't handle the advanced driver well
    if (strstr(szName, "FOREVER")       != 0)   myConfig.crtcDriver = CRTC_DRV_STANDARD;    // Batman Forever Demo doesn't handle the advanced driver well
    if (strstr(szName, "CAULDRON")      != 0)   myConfig.crtcDriver = CRTC_DRV_ADVANCED;    // Super Cauldron needs the Advanced Driver
    if (strstr(szName, "PREHISTORIK")   != 0)   myConfig.crtcDriver = CRTC_DRV_ADVANCED;    // Prehistorik II needs the Advanced Driver
    if (strstr(szName, "CHIPS")         != 0)   myConfig.crtcDriver = CRTC_DRV_ADVANCED;    // Chips Challenge works slightly better with the Advanced Driver
    if (strstr(szName, "ORION")         != 0)   myConfig.crtcDriver = CRTC_DRV_ADVANCED;    // Orion Prime is slightly better with the Advanced Driver

    if (strstr(szName, "IANNA")         != 0)   myConfig.cpuAdjust   = 5;  // -2 CPU Adjust to remove graphical artifacts
    if (strstr(szName, "DIZZY3")        != 0)   myConfig.r52IntVsync = 1;  // Dizzy 3 - R52 Interrupt Forgiving to remove slowdown
    if (strstr(szName, "DIZZY 3")       != 0)   myConfig.r52IntVsync = 1;  // Dizzy 3 - R52 Interrupt Forgiving to remove slowdown
    if (strstr(szName, "DIZZY-III")     != 0)   myConfig.r52IntVsync = 1;  // Dizzy 3 - R52 Interrupt Forgiving to remove slowdown
    if (strstr(szName, "DIZZY III")     != 0)   myConfig.r52IntVsync = 1;  // Dizzy 3 - R52 Interrupt Forgiving to remove slowdown
    if (strstr(szName, "FANTASY WORLD") != 0)   myConfig.r52IntVsync = 1;  // Dizzy 3 - R52 Interrupt Forgiving to remove slowdown

    if (strstr(szName, "ROBOCOP")       != 0)   myConfig.waveDirect  = 1;  // Robocop uses digitized voice
    if (strstr(szName, "CHASEH")        != 0)   myConfig.waveDirect  = 1;  // Chase HQ uses digitized voice
    if (strstr(szName, "CHASE H")       != 0)   myConfig.waveDirect  = 1;  // Chase HQ uses digitized voice
    if (strstr(szName, "MANIC")         != 0)   myConfig.waveDirect  = 1;  // Manic Miner uses digitized sounds

    for (u16 slot=0; slot<MAX_CONFIGS; slot++)
    {
        if (AllConfigs[slot].game_crc == file_crc)  // Got a match?!
        {
            memcpy(&myConfig, &AllConfigs[slot], sizeof(struct Config_t));
            break;
        }
    }
}


// ------------------------------------------------------------------------------
// Options are handled here... we have a number of things the user can tweak
// and these options are applied immediately. The user can also save off
// their option choices for the currently running game into the NINTV-DS.DAT
// configuration database. When games are loaded back up, NINTV-DS.DAT is read
// to see if we have a match and the user settings can be restored for the game.
// ------------------------------------------------------------------------------
struct options_t
{
    const char  *label;
    const char  *option[18];
    u8          *option_val;
    u8           option_max;
};

const struct options_t Option_Table[2][20] =
{
    // Game Specific Configuration
    {
        {"AUTO LOAD",      {"NO", "YES"},                                                       &myConfig.autoLoad,          2},
        {"AUTO SIZE",      {"OFF", "ON"},                                                       &myConfig.autoSize,          2},
        {"AUTO FIRE",      {"OFF", "ON"},                                                       &myConfig.autoFire,          2},
        {"LCD JITTER",     {"OFF", "LIGHT", "HEAVY"},                                           &myConfig.jitter,            3},
        {"SCREEN TOP",     {"+0","+1","+2","+3","+4","+5","+6","+7","+8","+9","+10",
                            "+11","+12","+13","+14","+15","+16"},                               &myConfig.screenTop,        17},
        {"NDS D-PAD",      {"NORMAL", "DIAGONALS", "SLIDE-N-GLIDE"},                            &myConfig.dpad,              3},
        {"GAME SPEED",     {"100%", "110%", "120%", "130%", "90%", "80%"},                      &myConfig.gameSpeed,         6},
        {"MODE 1/2",       {"SCALE/COMPRESS", "320 PAN+SCAN"},                                  &myConfig.panAndScan,        2},
        {"R52  VSYNC",     {"NORMAL", "FORGIVING", "STRICT"},                                   &myConfig.r52IntVsync,       3},
        {"CPU ADJUST",     {"+0 (NONE)", "+1 CYCLES", "+2 CYCLES", "-4 CYCLES",
                            "-3 CYCLES", "-2 CYCLES", "-1 CYCLES"},                             &myConfig.cpuAdjust,         7},
        {"SOUND DRV",      {"NORMAL", "WAVE DIRECT"},                                           &myConfig.waveDirect,        2},
        {"DISK WRITE",     {"OFF", "ALLOWED"},                                                  &myConfig.diskWrite,         2},
        {"CRTC DRIVER",    {"STANDARD", "ADVANCED"},                                            &myConfig.crtcDriver,        2},

        {NULL,             {"",      ""},                                                       NULL,                        1},
    },
    // Global Options
    {
        {"FPS",            {"OFF", "ON", "ON FULLSPEED"},                                       &myGlobalConfig.showFPS,     3},
        {"DISK ROM",       {"AMSDOS", "PARADOS"},                                               &myGlobalConfig.diskROM,     2},
        {"START DIR",      {"/ROMS/CPC", "/ROMS/AMSTRAD", "LAST USED DIR"},                     &myGlobalConfig.lastDir,     3},
        {"SPLASH SCR",     {"AMSTRAD CROC", "CPC KEYBOARD"},                                    &myGlobalConfig.splashType,  2},
        {"KEYBD BRIGHT",   {"MAX BRIGHT", "DIM", "DIMMER", "DIMMEST"},                          &myGlobalConfig.keyboardDim, 4},

        {"DEBUGGER",       {"OFF", "BAD OPS", "DEBUG", "FULL DEBUG"},                           &myGlobalConfig.debugger,    4},
        {NULL,             {"",      ""},                                                       NULL,                        1},
    }
};


// ------------------------------------------------------------------
// Display the current list of options for the user.
// ------------------------------------------------------------------
u8 display_options_list(bool bFullDisplay)
{
    s16 len=0;

    DSPrint(1,21, 0, (char *)"                              ");
    if (bFullDisplay)
    {
        while (true)
        {
            sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][len].label, Option_Table[option_table][len].option[*(Option_Table[option_table][len].option_val)]);
            DSPrint(1,7+len, (len==0 ? 2:0), strBuf); len++;
            if (Option_Table[option_table][len].label == NULL) break;
        }

        // Blank out rest of the screen... option menus are of different lengths...
        for (int i=len; i<16; i++)
        {
            DSPrint(1,7+i, 0, (char *)"                               ");
        }
    }

    if (option_table == 1)
        DSPrint(1,22, 0, (char *)" B=EXIT, X=OPTIONS, START=SAVE  ");
    else
        DSPrint(1,22, 0, (char *)" B=EXIT, X=GLOBAL, START=SAVE   ");
    DSPrint(1,23, 0, (char *)"                                ");
    return len;
}


//*****************************************************************************
// Change Game Options for the current game
//*****************************************************************************
void SugarDSGameOptions(bool bIsGlobal)
{
    u8 optionHighlighted;
    u8 idx;
    bool bDone=false;
    int keys_pressed;
    int last_keys_pressed = 999;

    option_table = (bIsGlobal ? 1:0);

    idx=display_options_list(true);
    optionHighlighted = 0;
    while (keysCurrent() != 0)
    {
        WAITVBL;
    }
    while (!bDone)
    {
        MaxBrightness();
        keys_pressed = keysCurrent();
        if (keys_pressed != last_keys_pressed)
        {
            last_keys_pressed = keys_pressed;
            if (keysCurrent() & KEY_UP) // Previous option
            {
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,7+optionHighlighted,0, strBuf);
                if (optionHighlighted > 0) optionHighlighted--; else optionHighlighted=(idx-1);
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,7+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_DOWN) // Next option
            {
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,7+optionHighlighted,0, strBuf);
                if (optionHighlighted < (idx-1)) optionHighlighted++;  else optionHighlighted=0;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,7+optionHighlighted,2, strBuf);
            }

            if (keysCurrent() & KEY_RIGHT)  // Toggle option clockwise
            {
                *(Option_Table[option_table][optionHighlighted].option_val) = (*(Option_Table[option_table][optionHighlighted].option_val) + 1) % Option_Table[option_table][optionHighlighted].option_max;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,7+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_LEFT)  // Toggle option counterclockwise
            {
                if ((*(Option_Table[option_table][optionHighlighted].option_val)) == 0)
                    *(Option_Table[option_table][optionHighlighted].option_val) = Option_Table[option_table][optionHighlighted].option_max -1;
                else
                    *(Option_Table[option_table][optionHighlighted].option_val) = (*(Option_Table[option_table][optionHighlighted].option_val) - 1) % Option_Table[option_table][optionHighlighted].option_max;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,7+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_START)  // Save Options
            {
                SaveConfig(TRUE);
            }
            if (keysCurrent() & (KEY_X)) // Toggle Table
            {
                option_table ^= 1;
                idx=display_options_list(true);
                optionHighlighted = 0;
                while (keysCurrent() != 0)
                {
                    WAITVBL;
                }
            }
            if ((keysCurrent() & KEY_B) || (keysCurrent() & KEY_A))  // Exit options
            {
                option_table = 0;   // Reset for next time
                break;
            }
        }
        swiWaitForVBlank();
    }

    // Give a third of a second time delay...
    for (int i=0; i<20; i++)
    {
        swiWaitForVBlank();
    }

    return;
}

//*****************************************************************************
// Change Keymap Options for the current game
//*****************************************************************************
char szCha[34];
void DisplayKeymapName(u32 uY)
{
  sprintf(szCha," PAD UP    : %-16s",szKeyName[myConfig.keymap[0]]);
  DSPrint(2, 7,(uY==  7 ? 2 : 0),szCha);
  sprintf(szCha," PAD DOWN  : %-16s",szKeyName[myConfig.keymap[1]]);
  DSPrint(2, 8,(uY==  8 ? 2 : 0),szCha);
  sprintf(szCha," PAD LEFT  : %-16s",szKeyName[myConfig.keymap[2]]);
  DSPrint(2, 9,(uY==  9 ? 2 : 0),szCha);
  sprintf(szCha," PAD RIGHT : %-16s",szKeyName[myConfig.keymap[3]]);
  DSPrint(2,10,(uY==10 ? 2 : 0),szCha);
  sprintf(szCha," KEY A     : %-16s",szKeyName[myConfig.keymap[4]]);
  DSPrint(2,11,(uY== 11 ? 2 : 0),szCha);
  sprintf(szCha," KEY B     : %-16s",szKeyName[myConfig.keymap[5]]);
  DSPrint(2,12,(uY== 12 ? 2 : 0),szCha);
  sprintf(szCha," KEY X     : %-16s",szKeyName[myConfig.keymap[6]]);
  DSPrint(2,13,(uY== 13 ? 2 : 0),szCha);
  sprintf(szCha," KEY Y     : %-16s",szKeyName[myConfig.keymap[7]]);
  DSPrint(2,14,(uY== 14 ? 2 : 0),szCha);
  sprintf(szCha," START     : %-16s",szKeyName[myConfig.keymap[8]]);
  DSPrint(2,15,(uY== 15 ? 2 : 0),szCha);
  sprintf(szCha," SELECT    : %-16s",szKeyName[myConfig.keymap[9]]);
  DSPrint(2,16,(uY== 16 ? 2 : 0),szCha);
}

u8 keyMapType = 0;
void SwapKeymap(void)
{
    keyMapType = (keyMapType+1) % 4;
    switch (keyMapType)
    {
        case 0: MapPlayer1();  DSPrint(7,6,0,("**  JOYSTICK 1  **")); break;
        case 1: MapAllJoy();   DSPrint(7,6,0,("** JOY FIRE 123 **")); break;
        case 2: MapCursors();  DSPrint(7,6,0,("** CURSOR KEYS  **")); break;
        case 3: MapQAOP();     DSPrint(7,6,0,("**  QAOP-SPACE  **")); break;
    }
}


// ------------------------------------------------------------------------------
// Allow the user to change the key map for the current game and give them
// the option of writing that keymap out to a configuration file for the game.
// ------------------------------------------------------------------------------
void SugarDSChangeKeymap(void)
{
  u16 ucHaut=0x00, ucBas=0x00,ucL=0x00,ucR=0x00,ucY=7, bOK=0, bIndTch=0;

  // ------------------------------------------------------
  // Clear the screen so we can put up Key Map infomation
  // ------------------------------------------------------
  unsigned short dmaVal =  *(bgGetMapPtr(bg0b) + 24*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);

  // --------------------------------------------------
  // Give instructions to the user...
  // --------------------------------------------------
  DSPrint(1 ,19,0,("   D-PAD : CHANGE KEY MAP    "));
  DSPrint(1 ,20,0,("       B : RETURN MAIN MENU  "));
  DSPrint(1 ,21,0,("       X : SWAP KEYMAP TYPE  "));
  DSPrint(1 ,22,0,("   START : SAVE KEYMAP       "));
  DisplayKeymapName(ucY);

  bIndTch = myConfig.keymap[0];

  // -----------------------------------------------------------------------
  // Clear out any keys that might be pressed on the way in - make sure
  // NDS keys are not being pressed. This prevents the inadvertant A key
  // that enters this menu from also being acted on in the keymap...
  // -----------------------------------------------------------------------
  while ((keysCurrent() & (KEY_TOUCH | KEY_B | KEY_A | KEY_X | KEY_UP | KEY_DOWN))!=0)
      ;
  WAITVBL;

  while (!bOK)
  {
    MaxBrightness();
    if (keysCurrent() & KEY_UP) {
      if (!ucHaut) {
        DisplayKeymapName(32);
        ucY = (ucY == 7 ? 16 : ucY -1);
        bIndTch = myConfig.keymap[ucY-7];
        ucHaut=0x01;
        DisplayKeymapName(ucY);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
    }
    else {
      ucHaut = 0;
    }
    if (keysCurrent() & KEY_DOWN)
    {
      if (!ucBas) {
        DisplayKeymapName(32);
        ucY = (ucY == 16 ? 7 : ucY +1);
        bIndTch = myConfig.keymap[ucY-7];
        ucBas=0x01;
        DisplayKeymapName(ucY);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
    }
    else {
      ucBas = 0;
    }

    if (keysCurrent() & KEY_START)
    {
        SaveConfig(true); // Save options
    }

    if (keysCurrent() & KEY_B)
    {
      bOK = 1;  // Exit menu
    }

    if (keysCurrent() & KEY_LEFT)
    {
        if (ucL == 0) {
          bIndTch = (bIndTch == 0 ? (MAX_KEY_OPTIONS-1) : bIndTch-1);
          ucL=1;
          myConfig.keymap[ucY-7] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else {
          ucL++;
          if (ucL > 7) ucL = 0;
        }
    }
    else
    {
        ucL = 0;
    }

    if (keysCurrent() & KEY_RIGHT)
    {
        if (ucR == 0)
        {
          bIndTch = (bIndTch == (MAX_KEY_OPTIONS-1) ? 0 : bIndTch+1);
          ucR=1;
          myConfig.keymap[ucY-7] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else
        {
          ucR++;
          if (ucR > 7) ucR = 0;
        }
    }
    else
    {
        ucR=0;
    }

    // Swap Player 1 and Player 2 keymap
    if (keysCurrent() & KEY_X)
    {
        SwapKeymap();
        bIndTch = myConfig.keymap[ucY-7];
        DisplayKeymapName(ucY);
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(7,6,0,("                  "));
        while (keysCurrent() & KEY_X)
            ;
        WAITVBL
    }
    swiWaitForVBlank();
  }
  while (keysCurrent() & KEY_B);
}


// -----------------------------------------------------------------------------------------
// At the bottom of the main screen we show the currently selected filename, size and CRC32
// -----------------------------------------------------------------------------------------
void DisplayFileName(void)
{
    sprintf(szName, "[%d K] [CRC: %08X]", file_size/1024, file_crc);
    DSPrint((16 - (strlen(szName)/2)),20,0,szName);

    sprintf(szName,"%s",gpFic[ucGameChoice].szName);
    for (u8 i=strlen(szName)-1; i>0; i--) if (szName[i] == '.') {szName[i]=0;break;}
    if (strlen(szName)>30) szName[30]='\0';
    DSPrint((16 - (strlen(szName)/2)),22,0,szName);
    if (strlen(gpFic[ucGameChoice].szName) >= 35)   // If there is more than a few characters left, show it on the 2nd line
    {
        if (strlen(gpFic[ucGameChoice].szName) <= 60)
        {
            sprintf(szName,"%s",gpFic[ucGameChoice].szName+30);
        }
        else
        {
            sprintf(szName,"%s",gpFic[ucGameChoice].szName+strlen(gpFic[ucGameChoice].szName)-30);
        }

        if (strlen(szName)>30) szName[30]='\0';
        DSPrint((16 - (strlen(szName)/2)),23,0,szName);
    }
}

//*****************************************************************************
// Display info screen and change options "main menu"
//*****************************************************************************
void dispInfoOptions(u32 uY)
{
    DSPrint(2, 7,(uY== 7 ? 2 : 0),("         LOAD  GAME         "));
    DSPrint(2, 9,(uY== 9 ? 2 : 0),("         PLAY  GAME         "));
    DSPrint(2,11,(uY==11 ? 2 : 0),("       DEFINE  KEYS         "));
    DSPrint(2,13,(uY==13 ? 2 : 0),("         GAME  OPTIONS      "));
    DSPrint(2,15,(uY==15 ? 2 : 0),("       GLOBAL  OPTIONS      "));
    DSPrint(2,17,(uY==17 ? 2 : 0),("         QUIT  EMULATOR     "));
}

// --------------------------------------------------------------------
// Some main menu selections don't make sense without a game loaded.
// --------------------------------------------------------------------
void NoGameSelected(u32 ucY)
{
    unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    DSPrint(5,10,0,("   NO GAME SELECTED   "));
    DSPrint(5,12,0,("  PLEASE, USE OPTION  "));
    DSPrint(5,14,0,("      LOAD  GAME      "));
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    while (!(keysCurrent()  & (KEY_START | KEY_A)));
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    dispInfoOptions(ucY);
}


void ReadFileCRCAndConfig(void)
{
    // Reset the mode related vars...
    keyMapType = 0;

    // ----------------------------------------------------------------------------------
    // Clear the entire ROM buffer[] - fill with 0xFF to emulate non-responsive memory
    // ----------------------------------------------------------------------------------
    memset(ROM_Memory, 0xFF, MAX_ROM_SIZE);

    if (strstr(gpFic[ucGameChoice].szName, ".dsk") != 0) amstrad_mode = MODE_DSK;
    if (strstr(gpFic[ucGameChoice].szName, ".DSK") != 0) amstrad_mode = MODE_DSK;
    if (strstr(gpFic[ucGameChoice].szName, ".cpr") != 0) amstrad_mode = MODE_CPR;
    if (strstr(gpFic[ucGameChoice].szName, ".CPR") != 0) amstrad_mode = MODE_CPR;
    if (strstr(gpFic[ucGameChoice].szName, ".sna") != 0) amstrad_mode = MODE_SNA;
    if (strstr(gpFic[ucGameChoice].szName, ".SNA") != 0) amstrad_mode = MODE_SNA;
    if (strstr(gpFic[ucGameChoice].szName, ".dan") != 0) amstrad_mode = MODE_DAN;
    if (strstr(gpFic[ucGameChoice].szName, ".DAN") != 0) amstrad_mode = MODE_DAN;

    // Grab the all-important file CRC - this also loads the file into ROM_Memory[]
    getfile_crc(gpFic[ucGameChoice].szName);

    FindConfig(gpFic[ucGameChoice].szName);    // Try to find keymap and config for this file...
}


// ----------------------------------------------------------------------
// Read file twice and ensure we get the same CRC... if not, do it again
// until we get a clean read. Return the filesize to the caller...
// ----------------------------------------------------------------------
u32 ReadFileCarefully(char *filename, u8 *buf, u32 buf_size, u32 buf_offset)
{
    u32 crc1 = 0;
    u32 crc2 = 1;
    u32 fileSize = 0;

    // --------------------------------------------------------------------------------------------
    // I've seen some rare issues with reading files from the SD card on a DSi so we're doing
    // this slow and careful - we will read twice and ensure that we get the same CRC both times.
    // --------------------------------------------------------------------------------------------
    do
    {
        // Read #1
        crc1 = 0xFFFFFFFF;
        FILE* file = fopen(filename, "rb");
        if (file)
        {
            if (buf_offset) fseek(file, buf_offset, SEEK_SET);
            fileSize = fread(buf, 1, buf_size, file);
            crc1 = getCRC32(buf, buf_size);
            fclose(file);
        }

        // Read #2
        crc2 = 0xFFFFFFFF;
        FILE* file2 = fopen(filename, "rb");
        if (file2)
        {
            if (buf_offset) fseek(file2, buf_offset, SEEK_SET);
            fread(buf, 1, buf_size, file2);
            crc2 = getCRC32(buf, buf_size);
            fclose(file2);
        }
   } while (crc1 != crc2); // If the file couldn't be read, file_size will be 0 and the CRCs will both be 0xFFFFFFFF

   return fileSize;
}

// --------------------------------------------------------------------
// Let the user select new options for the currently loaded game...
// --------------------------------------------------------------------
void sugarDSChangeOptions(void)
{
  u16 ucHaut=0x00, ucBas=0x00,ucA=0x00,ucY=7, bOK=0;

  // Lower Screen Background
  BottomScreenOptions();

  dispInfoOptions(ucY);

  if (ucGameChoice != -1)
  {
      DisplayFileName();
  }

  while (!bOK)
  {
    MaxBrightness();
    if (keysCurrent()  & KEY_UP) {
      if (!ucHaut) {
        dispInfoOptions(32);
        ucY = (ucY == 7 ? 17 : ucY -2);
        ucHaut=0x01;
        dispInfoOptions(ucY);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
    }
    else {
      ucHaut = 0;
    }
    if (keysCurrent()  & KEY_DOWN) {
      if (!ucBas) {
        dispInfoOptions(32);
        ucY = (ucY == 17 ? 7 : ucY +2);
        ucBas=0x01;
        dispInfoOptions(ucY);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
    }
    else {
      ucBas = 0;
    }
    if (keysCurrent()  & KEY_A) {
      if (!ucA) {
        ucA = 0x01;
        switch (ucY) {
          case 7 :      // LOAD GAME
            SugarDSChooseGame(false);
            BottomScreenOptions();
            if (ucGameChoice != -1)
            {
                ReadFileCRCAndConfig(); // Get CRC32 of the file and read the config/keys
                DisplayFileName();    // And put up the filename on the bottom screen
            }
            ucY = 9;
            dispInfoOptions(ucY);
            break;
          case 9 :     // PLAY GAME
            if (ucGameChoice != -1)
            {
                bOK = 1;
            }
            else
            {
                NoGameSelected(ucY);
            }
            break;
          case 11 :     // REDEFINE KEYS
            if (ucGameChoice != -1)
            {
                SugarDSChangeKeymap();
                BottomScreenOptions();
                dispInfoOptions(ucY);
                DisplayFileName();
            }
            else
            {
                NoGameSelected(ucY);
            }
            break;
          case 13 :     // GAME OPTIONS
            if (ucGameChoice != -1)
            {
                SugarDSGameOptions(false);
                BottomScreenOptions();
                dispInfoOptions(ucY);
                DisplayFileName();
            }
            else
            {
               NoGameSelected(ucY);
            }
            break;

          case 15 :     // GLOBAL OPTIONS
            SugarDSGameOptions(true);
            TopScreenOptions();
            BottomScreenOptions();
            dispInfoOptions(ucY);
            DisplayFileName();
            break;

          case 17 :     // QUIT EMULATOR
            exit(1);
            break;
        }
      }
    }
    else
      ucA = 0x00;
    if (keysCurrent()  & KEY_START) {
      if (ucGameChoice != -1)
      {
        bOK = 1;
      }
      else
      {
        NoGameSelected(ucY);
      }
    }
    swiWaitForVBlank();
  }
  while (keysCurrent()  & (KEY_START | KEY_A));
}

//*****************************************************************************
// Displays a message on the screen
//*****************************************************************************
ITCM_CODE void DSPrint(int iX,int iY,int iScr,char *szMessage)
{
  u16 *pusScreen,*pusMap;
  u16 usCharac;
  char *pTrTxt=szMessage;

  pusScreen=(u16*) (iScr != 1 ? bgGetMapPtr(bg1b) : bgGetMapPtr(bg1))+iX+(iY<<5);
  pusMap=(u16*) (iScr != 1 ? (iScr == 6 ? bgGetMapPtr(bg0b)+24*32 : (iScr == 0 ? bgGetMapPtr(bg0b)+24*32 : bgGetMapPtr(bg0b)+26*32 )) : bgGetMapPtr(bg0)+51*32 );

  while((*pTrTxt)!='\0' )
  {
    char ch = *pTrTxt++;
    if (ch >= 'a' && ch <= 'z') ch -= 32;   // Faster than strcpy/strtoupper

    if (((ch)<' ') || ((ch)>'_'))
      usCharac=*(pusMap);                   // Will render as a vertical bar
    else if((ch)<'@')
      usCharac=*(pusMap+(ch)-' ');          // Number from 0-9 or punctuation
    else
      usCharac=*(pusMap+32+(ch)-'@');       // Character from A-Z
    *pusScreen++=usCharac;
  }
}

/******************************************************************************
* Routine FadeToColor :  Fade from background to black or white
******************************************************************************/
void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait) {
  unsigned short ucFade;
  unsigned char ucBcl;

  // Fade-out to black
  if (ucScr & 0x01) REG_BLDCNT=ucBG;
  if (ucScr & 0x02) REG_BLDCNT_SUB=ucBG;
  if (ucSens == 1) {
    for(ucFade=0;ucFade<valEnd;ucFade++) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
  else {
    for(ucFade=16;ucFade>valEnd;ucFade--) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
}


/*********************************************************************************
 * Keyboard Key Buffering Engine...
 ********************************************************************************/
u8 BufferedKeys[32];
u8 BufferedKeysWriteIdx=0;
u8 BufferedKeysReadIdx=0;
void BufferKey(u8 key)
{
    BufferedKeys[BufferedKeysWriteIdx] = key;
    BufferedKeysWriteIdx = (BufferedKeysWriteIdx+1) % 32;
}

// Buffer a whole string worth of characters...
void BufferKeys(char *str)
{
    for (int i=0; i<strlen(str); i++)
    {
        if (str[i] >= 32)
        {
            BufferKey((u8)str[i]);
        }
    }
}

// ---------------------------------------------------------------------------------------
// Called every frame... so 1/50th or 1/60th of a second. We will virtually 'press' and
// hold the key for roughly a tenth of a second and be smart about shift keys...
// ---------------------------------------------------------------------------------------
void ProcessBufferedKeys(void)
{
    static u8 next_dampen_time = 10;
    static u8 dampen = 0;
    static u8 buf_held = 0;

    if (BufferedKeysReadIdx == BufferedKeysWriteIdx) return;

    if (++dampen >= next_dampen_time) // Roughly 50ms... experimentally good enough for all systems.
    {
        if (BufferedKeysReadIdx != BufferedKeysWriteIdx)
        {
            last_special_key = 0;

            buf_held = BufferedKeys[BufferedKeysReadIdx];
            BufferedKeysReadIdx = (BufferedKeysReadIdx+1) % 32;
            if (buf_held == 255) {buf_held = 0; next_dampen_time=20;}
            else if (buf_held == 254) {buf_held = 0; next_dampen_time=15;}
            else next_dampen_time = 10;
        } else buf_held = 0;
        dampen = 0;

        // See if the shift key should be virtually pressed along with this buffered key...
        if (buf_held == '"')
        {
            last_special_key = 1;
            kbd_keys[kbd_keys_pressed++] = KBD_KEY_SFT;
            buf_held = '2';
        }
        // Check all of the characters that could be typed as shifted-values
        else if (buf_held == '+')
        {
            last_special_key = 1;
            kbd_keys[kbd_keys_pressed++] = KBD_KEY_SFT;
            buf_held = ';';
        }
        else if (buf_held == '!')
        {
            last_special_key = 1;
            kbd_keys[kbd_keys_pressed++] = KBD_KEY_SFT;
            buf_held = '1';
        }
        else if (buf_held == '#')
        {
            last_special_key = 1;
            kbd_keys[kbd_keys_pressed++] = KBD_KEY_SFT;
            buf_held = '3';
        }
        else if (buf_held == '$')
        {
            last_special_key = 1;
            kbd_keys[kbd_keys_pressed++] = KBD_KEY_SFT;
            buf_held = '4';
        }
        else if (buf_held == '%')
        {
            last_special_key = 1;
            kbd_keys[kbd_keys_pressed++] = KBD_KEY_SFT;
            buf_held = '5';
        }
        else if (buf_held == '&')
        {
            last_special_key = 1;
            kbd_keys[kbd_keys_pressed++] = KBD_KEY_SFT;
            buf_held = '6';
        }
        else if (buf_held == '(')
        {
            last_special_key = 1;
            kbd_keys[kbd_keys_pressed++] = KBD_KEY_SFT;
            buf_held = '8';
        }
        else if (buf_held == ')')
        {
            last_special_key = 1;
            kbd_keys[kbd_keys_pressed++] = KBD_KEY_SFT;
            buf_held = '9';
        }
        else if (buf_held == '_')
        {
            last_special_key = 1;
            kbd_keys[kbd_keys_pressed++] = KBD_KEY_SFT;
            buf_held = '0';
        }
        else if (buf_held == '*')
        {
            last_special_key = 1;
            kbd_keys[kbd_keys_pressed++] = KBD_KEY_SFT;
            buf_held = ':';
        }
        else if (buf_held == '=')
        {
            last_special_key = 1;
            kbd_keys[kbd_keys_pressed++] = KBD_KEY_SFT;
            buf_held = '-';
        }
        else if (buf_held == '|')
        {
            last_special_key = 1;
            kbd_keys[kbd_keys_pressed++] = KBD_KEY_SFT;
            buf_held = '@';
        }
    }
    else if (dampen >= (next_dampen_time/2))
    {
        buf_held = 0;
        last_special_key = 0;
    }

    if (buf_held) {kbd_keys[kbd_keys_pressed++] = buf_held;}
}

/*********************************************************************************
 * Init Amstrad CPC emulation engine for the chosen game...
 ********************************************************************************/
u8 AmstradInit(char *szGame)
{
    u8 RetFct;

    // We've got some debug data we can use for development... reset these.
    memset(debug, 0x00, sizeof(debug));
    DX = DY = 0;

    // -----------------------------------------------------------------
    // set the first two banks as background memory and the third as sub background memory
    // bank D is not used..if you need a bigger background then you will need to map
    // more vram banks consecutively (VRAM A-D are all 0x20000 bytes in size)
    // -----------------------------------------------------------------
    powerOn(POWER_ALL_2D);

    videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE);

    bgInit(3, BgType_Bmp8, BgSize_B8_512x512, 0,0);
    bgInit(2, BgType_Bmp8, BgSize_B8_512x512, 0,0);

    REG_BLDCNT = BLEND_ALPHA | BLEND_SRC_BG2 | BLEND_DST_BG3;
    REG_BLDALPHA = (8 << 8) | 8; // 50% / 50%

    vramSetPrimaryBanks(VRAM_A_MAIN_BG_0x06000000, VRAM_B_MAIN_BG_0x06020000, VRAM_C_SUB_BG , VRAM_D_LCD);

    REG_BG3CNT = BG_BMP8_512x512;

    int cxBG = (myConfig.offsetX << 8);
    int cyBG = (myConfig.offsetY) << 8;
    int xdxBG = ((320 / myConfig.scaleX) << 8) | (320 % myConfig.scaleX) ;
    int ydyBG = ((200 / myConfig.scaleY) << 8) | (200 % myConfig.scaleY);

    REG_BG2X = cxBG;
    REG_BG2Y = cyBG;
    REG_BG3X = cxBG;
    REG_BG3Y = cyBG;

    REG_BG2PA = xdxBG;
    REG_BG2PD = ydyBG;
    REG_BG3PA = xdxBG;
    REG_BG3PD = ydyBG;

    vramSetBankD(VRAM_D_LCD);        // Not using this for video but 128K of faster RAM always useful!  Mapped at 0x06860000 -   Unused - reserved for future use
    vramSetBankE(VRAM_E_LCD);        // Not using this for video but 64K of faster RAM always useful!   Mapped at 0x06880000 -   ..
    vramSetBankF(VRAM_F_LCD);        // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x06890000 -   ..
    vramSetBankG(VRAM_G_LCD);        // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x06894000 -   ..
    vramSetBankH(VRAM_H_LCD);        // Not using this for video but 32K of faster RAM always useful!   Mapped at 0x06898000 -   ..
    vramSetBankI(VRAM_I_LCD);        // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x068A0000 -   16K used for SID waveform table cache

    RetFct = loadgame(szGame);       // Load up the CPC Disk game

    ResetAmstrad();

    // Return with result
    return (RetFct);
}

/*********************************************************************************
 * Run the emul
 ********************************************************************************/
void amstrad_emulate(void)
{
  ResetZ80(&CPU);                       // Reset the CZ80 core CPU
  amstrad_reset();                      // Ensure the Emulation is ready
  BottomScreenKeyboard();               // Show the game-related screen with keypad / keyboard
}

u8 CPC_palette[32*3] = {
  0x00,0x00,0x00,   // Black
  0x00,0x00,0x80,   // Blue
  0x00,0x00,0xFF,   // Bright Blue
  0x80,0x00,0x00,   // Red
  0x80,0x00,0x80,   // Magenta
  0x80,0x00,0xFF,   // Mauve
  0xFF,0x00,0x00,   // Bright Red
  0xFF,0x00,0x80,   // Purple
  0xFF,0x00,0xFF,   // Bright Magenta
  0x00,0x80,0x00,   // Green
  0x00,0x80,0x80,   // Cyan
  0x00,0x80,0xFF,   // Sky Blue
  0x80,0x80,0x00,   // Yellow
  0x80,0x80,0x80,   // White
  0x80,0x80,0xFF,   // Pastel Blue
  0xFF,0x80,0x00,   // Orange
  0xFF,0x80,0x80,   // Pink
  0xFF,0x80,0xFF,   // Pastel Magenta
  0x00,0xFF,0x00,   // Bright Green
  0x00,0xFF,0x80,   // Sea Green
  0x00,0xFF,0xFF,   // Bright Cyan
  0x80,0xFF,0x00,   // Lime
  0x80,0xFF,0x80,   // Pastel Green
  0x80,0xFF,0xFF,   // Pastel Cyan
  0xFF,0xFF,0x00,   // Bright Yellow
  0xFF,0xFF,0x80,   // Pastel Yellow
  0xFF,0xFF,0xFF,   // Bright White

  0x00,0x00,0x80,   // Almost Blue
  0xFF,0x00,0x80,   // Almost Purple
  0x80,0x80,0x80,   // Almost White
  0x00,0xFF,0x80,   // Almost Sea Green
  0xFF,0xFF,0x80,   // Almost Pastel Yellow
};


/*********************************************************************************
 * Set Amstrad color palette... 32 colors (27 actuall)
 ********************************************************************************/
void amstrad_set_palette(void)
{
  u8 uBcl,r,g,b;

  for (uBcl=0;uBcl<32;uBcl++)
  {
    r = (u8) ((float) CPC_palette[uBcl*3+0]*0.121568f);
    g = (u8) ((float) CPC_palette[uBcl*3+1]*0.121568f);
    b = (u8) ((float) CPC_palette[uBcl*3+2]*0.121568f);

    SPRITE_PALETTE[uBcl] = RGB15(r,g,b);
    BG_PALETTE[uBcl] = RGB15(r,g,b);
  }
}


/*******************************************************************************
 * Compute the file CRC - this will be our unique identifier for the game
 * for saving HI SCORES and Configuration / Key Mapping data.
 *******************************************************************************/
void getfile_crc(const char *filename)
{
    DSPrint(11,13,6, "LOADING...");

    // ---------------------------------------------------------------
    // The CRC is used as a unique ID to save out configuration data.
    // ---------------------------------------------------------------
    file_crc = getFileCrc(filename);

    // --------------------------------------------------------------------
    // Since we are a disk-based system that might write back to the disk,
    // we have to base the master file CRC on the name of the .dsk file.
    // --------------------------------------------------------------------
    if (amstrad_mode == MODE_DSK)
    {
        file_crc = getCRC32((u8*)filename, strlen(filename));
    }

    DSPrint(11,13,6, "          ");
}


/** loadgame() ******************************************************************/
/* Open a rom file from file system and load it into the ROM_Memory[] buffer    */
/********************************************************************************/
u8 loadgame(const char *filename)
{
  u8 bOK = 0;
  int romSize = 0;

  FILE* handle = fopen(filename, "rb");
  if (handle != NULL)
  {
    // Save the initial filename and file - we need it for save/restore of state
    strcpy(initial_file, filename);
    getcwd(initial_path, MAX_FILENAME_LEN);

    // -----------------------------------------------------------------------
    // See if we are loading a file from a directory different than our
    // last saved directory... if so, we save this new directory as default.
    // -----------------------------------------------------------------------
    if (myGlobalConfig.lastDir)
    {
        if (strcmp(initial_path, myGlobalConfig.szLastPath) != 0)
        {
            SaveConfig(FALSE);
        }
    }

    // Get file size the 'fast' way - use fstat() instead of fseek() or ftell()
    struct stat stbuf;
    (void)fstat(fileno(handle), &stbuf);
    romSize = stbuf.st_size;
    fclose(handle); // We only need to close the file - the game ROM is now sitting in ROM_Memory[] from the getFileCrc() handler

    last_file_size = (u32)romSize;
  }

  return bOK;
}

void vblankIntro()
{
  vusCptVBL++;
}

// --------------------------------------------------------------
// Intro with portabledev logo and new PHEONIX-EDITION version
// --------------------------------------------------------------
void intro_logo(void)
{
  bool bOK;

  // Init graphics
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE );
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE );
  vramSetBankA(VRAM_A_MAIN_BG); vramSetBankC(VRAM_C_SUB_BG);
  irqSet(IRQ_VBLANK, vblankIntro);
  irqEnable(IRQ_VBLANK);

  // Init BG
  int bg1 = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);

  // Init sub BG
  int bg1s = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);

  REG_BLDCNT = BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0; REG_BLDY = 16;
  REG_BLDCNT_SUB = BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0; REG_BLDY_SUB = 16;

  mmEffect(SFX_MUS_INTRO);

  // Show Splash Screen
  decompress(splash_topTiles, bgGetGfxPtr(bg1), LZ77Vram);
  decompress(splash_topMap, (void*) bgGetMapPtr(bg1), LZ77Vram);
  dmaCopy((void *) splash_topPal,(u16*) BG_PALETTE,256*2);

  decompress(splashTiles, bgGetGfxPtr(bg1s), LZ77Vram);
  decompress(splashMap, (void*) bgGetMapPtr(bg1s), LZ77Vram);
  dmaCopy((void *) splashPal,(u16*) BG_PALETTE_SUB,256*2);

  FadeToColor(0,BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0,3,0,3);

  bOK=false;
  while (!bOK) { if ( !(keysCurrent() & 0x1FFF) ) bOK=true; } // 0x1FFF = key or touch screen
  vusCptVBL=0;bOK=false;
  while (!bOK && (vusCptVBL<3*60)) { if (keysCurrent() & 0x1FFF ) bOK=true; }
  bOK=false;
  while (!bOK) { if ( !(keysCurrent() & 0x1FFF) ) bOK=true; }

  FadeToColor(1,BLEND_FADE_WHITE | BLEND_SRC_BG0 | BLEND_DST_BG0,3,16,3);
}

// -------------------------------------------
// Not needed presently for the Amstrad CPC
// -------------------------------------------
void PatchZ80(register Z80 *r)
{
}

// -----------------------------------------------------------------
// Trap and report illegal opcodes to the SugarDS debugger...
// -----------------------------------------------------------------
void Trap_Bad_Ops(char *prefix, byte I, word W)
{
    if (myGlobalConfig.debugger)
    {
        char tmp[32];
        sprintf(tmp, "ILLOP: %s %02X %04X", prefix, I, W);
        DSPrint(0,0,6, tmp);
    }
}

// Needed for the printf.h replacement library
void _putchar(char character) {}

// End of file
