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

#include "common/errors.hpp"
#include "linker/linker.hpp"
#include <stdexcept>
#include <unordered_map>
#include <stack>
#include <queue>
#include <unordered_set>

#include <iostream> // DEBUG


namespace arane {
  
  linker::linker (error_tracker& errs)
    : errs (errs)
  {
    this->primary_mod_index = -1;
  }
  
  linker::~linker ()
  {
    for (module_info& mod : this->mods)
      delete mod.mod;
  }
  
  
  
  /* 
   * Inserts the specified module into the list of modules that should be
   * linked together.
   */
  void
  linker::add_module (module *mod)
  {
    this->mods.push_back ({
      .mod = mod,
      .primary = false,
    });
  }
  
  void
  linker::add_primary_module (module *mod)
  {
    if (this->primary_mod_index != -1)
      throw std::runtime_error ("a primary module has already been added");
    
    this->mods.push_back ({
      .mod = mod,
      .primary = true,
    });
    this->primary_mod_index = this->mods.size () - 1;
  }
  
  
  
  /* 
   * Determins in what order the modules should be loaded in, based
   * on their dependencies.
   */
  bool
  linker::determine_module_order (std::vector<module *>& out)
  {
    // construct name map
    std::unordered_map<std::string, module *> mod_map;
    for (auto inf : this->mods)
      mod_map[inf.mod->get_name ()] = inf.mod;
    
    
    std::unordered_set<module *> processed;
    std::stack<module *> stk;
    std::queue<module *> work;
    
    // primary module
    module *pmod = this->mods[this->primary_mod_index].mod;
    stk.push (pmod);
    processed.insert (pmod);
    for (const std::string& dep : pmod->get_dependencies ())
      {
        auto itr = mod_map.find (dep);
        if (itr == mod_map.end ())
          {
            this->errs.error (ES_LINKER, "cannot locate dependency `" + dep + "'");
            return false;
          }
        
        if (processed.find (itr->second) == processed.end ())
          {
            work.push (itr->second);
            processed.insert (itr->second);
          }
      }
    
    while (!work.empty ())
      {
        module *mod = work.front ();
        work.pop ();
        
        stk.push (mod);
        
        // handle dependencies
        for (const std::string& dep : mod->get_dependencies ())
          {
            auto itr = mod_map.find (dep);
            if (itr == mod_map.end ())
              {
                this->errs.error (ES_LINKER, "cannot locate dependency `" + dep + "'");
                return false;
              }
            
            if (processed.find (itr->second) == processed.end ())
              {
                work.push (itr->second);
                processed.insert (itr->second);
              }
          }
      }
    
    // extract results
    while (!stk.empty ())
      {
        out.push_back (stk.top ());
        stk.pop ();
      }
    
    return true;
  }
  
  
  
  static unsigned int
  _sections_total_size_until (const std::string& sect_name, module *target,
    std::vector<module *>& mods)
  {
    unsigned int s = 0;
    for (module *m : mods)
      {
        if (m == target)
          break;
        module_section *sect = m->get_section (sect_name);
        if (sect)
          s += sect->data.get_size ();
      }
    return s;
  }
  
  static unsigned int
  _sections_total_size (const std::string& sect_name, std::vector<module *>& mods)
  {
    return _sections_total_size_until (sect_name, nullptr, mods);
  }
  
  
  
  /* 
   * Handles the specified module's relocation entries.
   */
  void
  linker::handle_relocations (module *mod, std::vector<module *>& prev)
  {
    unsigned int code_start = _sections_total_size ("code", prev);
    unsigned int data_start = _sections_total_size ("data", prev);
    
    auto& code_buf = this->exec->get_code ();
    
    auto& relocs = mod->get_relocations ();
    for (auto reloc : relocs)
      {
        switch (reloc.type)
          {
          case REL_CODE:
            {
              code_buf.push ();
              code_buf.set_pos (code_start + reloc.pos);
              switch (reloc.size)
                {
                case 4:
                  code_buf.put_int (code_start + reloc.dest);
                  break;
                }
              code_buf.pop ();
            }
            break;
          
          case REL_DATA_CSTR:
            {
              code_buf.push ();
              code_buf.set_pos (code_start + reloc.pos);
              switch (reloc.size)
                {
                case 4:
                  code_buf.put_int (data_start + reloc.dest);
                  break;
                }
              code_buf.pop ();
            }
            break;
          }
      }
  }
  
  
  
  /* 
   * Resolves cross-module subroutine uses.
   */
  bool
  linker::handle_imports (module *mod, std::vector<module *>& prev)
  {
    unsigned int code_start = _sections_total_size ("code", prev);
    
    auto& imsubs = mod->get_imported_subs ();
    for (auto& imp : imsubs)
      {
        // find a matching export in the previous modules
        module *emod = nullptr;
        exported_sub *exsub = nullptr;
        for (module *m : prev)
          {
            // must be part of the module's dependency list
            auto& deps = mod->get_dependencies ();
            if (deps.find (m->get_name ()) == deps.end ())
              continue;
            
            auto& exps = m->get_exported_subs ();
            for (auto& exp : exps)
              if (exp.name == imp.name)
                {
                  exsub = &exp;
                  emod = m;
                  break;
                }
          }
        
        if (!emod)
          {
            this->errs.error (ES_LINKER, "unresolved subroutine call `" + imp.name + "'",
              imp.ln, imp.col);
            return false;
          }
        
        auto& code_buf = this->exec->get_code ();
        code_buf.push ();
        code_buf.set_pos (code_start + imp.pos);
        code_buf.put_int (
          _sections_total_size_until ("code", emod, prev) + exsub->pos);        
        code_buf.pop ();
      }
    
    return true;
  }
  
  
  
  /* 
   * Links the inserted modules into a standalone executable.
   * The returned object should be deleted once no longer in use.
   */
  executable*
  linker::link ()
  {
    if (this->primary_mod_index == -1)
      throw std::runtime_error ("primary module not supplied");
    
    std::vector<module *> order;
    if (!this->determine_module_order (order))
      return nullptr;
    
    this->exec = new executable ();
    
    std::vector<module *> processed;
    for (unsigned int i = 0; i < order.size (); ++i)
      { 
        module *mod = order[i];
         
        // append the module's code and data sections to the end.
        module_section *code_sect = mod->get_section ("code");
        this->exec->get_code ().put_bytes (code_sect->data.get_data (),
          code_sect->data.get_size ());
        
        module_section *data_sect = mod->get_section ("data");
        if (data_sect)
          {
            this->exec->get_data ().put_bytes (data_sect->data.get_data (),
              data_sect->data.get_size ());
          }
        
        this->handle_relocations (mod, processed);
        this->handle_imports (mod, processed);
        processed.push_back (mod);
      }
    
    this->exec->get_code ().put_byte (0xF0); // exit
    
    return this->exec;
  }
} 

