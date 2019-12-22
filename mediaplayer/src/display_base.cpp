/*
 * MobiAqua Media Player
 *
 * Copyright (C) 2013-2019 Pawel Kolodziejski
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <assert.h>

#include "basetypes.h"
#include "display_base.h"
#include "display_fbdev.h"

namespace MediaPLayer {

Display::Display() :
		_initialized(false) {
}

Display *CreateDisplay(DISPLAY_TYPE displayType) {
	switch (displayType) {
	case DISPLAY_FBDEV:
		return new DisplayFBDev();
	default:
		return nullptr;
	}
}

} // namespace
