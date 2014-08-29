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

#ifndef _ARANE__COMPILER__AST_TOOLS__H_
#define _ARANE__COMPILER__AST_TOOLS__H_

#include "parser/ast.hpp"
#include <string>
#include <functional>


namespace arane {
  
  namespace ast {
    
    /* 
     * 
     */
    int fold (ast_node *ast, int val,
      const std::function<int (ast_node *, int)>& func,
      const std::function<bool (ast_node *)>& ignore);
    
    /* 
     * Counts the number AST nodes that match the specified predicate.
     */
    int count (ast_node *ast, const std::function<bool (ast_node *)>& pred);
    
    /* 
     * Counts the number of types the specified identifier is used in the
     * given AST node.
     */
    int count_ident_uses (ast_node *ast, ast_ident_type type,
      const std::string& name);
  }
}

#endif

