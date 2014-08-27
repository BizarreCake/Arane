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

#ifndef _ARANE__COMPILER__PACKAGE__H_
#define _ARANE__COMPILER__PACKAGE__H_

#include "compiler/sub.hpp"
#include <string>
#include <vector>
#include <unordered_map>


namespace arane {
  
  enum package_type
  {
    PT_PACKAGE,
    PT_MODULE,
  };
  
  class compiler;
  
  /* 
   * Keeps track of the current package/namespace.
   * Functions very similarily to a frame.
   */
  class package
  {
    compiler& comp;
    
    package_type type;
    package *parent;
    std::vector<package *> children;
    
    std::string name;
    
    std::vector<subroutine_info> subs;
    std::unordered_map<std::string, int> sub_map;
    
  public:
    inline package_type get_type () const { return this->type; }
    inline package* get_parent () const { return this->parent; }
    inline std::vector<package *>& get_children () { return this->children; }
    inline const std::string& get_name () const { return this->name; }
    
    inline std::vector<subroutine_info>& get_subs () { return this->subs; }
    
  public:
    package (compiler& comp, package_type type, const std::string& name, package *parent);
    ~package ();
    
  public:
    /* 
     * Returns the full path to this module.
     */
    std::string get_path ();
    
    
    
    /* 
     * Finds and returns the subroutine info structure associated with the
     * subroutine whose name matches the one specified, or null if not found.
     */
    subroutine_info* find_sub (const std::string& name);
    
    /* 
     * Returns the subroutine info structure associated with the subroutine
     * whose name matches the one specified.
     * If not found, one will be created (with marked set to false).
     */
    subroutine_info& get_sub (const std::string& name);
    
    
    
    /* 
     * Marks the specified package as a subpackage of this one.
     */
    void add_subpackage (package *pack);
    
    /* 
     * Returns the subpackage that contains the object given by the specified
     * relative path.
     */
    package* get_subpackage_containing (const std::string& path);
  };
}

#endif

