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

#ifndef _P6__CODEGEN__H_
#define _P6__CODEGEN__H_

#include "common/byte_buffer.hpp"
#include <unordered_map>
#include <vector>
#include <string>


namespace p6 {
  
  /* 
   * Wraps around a byte buffer, and provides methods to emit bytecode.
   */
  class code_generator
  {
  private:
    struct label_t
    {
      unsigned int pos;
    };
    
    struct label_use_t
    {
      int lbl;
      unsigned int pos;
      bool abs;
      char size;
    };
    
  private:
    byte_buffer& buf;
    
    std::unordered_map<int, label_t> labels;
    std::vector<label_use_t> label_uses;
    int next_lbl_id;
    
  public:
    inline byte_buffer& get_buffer () { return this->buf; }
    
  public:
    /* 
     * Constructs a code generator on top of the specified byte buffer.
     */
    code_generator (byte_buffer& buf);
    
  public:
    /* 
     * Creates a label and returns its identifier.
     */
    int create_label ();
    
    /* 
     * Marks the position of the specified label.
     */
    void mark_label (int lbl);
    
    /* 
     * Updates all code locations that reference marked labels.
     */
    void fix_labels ();
    
    /* 
     * Returns the position of the specified marked label, or -1 if it's not
     * marked or invalid.
     */
    int get_label_pos (int lbl);
    
    
    
    /* 
     * Instructions:
     */
//------------------------------------------------------------------------------
    
    void emit_push_int (long long val);
    void emit_push_cstr (unsigned int pos);
    void emit_push_undef ();
    void emit_pop ();
    void emit_dup ();
    void emit_dupn (unsigned char n);
    void emit_load_global (unsigned int pos);
    void emit_store_global (unsigned int pos);
    
    void emit_add ();
    void emit_sub ();
    void emit_mul ();
    void emit_div ();
    void emit_mod ();
    void emit_concat ();
    void emit_is_false ();
    void emit_is_true ();
    void emit_ref ();
    void emit_deref ();
    void emit_ref_assign ();
    void emit_box ();
    
    void emit_jmp (int lbl);
    void emit_je (int lbl);
    void emit_jne (int lbl);
    void emit_jl (int lbl);
    void emit_jle (int lbl);
    void emit_jg (int lbl);
    void emit_jge (int lbl);
    
    void emit_alloc_array (unsigned int len);
    void emit_array_set ();
    void emit_array_get ();
    void emit_box_array (unsigned char count);
    
    void emit_to_str ();
    void emit_to_int ();
    
    void emit_push_frame (unsigned int locs);
    void emit_pop_frame ();
    void emit_load (unsigned int index);
    void emit_store (unsigned int index);
    void emit_storeload (unsigned int index);
    void emit_load_ref (unsigned int index);
    
    void emit_call_builtin (const std::string& name, unsigned char param_count);
    void emit_call (int lbl);
    void emit_return ();
    void emit_arg_load (unsigned char index);
    void emit_arg_store (unsigned char index);
    void emit_arg_load_ref (unsigned char index);
    void emit_load_arg_array ();
    
    void emit_exit ();
    void emit_checkpoint (int n);
    
//------------------------------------------------------------------------------
  };
}

#endif

