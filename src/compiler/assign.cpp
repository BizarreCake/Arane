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

#include "compiler/compiler.hpp"
#include "compiler/codegen.hpp"
#include "compiler/frame.hpp"
#include "common/utils.hpp"


namespace arane {
  
  /* 
   * Right-hand side value is expected to be at the top of the stack.
   */
  void
  compiler::assign_in_stack (ast_expr *lhs, bool keep_result = true)
  {
    switch (lhs->get_type ())
      {
      case AST_IDENT:
        {
          ast_ident *ident = static_cast<ast_ident *> (lhs);
          frame& frm = this->top_frame ();
          
          variable *var = frm.get_local (ident->get_name ());
          if (var)
            {
              if (keep_result)
                this->cgen->emit_storeload (var->index);
              else
                this->cgen->emit_store (var->index);
            }
          else
            {
              var = frm.get_arg (ident->get_name ());
              if (var)
                {
                  if (keep_result)
                    this->cgen->emit_dup ();
                  this->cgen->emit_arg_store (var->index);
                }
              else
                {
                  if (keep_result)
                    this->cgen->emit_dup ();
                  this->cgen->emit_store_global (this->insert_string (ident->get_name ()));
                }
            }
        }
        break;
      
      default:
        this->errs.error (ES_COMPILER, "invalid left-hand side type in assignment",
          lhs->get_line (), lhs->get_column ());
        return;
      }
  }
  
  
  void
  compiler::enforce_assignment_type (const type_info& lhs_type, ast_expr *rhs)
  {
    if (lhs_type.is_none ())
      return;
    
    auto dt = this->deduce_type (rhs);
    if (dt.is_none ())
      {
        this->cgen->emit_to_compatible (lhs_type);
      }
    else
      {
        auto tc = dt.check_compatibility (lhs_type);
        if (tc == TC_INCOMPATIBLE)
          {
            this->errs.error (ES_COMPILER,
              "attempting to assign a value of an incompatible type `"
              + dt.str () + "' where `" + lhs_type.str () + "' is expected",
              rhs->get_line (), rhs->get_column ());
            return;
          }
        else if (tc == TC_CASTABLE)
          {
            this->cgen->emit_to_compatible (lhs_type);
          }
      }
  }
  
  
  void
  compiler::assign_to_ident (ast_ident *lhs, ast_expr *rhs)
  {
    if (rhs->get_type () == AST_LIST)
      {
        if (lhs->get_ident_type () != AST_IDENT_ARRAY)
          {
            this->errs.error (ES_COMPILER, "can assign a list to an array only",
              lhs->get_line (), lhs->get_column ());
            return;
          }
      }
    
    this->compile_expr (rhs);
    
    frame& frm = this->top_frame ();
    variable *var = frm.get_local (lhs->get_name ());
    if (var)
      {
        this->enforce_assignment_type (var->type, rhs);
        this->cgen->emit_storeload (var->index);
      }
    else
      {
        var = frm.get_arg (lhs->get_name ());
        if (var)
          {
            // make sure we can assign to this variable
            auto sig = this->get_curr_sub_sig ();
            for (auto& p : sig->params)
              if (p.name == lhs->get_name ())
                {
                  if (!p.is_copy && !p.is_rw)
                    {
                      this->errs.error (ES_COMPILER,
                        "cannot assign to readonly parameter `"
                        + lhs->get_decorated_name () + "' (must use `copy' "
                        " or `rw' traits)",
                        lhs->get_line (), lhs->get_column ());
                      return;
                    }
                }
            
            this->enforce_assignment_type (var->type, rhs);
            this->cgen->emit_dup ();
            this->cgen->emit_arg_store (var->index);
          }
        else
          {
            this->cgen->emit_dup ();
            this->cgen->emit_store_global (this->insert_string (lhs->get_name ()));
          }
      }
  }
  
  
  void
  compiler::assign_to_list (ast_list *lhs, ast_expr *rhs)
  {
    switch (rhs->get_type ())
      {
      case AST_LIST:
        {
          ast_list *lst = static_cast<ast_list *> (rhs);
          unsigned int lhs_len = lhs->get_elems ().size ();
          unsigned int rhs_len = lst->get_elems ().size ();
          this->cgen->emit_alloc_array (lhs_len);
          
          unsigned int i;
          for (i = 0; i < lhs_len && i < rhs_len; ++i)
            {
              this->cgen->emit_dup ();
              this->cgen->emit_push_int (i);
              this->compile_assign (lhs->get_elems ()[i], lst->get_elems ()[i]);
              this->cgen->emit_array_set ();
            }
          
          ast_undef undef;
          for (; i < lhs_len; ++i)
            {
              this->cgen->emit_dup ();
              this->cgen->emit_push_int (i);
              this->compile_assign (lhs->get_elems ()[i], &undef);
              this->cgen->emit_array_set ();
            }
        }
        break;
      
      case AST_IDENT:
        {
          ast_ident *ident = static_cast<ast_ident *> (rhs);
          if (ident->get_ident_type () != AST_IDENT_ARRAY)
            {
              this->errs.error (ES_COMPILER, "can only assign an array to a list",
                lhs->get_line (), lhs->get_column ());
              return;
            }
          
          unsigned int lhs_len = lhs->get_elems ().size ();
          this->cgen->emit_alloc_array (lhs_len);
          
          this->compile_expr (ident);
          
          for (unsigned int i = 0; i < lhs_len; ++i)
            {
              this->cgen->emit_dupn (1);
              this->cgen->emit_push_int (i);
              
              this->cgen->emit_dupn (2);
              this->cgen->emit_push_int (i);
              this->cgen->emit_array_get ();
              this->assign_in_stack (lhs->get_elems ()[i]);
              
              this->cgen->emit_array_set ();
            }
        }
        break;
      
      // some type of expression
      default:
        unsigned int lhs_len = lhs->get_elems ().size ();
        this->cgen->emit_alloc_array (lhs_len);
        
        this->compile_expr (rhs);
        
        for (unsigned int i = 0; i < lhs_len; ++i)
          {
            this->cgen->emit_dupn (1);
            this->cgen->emit_push_int (i);
            
            this->cgen->emit_dupn (2);
            this->cgen->emit_push_int (i);
            this->cgen->emit_array_get ();
            this->assign_in_stack (lhs->get_elems ()[i]);
            
            this->cgen->emit_array_set ();
          }
        return;
      }
  }
  
  
  void
  compiler::assign_to_subscript (ast_subscript *lhs, ast_expr *rhs)
  {
    this->compile_expr (rhs);
    
    // make sure types match
    auto ti = this->deduce_type (lhs->get_expr ());
    if (!ti.is_none ())
      {
        if (ti.types[0].type != TYPE_ARRAY)
          {
            this->errs.error (ES_COMPILER,
              "attempting to subscript a non-array type `" + ti.str () + "'",
              lhs->get_line (), lhs->get_column ());
            return;
          }
        ti.types.erase (ti.types.begin ());
        
        auto dt = this->deduce_type (rhs);
        if (dt.is_none ())
          {
            // defer type checking to runtime
            this->cgen->emit_to_compatible (ti);
          }
        else
          {
            auto tc = dt.check_compatibility (ti);
            if (tc == TC_INCOMPATIBLE)
              {
                this->errs.error (ES_COMPILER,
                "attempting to assign a value of an incompatible type `"
                + dt.str () + "' where `" + ti.str () + "' is expected",
                rhs->get_line (), rhs->get_column ());
              return;
            }
          else if (tc == TC_CASTABLE)
            {
              this->cgen->emit_to_compatible (ti);
            }
          }
      }
    
    this->compile_expr (lhs->get_expr ());
    this->compile_expr (lhs->get_index ());
    this->cgen->emit_to_int ();
    this->cgen->emit_dupn (2);
    this->cgen->emit_array_set ();
  }
  
  
  
  void
  compiler::assign_to_deref (ast_deref *lhs, ast_expr *rhs)
  {
    this->compile_expr (lhs->get_expr ());
    this->compile_expr (rhs);
    this->cgen->emit_ref_assign ();
  }
  
  
  
  static bool
  _is_declarative_unop (ast_named_unop *unop)
  {
    switch (unop->get_op ())
      {
      case AST_UNOP_MY:
        return true;
      
      default:
        return false;
      }
  }
  
  
  static bool
  _add_local (error_tracker& errs, frame& frm, ast_expr *expr, const type_info& ti)
  {
    switch (expr->get_type ())
      {
      case AST_IDENT:
        {
          ast_ident *ident = static_cast<ast_ident *> (expr);
          const std::string& name = ident->get_name ();
          if (name.find (':') != std::string::npos)
            {
              errs.error (ES_COMPILER,
                "invalid local variable name `" + ident->get_decorated_name () + "'",
                expr->get_line (), expr->get_column ());
              return false;
            }
          
          frm.add_local (name, utils::get_boxed (ti, ident->get_ident_type ()));
        }
        break;
      
      case AST_LIST:
        {
          ast_list *lst = static_cast<ast_list *> (expr);
          for (ast_expr *elem : lst->get_elems ())
            {
              if (elem->get_type () == AST_IDENT)
                {
                  ast_ident *ident = static_cast<ast_ident *> (elem);
                  frm.add_local (ident->get_name (),
                    utils::get_boxed (ti, ident->get_ident_type ()));
                }
            }
        }
        break;
      
      case AST_OF_TYPE:
        {
          ast_of_type *atn = static_cast<ast_of_type *> (expr);
          _add_local (errs, frm, atn->get_expr (), atn->get_typeinfo ());
        }
        break;
      
      default: ;
      }
    
    return true;
  }
  
  static bool
  _add_locals (error_tracker& errs, ast_named_unop *unop, frame& frm)
  {
    return _add_local (errs, frm, unop->get_param (), type_info::none ());
  }
  
  void
  compiler::compile_assign (ast_expr *lhs, ast_expr *rhs)
  {
    switch (lhs->get_type ())
      {
      case AST_NAMED_UNARY:
        {
          ast_named_unop *unop = static_cast<ast_named_unop *> (lhs);
          if (_is_declarative_unop (unop))
            {
              if (unop->get_op () == AST_UNOP_MY)
                if (!_add_locals (this->errs, unop, this->top_frame ()))
                  return;
              
              return this->compile_assign (unop->get_param (), rhs);
            }
          else
            {
              this->errs.error (ES_COMPILER, "invalid left-hand side type in assignment",
                lhs->get_line (), lhs->get_column ());
              return;
            }
        }
      
      case AST_OF_TYPE:
        this->compile_assign ((static_cast<ast_of_type *> (lhs))->get_expr (), rhs);
        break;
      
      
      case AST_IDENT:
        this->assign_to_ident (static_cast<ast_ident *> (lhs), rhs);
        break;
      
      case AST_SUBSCRIPT:
        this->assign_to_subscript (static_cast<ast_subscript *> (lhs), rhs);
        break;
      
      case AST_LIST:
        this->assign_to_list (static_cast<ast_list *> (lhs), rhs);
        break;
      
      case AST_DEREF:
        this->assign_to_deref (static_cast<ast_deref *> (lhs), rhs);
        break;
      
      default:
        this->errs.error (ES_COMPILER, "invalid left-hand side type in assignment",
          lhs->get_line (), lhs->get_column ());
        return;
      }
  }
  
  
  
  void
  compiler::compile_prefix_inc (ast_prefix *ast)
  {
    ast_expr *expr = ast->get_expr ();
    
    this->compile_expr (expr);
    this->cgen->emit_push_int (1);
    this->cgen->emit_add ();
    
    this->assign_in_stack (expr);
  }
  
  void
  compiler::compile_prefix_dec (ast_prefix *ast)
  {
    ast_expr *expr = ast->get_expr ();
    
    this->compile_expr (expr);
    this->cgen->emit_push_int (1);
    this->cgen->emit_sub ();
    
    this->assign_in_stack (expr);
  }
  
  
  
  void
  compiler::compile_postfix_inc (ast_postfix *ast)
  {
    ast_expr *expr = ast->get_expr ();
    
    this->compile_expr (expr);
    this->cgen->emit_dup ();
    this->cgen->emit_push_int (1);
    this->cgen->emit_add ();
    this->assign_in_stack (expr, false);
  }
  
  void
  compiler::compile_postfix_dec (ast_postfix *ast)
  {
    ast_expr *expr = ast->get_expr ();
    
    this->compile_expr (expr);
    this->cgen->emit_dup ();
    this->cgen->emit_push_int (1);
    this->cgen->emit_sub ();
    this->assign_in_stack (expr, false);
  }
}

