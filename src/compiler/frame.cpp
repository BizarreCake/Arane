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

#include "compiler/frame.hpp"


namespace arane {
  
  variable_type
  tn_type_to_var_type (ast_typename_type type)
  {
    switch (type)
      {
      case AST_TN_INT_NATIVE:     return VT_INT_NATIVE;
      case AST_TN_INT:            return VT_INT;
      
      default:
        return VT_NONE;
      }
  }
  
  
  
  frame::frame (frame_type type, frame *parent)
  {
    this->type = type;
    this->parent = parent;
    this->next_loc_index = 0;
  }
  
  
  
  int
  frame::get_next_loc_index ()
  {
    if (this->parent && this->type != FT_SUBROUTINE)
      return this->parent->get_next_loc_index ();
    
    return this->next_loc_index++;
  }
  
  
  
  /* 
   * Returns the local variable whose name matches the one specified, or
   * nullptr if not found.
   */
  variable*
  frame::get_local (const std::string& name)
  {
    auto itr = this->loc_map.find (name);
    if (itr == this->loc_map.end ())
      {
        if (this->parent)
          return this->parent->get_local (name);
        else
          return nullptr;
      }
    return &this->locs[itr->second];
  }
  
  /* 
   * Returns the argument whose name matches the one specified, or nullptr
   * if not found.
   */
  variable*
  frame::get_arg (const std::string& name)
  {
    // only applies to subroutines
    if (this->type != FT_SUBROUTINE)
      {
        if (this->parent)
          return this->parent->get_arg (name);
        return nullptr;
      }
    
    auto itr = this->arg_map.find (name);
    if (itr == this->arg_map.end ())
      return nullptr;
    return &this->args[itr->second];
  }
  
  
  
  /* 
   * Inserts a new local variable to the frame's local variable list.
   */
  void
  frame::add_local (const std::string& name, variable_type typ)
  {
    int index = this->locs.size ();
    this->locs.push_back ({
      .index = this->get_next_loc_index (),
      .name = name,
      .type = typ,
    });
    
    this->loc_map[name] = index;
  }
  
  /* 
   * Inserts a new argument to the frame's argument list.
   */
  void
  frame::add_arg (const std::string& name, variable_type typ)
  {
    int index = this->args.size ();
    this->args.push_back ({
      .index = index,
      .name = name,
      .type = typ,
    });
    
    this->arg_map[name] = index;
  }
  
  
  
  /* 
   * Allocates an anonymous local variable and returns its index.
   */
  int
  frame::alloc_local ()
  {
    return this->get_next_loc_index ();
  }
}

