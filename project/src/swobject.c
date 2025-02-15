// Emacs style mode select -*- C++ -*-
//---------------------------------------------------------------------------
//
// $Id$
//
// Copyright(C) 1984-2000 David L. Clark
// Copyright(C) 2001-2005 Simon Howard
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2 of the License, or (at your
// option) any later version. This program is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
// the GNU General Public License for more details. You should have
// received a copy of the GNU General Public License along with this
// program; if not, write to the Free Software Foundation, Inc., 59 Temple
// Place - Suite 330, Boston, MA 02111-1307, USA.
//
//---------------------------------------------------------------------------
//
//        swobject -      SW object allocation and deallocation
//
//---------------------------------------------------------------------------

#include "sw.h"
#include "swmain.h"
#include "swobject.h"

OBJECTS *allocobj(void)
{
	OBJECTS *ob;

	if (!objfree) {
		return NULL;
	}

	ob = objfree;
	objfree = ob->ob_next;

	ob->ob_next = NULL;
	ob->ob_prev = objbot;

	if (objbot) {
		objbot->ob_next = ob;
	} else {
		objtop = ob;
	}

	ob->ob_sound = NULL;
	ob->ob_drwflg = 0;
	ob->ob_onmap = FALSE;

	objbot = ob;

	return ob;
}



void deallobj(OBJECTS * obp)
{
	OBJECTS *ob=obp;
	OBJECTS *obb = ob->ob_prev;

	if (obb) {
		obb->ob_next = ob->ob_next;
	} else {
		objtop = ob->ob_next;
	}

	obb = ob->ob_next;

	if (obb) {
		obb->ob_prev = ob->ob_prev;
	} else {
		objbot = ob->ob_prev;
	}

	ob->ob_next = 0;
	if (delbot) {
		delbot->ob_next = ob;
	} else {
		deltop = ob;
	}

	delbot = ob;


}

//---------------------------------------------------------------------------
//
// $Log$
// Revision 1.4  2005/04/29 19:25:28  fraggle
// Update copyright to 2005
//
// Revision 1.3  2004/10/15 18:51:24  fraggle
// Fix the map. Rename dispworld to dispmap as this is what it really does.
//
// Revision 1.2  2003/06/08 02:39:25  fraggle
// Initial code to remove XOR based drawing
//
// Revision 1.1.1.1  2003/02/14 19:03:17  fraggle
// Initial Sourceforge CVS import
//
//
// sdh 14/2/2003: change license header to GPL
// sdh 21/10/2001: rearranged headers, added cvs tags
// sdh 21/10/2001: reformatted with indent. adjusted some code by hand
//                 to make more readable
// sdh 19/10/2001: removed externs (now in headers)
// sdh 18/10/2001: converted all functions to ANSI-style arguments
//
//
// 87-03-09        Microsoft compiler.
// 84-10-31        Atari
// 84-06-12        PCjr Speed-up
// 84-02-07        Development
//
//---------------------------------------------------------------------------

