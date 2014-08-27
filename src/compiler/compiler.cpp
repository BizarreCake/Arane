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

#include <iostream> // DEBUG
#include <fstream> // DEBUG


namespace arane {
  
  compiler::compiler (error_tracker& errs, ast_store& asts)
    : errs (errs), asts (asts), sigs (asts)
  {
    this->mod = nullptr;
    this->cgen = nullptr;
    this->orig_ctx = nullptr;
  }
  
  compiler::~compiler ()
  {
    for (frame *f : this->all_frms)
      delete f;
    for (package *p : this->all_packs)
      delete p;
    for (compilation_context *ctx : this->all_ctxs)
      delete ctx;
    delete this->orig_ctx;
    
    delete this->cgen;
  }
  
  
  
  /* 
   * Frames and modules.
   */
//------------------------------------------------------------------------------
  
  void
  compiler::push_frame (frame_type type)
  {
    frame *frm = new frame (type,
      this->frms.empty () ? nullptr : this->frms.back ());
    this->frms.push_back (frm);
    this->all_frms.push_back (frm);
  }
  
  void
  compiler::pop_frame ()
  {
    this->frms.pop_back ();
  }
  
  frame&
  compiler::top_frame ()
  {
    return *this->frms.back ();
  }
  
  
  
  void
  compiler::push_package (package_type type, const std::string& name)
  {
    package *parent = this->packs.empty () ? nullptr : this->packs.back ();
    package *pack = new package (*this, type, name, parent);
    if (parent)
      parent->add_subpackage (pack);
    this->packs.push_back (pack);
    this->all_packs.push_back (pack);
  }
  
  void
  compiler::pop_package ()
  {
    this->packs.pop_back ();
  }
  
  package&
  compiler::top_package ()
  {
    return *this->packs.back ();
  }
  
  package&
  compiler::global_package ()
  {
    return *this->packs.front ();
  }
  
  
  
  compilation_context*
  compiler::save_context ()
  {
    compilation_context *ctx = new compilation_context ();
    
    ctx->frms = this->frms;
    ctx->packs = this->packs;
    
    this->all_ctxs.push_back (ctx);
    return ctx;
  }
  
  void
  compiler::restore_context (compilation_context *ctx)
  {
    this->frms = ctx->frms;
    this->packs = ctx->packs;
  }
  
  void
  compiler::push_context (compilation_context *ctx)
  {
    if (this->ctxs.empty ())
      {
        // save original context
        this->orig_ctx = this->save_context ();
      }
    
    this->restore_context (ctx);
    this->ctxs.push_back (ctx);
  }
  
  void
  compiler::pop_context ()
  {
    this->ctxs.pop_back ();
    if (this->ctxs.empty ())
      {
        this->restore_context (this->orig_ctx);
        this->orig_ctx = nullptr;
      }
    else
      this->restore_context (this->ctxs.back ()); // restore previous context
  }
  
  
  
  /* 
   * Defers a compilation to the end of the compilation phase, once the
   * entire AST tree has been analyzed.
   * 
   * TODO: Might be better to just run a light check on the AST tree for
   *       any necessary information instead of deferring compilation...
   */
  void
  compiler::compile_later (compilation_context *ctx, int ph_lbl,
    compile_callback cb, ast_node *ast, void *extra)
  {
    this->dcomps.push ({
      .ctx = ctx,
      .ph_lbl = ph_lbl,
      .cb = cb,
      .ast = ast,
      .extra = extra,
    });
  }
  
  
  
//------------------------------------------------------------------------------
  
  /* 
   * Inserts a relocation.
   * Because the code generator may rearrange the contents of the code
   * section in any way it likes, raw offsets should not be used until the
   * end of the compilation phase, but labels instead.
   * 
   * If the relocation's destination lies in the code section, the third
   * parameter (`dest') should hold a label, otherwise a raw offset.
   */
  void
  compiler::insert_reloc (relocation_type type, int src_lbl, int dest,
    unsigned char size, int src_add)
  {
    c_reloc reloc {
      .type = type,
      .src = src_lbl,
      .dest = dest,
      .size = size,
      .src_add = src_add,
    };
    
    this->relocs.push_back (reloc);
  }
  
  /* 
   * Inserts the specified string into the module's data section and returns
   * its index.  If the same string has already been added, the previous
   * string's index will be returned instead.
   */
  unsigned int
  compiler::insert_string (const std::string& str)
  {
    auto itr = this->data_str_map.find (str);
    if (itr != this->data_str_map.end ())
      return itr->second;
    
    auto& data = this->mod->get_section ("data")->data;
    unsigned int index = data.get_pos ();
    data.put_int (str.length ());
    data.put_bytes ((const unsigned char *)str.c_str (),
      str.size () + 1);
    data_str_map[str] = index;
    return index;
  }
  
  
  
  /* 
   * Searches all packages in the compiled unit for the specified subroutine.
   * This function accepts a full path to the subroutine (<pack1>::...::<name>).
   */
  subroutine_info*
  compiler::find_sub (const std::string& path)
  {
    package *pack = this->packs.front (); // topmost package
    
    std::string::size_type prev_col = 0, next_col;
    while ((next_col = path.find ("::", prev_col)) != std::string::npos)
      {
        // extract package name
        std::string pack_name = path.substr (prev_col, next_col - prev_col);
        
        // find this module
        package *child = nullptr;
        auto& children = pack->get_children ();
        for (package *p : children)
          if (p->get_name () == pack_name)
            {
              child = p;
              break;
            }
        if (!child)
          return nullptr;
          
        pack = child;
        prev_col = next_col + 2;
      }
    
    // subroutine name
    std::string name = path.substr (prev_col);
    return pack->find_sub (name);
  }
  
  
  
  /* 
   * Handles work deferred to the end of the compilation phase.
   */
  void
  compiler::handle_deferred ()
  {
    while (!this->dcomps.empty ())
      {
        auto dc = this->dcomps.front ();
        this->dcomps.pop ();
        
        this->push_context (dc.ctx);
        
        this->cgen->move_to_label (dc.ph_lbl);
        this->cgen->placeholder_start ();
        (this->* dc.cb) (dc.ast, dc.extra);
        this->cgen->placeholder_end ();
        
        this->pop_context ();
      }
  }
  
  
  
  /* 
   * Inserts all required relocation entries into the compiled module.
   * 
   * Must be called once the code section will no longer be rearranged.
   */
  void
  compiler::mark_relocs ()
  {
    for (subroutine_use& suse : this->sub_uses)
      {
        subroutine_info *sub = this->find_sub (suse.name);
        if (sub && sub->marked)
          {
            this->insert_reloc (REL_CODE, suse.pos, sub->lbl, 4,
              1 /* skip opcode */);
          }
        else
          {
            this->mod->import_sub (suse.name,
              this->cgen->get_label_pos (suse.pos) + 1, // skip the opcode
              suse.ast->get_line (), suse.ast->get_column ());
          }
      }
    
    for (c_reloc rel : this->relocs)
      {
        switch (rel.type)
          {
          case REL_CODE:
            this->mod->add_reloc ({
              .type = REL_CODE,
              .pos = (unsigned int)(this->cgen->get_label_pos (rel.src) + rel.src_add),
              .dest = (unsigned int)this->cgen->get_label_pos (rel.dest),
              .size = rel.size
            });
            break;
          
          case REL_DATA_CSTR:
            this->mod->add_reloc ({
              .type = REL_DATA_CSTR,
              .pos = (unsigned int)(this->cgen->get_label_pos (rel.src) + rel.src_add),
              .dest = (unsigned int)rel.dest,
              .size = rel.size
            });
            break;
          }
      }
  }
  
  
  
  void
  compiler::compile_program (ast_program *program)
  {
    // we compile a program as if it's an ordinary subroutine that's called
    // immediately.
    
    // GLOBAL package
    this->push_package (PT_PACKAGE, "GLOBAL");
    
    this->sigs.parse (program);
    
    ast_sub *sub = static_cast<ast_sub *> (program);
    this->compile_sub (sub);
    
    //this->emit_sub_calls ();
    this->handle_deferred ();
    this->mark_relocs ();
    
    auto& inf = *this->find_sub ("#PROGRAM");
    this->cgen->seek_to_end ();
    this->cgen->emit_call (inf.lbl, 0);
    this->mod->add_reloc ({
      .type = REL_CODE,
      .pos  = this->cgen->get_buffer ().get_pos () - 5,
      .dest = (unsigned int)this->cgen->get_label_pos (inf.lbl),
      .size = 4,
    });
    
    this->pop_package ();
  }
  
  /* 
   * Compiles the specified AST tree into a module.
   * The returned module should be deleted once no longer in use.
   */
  module*
  compiler::compile (const std::string& mod_name, ast_program *program)
  {
    this->mod = new module (mod_name);
    this->mod->add_section ("code");
    this->mod->add_section ("data");
    
    this->cgen = new code_generator (this->mod->get_section ("code")->data);
    
    this->compile_program (program);
    
    this->cgen->fix_labels ();
    
    return this->mod;
  }
}

