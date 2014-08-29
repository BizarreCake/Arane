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

#include "compiler/asttools.hpp"


namespace arane {
  
  namespace ast {
    
    /* 
     * 
     */
    int
    fold (ast_node *ast, int val,
      const std::function<int (ast_node *, int)>& func,
      const std::function<bool (ast_node *)>& ignore)
    {
      if (ignore (ast))
        return val;
      
      val = func (ast, val);
      
      // check child nodes
      switch (ast->get_type ())
        {
        case AST_LIST:
        case AST_ANONYM_ARRAY:
          {
            auto lst = static_cast<ast_list *> (ast);
            for (auto elem : lst->get_elems ())
              val = fold (elem, val, func, ignore);
          }
          break;
        
        case AST_SUB_CALL:
          val = fold ((static_cast<ast_sub_call *> (ast))->get_params (), val,
            func, ignore);
          break;
        
        case AST_SUBSCRIPT:
          {
            auto subsc = static_cast<ast_subscript *> (ast);
            val = fold (subsc->get_expr (), val, func, ignore);
            val = fold (subsc->get_index (), val, func, ignore);
          }
          break;
        
        case AST_BINARY:
          {
            auto binop = static_cast<ast_binop *> (ast);
            val = fold (binop->get_lhs (), val, func, ignore);
            val = fold (binop->get_rhs (), val, func, ignore);
          }
          break;
        
        case AST_NAMED_UNARY:
          {
            auto unop = static_cast<ast_named_unop *> (ast);
            val = fold (unop->get_param (), val, func, ignore);
          }
          break;
        
        case AST_EXPR_STMT:
          val = fold ((static_cast<ast_expr_stmt *> (ast))->get_expr (),
            val, func, ignore);
          break;
        
        case AST_BLOCK:
          {
            auto blk = static_cast<ast_block *> (ast);
            for (auto stmt : blk->get_stmts ())
              val = fold (stmt, val, func, ignore);
          }
          break;
        
        case AST_PROGRAM:
        case AST_SUB:
          {
            auto sub = static_cast<ast_sub *> (ast);
            val = fold (sub->get_body (), val, func, ignore);
          }
          break;
        
        case AST_RETURN:
          {
            auto ret = static_cast<ast_return *> (ast);
            val = fold (ret->get_expr (), val, func, ignore);
          }
        
        case AST_IF:
          {
            auto aif = static_cast<ast_if *> (ast);
            val = fold (aif->get_main_part ().cond, val, func, ignore);
            val = fold (aif->get_main_part ().body, val, func, ignore);
            if (aif->get_else_part ())
              val = fold (aif->get_else_part (), val, func, ignore);
            auto elsifs = aif->get_elsif_parts ();
            for (auto p : elsifs)
              {
                val = fold (p.cond, val, func, ignore);
                val = fold (p.body, val, func, ignore);
              }
          }
          break;
        
        case AST_REF:
          val = fold ((static_cast<ast_ref *> (ast))->get_expr (),
            val, func, ignore);
          break;
        
        case AST_DEREF:
          val = fold ((static_cast<ast_deref *> (ast))->get_expr (),
            val, func, ignore);
          break;
        
        case AST_WHILE:
          {
            auto aw = static_cast<ast_while *> (ast);
            val = fold (aw->get_cond (), val, func, ignore);
            val = fold (aw->get_body (), val, func, ignore);
          }
          break;
        
        case AST_FOR:
          {
            auto afor = static_cast<ast_for *> (ast);
            val = fold (afor->get_arg (), val, func, ignore);
            val = fold (afor->get_var (), val, func, ignore);
            val = fold (afor->get_body (), val, func, ignore);
          }
          break;
        
        case AST_RANGE:
          {
            auto ran = static_cast<ast_range *> (ast);
            val = fold (ran->get_lhs (), val, func, ignore);
            val = fold (ran->get_rhs (), val, func, ignore);
          }
          break;
        
        case AST_LOOP:
          {
            auto loop = static_cast<ast_loop *> (ast);
            val = fold (loop->get_init (), val, func, ignore);
            val = fold (loop->get_cond (), val, func, ignore);
            val = fold (loop->get_step (), val, func, ignore);
          }
          break;
        
        case AST_MODULE:
        case AST_PACKAGE:
          {
            auto pack = static_cast<ast_package *> (ast);
            val = fold (pack->get_body (), val, func, ignore);
          }
          break;
        
        case AST_CONDITIONAL:
          {
            auto cond = static_cast<ast_conditional *> (ast);
            val = fold (cond->get_test (), val, func, ignore);
            val = fold (cond->get_conseq (), val, func, ignore);
            val = fold (cond->get_alt (), val, func, ignore);
          }
          break;
        
        case AST_OF_TYPE:
          val = fold ((static_cast<ast_of_type *> (ast))->get_expr (),
            val, func, ignore);
          break;
        
        case AST_PREFIX:
          val = fold ((static_cast<ast_prefix *> (ast))->get_expr (),
            val, func, ignore);
          break;
        
        case AST_POSTFIX:
          val = fold ((static_cast<ast_prefix *> (ast))->get_expr (),
            val, func, ignore);
          break;
        
        default: ;
        }
      
      return val;
    }
    
    
    
    /* 
     * Counts the number AST nodes that match the specified predicate.
     */
    int
    count (ast_node *ast, const std::function<bool (ast_node *)>& pred)
    {
      return fold (ast, 0,
        [&pred] (ast_node *node, int val) -> int
          {
            return val + (pred (node) ? 1 : 0);
          },
        [] (ast_node *node) { return false; });
    }
    
    
    /* 
     * Counts the number of types the specified identifier is used in the
     * given AST node.
     */
    int
    count_ident_uses (ast_node *ast, ast_ident_type type,
      const std::string& name)
    {
      return count (ast,
        [type, &name] (ast_node *n) -> bool
          {
            if (n->get_type () == AST_IDENT)
              {
                ast_ident *ident = static_cast<ast_ident *> (n);
                if (ident->get_ident_type () == type &&
                    ident->get_name () == name)
                  return true;
              }
            return false;
          });
    }
  }
}

