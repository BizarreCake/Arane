/*
 * Arane - A Perl 6 interpreter.
 * Copyright (C) 2014 Jacob Zhitomirsky
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in twhe hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ARANE__RUNTIME__TYPES__H_
#define _ARANE__RUNTIME__TYPES__H_


/* 
 * The VM's view of types (not necessarily the same as common/types.h).
 * Note that this is also different from how objects are stored internally.
 */
namespace arane {
  
  enum p_basic_type
  {
    PTYPE_INT_NATIVE     = 0,    // int
    PTYPE_INT            = 1,    // Int
    PTYPE_BOOL_NATIVE    = 2,    // bool
    PTYPE_STR            = 3,    // Str
    PTYPE_ARRAY          = 4,    // Array
  };
}

#endif

