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

#include "compiler/package.hpp"
#include "compiler/compiler.hpp"
#include "compiler/codegen.hpp"
#include "common/utils.hpp"
#include <stack>
#include <stdexcept>


namespace arane {
  
  package::package (compiler& comp, package_type type, const std::string& name,
    package *parent)
    : comp (comp), name (name)
  {
    this->type = type;
    this->parent = parent;
  }
  
  
  
  /* 
   * Returns the full path to this module.
   */
  std::string
  package::get_path ()
  {
    std::stack<package *> stk;
    
    package *pack = this;
    while (pack)
      {
        stk.push (pack);
        pack = pack->get_parent ();
      }
    
    std::string path;
    stk.pop (); // pop global package (no need for `GLOBAL::')
    while (!stk.empty ())
      {
        package *pack = stk.top ();
        stk.pop ();
        
        path.append (pack->get_name ());
        if (!stk.empty ())
          path.append ("::");
      }
    
    return path;
  }
  
  static package*
  _follow_package_chain (package *top, const std::string& path)
  {
    package *pack = top;
    
    std::string::size_type prev_col = 0, next_col;
    while ((next_col = path.find ("::", prev_col)) != std::string::npos)
      {
        // extract package name
        std::string pack_name = path.substr (prev_col, next_col - prev_col);
        
        // find this module
        package *child = nullptr;
        auto& children = pack->get_children ();
        for (package *p : children)
          if (p->get_name () == pack_name)
            {
              child = p;
              break;
            }
        if (!child)
          return nullptr;
          
        pack = child;
        prev_col = next_col + 2;
      }
    
    return pack;
  }
  
  
  
  /* 
   * Finds and returns the subroutine info structure associated with the
   * subroutine whose name matches the one specified, or null if not found.
   */
  subroutine_info*
  package::find_sub (const std::string& name)
  {
    if (name.find ("::") != std::string::npos)
      {
        package *pack = _follow_package_chain (this, name);
        if (!pack)
          return nullptr;
        return pack->find_sub (utils::strip_packages (name));
      }
    
    auto itr = this->sub_map.find (name);
    if (itr != this->sub_map.end ())
      return &this->subs[itr->second];
    
    if (this->parent)
      return this->parent->find_sub (name);
    return nullptr;
  }
  
  /* 
   * Returns the subroutine info structure associated with the subroutine
   * whose name matches the one specified.
   * If not found, one will be created (with marked set to false).
   */
  subroutine_info&
  package::get_sub (const std::string& name)
  {
    if (name.find ("::") != std::string::npos)
      {
        package *pack = _follow_package_chain (this, name);
        if (!pack)
          throw std::runtime_error ("get_sub: package not found");
        return pack->get_sub (utils::strip_packages (name));
      }
    
    subroutine_info *sub = this->find_sub (name);
    if (sub)
      return *sub;
    
    unsigned int index = this->subs.size ();
    this->subs.push_back ({
      .marked = false,
      .lbl = this->comp.cgen->create_label (),
      .name = name,
    });
    
    this->sub_map[name] = index;
    return this->subs.back ();
  }
  
  
  
  /* 
   * Marks the specified package as a subpackage of this one.
   */
  void
  package::add_subpackage (package *pack)
  {
    this->children.push_back (pack);
  }
  
  
  /* 
   * Returns the subpackage that contains the object given by the specified
   * relative path.
   */
  package*
  package::get_subpackage_containing (const std::string& path)
  {
    return _follow_package_chain (this, path);
  }
}

