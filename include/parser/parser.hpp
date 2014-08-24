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

#ifndef _ARANE__PARSER__H_
#define _ARANE__PARSER__H_

#include "parser/ast.hpp"
#include "common/errors.hpp"
#include <exception>
#include <string>
#include <vector>


namespace arane {
  
  /* 
   * Thrown by the paser when too many errors have been accumulated.
   */
  class parse_error: public std::exception
  {
  public:
    parse_error ()
      : std::exception ()
      { }
  };
  
  
  
  /* 
   * The parser takes Perl source code as input, and produces an AST tree
   * representing the parsed program as output.
   */
  class parser
  {
    error_tracker& errs;
    
  public:
    inline error_tracker& get_errors () { return this->errs; }
    
  public:
    parser (error_tracker& errs);
    
  public:
    /* 
     * Parses the Perl code contained in the specified input stream and
     * returns an AST tree representation.
     */
    ast_program* parse (std::istream& strm);
    
    /* 
     * Parses the Perl code contained in the specified string and
     * returns an AST tree representation.
     */
    ast_program* parse (const std::string& str);
  };
}

#endif

