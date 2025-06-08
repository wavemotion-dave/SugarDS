# SugarDS
![image](./arm9/gfx_data/splash.png)
![image](./png/keyboard.png)

SugarDS is an Amstrad CPC 646 and 6128 Emulator for the DS/DSi

Features :
-----------------------
* Emulates CPC 464 (64K) and CPC 6128 (128K) with 512K of extended RAM available.
* Loads .SNA and .DSK files up to 1024K total length (single and double sided).
* Plus2CPC Cartridge Support to load .CPR files up to 512K.
* Partial Dandanator Cartridge Support - Enough for Sword of Ianna and Los Amores de Brunilda. Rename files to .dan to load.
* Emulates CRTC Type 3 roughly - with provisions to handle split screen, rupture, smooth vertical scroll and a reasonable facsimile of smooth horizontal scroll.
* Full button mapping - supporting all 3 possible joystick buttons of the Amstrad as well as mapping buttons to keyboard keys.
* Full touch-screen Amstrad keyboard styled after the colorful CPC 464.
* Save / Load state so you can pick up where you left off.

Copyright :
-----------------------
SugarDS is Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)

Bits of pieces of this emulator have been glued and attached from 
a large number of sources - along with a healthy amount of code by 
this author to pull it all together. Although it was hard to trace 
everythign to the original sources, I believe all sources have 
released their material under the GNU General Public License and, 
as such, the SugarDS emulator follows suit.

If I've misrepresented any pervious authors work here, please 
contact me and I'll put it right.

* CrocoDS: CPC Emulator for the DS - Copyright (c) 2013 Miguel Vanhove (Kyuran)
* Win-CPC: Amstrad CPC Emulator - Copyright (c) 2012 Ludovic Deplanque.
* Caprice32: Amstrad CPC Emulator - Copyright (c) 1997-2004 Ulrich Doewich.
* Arnold: Amstrad CPC Emulator - Copyright (c) 1995-2002, 2007 Andreas Micklei and Kevin Thacker

As far as I'm concerned, you can use this code in whatever way suits you provided you 
continue to release the sources under the original copyright notice (see below) which
appeared to be the intention of all the pioneers who came before me.

The sound driver (ay38910) are libraries from FluBBa (Fredrik Ahlström) 
and those copyrights remain his.

Royalty Free Music for the opening jingle provided by Anvish Parker

lzav compression (for save states) is Copyright (c) 2023-2025 Aleksey 
Vaneev and used by permission of the generous MIT license.

The Amstrad CPC logo is used without permission but with the maximum
of respect and love.

The SugarDS emulator is offered as-is, without any warranty.


BIOS Files :
-----------------------
Following in the footsteps of virtually all other Amstrad CPC emulators, the BIOS
files are included in the emulator and don't need to be sourced by the user. This
is according to a note from Cliff Lawson:

"If you are the author of such an emulator then you don't need to write and ask me for 
Amstrad's permission to distribute copies of the CPC ROMs. Amstrad's stance on this is
that we are happy for you to redistribute copies of our copyrighted code as long as 
a) copyright messages are not changed and 
b) either in the program or the documentation you acknowledge that "Amstrad has kindly 
given it's permission for it's copyrighted material to be redistributed but Amstrad 
retains it's copyright."

So the Amstrad CPC BIOS files are still copyrighted by Amstrad - in whatever form
that may exist. If you are in an official capacity as the copyright owner of the 
BIOS files and wish these stripped from the emulator, please contact me and I'm 
happy to do so!  Otherwise - many thanks for the indirect permission to use them.


## Original Copyright Notice for the Software (not BIOS files)
```
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
```


Configuration :
-----------------------
There is quite a few configuration options that you can explore. Some options are global for the emulator
and some are specific to one game. However, of primary importance are these two which you must understand:

* V52 Sync
* CPU Adjust

Normally you should touch neither adjustment... unless the game is not running correct. In that case, try the following:

If the game is running slower than on real hardware (e.g. Dizzy III which slows down when you move) then adjust the V52 Sync to 'Forgiving'.

If the game has minor graphical glitches - say a stray line (e.g. Sword of Ianna on screen transitions) then try a CPU Adjust of -1 or -2.

Key Mapping :
-----------------------
Shoulder button L + D-Pad for offset
Shoulder button R + D-Pad for scale (turn off auto-scale)

It's critical that you use the L/R buttons to shift/scale the screen as the Amstrad
CPC will often utilize more pixels than the DS has resolution so you will be forced
to either compress (squash) the screen or else position the screen carefully and map
one of the buttons to PAN UP or PAN DOWN the screen briefly - this works really well
for games that show status or score at the top or bottom of the screen but otherwise
doesn't really affect the main gameplay area. 

DISK Support :
-----------------------
.DSK files up to the maximum allowed by 3.5" drives using PARADOS is roughly 720K. Most
disks should auto-load but if your disk does not, it should provide a catalog of the
possible filenames that could be used to run the program. One trick is to include
the command you want to run in the filename of the .DSK file itself. That helps the
auto-type detection algorithm. To force a very specific run command, place the name
of the Amstrad CPC file you wish to run in double brackets like this:

"Orion Prime 3_5 Inch Disk [[ORION]].dsk"

This when that .dsk file is loaded, the emulator will sense what's between the double
brackets and issue the proper RUN "ORION" command

Cartridge Support :
-----------------------
Although .CPR cartridges up to 512K are supported, this is not a CPC+ (plus) emulator.
So games like ALCON 2020 work fine... as do any other .CPR games that do not specifically
utilize the GX4000 or Amstrad CPC+ (plus) graphics capabilities. 

Dandanator Support :
-----------------------
Mainly to run two games - Sword of Ianna and Los Amores de Brunilda. Rename those .rom
files as .dan files so that they will be loaded as Dandanator files within the SugarDS
emulator. Note that we do not support Flash writes on these carts - but the games
themselves should be playable and you can use the normal emulator save/load states
to save your progress.

SNA Support :
-----------------------
Memory snapshots are supported for both 64K and 128K machines. You should strongly prefer
to use .DSK or .CPR (or .DAN) files as snapshots cannot save data nor can they multi-load.

Known Issues :
-----------------------
* Prehistorik II has major graphical glitches.
* Pinball Dreams has graphical glitches on the opening lead-in screens... gameplay is better.
* Dizzy III requires that you set the 'V52 Sync' option to 'Forgiving' so it plays at the right speed.


Version History :
-----------------------

Version 0.9 - 08-Jun-2025 by wavemotion-dave
* Fix disk write-back so it doesn't potentially corrupt a .dsk file! Sorry about that.
* Improved save/load state so it preserves Dandanator carts and Extended Memory > 128K.
* More robust CRTC handling to fix games like Hypernoid Zero, Galactic Tomb 128K and Bomb Jack Extra Sugar.
* You can now put [[cmd]] in the title of the .dsk file to force a RUN command.
* Other cleanups, minor timing improvements and fixes as time allowed.

Version 0.8 - 06-Jun-2025 by wavemotion-dave
* Tweaks to the Z80 core timing to get it closer to real Amstrad performance. Fewer graphical glitches in games.
* Improved two-sided disk support.
* Memory expansion up to 512K is now supported - Mighty Steel Fighters now runs.
* Dandanator support for cart-banking (only - no EEPROM writes).
* Screen dimming of bottom screen added.
* Other minor cleanups and fixes as time allowed.

Version 0.7 - 04-Jun-2025 by wavemotion-dave
* Double sided disks now supported. Orion Prime in 3.5" floppy works!
* Disks now auto-persist back to the SD card when written.
* CPU Z80 timing improved - more games run more correctly.
* Better auto-filename support for launching disk games.
* Number pad added - press the number pad icon on the main keyboard.
* Mode 2 support improved - use PAN or COMPRESSED mode and upper screen region (the Amstrad Logo area) to pan.
* New Amstrad Croc top screen available - use Global Settings.
* Improved auto-scale in the X (horizontal) direction.
* Minor tweaks and bug fixes as time allowed.

Version 0.6 - 02-Jun-2025 by wavemotion-dave
* Cartridge .CPR support up to 512K
* Disk support is now up to 3.5" 80-track maximums (720K)
* Multi-disk support added - you can swap disks mid-game via the menu
* Cleanup of Z80 timings so more games run more correctly with fewer graphical glitching

Version 0.5 - 01-Jun-2025 by wavemotion-dave
* First public beta. Enjoy!
