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
#include "common/utils.hpp"


namespace arane {
  
  void
  compiler::compile_expr_stmt (ast_expr_stmt *ast)
  {
    ast_expr *inner = ast->get_expr ();
    this->compile_expr (inner);
    this->cgen->emit_pop ();
  }
  
  
  
  void
  compiler::compile_if (ast_if *ast)
  {
    int lbl_mpart_false = this->cgen->create_label ();
    int lbl_done = this->cgen->create_label ();
    
    // 
    // main part
    // 
    
    auto main_part = ast->get_main_part ();
    this->compile_expr (main_part.cond);
    this->cgen->emit_to_bool ();
    this->cgen->emit_jf (lbl_mpart_false);
    
    // main part true
    this->compile_block (main_part.body);
    this->cgen->emit_jmp (lbl_done);
    
    // main part false
    this->cgen->mark_label (lbl_mpart_false);
    
    // 
    // elsifs.
    // 
    auto& elsifs = ast->get_elsif_parts ();
    for (auto elsif : elsifs)
      {
        int lbl_part_false = this->cgen->create_label ();
        
        this->compile_expr (elsif.cond);
        this->cgen->emit_to_bool ();
        this->cgen->emit_jf (lbl_part_false);
        
        // part true
        this->compile_block (elsif.body);
        this->cgen->emit_jmp (lbl_done);
        
        // part false
        this->cgen->mark_label (lbl_part_false);
      }
    
    // 
    // else
    // 
    if (ast->get_else_part ())
      {
        this->compile_block (ast->get_else_part ());
      }
    
    this->cgen->mark_label (lbl_done);
  }
  
  
  
  void
  compiler::compile_while (ast_while *ast)
  {
    int lbl_done = this->cgen->create_label ();
    int lbl_loop = this->cgen->create_label ();
    
    this->push_frame (FT_LOOP);
    frame& frm = this->top_frame ();
    frm.extra["subtype"] = FST_WHILE;
    frm.extra["last"] = lbl_done;
    frm.extra["next"] = lbl_loop;
    
    // test
    this->cgen->mark_label (lbl_loop);
    this->compile_expr (ast->get_cond ());
    this->cgen->emit_to_bool ();
    this->cgen->emit_jf (lbl_done);
    
    // body
    this->compile_block (ast->get_body (), false);
    this->cgen->emit_jmp (lbl_loop);
    
    this->cgen->mark_label (lbl_done);
    
    this->pop_frame ();
  }
  
  
  
  void
  compiler::compile_for_on_range (ast_for *ast)
  {
    // the loop variable is also the index variable.
    
    ast_range *ran = static_cast<ast_range *> (ast->get_arg ());
    
    int lbl_done = this->cgen->create_label ();
    int lbl_loop = this->cgen->create_label ();
    
    this->push_frame (FT_LOOP);
    frame& frm = this->top_frame ();
    
    if (ast->get_var ())
      {
        type_info ti {};
        ti.push_basic (TYPE_INT_NATIVE);
        frm.add_local (ast->get_var ()->get_name (), ti);
      }
    int loop_var = ast->get_var ()
      ? frm.get_local (ast->get_var ()->get_name ())->index
      : frm.alloc_local ();
    
    // store range end
    int end_var = frm.alloc_local ();
    this->compile_expr (ran->get_rhs ());
    this->cgen->emit_store (end_var);
    
    // setup index variable
    this->compile_expr (ran->get_lhs ());
    if (ran->lhs_exclusive ())
      {
        this->cgen->emit_push_int (1);
        this->cgen->emit_add ();
      }
    this->cgen->emit_store (loop_var);
    
    frm.extra["subtype"] = FST_FOR;
    frm.extra["last"] = lbl_done;
    frm.extra["next"] = lbl_loop;
    frm.extra["loop_var"] = loop_var;
    frm.extra["index_var"] = loop_var;
    frm.extra["on_range"] = 1;
    
    this->cgen->emit_push_microframe ();
    
    // test
    this->cgen->mark_label (lbl_loop);
    this->cgen->emit_load (loop_var);
    this->cgen->emit_load (end_var);
    if (ran->rhs_exclusive ())
      this->cgen->emit_jge (lbl_done);
    else
      this->cgen->emit_jg (lbl_done);
    
    // body
    this->cgen->emit_load (loop_var);
    this->cgen->emit_store_def ();
    this->compile_block (ast->get_body (), false);
    
    // increment index variable
    this->cgen->emit_load (loop_var);
    this->cgen->emit_push_int (1);
    this->cgen->emit_add ();
    this->cgen->emit_store (loop_var);
    this->cgen->emit_jmp (lbl_loop);
    
    this->cgen->mark_label (lbl_done);
    this->cgen->emit_pop_microframe ();
    
    this->pop_frame ();
  }
  
  void
  compiler::compile_for (ast_for *ast)
  {
    if (ast->get_arg ()->get_type () == AST_RANGE)
      {
        this->compile_for_on_range (ast);
        return;
      }
    
    int lbl_done = this->cgen->create_label ();
    int lbl_loop = this->cgen->create_label ();
    
    this->push_frame (FT_LOOP);
    frame& frm = this->top_frame ();
    
    int loop_var = -1;
    if (ast->get_var ())
      {
        frm.add_local (ast->get_var ()->get_name ());
        loop_var = frm.get_local (ast->get_var ()->get_name ())->index;
      }
    
    // setup index variable
    int index_var = frm.alloc_local ();
    this->cgen->emit_push_int (0);
    this->cgen->emit_store (index_var);
    
    
    frm.extra["subtype"] = FST_FOR;
    frm.extra["last"] = lbl_done;
    frm.extra["next"] = lbl_loop;
    frm.extra["loop_var"] = loop_var;
    frm.extra["index_var"] = index_var;
    frm.extra["on_range"] = 0;
    
    
    // list
    int list_var = frm.alloc_local ();
    this->compile_expr (ast->get_arg ());
    this->cgen->emit_storeload (list_var);
    
    // list length
    int length_var = frm.alloc_local ();
    this->cgen->emit_call_builtin ("elems", 1);
    this->cgen->emit_store (length_var);
    
    this->cgen->emit_push_microframe ();
    
    // test
    this->cgen->mark_label (lbl_loop);
    this->cgen->emit_load (index_var);
    this->cgen->emit_load (length_var);
    this->cgen->emit_jge (lbl_done);
    
    // body
    this->cgen->emit_load (list_var);
    this->cgen->emit_load (index_var);
    this->cgen->emit_array_get ();
    if (loop_var != -1)
      this->cgen->emit_storeload (loop_var);
    this->cgen->emit_store_def ();
    this->compile_block (ast->get_body (), false);

    
    // increment index variable
    this->cgen->emit_load (index_var);
    this->cgen->emit_push_int (1);
    this->cgen->emit_add ();
    this->cgen->emit_store (index_var);
    this->cgen->emit_jmp (lbl_loop);
    
    this->cgen->mark_label (lbl_done);
    this->cgen->emit_pop_microframe ();
    
    this->pop_frame ();
  }
  
  
  
  void
  compiler::compile_loop (ast_loop *ast)
  {
    int lbl_done = this->cgen->create_label ();
    int lbl_loop = this->cgen->create_label ();
    
    this->push_frame (FT_LOOP);
    frame& frm = this->top_frame ();
    frm.extra["subtype"] = FST_LOOP;
    frm.extra["last"] = lbl_done;
    frm.extra["next"] = lbl_loop;
    
    // init
    if (ast->get_init ())
      {
        this->compile_expr (ast->get_init ());
        this->cgen->emit_pop ();
      }
    
    // cond
    this->cgen->mark_label (lbl_loop);
    if (ast->get_cond ())
      {
        this->compile_expr (ast->get_cond ());
        this->cgen->emit_to_bool ();
        this->cgen->emit_jf (lbl_done);
      }
    
    // body
    this->compile_block (ast->get_body ());
    
    // step
    if (ast->get_step ())
      {
        this->compile_expr (ast->get_step ());
        this->cgen->emit_pop ();
      }
    
    this->cgen->emit_jmp (lbl_loop);
    
    this->cgen->mark_label (lbl_done);
    
    this->pop_frame ();
  }
  
  
  
  void
  compiler::compile_block (ast_block *ast, bool create_frame)
  {
    if (create_frame)
      this->push_frame (FT_BLOCK);
    for (ast_stmt *stmt : ast->get_stmts ())
      {
        this->compile_stmt (stmt);
      }
    if (create_frame)
      this->pop_frame ();
  }
  
  
  
  void
  compiler::compile_use (ast_use *ast)
  {
    this->mod->add_dependency (ast->get_value ());
    
    // parse signatures of file
    try
      {
        this->sigs.parse (utils::module_name_to_path (ast->get_value ()));
      }
    catch (const std::exception& ex)
      { }
  }
  
  
  
  void
  compiler::compile_package (ast_package *ast)
  {
    this->push_package (PT_PACKAGE, ast->get_name ());
    this->compile_block (ast->get_body ());
    this->pop_package ();
  }
  
  
  
  void
  compiler::compile_module (ast_module *ast)
  {
    this->push_package (PT_MODULE, ast->get_name ());
    this->compile_block (ast->get_body ());
    this->pop_package ();
  }
  
  
  
  void
  compiler::compile_stmt (ast_stmt *ast)
  {
    switch (ast->get_type ())
      {
      case AST_EXPR_STMT:
        this->compile_expr_stmt (static_cast<ast_expr_stmt *> (ast));
        break;
        
      case AST_BLOCK:
        this->compile_block (static_cast<ast_block *> (ast));
        break;
        
      case AST_SUB:
        this->compile_sub (static_cast<ast_sub *> (ast));
        break;
      
      case AST_RETURN:
        this->compile_return (static_cast<ast_return *> (ast));
        break;
      
      case AST_IF:
        this->compile_if (static_cast<ast_if *> (ast));
        break;
        
      case AST_WHILE:
        this->compile_while (static_cast<ast_while *> (ast));
        break;
        
      case AST_FOR:
        this->compile_for (static_cast<ast_for *> (ast));
        break;
      
      case AST_LOOP:
        this->compile_loop (static_cast<ast_loop *> (ast));
        break;
      
      case AST_USE:
        this->compile_use (static_cast<ast_use *> (ast));
        break;
      
      case AST_MODULE:
        this->compile_module (static_cast<ast_module *> (ast));
        break;
      
      case AST_PACKAGE:
        this->compile_package (static_cast<ast_package *> (ast));
        break;
        
      default:
        throw std::runtime_error ("invalid statement type");
      }
  }
}

