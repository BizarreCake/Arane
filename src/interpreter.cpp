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

#include "interpreter.hpp"

#include "parser/ast_store.hpp"
#include "parser/parser.hpp"
#include "compiler/compiler.hpp"
#include "linker/linker.hpp"
#include "runtime/vm.hpp"
#include "common/utils.hpp"

#include <fstream>
#include <iostream>


namespace arane {
  
  static void
  _print_errors (const std::string& path, error_tracker& errs)
  {
    auto& entries = errs.get_entries ();
    for (auto& entry : entries)
      {
        switch (entry.stage)
          {
          case ES_LEXER:  std::cout << "lex:"; break;
          case ES_PARSER:  std::cout << "parse:"; break;
          case ES_COMPILER:  std::cout << "compile:"; break;
          case ES_LINKER:  std::cout << "link:"; break;
          }
        
        if (!path.empty ())
          std::cout << path << ":";
        if (entry.ln != -1)
          std::cout << entry.ln << ":";
        if (entry.col != -1)
          std::cout << entry.col << ":";
        std::cout << " ";
        switch (entry.type)
          {
          case ET_INFO: std::cout << "info: "; break;
          case ET_WARNING: std::cout << "warning: "; break;
          case ET_ERROR: std::cout << "error: "; break;
          }
        
        std::cout << entry.text << std::endl;
      }
  }
  
  
  
  module*
  interpreter::compile_module (const std::string& name, const std::string& path,
    std::unordered_set<std::string>& deps)
  {
    error_tracker errs {5};
    
    // parse
    ast_program *prog = nullptr;
    try
      {
        prog = this->asts.parse (path, errs);
      }
    catch (const compilation_error&)
      {
        _print_errors (path, errs);
        return nullptr;
      }
    catch (const std::exception& ex)
      {
        throw;
      }
    
    // compile
    compiler comp {errs, this->asts};
    module *mod = nullptr;
    try
      {
        mod = comp.compile (name, prog);
      }
    catch (const compilation_error&)
      {
        _print_errors (path, errs);
        return nullptr;
      }
    catch (const std::exception& ex)
      {
        throw;
      }
    if (errs.got_errors ())
      {
        _print_errors (path, errs);
        return nullptr;
      }
    
    deps = mod->get_dependencies ();
    return mod;
  }
  
  module*
  interpreter::compile_module (std::istream& strm,
    std::unordered_set<std::string>& deps)
  {
    error_tracker errs {5};
    
    // parse
    ast_program *prog = nullptr;
    try
      {
        parser pr {errs};
        prog = pr.parse (strm);
      }
    catch (const compilation_error&)
      {
        _print_errors ("", errs);
        return nullptr;
      }
    catch (const std::exception& ex)
      {
        throw;
      }
    
    // compile
    compiler comp {errs, this->asts};
    module *mod = nullptr;
    try
      {
        mod = comp.compile ("<stream>", prog);
      }
    catch (const compilation_error&)
      {
        _print_errors ("", errs);
        return nullptr;
      }
    catch (const std::exception& ex)
      {
        throw;
      }
    if (errs.got_errors ())
      {
        _print_errors ("", errs);
        return nullptr;
      }
    
    deps = mod->get_dependencies ();
    return mod;
  }
  
  
  
  
  
  executable*
  interpreter::compile_program (const std::string& path)
  {
    error_tracker errs {10};
    linker lnk {errs};
    std::queue<std::string> work;
    std::unordered_set<std::string> all_deps;
    module *pmod = nullptr;
    std::vector<module *> mods;
    executable *exec;
    
    // primary module
    {
      std::unordered_set<std::string> deps;
      pmod = this->compile_module ("#MAIN", path, deps);
      if (!pmod)
        return nullptr;
      
      for (auto& str : deps)
        {
          auto itr = all_deps.find (str);
          if (itr == all_deps.end ())
            {
              work.push (str);
              all_deps.insert (str);
            }
        } 
    }
    
    // dependencies
    while (!work.empty ())
      {
        std::string mod_name = work.front ();
        work.pop ();
        
        std::unordered_set<std::string> deps;
        module *mod = this->compile_module (mod_name,
          utils::module_name_to_path (mod_name), deps);
        if (!mod)
          goto fail;
        
        mods.push_back (mod);
        for (auto& str : deps)
          {
            auto itr = all_deps.find (str);
            if (itr == all_deps.end ())
              {
                work.push (str);
                all_deps.insert (str);
              }
          }
      }
    
    lnk.add_primary_module (pmod);
    for (module *mod : mods)
      lnk.add_module (mod);
    
    exec = lnk.link ();
    if (errs.got_errors ())
      {
        _print_errors ("", errs);
        return nullptr;
      }
    
    return exec;
    
  fail:
    delete pmod;
    for (module *mod : mods)
      delete mod;
    return nullptr;
  }
  
  executable*
  interpreter::compile_program (std::istream& strm)
  {
    error_tracker errs {10};
    linker lnk {errs};
    std::queue<std::string> work;
    std::unordered_set<std::string> all_deps;
    module *pmod = nullptr;
    std::vector<module *> mods;
    executable *exec;
    
    // primary module
    {
      std::unordered_set<std::string> deps;
      pmod = this->compile_module (strm, deps);
      if (!pmod)
        return nullptr;
      
      for (auto& str : deps)
        {
          auto itr = all_deps.find (str);
          if (itr == all_deps.end ())
            {
              work.push (str);
              all_deps.insert (str);
            }
        } 
    }
    
    // dependencies
    while (!work.empty ())
      {
        std::string mod_name = work.front ();
        work.pop ();
        
        std::unordered_set<std::string> deps;
        module *mod = this->compile_module (mod_name,
          utils::module_name_to_path (mod_name), deps);
        if (!mod)
          goto fail;
        
        mods.push_back (mod);
        for (auto& str : deps)
          {
            auto itr = all_deps.find (str);
            if (itr == all_deps.end ())
              {
                work.push (str);
                all_deps.insert (str);
              }
          }
      }
    
    lnk.add_primary_module (pmod);
    for (module *mod : mods)
      lnk.add_module (mod);
    
    exec = lnk.link ();
    if (errs.got_errors ())
      {
        _print_errors ("", errs);
        return nullptr;
      }
    
    return exec;
    
  fail:
    delete pmod;
    for (module *mod : mods)
      delete mod;
    return nullptr;
  }
  
  
  
  /* 
   * Runs the program located in the specified path.
   */
  int
  interpreter::interpret (const std::string& path)
  {
    executable *exec = this->compile_program (path);
    if (!exec)
      return -1;
    this->asts.clear ();
    
    // DEBUG
    {
      std::ofstream fs { "out.code.a", std::ios_base::out };
      fs.write ((char *)exec->get_code ().get_data (), exec->get_code ().get_size ());
    }
    {
      std::ofstream fs { "out.data.a", std::ios_base::out };
      fs.write ((char *)exec->get_data ().get_data (), exec->get_data ().get_size ());
    }
    
    /* 
     * Run
     */
    virtual_machine vm {};
    try
      {
        vm.run (*exec);
      }
    catch (const std::exception& ex)
      {
        std::cout << "an internal exception was caught while running the program:" << std::endl;
        std::cout << "\t" << ex.what () << std::endl;
      }
    
    return 0;
  }
  
  
  
  /* 
   * Runs the program in the specified stream.
   */
  int
  interpreter::interpret (std::istream& strm)
  {
    executable *exec = this->compile_program (strm);
    if (!exec)
      return -1;
    this->asts.clear ();
    
    /* 
     * Run
     */
    virtual_machine vm {};
    try
      {
        vm.run (*exec);
      }
    catch (const std::exception& ex)
      {
        std::cout << "an internal exception was caught while running the program:" << std::endl;
        std::cout << "\t" << ex.what () << std::endl;
      }
    
    return 0;
  }
}

