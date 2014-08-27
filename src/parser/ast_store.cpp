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

#include "parser/ast_store.hpp"
#include "parser/parser.hpp"
#include <fstream>


namespace arane {
  
  ast_store::~ast_store ()
  {
    this->clear ();
  }
  
  
  
  /* 
   * Parses the file located in the specified path and returns an AST tree.
   */
  ast_program*
  ast_store::parse (const std::string& path, error_tracker& errs)
  {
    auto itr = this->asts.find (path);
    if (itr != this->asts.end ())
      return itr->second;
    
    std::ifstream fs {path};
    if (!fs)
      throw std::runtime_error ("cannot parse file: " + path + " (does not exist)");
    
    parser pr {errs};
    ast_program *prog = nullptr;
    try
      {
        prog = pr.parse (fs);
      }
    catch (const std::exception&)
      {
        throw;
      }
    
    this->asts[path] = prog;
    return prog;
  }
  
  
  /* 
   * Clears the cache.
   */
  void
  ast_store::clear ()
  {
    for (auto p : this->asts)
      delete p.second;
    this->asts.clear ();
  }
}

