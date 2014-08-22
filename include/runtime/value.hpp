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

#ifndef _P6__RUNTIME__VALUE__H_
#define _P6__RUNTIME__VALUE__H_

#include <string>


namespace p6 {
  
  class virtual_machine;
  
  
  /* 
   * Currently supported value types.
   */
  enum p_value_type: unsigned char
  {
    PERL_UNDEF,
    PERL_REF,
    PERL_INT,
    PERL_CSTR,    // static string
    PERL_DSTR,    // dynamic string
    PERL_ARRAY,
    
    // special value type used internally by the virtual machine.
    PERL_INTERNAL,
  };
  
  p_value_type p_get_ref_type (p_value_type type);
  
  
  
  /* 
   * Represents an arbitrary Perl value.
   */
  struct p_value
  {
    union
      {
        long long i64;
        struct
          {
            const char *data;
            unsigned int len;
          } cstr;
        struct
          {
            char *data;
            unsigned int len;
            unsigned int cap;
          } str;
        struct
          {
            p_value *data;
            unsigned int len;
            unsigned int cap;
          } arr;
        p_value *ref;
      } val;
    
    // fields used by the GC:
    bool is_gc;               // to differentiate from plain references
    p_value *gc_forward;
    unsigned char gc_state;
    unsigned char gc_protect;
    
    p_value_type type;
  };
  
  /* 
   * Removes GC protection from the specified value.
   */
  void p_value_unprotect (p_value *val);
  
  /* 
   * Performs a deep copy.
   */
  void p_value_copy (p_value& dest, p_value& src);

  /* 
   * Returns a textual representation of the specified value.
   */  
  std::string p_value_str (p_value& val);
  
  /*
   * Checks whether the contents of the two specified values are references
   * to the same data.
   */
  bool p_value_ref_eq (p_value& a, p_value& b);
  
  
  
  long long p_value_array_length (p_value& arr);
  
  
  /* 
   * Casting:
   */
  
  p_value p_value_to_str (p_value& val, virtual_machine& vm);
  p_value p_value_to_int (p_value& val, virtual_machine& vm);
  
  
  /* 
   * Comparison:
   */
  
  bool p_value_eq (p_value& a, p_value& b);
  bool p_value_lt (p_value& a, p_value& b);
  bool p_value_lte (p_value& a, p_value& b);
  bool p_value_gt (p_value& a, p_value& b);
  bool p_value_gte (p_value& a, p_value& b);
  
  bool p_value_is_false (p_value& val);
  
  /* 
   * Operations:
   */
  
  p_value p_value_add (p_value& a, p_value& b, virtual_machine& vm);
  p_value p_value_sub (p_value& a, p_value& b, virtual_machine& vm);
  p_value p_value_mul (p_value& a, p_value& b, virtual_machine& vm);
  p_value p_value_div (p_value& a, p_value& b, virtual_machine& vm);
  p_value p_value_mod (p_value& a, p_value& b, virtual_machine& vm);
  
  p_value p_value_concat (p_value& a, p_value& b, virtual_machine& vm);
}

#endif

