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
#include "common/utils.hpp"


namespace arane {
  
  bool
  compiler::deduce_type_of_ident (ast_ident *ast, type_info& ti)
  {
    auto& frm = this->top_frame ();
    auto var = frm.get_local (ast->get_name ());
    if (var)
      {
        ti = var->type;
        return true;
      }
    else
      {
        var = frm.get_arg (ast->get_name ());
        if (var)
          {
            ti = var->type;
            return true;
          }
      }
    
    return false;
  }
  
  
  bool
  compiler::deduce_type_of_sub_call (ast_sub_call *ast, type_info& ti)
  {
    std::string name = ast->get_name ();
    
    // find the package the subroutine's in, starting with the topmost
    // one, going down.
    package *pack = &this->top_package ();
    while (pack)
      {
        subroutine_info* s = pack->find_sub (name);
        if (!s)
          pack = pack->get_parent ();
        else
          {
            // turn relative name into absolute path.
            std::string abs_path;
            package *sp = pack->get_subpackage_containing (name);
            abs_path = sp->get_path ();
            if (!abs_path.empty ())
              abs_path.append ("::");
            abs_path.append (utils::strip_packages (s->name));
            name = abs_path;
            break;
          }
      }
    
    auto sig = this->sigs.find_sub (name);
    if (!sig)
      return false;
    
    ti = sig->ret_ti;
    return true;
  }
  
  
  bool
  compiler::deduce_type_of_binop_add (ast_binop *ast, type_info& ti)
  {
    auto lhs = ast->get_lhs ();
    auto rhs = ast->get_rhs ();
    
    type_info lhs_type = this->deduce_type (lhs);
    type_info rhs_type = this->deduce_type (rhs);
    
    if (lhs_type.is_none () || rhs_type.is_none ())
      return false;
    
    if (lhs_type.types[0].type == TYPE_INT ||
        rhs_type.types[0].type == TYPE_INT)
      {
        ti.push_basic (TYPE_INT);
        return true;
      }
    
    if (lhs_type.types[0].type == TYPE_INT_NATIVE &&
        rhs_type.types[0].type == TYPE_INT_NATIVE)
      {
        ti.push_basic (TYPE_INT_NATIVE);
        return true;
      }
    
    return false;
  }
  
  bool
  compiler::deduce_type_of_binop_sub (ast_binop *ast, type_info& ti)
  {
    auto lhs = ast->get_lhs ();
    auto rhs = ast->get_rhs ();
    
    type_info lhs_type = this->deduce_type (lhs);
    type_info rhs_type = this->deduce_type (rhs);
    
    if (lhs_type.is_none () || rhs_type.is_none ())
      return false;
    
    if (lhs_type.types[0].type == TYPE_INT ||
        rhs_type.types[0].type == TYPE_INT)
      {
        ti.push_basic (TYPE_INT);
        return true;
      }
    
    if (lhs_type.types[0].type == TYPE_INT_NATIVE &&
        rhs_type.types[0].type == TYPE_INT_NATIVE)
      {
        ti.push_basic (TYPE_INT_NATIVE);
        return true;
      }
    
    return false;
  }
  
  bool
  compiler::deduce_type_of_binop_mul (ast_binop *ast, type_info& ti)
  {
    auto lhs = ast->get_lhs ();
    auto rhs = ast->get_rhs ();
    
    type_info lhs_type = this->deduce_type (lhs);
    type_info rhs_type = this->deduce_type (rhs);
    
    if (lhs_type.is_none () || rhs_type.is_none ())
      return false;
    
    if (lhs_type.types[0].type == TYPE_INT ||
        rhs_type.types[0].type == TYPE_INT)
      {
        ti.push_basic (TYPE_INT);
        return true;
      }
    
    if (lhs_type.types[0].type == TYPE_INT_NATIVE &&
        rhs_type.types[0].type == TYPE_INT_NATIVE)
      {
        ti.push_basic (TYPE_INT_NATIVE);
        return true;
      }
    
    return false;
  }
  
  bool
  compiler::deduce_type_of_binop_div (ast_binop *ast, type_info& ti)
  {
    auto lhs = ast->get_lhs ();
    auto rhs = ast->get_rhs ();
    
    type_info lhs_type = this->deduce_type (lhs);
    type_info rhs_type = this->deduce_type (rhs);
    
    if (lhs_type.is_none () || rhs_type.is_none ())
      return false;
    
    if (lhs_type.types[0].type == TYPE_INT ||
        rhs_type.types[0].type == TYPE_INT)
      {
        ti.push_basic (TYPE_INT);
        return true;
      }
    
    if (lhs_type.types[0].type == TYPE_INT_NATIVE &&
        rhs_type.types[0].type == TYPE_INT_NATIVE)
      {
        ti.push_basic (TYPE_INT_NATIVE);
        return true;
      }
    
    return false;
  }
  
  bool
  compiler::deduce_type_of_binop_mod (ast_binop *ast, type_info& ti)
  {
    auto lhs = ast->get_lhs ();
    auto rhs = ast->get_rhs ();
    
    type_info lhs_type = this->deduce_type (lhs);
    type_info rhs_type = this->deduce_type (rhs);
    
    if (lhs_type.is_none () || rhs_type.is_none ())
      return false;
    
    if (lhs_type.types[0].type == TYPE_INT ||
        rhs_type.types[0].type == TYPE_INT)
      {
        ti.push_basic (TYPE_INT);
        return true;
      }
    
    if (lhs_type.types[0].type == TYPE_INT_NATIVE &&
        rhs_type.types[0].type == TYPE_INT_NATIVE)
      {
        ti.push_basic (TYPE_INT_NATIVE);
        return true;
      }
    
    return false;
  }
  
  bool
  compiler::deduce_type_of_binop_cmp (ast_binop *ast, type_info& ti)
  {
    ti.types.push_back ({
      .type = TYPE_BOOL_NATIVE,
    });
    return true;
  }
  
  bool
  compiler::deduce_type_of_binop_concat (ast_binop *ast, type_info& ti)
  {
    ti.types.push_back ({
      .type = TYPE_STR,
    });
    return true;
  }
  
  
  bool
  compiler::deduce_type_of_binop (ast_binop *ast, type_info& ti)
  {
    switch (ast->get_op ())
      {
      case AST_BINOP_ADD:
        return this->deduce_type_of_binop_add (ast, ti);
      
      case AST_BINOP_SUB:
        return this->deduce_type_of_binop_sub (ast, ti);
      
      case AST_BINOP_MUL:
        return this->deduce_type_of_binop_mul (ast, ti);
      
      case AST_BINOP_DIV:
        return this->deduce_type_of_binop_div (ast, ti);
      
      case AST_BINOP_MOD:
        return this->deduce_type_of_binop_mod (ast, ti);
      
      case AST_BINOP_EQ_S:
      case AST_BINOP_EQ:
      case AST_BINOP_NE:
      case AST_BINOP_LT:
      case AST_BINOP_LE:
      case AST_BINOP_GT:
      case AST_BINOP_GE:
        return this->deduce_type_of_binop_cmp (ast, ti);
      
      case AST_BINOP_CONCAT:
        return this->deduce_type_of_binop_concat (ast, ti);
        
      
      default: ;
      }
    
    return false;
  }
  
  
  
  bool
  compiler::deduce_type_of_conditional (ast_conditional *ast, type_info& ti)
  {
    type_info conseq_ti = this->deduce_type (ast->get_conseq ());
    type_info alt_ti = this->deduce_type (ast->get_alt ());
    
    if (conseq_ti == alt_ti)
      {
        ti = conseq_ti;
        return true;
      }
    
    return false;
  }
  
  
  
  bool
  compiler::deduce_type_of_prefix (ast_prefix *ast, type_info& ti)
  {
    switch (ast->get_op ())
      {
      case AST_PREFIX_INC:
      case AST_PREFIX_DEC:
        ti = this->deduce_type (ast->get_expr ());
        return true;
      
      case AST_PREFIX_STR:
        ti.push_basic (TYPE_STR);
        return true;
      }
    
    return false;
  }
  
  bool
  compiler::deduce_type_of_postfix (ast_postfix *ast, type_info& ti)
  {
    switch (ast->get_op ())
      {
      case AST_POSTFIX_INC:
      case AST_POSTFIX_DEC:
        ti = this->deduce_type (ast->get_expr ());
        return true;
      }
    
    return false;
  }
  
  
  
  /* 
   * Attempts to statically deduce the type of the specified expression given
   * the current state of the compiler.
   */
  type_info
  compiler::deduce_type (ast_expr *ast)
  {
    type_info ti {};
    switch (ast->get_type ())
      {
      case AST_INTEGER:
        ti.push_basic (TYPE_INT_NATIVE);
        return ti;
      
      case AST_BOOL:
        ti.push_basic (TYPE_BOOL_NATIVE);
        return ti;
      
      case AST_STRING:
      case AST_INTERP_STRING:
        ti.push_basic (TYPE_STR);
        return ti;
      
      case AST_IDENT:
        if (deduce_type_of_ident (static_cast<ast_ident *> (ast), ti))
          return ti;
        break;
      
      case AST_BINARY:
        if (deduce_type_of_binop (static_cast<ast_binop *> (ast), ti))
          return ti;
        break;
      
      case AST_SUB_CALL:
        if (deduce_type_of_sub_call (static_cast<ast_sub_call *> (ast), ti))
          return ti;
        break;
      
      case AST_CONDITIONAL:
        if (deduce_type_of_conditional (static_cast<ast_conditional *> (ast), ti))
          return ti;
        break;
      
      case AST_PREFIX:
        if (deduce_type_of_prefix (static_cast<ast_prefix *> (ast), ti))
          return ti;
        break;
      
      case AST_POSTFIX:
        if (deduce_type_of_postfix (static_cast<ast_postfix *> (ast), ti))
          return ti;
        break;
      
      default: ;
      }
    
    return type_info::none ();
  }
}

