/*
 * MobiAqua Media Player
 *
 * Copyright (C) 2013 Pawel Kolodziejski <aquadran at users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef TYPEDEFS_H
#define TYPEDEFS_H

typedef unsigned char		U8;
typedef signed char			S8;

typedef unsigned short		U16;
typedef signed short		S16;

typedef unsigned int		U32;
typedef signed int			S32;

typedef unsigned long long	U64;
typedef signed long long	S64;

typedef int					STATUS;
#define S_OK				1
#define S_FAIL				0

typedef int					bool;
#define true				1
#define false				0

#undef NULL
#define NULL				((void *)0)

#ifndef MIN
#define MIN(a, b)			((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b)			((a) > (b) ? (a) : (b))
#endif

#define SIZE_OF_ARRAY(a) 	((int)(sizeof(a) / sizeof(a[0])))

#endif
