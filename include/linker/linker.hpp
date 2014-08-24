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

#ifndef _ARANE__LINKER__H_
#define _ARANE__LINKER__H_

#include "linker/module.hpp"
#include "linker/executable.hpp"
#include <vector>


namespace arane {

  class error_tracker;

  /* 
   * Takes a list of modules as input and produces a standalone bytecode
   * executable as output, which could then be passed over to the virtual
   * machine for execution.
   */
  class linker
  {
  private:
    struct module_info
    {
      module *mod;
      bool primary;
    };
    
  private:
    error_tracker& errs;
    std::vector<module_info> mods;
    int primary_mod_index;
    
    executable *exec;
    
  private:
    /* 
     * Determins in what order the modules should be loaded in, based
     * on their dependencies.
     */
    bool determine_module_order (std::vector<module *>& out);
    
    /* 
     * Handles the specified module's relocation entries.
     */
    void handle_relocations (module *mod, std::vector<module *>& prev);
    
    /* 
     * Resolves cross-module subroutine uses.
     */
    bool handle_imports (module *mod, std::vector<module *>& prev);
    
  public:
    linker (error_tracker& errs);
    ~linker ();
    
  public:
    /* 
     * Inserts the specified module into the list of modules that should be
     * linked together.
     */
    void add_module (module *mod);
    void add_primary_module (module *mod);
    
    
    
    /* 
     * Links the inserted modules into a standalone executable.
     * The returned object should be deleted once no longer in use.
     */
    executable* link ();
  };
}

#endif

