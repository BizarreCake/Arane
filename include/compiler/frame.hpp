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

#ifndef _ARANE__COMPILER__FRAME__H_
#define _ARANE__COMPILER__FRAME__H_

#include "parser/ast.hpp"
#include <string>
#include <vector>
#include <unordered_map>


namespace arane {
  
  enum variable_type
  {
    VT_NONE,
    VT_INT_NATIVE,
    VT_INT,
  };
  
  variable_type tn_type_to_var_type (ast_typename_type type);
  
  
  struct variable
  {
    int index;
    std::string name;
    variable_type type;
  };
  
  
  enum frame_type
  {
    FT_SUBROUTINE,
    FT_BLOCK,
    FT_LOOP,  // refers to loop statements such as `while', `for', `loop', etc...
              // but not just `loop'!
  };
  
  enum frame_sub_type
  {
    FST_WHILE,
    FST_FOR,
    FST_LOOP,
  };
  
  
  /* 
   * Frames keep track of all local variables and arguments in scope, and
   * are created inside a subroutine's body.
   */
  class frame
  {
    frame_type type;
    frame *parent;
    
    int next_loc_index;
    std::vector<variable> locs;
    std::unordered_map<std::string, int> loc_map;
    
    std::vector<variable> args;
    std::unordered_map<std::string, int> arg_map;
    
  public:
    std::unordered_map<std::string, int> extra;
    
  private:
    int get_next_loc_index ();
    
  public:
    inline frame_type get_type () const { return this->type; }
    inline frame* get_parent () const { return this->parent; }
    
  public:
    frame (frame_type type, frame *parent = nullptr);
    
  public:
    /* 
     * Returns the local variable whose name matches the one specified, or
     * nullptr if not found.
     */
    variable* get_local (const std::string& name);
    
    /* 
     * Returns the argument whose name matches the one specified, or nullptr
     * if not found.
     */
    variable* get_arg (const std::string& name);
    
    
    
    /* 
     * Inserts a new local variable to the frame's local variable list.
     */
    void add_local (const std::string& name, variable_type typ = VT_NONE);
    
    /* 
     * Inserts a new argument to the frame's argument list.
     */
    void add_arg (const std::string& name, variable_type typ = VT_NONE);
    
    
    
    /* 
     * Allocates an anonymous local variable and returns its index.
     */
    int alloc_local ();
  };
}

#endif

