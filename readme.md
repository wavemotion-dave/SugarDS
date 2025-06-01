# SugarDS
![image](./arm9/gfx_data/splash.png)
![image](./png/keyboard.png)

SugarDS is an Amstrad CPC 646 and 6128 Emulator for the DS/DSi

Features :
-----------------------
* Emulates CPC 464 (64K) and CPC 6128 (128K)
* Loads .SNA and .DSK files up to 1024K total length
* Plus2CPC Cartridge Support to load .CPR files up to 512K
* Emulates CRTC Type 3 roughly - with provisions to handle split screen, rupture, smooth vertical scroll and a reasonable facsimile of smooth horizontal scroll
* Save / Load state so you can pick up where you left off

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
Coming soon - and there's LOTs to talk about!

Key Mapping :
-----------------------
Shoulder button L + D-Pad for offset
Shoulder button R + D-Pad for scale (turn off auto-scale)

DISK Support :
-----------------------
.DSK files up to the maximum allowed by 3.5" drives using PARADOS is roughly 720K.

Cartridge Support :
-----------------------
Although .CPR cartridges up to 512K are supported, this is not a CPC+ (plus) emulator.

SNA Support :
-----------------------
TBD

Known Issues :
-----------------------
Prehistorik II has major graphical glitches.
Pinball Dreams has graphical glitches on the opening lead-in screens... gameplay is fine.


Version History :
-----------------------

Version 0.6 - 02-Jun-2025 by wavemotion-dave
* Cartridge .CPR support up to 512K

Version 0.5 - 01-Jun-2025 by wavemotion-dave
* First public beta. Enjoy!
