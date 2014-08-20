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

#include "runtime/vm.hpp"
#include "runtime/gc.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>

#include <iomanip> // DEBUG

// DEBUG
#include <thread>
#include <chrono>


namespace p6 {
  
#define STACK_SIZE        8192
  
  virtual_machine::virtual_machine ()
    : out (&std::cout), in (&std::cin), gc (*this)
  {
    this->stack = new p_value [STACK_SIZE];
    this->sp = 0;
    this->bp = 0;
  }
  
  virtual_machine::~virtual_machine ()
  {
    delete[] this->stack;
  }
  
  
  
  void
  virtual_machine::set_in (std::istream *strm)
  {
    this->in = strm;
  }
  
  void
  virtual_machine::set_out (std::ostream *strm)
  {
    this->out = strm;
  }
  
  
  
  /* 
   * Builtin subroutines:
   */
//------------------------------------------------------------------------------
  
  p_value&
  virtual_machine::get_param_array ()
  {
    auto params = &stack[sp - 1];
    while (params->type == PERL_REF && params->val.ref)
      params = params->val.ref;
    if (params->type != PERL_ARRAY)
      throw std::runtime_error ("parameter array not passed as an array");
    return *params;
  }
  
  
  
  void
  virtual_machine::builtin_print (unsigned char ac_params)
  {
    auto& params = this->get_param_array ();
    int param_count = params.val.arr.len;
    -- sp;
    
    for (int i = 0; i < param_count; ++i)
      {
        p_value& val = params.val.arr.data[i];
        *this->out << p_value_str (val);
      }
    
    stack[sp].type = PERL_INT;
    stack[sp].val.i64 = 1;
    ++ sp;
  }
  
  void
  virtual_machine::builtin_push (unsigned char ac_params)
  {
    auto& params = this->get_param_array ();
    int param_count = params.val.arr.len;
    -- sp;
    
    if (param_count < 2)
      {
        std::cout << "builtin `push' expects at least 2 parameters" << std::endl;
        return;
      }
    
    p_value *arr = &params.val.arr.data[0];
    while (arr && arr->type == PERL_REF)
      arr = arr->val.ref;
    if (!arr)
      return;
    if (arr->type != PERL_ARRAY)
      {
        std::cout << "first parameter passed to builtin `push' is not an array" << std::endl;
        return;
      }
    
    // make sure there's enough space
    auto& data = arr->val.arr;
    if ((data.len + (param_count - 1)) > data.cap)
      {
        // resize
        
        unsigned int ncap = data.cap + param_count - 1;
        p_value *narr = new p_value [ncap];
        for (unsigned int i = 0; i < data.len; ++i)
          narr[i] = data.data[i];
        
        data.cap = ncap;
        delete[] data.data;
        data.data = narr;
      }
    
    for (int i = 1; i < param_count; ++i)
      data.data[data.len++] = params.val.arr.data[i];
    
    stack[sp++].type = PERL_UNDEF;
  }
  
  void
  virtual_machine::builtin_elems (unsigned char ac_params)
  {
    auto& params = this->get_param_array ();
    int param_count = params.val.arr.len;
    -- sp;
    
    stack[sp].type = PERL_INT;
    stack[sp].val.i64 = param_count;
    ++ sp;
  }
  
  void
  virtual_machine::builtin_range (unsigned char ac_params)
  {
    auto& params = this->get_param_array ();
    int param_count = params.val.arr.len;
    -- sp;
    
    if (param_count != 4)
      {
        std::cout << "builtin `range' expects 4 parameters" << std::endl;
        return;
      }
    
    auto& lhs = params.val.arr.data[0];
    auto& rhs = params.val.arr.data[1];
    auto& s_lhs_exc = params.val.arr.data[2];
    auto& s_rhs_exc = params.val.arr.data[3];
    
    if (s_lhs_exc.type != PERL_INT || s_rhs_exc.type != PERL_INT ||
        lhs.type != PERL_INT || rhs.type != PERL_INT)
      {
        std::cout << "invalid parameters passed to builtin `range'" << std::endl;
        return;
      }
    
    bool lhs_exc = (s_lhs_exc.val.i64 == 1);
    bool rhs_exc = (s_rhs_exc.val.i64 == 1);
    long long start = lhs.val.i64;
    long long end = rhs.val.i64;
    
    if (lhs_exc)
      ++ start;
    if (rhs_exc)
      -- end;
    
    int count = end - start + 1;
    if (count < 0)
      count = 0;
    int cap = count ? count : 1;
    
    p_value *data = this->gc.alloc (true);
    data->type = PERL_ARRAY;
    data->val.arr.len = count;
    data->val.arr.cap = cap;
    data->val.arr.data = new p_value [cap];
    for (int i = 0; i < count; ++i)
      {
        p_value &ch = data->val.arr.data[i];
        ch.type = PERL_INT;
        ch.val.i64 = start + i;
      }
    
    p_value& val = stack[sp++];
    val.type = PERL_REF;
    val.val.ref = data;
    p_value_unprotect (data);
  }
  
  void
  virtual_machine::builtin_substr (unsigned char ac_params)
  {
    auto& params = this->get_param_array ();
    int param_count = params.val.arr.len;
    -- sp;
    
    if (param_count < 2)
      {
        std::cout << "builtin `substr' expects at least 2 parameters" << std::endl;
        return;
      }
    
    // string
    auto str = &params.val.arr.data[0];
    long long str_len = 0;
    if (str->type == PERL_CSTR)
      str_len = str->val.cstr.len;
    else if (str->type == PERL_REF)
      {
        if (str->val.ref->type != PERL_DSTR)
          {
            std::cout << "first parameter to builtin `substr' must be a string" << std::endl;
            return;
          }
        
        str = str->val.ref;
        str_len = str->val.str.len;
      }
    
    // offset
    if (params.val.arr.data[1].type != PERL_INT)
      {
        std::cout << "second parameter (offset) to builtin `substr' must be an integer" << std::endl;
        return;
      }
    long long off = params.val.arr.data[1].val.i64;
    
    // length
    int len = str_len - off;
    if (param_count > 2)
      {
        if (params.val.arr.data[2].type != PERL_INT)
          {
            std::cout << "third parameter (length) to builtin `substr' must be an integer" << std::endl;
            return;
          }
        
        len = params.val.arr.data[2].val.i64;
      }
    if (len < 0 || off >= str_len)
      len = 0;
    if (off + len > str_len)
      len = str_len - off;
    
    
    p_value *data = this->gc.alloc (true);
    data->type = PERL_DSTR;
    data->val.str.data = new char [len + 1];
    data->val.str.len = len;
    if (str->type == PERL_CSTR)
      {
        for (int i = 0; i < len; ++i)
          data->val.str.data[i] = str->val.cstr.data[off + i];
        data->val.str.data[len] = '\0';
      }
    else if (str->type == PERL_DSTR)
      {
        for (int i = 0; i < len; ++i)
          data->val.str.data[i] = str->val.str.data[off + i];
        data->val.str.data[len] = '\0';
      }
    
    p_value res;
    res.type = PERL_REF;
    res.val.ref = data;
    
    stack[sp++] = res;
    p_value_unprotect (data);
  }
  
  void
  virtual_machine::builtin_length (unsigned char ac_params)
  {
    auto& params = this->get_param_array ();
    int param_count = params.val.arr.len;
    -- sp;
    
    if (param_count != 1)
      {
        std::cout << "builtin `length' expects 1 parameter" << std::endl;
        return;
      }
    
    auto str = &params.val.arr.data[0];
    long long str_len = 0;
    if (str->type == PERL_CSTR)
      str_len = str->val.cstr.len;
    else if (str->type == PERL_REF)
      {
        if (str->val.ref->type != PERL_DSTR)
          {
            std::cout << "parameter passed to builtin `length' must be a string" << std::endl;
            return;
          }
        
        str = str->val.ref;
        str_len = str->val.str.len;
      }
    
    stack[sp].type = PERL_INT;
    stack[sp].val.i64 = str_len;
    ++ sp;
  }
  
  void
  virtual_machine::builtin_shift (unsigned char ac_params)
  {
    auto arr = (ac_params == 0)
      ? stack[bp - 2].val.ref    // default array
      : &this->get_param_array ();
    int arr_len = arr->val.arr.len;
    -- sp;
    
    if (arr_len == 0)
      {
        // array empty, push undef
        stack[sp++].type = PERL_UNDEF;
      }
    else
      {
        auto& data = arr->val.arr;
        stack[sp++] = data.data[0];
        
        // shift elements
        for (int i = 1; i < arr_len; ++i)
          data.data[i - 1] = data.data[i];
        -- data.len;
      }
  }
  
//------------------------------------------------------------------------------
  
  static inline void
  _unprotect_external (p_value& val)
  {
    if (val.type == PERL_REF)
      {
        if (val.val.ref)
          p_value_unprotect (val.val.ref);
      }
  }
  
  /* 
   * Executes the specified executable.
   * Throws exceptions of type `vm_error' on failure.
   */
  void
  virtual_machine::run (executable& exec)
  {
    const unsigned char *code = exec.get_code ().get_data ();
    const unsigned char *data = exec.get_data ().get_data ();
    
    const unsigned char *ptr = code;
    p_value *stack = this->stack;
    int& sp = this->sp;
    int& bp = this->bp;
    
#define CHECK_STACK_SPACE(COUNT)  \
  if (sp + (COUNT) > STACK_SIZE)  \
    throw std::runtime_error ("stack overflow");
    
    for (;;)
      {
        //std::cout << "0x" << std::hex << std::setfill ('0') << std::setw (2)
        //          << (int)*ptr << " [pos: " << std::dec << std::setfill (' ')
        //          << (int)(ptr - code) << "]" << std::endl;
        switch (*ptr++)
          {
          /* 
           * 00-0F: Stack manipulation.
           */
//------------------------------------------------------------------------------
          
          // push_int8 - push 8-bit integer as 64-bit integer.
          case 0x00:
            CHECK_STACK_SPACE(1)
            stack[sp].type = PERL_INT;
            stack[sp].val.i64 = *ptr++;
            ++ sp;
            break;
          
          // push_int64 - push 64-bit integer.
          case 0x01:
            CHECK_STACK_SPACE(1)
            stack[sp].type = PERL_INT;
            stack[sp].val.i64 = *((long long *)ptr);
            ptr += 8;
            ++ sp;
            break;
          
          // push_cstr - push static string from data section
          case 0x02:
            CHECK_STACK_SPACE(1)
            {
              unsigned int pos = *((unsigned int *)ptr);
              ptr += 4;
              
              stack[sp].type = PERL_CSTR;
              stack[sp].val.cstr.len = *((unsigned int *)(data + pos));
              stack[sp].val.cstr.data = (const char *)(data + pos + 4);
              ++ sp;
            }
            break;
          
          // push_undef
          case 0x03:
            CHECK_STACK_SPACE(1)
            stack[sp].type = PERL_UNDEF;
            ++ sp;
            break;
          
          // pop
          case 0x04:
            -- sp;
            break;
          
          // dup - duplicate topmost value in stack.
          case 0x05:
            CHECK_STACK_SPACE(1)
            stack[sp] = stack[sp - 1];
            ++ sp;
            break;
          
          // dupn
          case 0x06:
            CHECK_STACK_SPACE(1)
            stack[sp] = stack[sp - 1 - *ptr++];
            ++ sp;
            break;
          
          // load_global
          case 0x07:
            CHECK_STACK_SPACE(1)
            {
              unsigned int pos = *((unsigned int *)ptr);
              ptr += 4;
              
              auto itr = this->globs.find ((const char *)(data + pos + 4));
              if (itr == this->globs.end ())
                stack[sp].type = PERL_UNDEF;
              else
                stack[sp] = itr->second;
                
              ++ sp;
            }
            break;
          
          // store_global
          case 0x08:
            {
              unsigned int pos = *((unsigned int *)ptr);
              ptr += 4;
              
              this->globs[(const char *)(data + pos + 4)]
                = stack[--sp];
            }
            break;
          
//------------------------------------------------------------------------------
          
          
          /* 
           * 10-1F: Basic operations.
           */
//------------------------------------------------------------------------------
          
          // add
          case 0x10: 
            -- sp;
            stack[sp - 1] = p_value_add (stack[sp - 1], stack[sp], *this);
            _unprotect_external (stack[sp - 1]);
            break;
          
          // sub
          case 0x11:
            -- sp;
            stack[sp - 1] = p_value_sub (stack[sp - 1], stack[sp], *this);
            _unprotect_external (stack[sp - 1]);
            break;
          
          // mul
          case 0x12:
            -- sp;
            stack[sp - 1] = p_value_mul (stack[sp - 1], stack[sp], *this);
            _unprotect_external (stack[sp - 1]);
            break;
          
          // div
          case 0x13:
            -- sp;
            stack[sp - 1] = p_value_div (stack[sp - 1], stack[sp], *this);
            _unprotect_external (stack[sp - 1]);
            break;
          
          // div
          case 0x14:
            -- sp;
            stack[sp - 1] = p_value_mod (stack[sp - 1], stack[sp], *this);
            _unprotect_external (stack[sp - 1]);
            break;
          
          // concat - concatenate two values together into a string.
          case 0x15:
            -- sp;
            stack[sp - 1] = p_value_concat (stack[sp - 1], stack[sp], *this);
            _unprotect_external (stack[sp - 1]);
            break;
          
          // is_false
          case 0x16:
            {
              bool v = p_value_is_false (stack[sp - 1]);
              stack[sp - 1].type = PERL_INT;
              stack[sp - 1].val.i64 = v ? 1 : 0;
            }
            break;
          
          // is_true
          case 0x17:
            {
              bool v = p_value_is_false (stack[sp - 1]);
              stack[sp - 1].type = PERL_INT;
              stack[sp - 1].val.i64 = v ? 0 : 1;
            }
            break;
          
          // ref
          case 0x18:
            if (stack[sp - 1].type != PERL_REF)
              throw std::runtime_error ("cannot take reference of non-reference data type");
            stack[sp - 1].val.ref = &stack[sp - 1];
            stack[sp - 1].val.ref->is_gc = false;
            stack[sp - 1].type = PERL_REF;
            break;
          
          // deref
          case 0x19:
            stack[sp - 1] = *stack[sp - 1].val.ref;
            break;
          
          // ref_assign
          case 0x1A:
            -- sp;
            *stack[sp - 1].val.ref = stack[sp];
            break;
          
          // box
          case 0x1B:
            {
              p_value *nv = this->gc.alloc_copy (stack[sp - 1], true);
              
              stack[sp - 1].type = PERL_REF;
              stack[sp - 1].val.ref = nv;
              p_value_unprotect (nv);
            }
            break;
          
//------------------------------------------------------------------------------

          /* 
           * 20-2F: Branching.
           */
//------------------------------------------------------------------------------
          
          // jmp
          case 0x20:
            ptr += 2 + *((short *)ptr);
            break;
          
          // je - jump if equal
          case 0x21:
            if (p_value_eq (stack[sp - 2], stack[sp - 1]))
              ptr += *((short *)ptr);
            ptr += 2;
            sp -= 2;
            break;
          
          // jne - jump if not equal
          case 0x22:
            if (!p_value_eq (stack[sp - 2], stack[sp - 1]))
              ptr += *((short *)ptr);
            ptr += 2;
            sp -= 2;
            break;
          
          // jl - jump if less than
          case 0x23:
            if (p_value_lt (stack[sp - 2], stack[sp - 1]))
              ptr += *((short *)ptr);
            ptr += 2;
            sp -= 2;
            break;
          
          // jle - jump if less than or equal to
          case 0x24:
            if (p_value_lte (stack[sp - 2], stack[sp - 1]))
              ptr += *((short *)ptr);
            ptr += 2;
            sp -= 2;
            break;
          
          // jg - jump if greater than
          case 0x25:
            if (p_value_gt (stack[sp - 2], stack[sp - 1]))
              ptr += *((short *)ptr);
            ptr += 2;
            sp -= 2;
            break;
          
          // jge - jump if greater than or equal to
          case 0x26:
            if (p_value_gte (stack[sp - 2], stack[sp - 1]))
              ptr += *((short *)ptr);
            ptr += 2;
            sp -= 2;
            break;
          
//------------------------------------------------------------------------------
          
          /* 
           * 30-3F: Array manipulation.
           */
//------------------------------------------------------------------------------
          
          // alloc_array - allocate an array of a given length
          case 0x30:
            CHECK_STACK_SPACE(1)
            {
              unsigned int count = *((unsigned int *)ptr);
              ptr += 4;
              
              unsigned int cap = count ? count : 1;
              
              p_value *data = this->gc.alloc (true);
              data->type = PERL_ARRAY;
              data->val.arr.len = count;
              data->val.arr.cap = cap;
              data->val.arr.data = new p_value [cap];
              for (unsigned int i = 0; i < count; ++i)
                {
                  p_value &ch = data->val.arr.data[i];
                  ch.type = PERL_UNDEF;
                }
              
              p_value& val = stack[sp++];
              val.type = PERL_REF;
              val.val.ref = data;
              p_value_unprotect (data);
              
              this->gc.notify_increase (cap * sizeof (p_value));
            }
            break;
          
          // array_set - set an element within an array.
          case 0x31:
            {
              p_value& arr = stack[sp - 3];
              long long index = stack[sp - 2].val.i64;
              if (index < 0)
                throw std::runtime_error ("invalid index");
              
              if (arr.type == PERL_REF && arr.val.ref->type == PERL_ARRAY)
                {
                  auto& data = arr.val.ref->val.arr;
                  
                  // resize if necessary
                  if (index >= data.len)
                    {
                      // need more space?
                      if (index >= data.cap)
                        {
                          // resize internal buffer
                          
                          unsigned int ncap = data.cap * 18 / 10;
                          if (index >= ncap)
                            ncap = index + 1;
                          p_value *narr = new p_value [ncap];
                          for (unsigned int i = 0; i < data.len; ++i)
                            narr[i] = data.data[i];
                          for (unsigned int i = data.len; i <= index; ++i)
                            narr[i].type = PERL_UNDEF;
                          
                          this->gc.notify_increase ((ncap - data.cap) * sizeof (p_value));
                          data.cap = ncap;
                          delete[] data.data;
                          data.data = narr;
                        }
                      
                      data.len = index + 1;
                    }
                  
                  data.data[index] = stack[sp - 1];
                }
              
              sp -= 3;
            }
            break;
          
          // array_get
          case 0x32:
            {
              p_value& arr = stack[sp - 2];
              long long index = stack[sp - 1].val.i64;
              long long arr_len = p_value_array_length (arr);
              if (index < 0 || index >= arr_len)
                {
                  -- sp;
                  stack[sp - 1].type = PERL_UNDEF;
                  break;
                }
              
              if (arr.type == PERL_REF && arr.val.ref->type == PERL_ARRAY)
                {
                  auto& data = arr.val.ref->val.arr;
                  
                  -- sp;
                  stack[sp - 1] = data.data[index];
                }
              else
                {
                  -- sp;
                  stack[sp - 1].type = PERL_UNDEF;
                }
            }
            break;
          
          // box_array
          // NOTE: Any array parameters will have their elements inserted into
          //       the resulting array (flattening it out).
          case 0x33:
            {
              unsigned char count = *ptr++;
              if (count == 1)
                {
                  if (stack[sp - 1].type == PERL_REF &&
                      stack[sp - 1].val.ref &&
                      stack[sp - 1].val.ref->type == PERL_ARRAY)
                    {
                      // don't do anything
                      break;
                    }
                }
              
              unsigned int tcount = 0;
              for (unsigned int i = 0; i < count; ++i)
                {
                  auto& val = stack[sp - i - 1];
                  if (val.type == PERL_REF && val.val.ref && val.val.ref->type == PERL_ARRAY)
                    {
                      // flatten out
                      tcount += val.val.ref->val.arr.len;
                    }
                  else
                    ++ tcount;
                }
              
              unsigned int cap = tcount ? tcount : 1;
              
              p_value *data = this->gc.alloc (true);
              data->type = PERL_ARRAY;
              data->val.arr.len = tcount;
              data->val.arr.cap = cap;
              data->val.arr.data = new p_value [cap];
              unsigned int n = 0;
              for (unsigned int i = 0; i < count; ++i)
                {
                  auto& val = stack[sp - (count - i)];
                  if (val.type == PERL_REF && val.val.ref && val.val.ref->type == PERL_ARRAY)
                    {
                      // flatten out
                      auto& arr = *val.val.ref;
                      unsigned int arr_len = arr.val.arr.len;
                      for (unsigned int j = 0; j < arr_len; ++j)
                        data->val.arr.data[n++] = arr.val.arr.data[j];
                    }
                  else
                    data->val.arr.data[n++] = val;
                }
              
              sp -= count;
              
              p_value& val = stack[sp++];
              val.type = PERL_REF;
              val.val.ref = data;
              p_value_unprotect (data);
              this->gc.notify_increase (cap * sizeof (p_value));
            }
            break;
          
//------------------------------------------------------------------------------
         
         /*  
          * 40-4F: Casting.
          */
//------------------------------------------------------------------------------

        // to_str
        case 0x40:
          if (stack[sp - 1].type == PERL_CSTR ||
              (stack[sp - 1].type == PERL_REF && stack[sp - 1].val.ref && stack[sp - 1].val.ref->type == PERL_DSTR))
            break;
          stack[sp - 1] = p_value_to_str (stack[sp - 1], *this);
          _unprotect_external (stack[sp - 1]);
          break;
        
        // to_int
        case 0x41:
          stack[sp - 1] = p_value_to_int (stack[sp - 1], *this);
          _unprotect_external (stack[sp - 1]);
          break;
        
//------------------------------------------------------------------------------
          
          
          /* 
           * 60-6F: Frame manipulation.
           */
//------------------------------------------------------------------------------
          
          // push_frame - constructs a new frame
          case 0x60:
            {
              unsigned int locs = *((unsigned int *)ptr);
              ptr += 4;
              
              CHECK_STACK_SPACE(locs + 1)  // local variables + base pointer
              
              // remember current base pointer
              stack[sp].type = PERL_INTERNAL;
              stack[sp].val.i64 = bp;
              ++ sp;
              
              // reserve enough space for the local variables
              bp = sp - 1;
              sp += locs;
            }
            break;
          
          // pop_frame - destroys the topmost frame.
          case 0x61:
            {
              // destroy local variables
              // TODO: request the GC to free them?
              /*{
                unsigned int locs = sp - bp - 1;
                for (unsigned int i = 0; i < locs; ++i)
                  p_value_delete (stack[bp + 1 + i]);
              }
              */
              
              int pbp = stack[bp].val.i64;
              sp = bp;
              bp = pbp;
            }
            break;
          
          // load - load local variable onto stack.
          case 0x62:
            CHECK_STACK_SPACE(1)
            
            stack[sp] = stack[bp + 1 + *ptr++];
            ++ sp;
            break;
          
          // store - put topmost value into local variable.
          case 0x63:
            {
              unsigned int index = bp + 1 + *ptr++;
              
              -- sp;
              stack[index] = stack[sp];
            }
            break;
          
          // loadl - accepts 4-byte indices.
          case 0x64:
            CHECK_STACK_SPACE(1)
            stack[sp] = stack[bp + 1 + *((unsigned int *)ptr)];
            ++ sp;
            ptr += 4;
            break;
          
          // storel - accepts 4-byte indices.
          case 0x65:
            {
              unsigned int index = bp + 1 + *((unsigned int *)ptr);
              ptr += 4;
              
              -- sp;
              stack[index] = stack[sp];
            }
            break;
          
          // storeload - same as a store followed by a load
          case 0x66:
            {
              unsigned int index = bp + 1 + *ptr++;
              stack[index] = stack[sp - 1];
            }
            break;
          
          // storeloadl - accepts 4-byte indices.
          case 0x67:
            {
              unsigned int index = bp + 1 + *((unsigned int *)ptr);
              ptr += 4;
              
              stack[index] = stack[sp - 1];
            }
            break;
          
          // load_ref - loads a reference of local variable
          case 0x68:
            CHECK_STACK_SPACE(1)
            stack[sp].type = PERL_REF;
            stack[sp].val.ref = &stack[bp + 1 + *ptr++];
            ++ sp;
            break;
          
          // load_refl - loads a reference of local variable
          case 0x69:
            CHECK_STACK_SPACE(1)
            {
              stack[sp].type = PERL_REF;
              stack[sp].val.ref = &stack[bp + 1 + *((unsigned int *)ptr)];
              ++ sp;
              ptr += 4;
            }
            break;
          
//------------------------------------------------------------------------------
          
          /* 
           * 70-7F: Subroutines:
           */
//------------------------------------------------------------------------------
          
          // call_builtin
          case 0x70:
            {
              unsigned short index = *((unsigned short *)ptr);
              ptr += 2;
              
              unsigned char param_count = *ptr++;
              
              switch (index)
                {
                case 1: this->builtin_print (param_count); break;
                case 2: this->builtin_push (param_count); break;
                case 3: this->builtin_elems (param_count); break;
                case 4: this->builtin_range (param_count); break;
                case 5: this->builtin_substr (param_count); break;
                case 6: this->builtin_length (param_count); break;
                case 7: this->builtin_shift (param_count); break;
                }
            }
            break;
          
          // call
          case 0x71:
            {
              unsigned int pos = *((unsigned int *)ptr);
              ptr += 4;
              
              // push return address
              stack[sp].type = PERL_INTERNAL;
              stack[sp].val.i64 = ptr - code;
              ++ sp;
              
              ptr = code + pos;
            }
            break;
          
          // return
          case 0x72:
            {
              ptr = code + stack[bp - 1].val.i64;
              
              int ret_index = sp - 1;
              int pbp = stack[bp].val.i64;
              sp = bp - 2;
              bp = pbp;
              
              stack[sp++] = stack[ret_index];
            }
            break;
          
          // arg_load
          case 0x73:
            CHECK_STACK_SPACE(1)
            {
              auto arr = stack[bp - 2].val.ref;
              stack[sp++] = arr->val.arr.data[*ptr++];
            }
            break;
          
          // arg_store
          case 0x74:
            {
              auto arr = stack[bp - 2].val.ref;
              arr->val.arr.data[*ptr++] = stack[--sp];
            }
            break;
            
          // arg_load_ref - load reference to an argument
          case 0x75:
            CHECK_STACK_SPACE(1)
            {
              auto arr = stack[bp - 2].val.ref;
              
              stack[sp].type = PERL_REF;
              stack[sp].val.ref = &arr->val.arr.data[*ptr++];
              ++ sp;
            }
            break;
          
          // load_arg_array
          case 0x76:
            CHECK_STACK_SPACE(1)
            stack[sp++] = stack[bp - 2];
            break;
          
          // tail_call
          case 0x77:
            // TODO
            break;
                    
//------------------------------------------------------------------------------

          /* 
           * F0-FF: Other:
           */
//------------------------------------------------------------------------------
          
          // exit
          case 0xF0:
            goto done;
          
//------------------------------------------------------------------------------
          }
      }
  
  done: ;
    //this->gc.collect ();
  }
}

