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

#ifndef _ARANE__COMMON__UTILS__H_
#define _ARANE__COMMON__UTILS__H_

#include "common/types.hpp"
#include "parser/ast.hpp"
#include <string>


namespace arane {
  
  namespace utils {
    
    /* 
     * Strips package names from the specified path (e.g. Turns `Foo::bar' into
     * just `bar').
     */
    std::string strip_packages (const std::string& path);

    /* 
     * Turns a name such as 'Foo::Bar' in
     *     use Foo::Bar;
     * into /Foo/Bar.pm
     */    
    std::string module_name_to_path (const std::string& name);
    
    
    
    /* 
     * Returns the specified type boxed accordingly (e.g. into an array).
     */
    type_info get_boxed (const type_info& ti, ast_ident_type typ);
  }
}

#endif

