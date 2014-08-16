/*
 * P6 - A Perl 6 interpreter.
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
 * You should have received a copy of the GNwU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "runtime/gc.hpp"
#include "runtime/vm.hpp"

#include <iostream> // DEBUG


namespace p6 {
  
  enum: int
  {
    GC_WHITE,
    GC_GRAY,
    GC_BLACK,
  };
  
  
  static void
  _delete_data (p_value *obj)
  {
    switch (obj->type)
      {
      case PERL_DSTR:
        delete[] obj->val.str.data;
        break;
      
      case PERL_ARRAY:
        delete[] obj->val.arr.data;
        break;
      
      default: ;
      }
  }
  
  
  
  /* 
   * Constructs a garbage collector on top of the specified runtime stack.
   * The given stack will function as the collector's "root" set.
   */
  garbage_collector::garbage_collector (virtual_machine& vm)
    : vm (vm)
  {
    this->counter = 0;
  }
  
  garbage_collector::~garbage_collector ()
  {
    return;
    for (p_value *obj : this->objs)
      {
        _delete_data (obj);
        delete obj;
      }
  }
  
  
  
  static p_value*
  _get_data (p_value& val)
  {
    if (val.type == PERL_REF)
      return val.val.ref;
    return nullptr;
  }
  
  
  
  /* 
   * Moves all white objects the specified object references to the gray set.
   */
  void
  garbage_collector::paint_references (p_value *obj)
  {
    switch (obj->type)
      {
      case PERL_ARRAY:
        {
          auto& arr = obj->val.arr;
          for (unsigned int i = 0; i < arr.len; ++i)
            {
              p_value *data = _get_data (arr.data[i]);
              if (data)
                {
                  this->gray.push_back (data);
                  this->white.erase (data);
                }
            }
        }
        break;
      
      case PERL_REF:
        {
          p_value *data = obj->val.ref;
          if (data)
            {
              this->gray.push_back (data);
              this->white.erase (data);
            }
        }
        break;
      
      default: ;
      }
  }
  
  
  
  /* 
   * Performs a full garbage collection.
   */
  void
  garbage_collector::collect ()
  {
    // 
    // Setup.
    // 
    
    // the white set is initally filled with all tracable objects.
    this->white.clear ();
    for (p_value *val : this->objs)
      {
        this->white.insert (val);
      }
    
    // the gray set is populated with objects from the root set.
    int sp = this->vm.sp;
    this->gray.clear ();
    
    // root set: stack
    for (int i = 0; i < sp; ++i)
      {
        auto data = _get_data (this->vm.stack[i]);
        if (data)
          {
            this->gray.push_back (data);
            this->white.erase (data);
          }
      }
    
    // root set: globals
    for (auto p : this->vm.globs)
      {
        auto data = _get_data (p.second);
        if (data)
          {
            this->gray.push_back (data);
            this->white.erase (data);
          }
      }
    
    // the black set is initally empty.
    this->black.clear ();
    
    // 
    // Mark.
    // The process is as follows:
    //     1. Pick a gray object.
    //     2. Move it to the black set.
    //     3. Move all white objects it references to the gray set.
    //     4. Continue the process until no gray objects are left.
    // 
    while (!this->gray.empty ())
      {
        p_value *gobj = this->gray.back ();
        this->gray.pop_back ();
        this->black.push_back (gobj);
        this->paint_references (gobj);
      }
    
    // 
    // Sweep.
    // 
    for (p_value *val : this->white)
      {
        _delete_data (val);
        delete val;
      }
    this->white.clear ();
    
    this->objs = this->black;
    this->black.clear ();
  }
  
  
  
   /* 
   * Registers the specified object with the garbage collector.
   */
  void
  garbage_collector::register_object (p_value *obj)
  {
    this->objs.push_back (obj);
    if (this->counter++ >= 200)
      {
        this->counter = 0;
        this->collect ();
      }
  }
}

