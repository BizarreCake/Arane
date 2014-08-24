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

#include "runtime/builtins.hpp"
#include "runtime/vm.hpp"
#include "runtime/value.hpp"
#include <cstring>


namespace arane {
  
  void
  builtins::elems (virtual_machine& vm, int param_count)
  {
    auto stack = vm.stack;
    int& sp    = vm.sp;
    
    if (param_count != 1)
      throw vm_error ("builtin `elems' expects 1 parameter");
    
    auto& val = stack[sp - 1];
    if (!(val.type == PERL_REF &&
          val.val.ref &&
          val.val.ref->type == PERL_ARRAY))
      throw vm_error ("parameter passed to builtin `elems' is not an array");
    
    auto& data = val.val.ref->val.arr;
    
    sp -= param_count;
    stack[sp].type = PERL_INT;
    stack[sp].val.i64 = data.len;
    ++ sp;
  }
  
  
  
  void
  builtins::push (virtual_machine& vm, int param_count)
  {
    auto stack = vm.stack;
    int& sp    = vm.sp;
    
    if (param_count < 2)
      throw vm_error ("builtin `push' expects at least 2 parameters");
    
    auto& val = stack[sp - 1];
    if (!(val.type == PERL_REF &&
          val.val.ref &&
          val.val.ref->type == PERL_ARRAY))
      throw vm_error ("first parameter passed to builtin `push' is not an array");
    
    auto& data = val.val.ref->val.arr;
    if ((data.len + (param_count - 1)) > data.cap)
      {
        // resize array
        
        unsigned int ncap = data.len * 2;
        if ((data.len + (param_count - 1)) > ncap)
          ncap += param_count - 1;
        
        p_value *vals = new p_value [ncap];
        for (unsigned int i = 0; i < data.len; ++i)
          vals[i] = data.data[i];
        delete[] data.data;
        data.data = vals;
        
        vm.gc.notify_increase ((ncap - data.cap) * sizeof (p_value));
        data.cap = ncap;
      }
    
    for (int i = 1; i < param_count; ++i)
      data.data[data.len ++] = stack[sp - 1 - i];
    
    sp -= param_count;
    stack[sp++] = val;
  }
  
  
  
  void
  builtins::pop (virtual_machine& vm, int param_count)
  {
    auto stack = vm.stack;
    int& sp    = vm.sp;
    
    if (param_count != 1)
      throw vm_error ("builtin `pop' expects 1 parameter");
    
    auto& val = stack[sp - 1];
    if (!(val.type == PERL_REF &&
          val.val.ref &&
          val.val.ref->type == PERL_ARRAY))
      throw vm_error ("parameter passed to builtin `pop' is not an array");
    
    auto& data = val.val.ref->val.arr;
    if (data.len == 0)
      throw vm_error ("array passed to builtin `pop' is empty");
    
    sp -= param_count;
    stack[sp++] = data.data[-- data.len];
  }
  
  
  
  void
  builtins::shift (virtual_machine& vm, int param_count)
  {
    auto stack = vm.stack;
    int& sp    = vm.sp;
    
    if (param_count != 1)
      throw vm_error ("builtin `shift' expects 1 parameter");
    
    auto& val = stack[sp - 1];
    if (!(val.type == PERL_REF &&
          val.val.ref &&
          val.val.ref->type == PERL_ARRAY))
      throw vm_error ("parameter passed to builtin `shift' is not an array");
    
    auto& data = val.val.ref->val.arr;
    if (data.len == 0)
      throw vm_error ("array passed to builtin `shift' is empty");
    
    sp -= param_count;
    stack[sp++] = data.data[0];
    
    // shift array
    -- data.len;
    std::memmove (data.data, data.data + 1, data.len * sizeof (p_value));
  }
  
  
  
  void
  builtins::range (virtual_machine& vm, int param_count)
  {
    auto stack = vm.stack;
    int& sp    = vm.sp;
    
    if (param_count != 4)
      throw vm_error ("builtin `range' expects 4 parameters");
    
    if (stack[sp - 1].type != PERL_INT ||
        stack[sp - 2].type != PERL_INT ||
        stack[sp - 3].type != PERL_INT ||
        stack[sp - 4].type != PERL_INT)
      throw vm_error ("invalid parameters passed to builtin `range'");
    
    long long lhs = stack[sp - 1].val.i64;
    long long rhs = stack[sp - 2].val.i64;
    
    if (stack[sp - 3].val.i64)
      ++ lhs;
    if (stack[sp - 4].val.i64)
      -- rhs;
    long long count = (rhs < lhs) ? 0 : (rhs - lhs + 1);
    
    // create array
    p_value *data = vm.gc.alloc (true);
    data->type = PERL_ARRAY;
    auto& arr = data->val.arr;
    unsigned int cap = count ? count : 1;
    arr.len = count;
    arr.data = new p_value [cap];
    for (int i = 0; i < count; ++i)
      {
        auto& val = arr.data[i];
        val.type = PERL_INT;
        val.val.i64 = lhs + i;
      }
    
    sp -= param_count;
    stack[sp].type = PERL_REF;
    stack[sp].val.ref = data;
    ++ sp;
  }
}

