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

#ifndef _ARANE__INTERPRETER__H_
#define _ARANE__INTERPRETER__H_

#include "parser/ast_store.hpp"
#include <istream>
#include <unordered_set>


namespace arane {
  
  class executable;
  class module;
  
  /* 
   * Provides an interface that glues together the lexer, parser, compiler,
   * linker and virtual machine together in that order to interpret a Perl 6
   * program.
   */
  class interpreter
  {
    ast_store asts;
    
  private:
    module* compile_module (const std::string& name, const std::string& path,
      std::unordered_set<std::string>& deps);    
    module* compile_module (std::istream& strm,
      std::unordered_set<std::string>& deps);
    
    executable* compile_program (const std::string& path);
    executable* compile_program (std::istream& strm);
    
  public:
    /* 
     * Runs the program located in the specified path.
     */
    int interpret (const std::string& path);
    
    /* 
     * Runs the program in the specified stream.
     */
    int interpret (std::istream& strm);
  };
}

#endif

