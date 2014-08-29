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

#ifndef _ARANE__COMPILER__SIGNATURES__H_
#define _ARANE__COMPILER__SIGNATURES__H_

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "parser/ast.hpp"

namespace arane {
  
  class type_info;
  class ast_store;
  
  namespace sigs {
    
    struct subroutine_param
    {
      std::string name;
      type_info ti;
    };
    
    struct subroutine_info
    {
      std::string name;
      std::vector<subroutine_param> params;
      type_info ret_ti;
      bool uses_def_arr;  // if @_ is used
    };
  }
  
  
  /* 
   * Stores signatures the signatures of subroutines located in the file being
   * compiled and all of its dependencies.
   * 
   * To get those signatures, a light check is done on the files' AST trees.
   * Note that an AST store is used that caches AST trees, and so a file will
   * never be parsed twice.
   * 
   * In addition, error checking is not done, since it will be eventually
   * performed once the file gets its turn to be compiled.
   */
  class signatures
  {
    ast_store& asts;
    std::unordered_set<ast_program *> processed;
    
    std::vector<sigs::subroutine_info *> subs;
    std::unordered_map<std::string, int> sub_map;
    
  public:
    inline std::vector<sigs::subroutine_info *>&
    get_subs ()
      { return this->subs; }
    
  public:
    signatures (ast_store& asts);
    ~signatures ();
    
  private:
    /* 
     * Processes the specified AST program.
     */
    void process (ast_program *ast);
    
    void check (ast_node *ast, const std::string& path);
    void check_sub (ast_sub *ast, const std::string& path);
    
  public:
    /* 
     * Parses the specified AST tree.
     */
    void parse (ast_program *ast);
    
    /* 
     * Parses the file located at the specified path.
     */
    void parse (const std::string& path);
  
  public:
    /* 
     * Finds and returns the signature of the specified subroutine, or null
     * if not found.
     */
    sigs::subroutine_info* find_sub (const std::string& name);
  };
}

#endif

