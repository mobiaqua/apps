/*
 * MobiAqua Media Player
 *
 * Copyright (C) 2015 Pawel Kolodziejski <aquadran at users.sourceforge.net>
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

#ifndef YUV420_H
#define YUV420_H

#include "basetypes.h"

namespace MediaPLayer {

typedef struct {
	U32 w, h, dx, dy, dw, dh, y_stride, uv_stride;
} yuv420FrameInfo;

void yuv420_to_rgba_open(yuv420FrameInfo *dst, yuv420FrameInfo *src);
int yuv420_to_rgba_convert(U8 *vdst[], U8 *vsrc[], U8 *, U8 *);

void yuv420_to_nv12_open(yuv420FrameInfo *dst, yuv420FrameInfo *src);
int yuv420_to_nv12_convert(U8 *vdst[], U8 *vsrc[], U8 *, U8 *);

void yuv420_to_yuv422_open(yuv420FrameInfo *dst, yuv420FrameInfo *src);
int yuv420_to_yuv422_convert(U8 *vdst[], U8 *vsrc[], U8 *, U8 *);

} // namespace

#endif
