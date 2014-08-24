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

#include <iostream>
#include <fstream>
#include <unordered_set>
#include <queue>
#include "parser/parser.hpp"
#include "compiler/compiler.hpp"
#include "linker/linker.hpp"
#include "runtime/vm.hpp"

using namespace arane;



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


static std::string
_module_name_to_path (const std::string& name)
{
  std::string path;
  
  // replace :: with /
  for (size_t i = 0; i < name.length (); ++i)
    {
      if (name[i] == ':' && (i != name.length () - 1) && name[i + 1] == ':')
        {
          ++ i;
          path.push_back ('/');
        }
      else
        path.push_back (name[i]);
    }
  
  path.append (".pm");
  return path;
}

static module*
_compile_module (const std::string& name, const std::string& path, std::unordered_set<std::string>& deps)
{
  error_tracker errs {5};
  
  std::ifstream fs { path };
  if (!fs)
    { 
      std::cout << "Failed to open \"" << path << "\"" << std::endl;
      return nullptr;
    }
  
  // parse
  parser pr {errs};
  ast_program *prog = nullptr;
  try
    {
      prog = pr.parse (fs);
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
  compiler comp {errs};
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

static executable*
_compile_program (const std::string& path)
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
    pmod = _compile_module ("#MAIN", path, deps);
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
      module *mod = _compile_module (mod_name, _module_name_to_path (mod_name), deps);
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


int
main (int argc, char *argv[])
{
  if (argc != 2)
    {
      std::cout << "Usage: arane <file>" << std::endl;
      return -1;
    }
  
  executable *exec = _compile_program (argv[1]);
  if (!exec)
    return -1;
  
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
  
  delete exec;
  return 0;
}

