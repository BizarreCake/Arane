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


namespace arane {
  
  compiler::compiler (error_tracker& errs)
    : errs (errs)
  {
    this->mod = nullptr;
    this->cgen = nullptr;
  }
  
  compiler::~compiler ()
  {
    while (!this->frms.empty ())
      this->pop_frame ();
    for (package *p : this->all_packs)
      delete p;
    
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
  }
  
  void
  compiler::pop_frame ()
  {
    frame *frm = this->frms.back ();
    delete frm;
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
  
  
  
//------------------------------------------------------------------------------
  
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
   * Inserts all required relocation entries into the compiled module.
   */
  void
  compiler::mark_relocs ()
  {
    for (auto suse : this->sub_uses)
      {
        subroutine_info *sub = this->find_sub (suse.name);
        if (sub && sub->marked)
          {
            this->mod->add_reloc ({
              .type = REL_CODE,
              .pos  = suse.pos + 1,    // skip the opcode
              .dest = (unsigned int)this->cgen->get_label_pos (sub->lbl),
              .size = 4,
            });
          }
        else
          {
            this->mod->import_sub (suse.name, suse.pos + 1, // skip the opcode
              suse.ast->get_line (), suse.ast->get_column ());
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
    
    ast_sub *sub = static_cast<ast_sub *> (program);
    this->compile_sub (sub);
    
    auto& inf = *this->find_sub ("#PROGRAM");
    this->cgen->emit_alloc_array (0); // empty param array
    this->cgen->emit_call (inf.lbl);
    this->mod->add_reloc ({
      .type = REL_CODE,
      .pos  = this->cgen->get_buffer ().get_pos () - 4,
      .dest = (unsigned int)this->cgen->get_label_pos (inf.lbl),
      .size = 4,
    });
    
    this->mark_relocs ();
    
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

