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

#ifndef _ARANE__COMMON__TYPES__H_
#define _ARANE__COMMON__TYPES__H_

#include <vector>
#include <string>


namespace arane {
  
  /* 
   * Simple (non-hierarchical) Perl types.
   */
  enum basic_types
  {
    TYPE_INVALID,               // used by the parser
    
    TYPE_NONE,                  // unspecified
    
    TYPE_INT_NATIVE,            // int
    TYPE_INT,                   // Int
    TYPE_BOOL_NATIVE,           // bool
    TYPE_STR,                   // Str
    TYPE_ARRAY,                 // Array
    
    TYPE_OBJECT,                // custom
  };
  
  struct basic_type
  {
    basic_types type;
    std::string name; // if object
    
  public:
    bool operator== (const basic_type& other) const;
    bool operator!= (const basic_type& other) const;
  };
  
  
  
  enum type_compatibility
  {
    TC_INCOMPATIBLE,
    TC_CASTABLE,      // can be casted implicitly
    TC_COMPATIBLE,
  };
  
  /* 
   * Represents a (possibly hierarchical) Perl type.
   */
  struct type_info
  {
    std::vector<basic_type> types;
    
  public:
    /* 
     * Returns a type_info structure that represents no/any type.
     */
    static type_info none ();
    
  public:
    void push_basic (basic_types typ);

    /* 
     * Boxes the type into an array.
     */
    void to_array ();
    
    bool is_none () const;
    
    /* 
     * Returns true if this type can be safely casted (in a meaningful way)
     * into the specified type.
     */
    type_compatibility check_compatibility (const type_info& other) const;
    
    /* 
     * Returns a textual representation of the type.
     */
    std::string str () const;
    
  public:
    bool operator== (const type_info& other) const;
    bool operator!= (const type_info& other) const;
  };
}

#endif

