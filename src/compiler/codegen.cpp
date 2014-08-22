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

#include "compiler/codegen.hpp"
#include <stdexcept>


namespace p6 {
  
  /* 
   * Constructs a code generator on top of the specified byte buffer.
   */
  code_generator::code_generator (byte_buffer& buf)
    : buf (buf)
  {
    this->next_lbl_id = 0;
  }
  
  
  
  /* 
   * Creates a label and returns its identifier.
   */
  int
  code_generator::create_label ()
  {
    return ++this->next_lbl_id;
  }
  
  /* 
   * Marks the position of the specified label.
   */
  void
  code_generator::mark_label (int lbl)
  {
    this->labels[lbl] = {
      .pos = this->buf.get_pos (),
    };
  }
  
  /* 
   * Updates all code locations that reference marked labels.
   */
  void
  code_generator::fix_labels ()
  {
    unsigned int prev_pos = this->buf.get_pos ();
    
    std::vector<label_use_t> keep;
    for (auto use : this->label_uses)
      {
        auto itr = this->labels.find (use.lbl);
        if (itr == this->labels.end ())
          keep.push_back (use);
        else
          {
            label_t label = itr->second;
            this->buf.set_pos (use.pos);
            
            int val = use.abs ? label.pos : (label.pos - (use.pos + use.size));
            
            if (use.size == 4)
              this->buf.put_int ((int)val);
            else if (use.size == 2)
              this->buf.put_short ((short)val);
          }
      }
    
    this->label_uses = keep;
    this->buf.set_pos (prev_pos);
  }
  
  /* 
   * Returns the position of the specified marked label, or -1 if it's not
   * marked or invalid.
   */
  int
  code_generator::get_label_pos (int lbl)
  {
    auto itr = this->labels.find (lbl);
    if (itr == this->labels.end ())
      return -1;
    
    return itr->second.pos;
  }
  
  
  
  void
  code_generator::emit_push_int (long long val)
  {
    if (val <= 127 && val >= -128)
      {
        this->buf.put_byte (0x00);
        this->buf.put_byte (val);
      }
    else
      {
        this->buf.put_byte (0x01);
        this->buf.put_long (val);
      }
  }
  
  void
  code_generator::emit_push_cstr (unsigned int pos)
  {
    this->buf.put_byte (0x02);
    this->buf.put_int (pos);
  }
  
  void
  code_generator::emit_push_undef ()
  {
    this->buf.put_byte (0x03);
  }
  
  void
  code_generator::emit_pop ()
  {
    this->buf.put_byte (0x04);
  }
  
  void
  code_generator::emit_dup ()
  {
    this->buf.put_byte (0x05);
  }
  
  void
  code_generator::emit_dupn (unsigned char n)
  {
    this->buf.put_byte (0x06);
    this->buf.put_byte (n);
  }
  
  void
  code_generator::emit_load_global (unsigned int pos)
  {
    this->buf.put_byte (0x07);
    this->buf.put_int (pos);
  }
  
  void
  code_generator::emit_store_global (unsigned int pos)
  {
    this->buf.put_byte (0x08);
    this->buf.put_int (pos);
  }
  
  
  
  void
  code_generator::emit_add ()
  {
    this->buf.put_byte (0x10);
  }
  
  void
  code_generator::emit_sub ()
  {
    this->buf.put_byte (0x11);
  }
  
  void
  code_generator::emit_mul ()
  {
    this->buf.put_byte (0x12);
  }
  
  void
  code_generator::emit_div ()
  {
    this->buf.put_byte (0x13);
  }
  
  void
  code_generator::emit_mod ()
  {
    this->buf.put_byte (0x14);
  }
  
  void
  code_generator::emit_concat ()
  {
    this->buf.put_byte (0x15);
  }
  
  void
  code_generator::emit_is_false ()
  {
    this->buf.put_byte (0x16);
  }
  
  void
  code_generator::emit_is_true ()
  {
    this->buf.put_byte (0x17);
  }
  
  void
  code_generator::emit_ref ()
  {
    this->buf.put_byte (0x18);
  }
  
  void
  code_generator::emit_deref ()
  {
    this->buf.put_byte (0x19);
  }
  
  void
  code_generator::emit_ref_assign ()
  {
    this->buf.put_byte (0x1A);
  }
  
  void
  code_generator::emit_box ()
  {
    this->buf.put_byte (0x1B);
  }
  
  
  
  void
  code_generator::emit_jmp (int lbl)
  {
    this->buf.put_byte (0x20);
    
    this->label_uses.push_back ({
      .lbl = lbl,
      .pos = this->buf.get_pos (),
      .abs = false,
      .size = 2,
    });
    this->buf.put_short (0);
  }
  
  void
  code_generator::emit_je (int lbl)
  {
    this->buf.put_byte (0x21);
    
    this->label_uses.push_back ({
      .lbl = lbl,
      .pos = this->buf.get_pos (),
      .abs = false,
      .size = 2,
    });
    this->buf.put_short (0);
  }
  
  void
  code_generator::emit_jne (int lbl)
  {
    this->buf.put_byte (0x22);
    
    this->label_uses.push_back ({
      .lbl = lbl,
      .pos = this->buf.get_pos (),
      .abs = false,
      .size = 2,
    });
    this->buf.put_short (0);
  }
  
  void
  code_generator::emit_jl (int lbl)
  {
    this->buf.put_byte (0x23);
    
    this->label_uses.push_back ({
      .lbl = lbl,
      .pos = this->buf.get_pos (),
      .abs = false,
      .size = 2,
    });
    this->buf.put_short (0);
  }
  
  void
  code_generator::emit_jle (int lbl)
  {
    this->buf.put_byte (0x24);
    
    this->label_uses.push_back ({
      .lbl = lbl,
      .pos = this->buf.get_pos (),
      .abs = false,
      .size = 2,
    });
    this->buf.put_short (0);
  }
  
  void
  code_generator::emit_jg (int lbl)
  {
    this->buf.put_byte (0x25);
    
    this->label_uses.push_back ({
      .lbl = lbl,
      .pos = this->buf.get_pos (),
      .abs = false,
      .size = 2,
    });
    this->buf.put_short (0);
  }
  
  void
  code_generator::emit_jge (int lbl)
  {
    this->buf.put_byte (0x26);
    
    this->label_uses.push_back ({
      .lbl = lbl,
      .pos = this->buf.get_pos (),
      .abs = false,
      .size = 2,
    });
    this->buf.put_short (0);
  }
  
  
  void
  code_generator::emit_alloc_array (unsigned int len)
  {
    this->buf.put_byte (0x30);
    this->buf.put_int (len);
  }
  
  void
  code_generator::emit_array_set ()
  {
    this->buf.put_byte (0x31);
  }
  
  void
  code_generator::emit_array_get ()
  {
    this->buf.put_byte (0x32);
  }
  
  void
  code_generator::emit_box_array (unsigned char count)
  {
    this->buf.put_byte (0x33);
    this->buf.put_byte (count); 
  }
  
  
  
  void
  code_generator::emit_to_str ()
  {
    this->buf.put_byte (0x40);
  }
  
  void
  code_generator::emit_to_int ()
  {
    this->buf.put_byte (0x41);
  }
  
  
  
  void
  code_generator::emit_push_frame (unsigned int locs)
  {
    this->buf.put_byte (0x60);
    this->buf.put_int (locs);
  }
  
  void
  code_generator::emit_pop_frame ()
  {
    this->buf.put_byte (0x61);
  }
  
  void
  code_generator::emit_load (unsigned int index)
  {
    if (index <= 0xFF)
      {
        this->buf.put_byte (0x62);
        this->buf.put_byte (index);
      }
    else
      {
        this->buf.put_byte (0x64);
        this->buf.put_int (index);
      }
  }
  
  void
  code_generator::emit_store (unsigned int index)
  {
    if (index <= 0xFF)
      {
        this->buf.put_byte (0x63);
        this->buf.put_byte (index);
      }
    else
      {
        this->buf.put_byte (0x65);
        this->buf.put_int (index);
      }
  }
  
  void
  code_generator::emit_storeload (unsigned int index)
  {
    if (index <= 0xFF)
      {
        this->buf.put_byte (0x66);
        this->buf.put_byte (index);
      }
    else
      {
        this->buf.put_byte (0x67);
        this->buf.put_int (index);
      }
  }
  
  void
  code_generator::emit_load_ref (unsigned int index)
  {
    if (index <= 0xFF)
      {
        this->buf.put_byte (0x68);
        this->buf.put_byte (index);
      }
    else
      {
        this->buf.put_byte (0x69);
        this->buf.put_int (index);
      }
  }
  
  
  
  void
  code_generator::emit_call_builtin (const std::string& name,
    unsigned char param_count)
  {
    static const std::unordered_map<std::string, int> _index_map {
      { "print", 1 },
      { "push", 2 },
      { "elems", 3 },
      { "range", 4 },
      { "substr", 5 },
      { "length", 6 },
      { "shift", 7 },
    };
    
    auto itr = _index_map.find (name);
    if (itr == _index_map.end ())
      throw std::runtime_error ("unknown builtin subroutine name");
    
    this->buf.put_byte (0x70);
    this->buf.put_short (itr->second);
    this->buf.put_byte (param_count);
  }
  
  void
  code_generator::emit_call (int lbl)
  {
    this->buf.put_byte (0x71);
    
    this->label_uses.push_back ({
      .lbl = lbl,
      .pos = this->buf.get_pos (),
      .abs = true,
      .size = 4,
    });
    this->buf.put_int (0);
  }
  
  void
  code_generator::emit_return ()
  {
    this->buf.put_byte (0x72);
  }
  
  void
  code_generator::emit_arg_load (unsigned char index)
  {
    this->buf.put_byte (0x73);
    this->buf.put_byte (index);
  }
  
  void
  code_generator::emit_arg_store (unsigned char index)
  {
    this->buf.put_byte (0x74);
    this->buf.put_byte (index);
  }
  
  void
  code_generator::emit_arg_load_ref (unsigned char index)
  {
    this->buf.put_byte (0x75);
    this->buf.put_byte (index);
  }
  
  void
  code_generator::emit_load_arg_array ()
  {
    this->buf.put_byte (0x76);
  }
  
  
  
  void
  code_generator::emit_exit ()
  {
    this->buf.put_byte (0xF0);
  }
  
  void
  code_generator::emit_checkpoint (int n)
  {
    this->buf.put_byte (0xF1);
    this->buf.put_int (n);
  }
}

