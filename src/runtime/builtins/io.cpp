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


namespace arane {
  
  void
  builtins::print (virtual_machine& vm, int param_count)
  {
    auto stack = vm.stack;
    int& sp    = vm.sp;
    
    for (int i = 0; i < param_count; ++i)
      {
        auto& val = stack[sp - 1 - i];
        *vm.out << p_value_str (val);
      }
    
    sp -= param_count;
    stack[sp++].type = PERL_UNDEF;
  }
  
  
  
  void
  builtins::say (virtual_machine& vm, int param_count)
  {
    auto stack = vm.stack;
    int& sp    = vm.sp;
    
    for (int i = 0; i < param_count; ++i)
      {
        auto& val = stack[sp - 1 - i];
        *vm.out << p_value_str (val);
      }
    *vm.out << std::endl;
    
    sp -= param_count;
    stack[sp++].type = PERL_UNDEF;
  }
}

