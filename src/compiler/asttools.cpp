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
#include <unordered_set>


namespace arane {
  
  namespace ast {
    
    static int
    _fold (ast_node *ast, int val,
      const std::function<int (ast_node *, int)>& func,
      const std::function<bool (ast_node *)>& ignore, int depth)
    {
      if (!ast)
        return val;
      
      val = func (ast, val);
      if (depth > 0 && ignore (ast))
        return val;
      
      // check child nodes
      switch (ast->get_type ())
        {
        case AST_LIST:
        case AST_ANONYM_ARRAY:
          {
            auto lst = static_cast<ast_list *> (ast);
            for (auto elem : lst->get_elems ())
              val = _fold (elem, val, func, ignore, depth + 1);
          }
          break;
        
        case AST_SUB_CALL:
          val = _fold ((static_cast<ast_sub_call *> (ast))->get_params (), val,
            func, ignore, depth + 1);
          break;
        
        case AST_SUBSCRIPT:
          {
            auto subsc = static_cast<ast_subscript *> (ast);
            val = _fold (subsc->get_expr (), val, func, ignore, depth + 1);
            val = _fold (subsc->get_index (), val, func, ignore, depth + 1);
          }
          break;
        
        case AST_BINARY:
          {
            auto binop = static_cast<ast_binop *> (ast);
            val = _fold (binop->get_lhs (), val, func, ignore, depth + 1);
            val = _fold (binop->get_rhs (), val, func, ignore, depth + 1);
          }
          break;
        
        case AST_NAMED_UNARY:
          {
            auto unop = static_cast<ast_named_unop *> (ast);
            val = _fold (unop->get_param (), val, func, ignore, depth + 1);
          }
          break;
        
        case AST_EXPR_STMT:
          val = _fold ((static_cast<ast_expr_stmt *> (ast))->get_expr (),
            val, func, ignore, depth + 1);
          break;
        
        case AST_BLOCK:
          {
            auto blk = static_cast<ast_block *> (ast);
            for (auto stmt : blk->get_stmts ())
              val = _fold (stmt, val, func, ignore, depth + 1);
          }
          break;
        
        case AST_PROGRAM:
        case AST_SUB:
          {
            auto sub = static_cast<ast_sub *> (ast);
            val = _fold (sub->get_body (), val, func, ignore, depth + 1);
          }
          break;
        
        case AST_RETURN:
          {
            auto ret = static_cast<ast_return *> (ast);
            val = _fold (ret->get_expr (), val, func, ignore, depth + 1);
          }
         break;
        
        case AST_IF:
          {
            auto aif = static_cast<ast_if *> (ast);
            val = _fold (aif->get_main_part ().cond, val, func, ignore, depth + 1);
            val = _fold (aif->get_main_part ().body, val, func, ignore, depth + 1);
            if (aif->get_else_part ())
              val = _fold (aif->get_else_part (), val, func, ignore, depth + 1);
            auto elsifs = aif->get_elsif_parts ();
            for (auto p : elsifs)
              {
                val = _fold (p.cond, val, func, ignore, depth + 1);
                val = _fold (p.body, val, func, ignore, depth + 1);
              }
          }
          break;
        
        case AST_REF:
          val = _fold ((static_cast<ast_ref *> (ast))->get_expr (),
            val, func, ignore, depth + 1);
          break;
        
        case AST_DEREF:
          val = _fold ((static_cast<ast_deref *> (ast))->get_expr (),
            val, func, ignore, depth + 1);
          break;
        
        case AST_WHILE:
          {
            auto aw = static_cast<ast_while *> (ast);
            val = _fold (aw->get_cond (), val, func, ignore, depth + 1);
            val = _fold (aw->get_body (), val, func, ignore, depth + 1);
          }
          break;
        
        case AST_FOR:
          {
            auto afor = static_cast<ast_for *> (ast);
            val = _fold (afor->get_arg (), val, func, ignore, depth + 1);
            val = _fold (afor->get_var (), val, func, ignore, depth + 1);
            val = _fold (afor->get_body (), val, func, ignore, depth + 1);
          }
          break;
        
        case AST_RANGE:
          {
            auto ran = static_cast<ast_range *> (ast);
            val = _fold (ran->get_lhs (), val, func, ignore, depth + 1);
            val = _fold (ran->get_rhs (), val, func, ignore, depth + 1);
          }
          break;
        
        case AST_LOOP:
          {
            auto loop = static_cast<ast_loop *> (ast);
            val = _fold (loop->get_init (), val, func, ignore, depth + 1);
            val = _fold (loop->get_cond (), val, func, ignore, depth + 1);
            val = _fold (loop->get_step (), val, func, ignore, depth + 1);
          }
          break;
        
        case AST_MODULE:
        case AST_PACKAGE:
          {
            auto pack = static_cast<ast_package *> (ast);
            val = _fold (pack->get_body (), val, func, ignore, depth + 1);
          }
          break;
        
        case AST_CONDITIONAL:
          {
            auto cond = static_cast<ast_conditional *> (ast);
            val = _fold (cond->get_test (), val, func, ignore, depth + 1);
            val = _fold (cond->get_conseq (), val, func, ignore, depth + 1);
            val = _fold (cond->get_alt (), val, func, ignore, depth + 1);
          }
          break;
        
        case AST_OF_TYPE:
          val = _fold ((static_cast<ast_of_type *> (ast))->get_expr (),
            val, func, ignore, depth + 1);
          break;
        
        case AST_PREFIX:
          val = _fold ((static_cast<ast_prefix *> (ast))->get_expr (),
            val, func, ignore, depth + 1);
          break;
        
        case AST_POSTFIX:
          val = _fold ((static_cast<ast_prefix *> (ast))->get_expr (),
            val, func, ignore, depth + 1);
          break;
        
        default: ;
        }
      
      return val;
    }
    
    /* 
     * 
     */
    int
    fold (ast_node *ast, int val,
      const std::function<int (ast_node *, int)>& func,
      const std::function<bool (ast_node *)>& ignore, bool ignore_first)
    {
      return _fold (ast, val, func, ignore, ignore_first ? 1 : 0);
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
            if (!n)
              return false;
            
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
    
    
    
    /* 
     * Counts the number of local variables that must allocated in the
     * specified block.
     */
    int
    count_locals_needed (ast_block *mblock)
    {
#define ADD_VAR(NAME)                     \
  if (vars.find (NAME) == vars.end ())    \
    { vars.insert (NAME); ++var_count; }  \
    
      std::unordered_set<std::string> vars;
      return fold (mblock, 0,
        [mblock, &vars] (ast_node *node, int var_count) -> int
          {
            switch (node->get_type ())
              {
              case AST_NAMED_UNARY:
                {
                  auto unop = static_cast<ast_named_unop *> (node);
                  if (unop->get_op () == AST_UNOP_MY)
                    {
                      auto p = unop->get_param ();
                      if (p->get_type () == AST_IDENT)
                        {
                          ADD_VAR((static_cast<ast_ident *> (p))->get_name ())
                        }
                      else if (p->get_type () == AST_LIST)
                        {
                          auto lst = static_cast<ast_list *> (p);
                          for (auto elem : lst->get_elems ())
                            if (elem->get_type () == AST_IDENT)
                              {
                                ADD_VAR((static_cast<ast_ident *> (elem))->get_name ())
                              }
                        }
                      else if (p->get_type () == AST_OF_TYPE)
                        {
                          auto expr = (static_cast<ast_of_type *> (p))->get_expr ();
                          if (expr->get_type () == AST_IDENT)
                            {
                              ADD_VAR((static_cast<ast_ident *> (expr))->get_name ())
                            }
                          else
                            {
                              auto lst = static_cast<ast_list *> (expr);
                              for (auto elem : lst->get_elems ())
                                if (elem->get_type () == AST_IDENT)
                                  {
                                    ADD_VAR((static_cast<ast_ident *> (elem))->get_name ())
                                  }
                            }
                        }
                    }
                }
                break;
              
              case AST_BLOCK:
                // introduce new namespace
                if (mblock != node)
                  var_count += count_locals_needed (static_cast<ast_block *> (node));
                break;
              
              case AST_FOR:
                {
                  auto afor = static_cast<ast_for *> (node);
                  ++ var_count; // var
                  ++ var_count; // anonymous index variable
                  if (afor->get_arg ()->get_type () == AST_RANGE)
                    ++ var_count; // end variable
                  else
                    var_count += 2;
                }
                break;
              
              default: ;
              }
            
            return var_count;
          },
        
        // ignored types:
        [] (ast_node *node)
          {
            return (node->get_type () == AST_BLOCK);
          },
        false);
    }
  }
}

