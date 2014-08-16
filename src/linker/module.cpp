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

#include "linker/module.hpp"
#include <stdexcept>


namespace p6 {
  
  module::module (const std::string& name)
    : name (name)
    { }
  
  module::~module ()
  {
    for (module_section *sect : this->sects)
      delete sect;
  }
  
  
  
  /* 
   * Inserts a new section with the specified name and returns a pointer
   * to it.
   */
  module_section*
  module::add_section (const std::string& name)
  {
    auto itr = this->sect_names.find (name);
    if (itr != this->sect_names.end ())
      throw std::runtime_error ("section already exists");
    
    module_section *sect = new module_section (name);
    
    size_t index = this->sects.size ();
    this->sects.push_back (sect);
    this->sect_names[name] = index;
    
    return sect;
  }
  
  /* 
   * Returns the section whose name matches the one specified, or null if not
   * found.
   */
  module_section*
  module::get_section (const std::string& name)
  {
    auto itr = this->sect_names.find (name);
    if (itr == this->sect_names.end ())
      return nullptr;
    
    return this->sects[itr->second];
  }
  
  
  
  /* 
   * Inserts a subroutine into the exported subroutine list.
   */
  void
  module::export_sub (const std::string& name, unsigned int pos)
  {
    this->exsubs.push_back ({
      .name = name,
      .pos = pos,
    });
  }
  
  /* 
   * Inserts the specified subroutine import into the module.
   */    
  void
  module::import_sub (const std::string& name, unsigned int pos)
  {
    this->imsubs.push_back ({
      .name = name,
      .pos = pos,
    });
  }
  
  /* 
   * Inserts the specified relocatino into the relocation list.
   */
  void
  module::add_reloc (relocation reloc)
  {
    this->relocs.push_back (reloc);
  }
  
  /* 
   * Inserts a dependency for the specified module name.
   */
  void
  module::add_dependency (const std::string& mod_name)
  {
    this->deps.insert (mod_name);
  }
}

