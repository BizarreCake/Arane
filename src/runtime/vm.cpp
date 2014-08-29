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

#include "runtime/vm.hpp"
#include "runtime/gc.hpp"
#include "runtime/builtins.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>

#include <iomanip> // DEBUG

// DEBUG
#include <thread>
#include <chrono>


namespace arane {
  
#define STACK_SIZE        4096
  
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
  
  
  
  static inline void
  _unprotect_external (p_value& val)
  {
    if (val.type == PERL_REF)
      {
        if (val.val.ref)
          p_value_unprotect (val.val.ref);
      }
  }
  
  static void
  _flatten (p_value *stack, int& sp)
  {
    auto& arrv = stack[sp - 1];
    if (arrv.type == PERL_REF &&
        arrv.val.ref &&
        arrv.val.ref->type == PERL_ARRAY)
      {
        auto& data = arrv.val.ref->val.arr;
        
        -- sp;
        for (unsigned int i = 0; i < data.len; ++i)
          {
            stack[sp++] = data.data[i];
            _flatten (stack, sp);
          }
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
            stack[sp].val.i64 = (char)*ptr++;
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
          
          // push_true
          case 0x09:
            CHECK_STACK_SPACE(1)
            stack[sp].type = PERL_BOOL;
            stack[sp].val.bl = true;
            ++ sp;
            break;
          
          // push_false
          case 0x0A:
            CHECK_STACK_SPACE(1)
            stack[sp].type = PERL_BOOL;
            stack[sp].val.bl = false;
            ++ sp;
            break;
          
          // copy - performs a shallow copy of the top-most item on the stack.
          case 0x0B:
            CHECK_STACK_SPACE(1)
            stack[sp] = p_value_copy (stack[sp - 1], *this);
            ++ sp;
            _unprotect_external (stack[sp - 1]);
            break;
          
//------------------------------------------------------------------------------
          
          
          /* 
           * 10-1F: Basic operations.
           */
//------------------------------------------------------------------------------
          
          // add
          case 0x10: 
            stack[sp - 2] = p_value_add (stack[sp - 2], stack[sp - 1], *this);
            -- sp;
            _unprotect_external (stack[sp - 1]);
            break;
          
          // sub
          case 0x11:
            stack[sp - 2] = p_value_sub (stack[sp - 2], stack[sp - 1], *this);
            -- sp;
            _unprotect_external (stack[sp - 1]);
            break;
          
          // mul
          case 0x12:
            stack[sp - 2] = p_value_mul (stack[sp - 2], stack[sp - 1], *this);
            -- sp;
            _unprotect_external (stack[sp - 1]);
            break;
          
          // div
          case 0x13:
            stack[sp - 2] = p_value_div (stack[sp - 2], stack[sp - 1], *this);
            -- sp;
            _unprotect_external (stack[sp - 1]);
            break;
          
          // mod
          case 0x14:
            stack[sp - 2] = p_value_mod (stack[sp - 2], stack[sp - 1], *this);
            -- sp;
            _unprotect_external (stack[sp - 1]);
            break;
          
          // concat - concatenate two values together into a string.
          case 0x15:
            stack[sp - 2] = p_value_concat (stack[sp - 2], stack[sp - 1], *this);
            -- sp;
            _unprotect_external (stack[sp - 1]);
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
          
          // jt - jump if true (expects a bool)
          case 0x27:
            if (stack[-- sp].val.bl)
              ptr += *((short *)ptr);
            ptr += 2;
            break;
          
          // jf - jump if false (expects a bool)
          case 0x28:
            if (!stack[-- sp].val.bl)
              ptr += *((short *)ptr);
            ptr += 2;
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
          
          // arrayify - creates and array from the top-most elements in the stack.
          case 0x33:
            {
              unsigned short count = *((unsigned short *)ptr);
              ptr += 2;
              
              unsigned int cap = count ? count : 1;
              p_value *data = this->gc.alloc (true);
              data->type = PERL_ARRAY;
              auto& arr = data->val.arr;
              arr.cap = cap;
              arr.len = count;
              arr.data = new p_value[cap];
              for (unsigned int i = 0; i < count; ++i)
                {
                  arr.data[i] = stack[sp - (count - i)];
                }
              
              sp -= count;
              stack[sp].type = PERL_REF;
              stack[sp].val.ref = data;
              ++ sp;
              p_value_unprotect (data);
            }
            break;
          
          // flatten
          case 0x34:
            _flatten (stack, sp);
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
        
        // to_bint
        case 0x42:
          stack[sp - 1] = p_value_to_big_int (stack[sp - 1], *this);
          _unprotect_external (stack[sp - 1]);
          break;
        
        // to_bool
        case 0x43:
          stack[sp - 1] = p_value_to_bool (stack[sp - 1], *this);
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
              for (unsigned int i = 0; i < locs; ++i)
                stack[sp++].type = PERL_UNDEF;
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
            stack[sp].val.ref->is_gc = false;
            ++ sp;
            break;
          
          // load_refl - loads a reference of local variable
          case 0x69:
            CHECK_STACK_SPACE(1)
            {
              stack[sp].type = PERL_REF;
              stack[sp].val.ref = &stack[bp + 1 + *((unsigned int *)ptr)];
              stack[sp].val.ref->is_gc = false;
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
                case 0x100: builtins::print (*this, param_count); break; 
                case 0x101: builtins::say (*this, param_count); break;
                
                case 0x200: builtins::elems (*this, param_count); break; 
                case 0x201: builtins::push (*this, param_count); break;
                case 0x202: builtins::pop (*this, param_count); break;
                case 0x203: builtins::shift (*this, param_count); break;
                case 0x204: builtins::range (*this, param_count); break;
                }
            }
            break;
          
          // call
          case 0x71:
            {
              unsigned int pos = *((unsigned int *)ptr);
              ptr += 4;
              
              unsigned char paramc = *ptr++;
              
              // push return address
              stack[sp].type = PERL_INTERNAL;
              stack[sp].val.i64 = ptr - code;
              ++ sp;
              
              // parameter count
              stack[sp].type = PERL_INTERNAL;
              stack[sp].val.i64 = paramc;
              ++ sp;
              
              ptr = code + pos;
            }
            break;
          
          // return
          case 0x72:
            {
              ptr = code + stack[bp - 2].val.i64;
              
              unsigned char paramc = stack[bp - 1].val.i64;
              
              int ret_index = sp - 1;
              int pbp = stack[bp].val.i64;
              sp = bp - 2 - paramc;
              bp = pbp;
              
              stack[sp++] = stack[ret_index];
            }
            break;
          
          // arg_load
          case 0x73:
            CHECK_STACK_SPACE(1)
            stack[sp++] = stack[bp - 3 - *ptr++];
            break;
          
          // arg_store
          case 0x74:
            stack[bp - 3 - *ptr++] = stack[--sp];
            break;
            
          // arg_load_ref - load reference to an argument
          case 0x75:
            CHECK_STACK_SPACE(1)
            stack[sp].type = PERL_REF;
            stack[sp].val.ref = &stack[bp - 3 - *ptr++];
            stack[sp].val.ref->is_gc = false;
            ++ sp;
            break;
         
         // make_arg_array - creates an array from the top-most elements in the
         //                  stack without consuming them (elements are read in
         //                  reverse order).
         case 0x78:
          {
            unsigned short count = *((unsigned short *)ptr);
            ptr += 2;
            
            unsigned int cap = count ? count : 1;
            p_value *data = this->gc.alloc (true);
            data->type = PERL_ARRAY;
            auto& arr = data->val.arr;
            arr.cap = cap;
            arr.len = count;
            arr.data = new p_value[cap];
            for (unsigned int i = 0; i < count; ++i)
              {
                arr.data[i] = stack[sp - i - 1];
              }
            
            stack[sp].type = PERL_REF;
            stack[sp].val.ref = data;
            ++ sp;
            p_value_unprotect (data);
          }
          break;
                    
//------------------------------------------------------------------------------
          
          /* 
           * 80-8F: Types.
           */
//------------------------------------------------------------------------------
          
          // push_type
          case 0x80:
            CHECK_STACK_SPACE(1)
            stack[sp].type = PERL_TYPE;
            stack[sp].val.typ = (p_basic_type)*ptr++;
            ++ sp;
            break;
          
          // to_compatible
          case 0x81:
            {
              // number of types in type hierarchy.
              unsigned char tc = *ptr++;
              
              // form a type stack
              p_basic_type types[0x100];
              for (unsigned int i = 0; i < tc; ++i)
                types[i] = stack[sp - tc + i].val.typ;
              
              auto& val = stack[sp - tc - 1];
              stack[sp - tc - 1] = p_value_to_compatible (val, types, tc, *this);
              sp -= tc;
              _unprotect_external (stack[sp - 1]);
            }
            break;
          
//------------------------------------------------------------------------------
          
          
          
          /* 
           * F0-FF: Other:
           */
//------------------------------------------------------------------------------
          
          // exit
          case 0xF0:
            goto done;
          
          // checkpoint
          case 0xF1:
            {
              int n = *((int *)ptr);
              ptr += 4;
              
              std::cout << "### CHECKPOINT " << n << " ###" << std::endl;
            }
            break;
          
//------------------------------------------------------------------------------
          }
      }
  
  done: ;
  }
}

