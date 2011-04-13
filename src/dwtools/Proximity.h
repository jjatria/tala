#ifndef _Proximity_h_
#define _Proximity_h_
/* Proximity.h
 *
 * Copyright (C) 1993-2011 David Weenink
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 djmw 20020813 GPL header
 djmw 20110306 Latest modification.
*/

#ifndef _TableOfReal_h_
	#include "stat/TableOfReal.h"
#endif

#ifdef __cplusplus
	extern "C" {
#endif

#define Proximity_members TableOfReal_members
#define Proximity_methods TableOfReal_methods
class_create (Proximity, TableOfReal);

int Proximity_init (I, long numberOfPoints);

#ifdef __cplusplus
	}
#endif

#endif /* _Proximity_h_ */
