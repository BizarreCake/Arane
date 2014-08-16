/*
 * P6 - A Perl 6 interpreter.
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
 * You should have received a copy of the GNwU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "compiler/compiler.hpp"
#include "compiler/codegen.hpp"
#include "compiler/frame.hpp"
#include <stdexcept>


namespace p6 {
  
  void
  compiler::compile_undef (ast_undef *ast)
  {
    this->cgen->emit_push_undef ();
  }
  
  
  
  void
  compiler::compile_integer (ast_integer *ast)
  {
    this->cgen->emit_push_int (ast->get_value ());
  }
  
  
  
  void
  compiler::compile_ident (ast_ident *ast)
  {
    variable *var = nullptr;
    auto& frm = this->top_frame ();
    const std::string& name = ast->get_name ();
    
    var = frm.get_local (name);
    if (var)
      {
        // local variable
        this->cgen->emit_load (var->index);
      }
    else
      {
        var = frm.get_arg (name);
        if (var)
          {
            // subroutine parameter
            this->cgen->emit_arg_load (var->index);
          }
        else
          {
            // try special variable names
            if (name == "_")
              {
                if (ast->get_ident_type () == AST_IDENT_ARRAY)
                  {
                    // default array
                    this->cgen->emit_load_arg_array ();
                    return;
                  }
              }
            
            // global variable
            this->cgen->emit_load_global (this->insert_string (name));
          }
      }
  }
  
  
  
  void
  compiler::compile_string (ast_string *ast)
  {
    unsigned int str_index = this->insert_string (ast->get_value ());
    this->mod->add_reloc ({
      .type = REL_DATA_CSTR,
      .pos  = this->cgen->get_buffer ().get_pos () + 1, // skip opcode
      .dest = str_index,
      .size = 4,
    });
    this->cgen->emit_push_cstr (str_index);
  }
  
  
  
  void
  compiler::compile_interp_string (ast_interp_string *ast)
  {
    bool init = true;
    auto& entries = ast->get_entries ();
    if (entries.empty ())
      {
        ast_string str { "" };
        this->compile_string (&str);
        return;
      }
    
    for (auto ent : entries)
      {
        if (ent.type == ast_interp_string::ISET_PART)
          {
            unsigned int str_index = this->insert_string (ent.val.str);
            this->mod->add_reloc ({
              .type = REL_DATA_CSTR,
              .pos  = this->cgen->get_buffer ().get_pos () + 1, // skip opcode
              .dest = str_index,
              .size = 4,
            });
            this->cgen->emit_push_cstr (str_index);
          }
        else
          this->compile_expr (ent.val.expr);
        
        if (!init)
          this->cgen->emit_concat ();
        init = false;
      }
    
    this->cgen->emit_to_str ();
  }
  
  
  
  void
  compiler::compile_list (ast_list *ast)
  {
    auto& elems = ast->get_elems ();
    
    for (unsigned int i = 0; i < elems.size (); ++i)
      this->compile_expr (elems[i]);
    this->cgen->emit_box_array (elems.size ());
  }
  
  
  
  void
  compiler::compile_anonym_array (ast_anonym_array *ast)
  {
    auto& elems = ast->get_elems ();
    
    this->cgen->emit_alloc_array (elems.size ());
    for (unsigned int i = 0; i < elems.size (); ++i)
      {
        this->cgen->emit_dup ();
        this->cgen->emit_push_int (i);
        this->compile_expr (elems[i]);
        this->cgen->emit_array_set ();
      }
    
    this->cgen->emit_box ();
  }
  
  
  
  void
  compiler::compile_subscript (ast_subscript *ast)
  {
    this->compile_expr (ast->get_expr ());
    this->compile_expr (ast->get_index ());
    this->cgen->emit_to_int ();
    this->cgen->emit_array_get ();
  }
  
  
  
  void
  compiler::compile_unop_my (ast_named_unop *ast)
  {
    ast_expr *param = ast->get_param ();
    if (param->get_type () == AST_IDENT)
      {
        frame& frm = this->top_frame ();
        
        auto ident = static_cast<ast_ident *> (param);
        const std::string& name = ident->get_name ();
        if (name.find (':') != std::string::npos)
          {
            this->errs.error (ES_COMPILER,
              "invalid local variable name `" + ident->get_decorated_name () + "'",
              ast->get_line (), ast->get_column ());
            return;
          }
        
        variable *var = frm.get_local (name);
        if (!var)
          {
            frm.add_local (name);
            var = frm.get_local (name);
          }
        
        if (ident->get_ident_type () == AST_IDENT_ARRAY)
          this->cgen->emit_alloc_array (0); // allocate empty array
        else
          this->cgen->emit_push_undef ();
        
        this->cgen->emit_storeload (var->index);
      }
    else if (param->get_type () == AST_LIST)
      {
        // TODO
        this->cgen->emit_push_undef ();
      }
  }
  
  void
  compiler::compile_named_unop (ast_named_unop *ast)
  {
    switch (ast->get_op ())
      {
      case AST_UNOP_MY:
        this->compile_unop_my (ast);
        break;
      
      default:
        throw std::runtime_error ("invalid named unary operator type");
      }
  }
  
  
  
  static bool
  _is_cmp_binop_type (ast_binop_type type)
  {
    switch (type)
      {
      case AST_BINOP_EQ:
      case AST_BINOP_NE:
      case AST_BINOP_LT:
      case AST_BINOP_LE:
      case AST_BINOP_GT:
      case AST_BINOP_GE:
      
      case AST_BINOP_EQ_S:
        return true;
      
      default:
        return false;
      }
  }
  
  static bool
  _is_str_binop_type (ast_binop_type type)
  {
    switch (type)
      {
      case AST_BINOP_EQ_S:
        return true;
      
      default:
        return false;
      }
  }
  
  void
  compiler::compile_cmp_binop (ast_binop *ast)
  {
    if (_is_str_binop_type (ast->get_op ()))
      {
        // cast operands to strings
        this->compile_expr (ast->get_lhs ());
        this->cgen->emit_to_str ();
        this->compile_expr (ast->get_rhs ());
        this->cgen->emit_to_str ();
      }
    else
      {
        this->compile_expr (ast->get_lhs ());
        this->compile_expr (ast->get_rhs ());
      }
    
    int lbl_true = this->cgen->create_label ();
    int lbl_false = this->cgen->create_label ();
    int lbl_over = this->cgen->create_label ();
    
    switch (ast->get_op ())
      {
      
      case AST_BINOP_EQ_S:
      case AST_BINOP_EQ:  this->cgen->emit_je (lbl_true); break;
      case AST_BINOP_NE:  this->cgen->emit_jne (lbl_true); break;
      case AST_BINOP_LT:  this->cgen->emit_jl (lbl_true); break;
      case AST_BINOP_LE:  this->cgen->emit_jle (lbl_true); break;
      case AST_BINOP_GT:  this->cgen->emit_jg (lbl_true); break;
      case AST_BINOP_GE:  this->cgen->emit_jge (lbl_true); break;
      
      default: ;
      }
    
    this->cgen->mark_label (lbl_false);
    this->cgen->emit_push_int (0);
    this->cgen->emit_jmp (lbl_over);
    
    this->cgen->mark_label (lbl_true);
    this->cgen->emit_push_int (1);
    
    this->cgen->mark_label (lbl_over);
  }
  
  
  void
  compiler::compile_binop (ast_binop *ast)
  {
    // handled separately
    if (ast->get_op () == AST_BINOP_ASSIGN)
      {
        this->compile_assign (ast->get_lhs (), ast->get_rhs ());
        return;
      }
    else if (_is_cmp_binop_type (ast->get_op ()))
      {
        this->compile_cmp_binop (ast);
        return;
      }
    
    this->compile_expr (ast->get_lhs ());
    this->compile_expr (ast->get_rhs ());
    
    switch (ast->get_op ())
      {
      case AST_BINOP_ADD:
        this->cgen->emit_add ();
        break;
      
      case AST_BINOP_SUB:
        this->cgen->emit_sub ();
        break;
      
      case AST_BINOP_MUL:
        this->cgen->emit_mul ();
        break;
        
      case AST_BINOP_DIV:
        this->cgen->emit_div ();
        break;
      
      case AST_BINOP_MOD:
        this->cgen->emit_mod ();
        break;
      
      case AST_BINOP_CONCAT:
        this->cgen->emit_concat ();
        break;
      
      default:
        throw std::runtime_error ("invalid binop type");
      }
  }
  
  
  
  void
  compiler::compile_ref (ast_ref *ast)
  {
    if (ast->get_expr ()->get_type () == AST_IDENT)
      {
        auto ident = static_cast<ast_ident *> (ast->get_expr ());
        auto& frm = this->top_frame ();
        variable *var = frm.get_local (ident->get_name ());
        if (var)
          {
            // reference to local variable
            this->cgen->emit_load_ref (var->index);
            return;
          }
        else
          {
            var = frm.get_arg (ident->get_name ());
            if (var)
              {
                // reference to subroutine parameter
                this->cgen->emit_arg_load_ref (var->index);
                return;
              }
          }
      }
    
    this->compile_expr (ast->get_expr ());
    this->cgen->emit_ref ();
  }
  
  
  
  void
  compiler::compile_deref (ast_deref *ast)
  {
    this->compile_expr (ast->get_expr ());
    this->cgen->emit_deref ();
  }
  
  
  
  void
  compiler::compile_range (ast_range *ast)
  {
    // push parameters
    this->compile_expr (ast->get_lhs ());
    this->compile_expr (ast->get_rhs ());
    this->cgen->emit_push_int (ast->lhs_exclusive () ? 1 : 0);
    this->cgen->emit_push_int (ast->rhs_exclusive () ? 1 : 0);
    this->cgen->emit_box_array (4);
    
    this->cgen->emit_call_builtin ("range", 4);
  }
  
  
  
  void
  compiler::compile_expr (ast_expr *ast)
  {
    switch (ast->get_type ())
      {
      case AST_UNDEF:
        this->compile_undef (static_cast<ast_undef *> (ast));
        break;
      
      case AST_INTEGER:
        this->compile_integer (static_cast<ast_integer *> (ast));
        break;
      
      case AST_IDENT:
        this->compile_ident (static_cast<ast_ident *> (ast));
        break;
      
      case AST_NAMED_UNARY:
        this->compile_named_unop (static_cast<ast_named_unop *> (ast));
        break;
      
      case AST_STRING:
        this->compile_string (static_cast<ast_string *> (ast));
        break;
      
      case AST_INTERP_STRING:
        this->compile_interp_string (static_cast<ast_interp_string *> (ast));
        break;
      
      case AST_LIST:
        this->compile_list (static_cast<ast_list *> (ast));
        break;
      
      case AST_ANONYM_ARRAY:
        this->compile_anonym_array (static_cast<ast_anonym_array *> (ast));
        break;
      
      case AST_SUBSCRIPT:
        this->compile_subscript (static_cast<ast_subscript *> (ast));
        break;
      
      case AST_BINARY:
        this->compile_binop (static_cast<ast_binop *> (ast));
        break;
      
      case AST_SUB_CALL:
        this->compile_sub_call (static_cast<ast_sub_call *> (ast));
        break;
        
      case AST_REF:
        this->compile_ref (static_cast<ast_ref *> (ast));
        break;
      
      case AST_DEREF:
        this->compile_deref (static_cast<ast_deref *> (ast));
        break;
      
      case AST_RANGE:
        this->compile_range (static_cast<ast_range *> (ast));
        break;
        
      default:
        throw std::runtime_error ("invalid expression type");
      }
  }
}

