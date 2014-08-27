/*
 * Arane - A Perl 6 interpreter.
 * Copyright (C) 2014 Jacob Zhitomirsky
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ARANE__COMPILER__CONTEXT__H_
#define _ARANE__COMPILER__CONTEXT__H_

namespace arane {
  
  // forward decs:
  class frame;
  class package;
  
  
  /* 
   * The purpose of a compilation context is to save a "snapshot" of the state
   * of the compiler to be used to later.
   * 
   * When compiling subroutine calls, in order to optimize dynamic type checks
   * away if the types of the parameters are known beforehand, it is necessary
   * to defer this step to the end of the compilation phase so that all
   * subroutines are processed and their signatures available.  To do that, the
   * parameters are compiled at the end, once all frames and packages are no
   * longer in scope, and is why a context is saved.
   */
  struct compilation_context
  {
    std::deque<frame *> frms;
    
    std::deque<package *> packs;
  };
}

#endif

