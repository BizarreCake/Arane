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

#ifndef _P6__RUNTIME__GC__H_
#define _P6__RUNTIME__GC__H_

#include "runtime/value.hpp"
#include <unordered_set>
#include <vector>
#include <list>


namespace p6 {
  
  class virtual_machine;
  
  /* 
   * A pretty simple tri-color marking garbage collector.
   * A more efficient incremental generational moving garbage collector is
   * planned.
   */
  class garbage_collector
  {
    virtual_machine& vm;
    
    std::vector<p_value *> objs;
    std::unordered_set<p_value *> white;
    std::vector<p_value *> gray;
    std::vector<p_value *> black;
    
    unsigned int counter;
    
  public:
    /* 
     * Constructs a garbage collector on top of the specified runtime stack.
     * The given stack will function as the collector's "root" set.
     */
    garbage_collector (virtual_machine& vm);
    
    ~garbage_collector ();
    
  private:
    /* 
     * Moves all white objects the specified object references to the gray set.
     */
    void paint_references (p_value *obj);
    
  public:
    /* 
     * Performs a full garbage collection.
     */
    void collect ();
    
    /* 
     * Registers the specified object with the garbage collector.
     */
    void register_object (p_value *obj);
  };
}

#endif

