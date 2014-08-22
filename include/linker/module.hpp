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

#ifndef _P6__LINKER__MODULE__H_
#define _P6__LINKER__MODULE__H_

#include "common/byte_buffer.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>


namespace p6 {
  
  struct module_section
  {
    std::string name;
    byte_buffer data;
    
  public:
    module_section (const std::string& name)
      : name (name)
      { }
  };
  
  
  struct exported_sub
  {
    std::string name;
    unsigned int pos; // the location of the subroutine
  };
  
  struct imported_sub
  {
    std::string name;
    unsigned int pos; // the location of the call offset
    
    int ln, col; // position in code
  };
  
  
  enum relocation_type
  {
    REL_CODE,
    REL_DATA_CSTR,
  };
  
  /* 
   * An absolute relocation.
   */
  struct relocation
  {
    relocation_type type;
    unsigned int pos;
    unsigned int dest;
    unsigned char size;
  };
  
  
  /* 
   * Represents a single compilation unit.
   * Modules are generated as output by the compiler, and accepted as input by
   * the linker. 
   */
  class module
  {
    std::string name;
    std::vector<module_section *> sects;
    std::unordered_map<std::string, size_t> sect_names;
    
    std::unordered_set<std::string> deps;
    std::vector<exported_sub> exsubs;
    std::vector<imported_sub> imsubs;
    std::vector<relocation> relocs;
    
  public:
    inline const std::string& get_name () const { return this->name; }
    inline std::unordered_set<std::string>& get_dependencies () { return this->deps; }
    inline std::vector<imported_sub>& get_imported_subs () { return this->imsubs; }
    inline std::vector<exported_sub>& get_exported_subs () { return this->exsubs; }
    inline std::vector<relocation>& get_relocations () { return this->relocs; }
    
  public:
    module (const std::string& name);
    ~module ();
    
  public:
    /* 
     * Inserts a new section with the specified name and returns a pointer
     * to it.
     */
    module_section* add_section (const std::string& name);
    
    /* 
     * Returns the section whose name matches the one specified, or null if not
     * found.
     */
    module_section* get_section (const std::string& name);
    
    
    
    /* 
     * Inserts a subroutine into the exported subroutine list.
     */
    void export_sub (const std::string& name, unsigned int pos);
    
    /* 
     * Inserts the specified subroutine import into the module.
     */    
    void import_sub (const std::string& name, unsigned int pos, int ln, int col);
    
    /* 
     * Inserts the specified relocatino into the relocation list.
     */
    void add_reloc (relocation reloc);
    
    /* 
     * Inserts a dependency for the specified module name.
     */
    void add_dependency (const std::string& mod_name);
  };
}

#endif

