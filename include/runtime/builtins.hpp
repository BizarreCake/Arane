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

#ifndef _ARANE__RUNTIME__BUILTINS__H_
#define _ARANE__RUNTIME__BUILTINS__H_


namespace arane {
  
  class virtual_machine;
  
  /* 
   * Builtin funtions used by the virtual machine.
   */
  class builtins
  {
  public:
    /* 
     * IO:
     */
    static void print (virtual_machine& vm, int param_count);
    static void say (virtual_machine& vm, int param_count);
    
    /* 
     * Arrays:
     */
    static void elems (virtual_machine& vm, int param_count);
    static void push (virtual_machine& vm, int param_count);
    static void pop (virtual_machine& vm, int param_count);
    static void shift (virtual_machine& vm, int param_count);
    static void range (virtual_machine& vm, int param_count);
  };
} 

#endif

