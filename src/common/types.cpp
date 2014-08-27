
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

#include "common/types.hpp"
#include <stdexcept>


namespace arane {
  
  /* 
   * Returns a type_info structure that represents no/any type.
   */
  type_info
  type_info::none ()
  {
    type_info ti;
    ti.types.push_back ({
      .type = TYPE_NONE,
      .name = "",
    });
    
    return ti;
  }
  
  
  
  void
  type_info::push_basic (basic_types typ)
  {
    if (!this->types.empty () && this->types.back ().type == TYPE_NONE)
      this->types.pop_back ();
    
    this->types.push_back ({
      .type = typ,
    });
  }
  
  
  
  bool
  type_info::is_none () const
  {
    return this->types.empty () || (this->types[0].type == TYPE_NONE);
  }
  
  
  
  /* 
   * Returns true if this type can be safely casted (in a meaningful way)
   * into the specified type.
   */
  type_compatibility
  type_info::check_compatibility (const type_info& other) const
  {
    if (this->types.size () != 1 ||
        other.types.size () != 1)
      throw std::runtime_error ("hierarchical types not supported yet");
    
    if (this->types[0] == other.types[0])
      return TC_COMPATIBLE;
    
    switch (this->types[0].type)
      {
      case TYPE_INT_NATIVE:
        switch (other.types[0].type)
          {
          case TYPE_INT:
            return TC_CASTABLE;
          
          default: ;
          }
        break;
      
      default: ;
      }
    
    return TC_INCOMPATIBLE;
  }
  
  
  
  /* 
   * Returns a textual representation of the type.
   */
  std::string
  type_info::str () const
  {
    std::string s;
    
    for (unsigned int i = 0; i < this->types.size (); ++i)
      {
        auto& bt = this->types[i];
        switch (bt.type)
          {
          case TYPE_INVALID:      s.append ("<invalid>"); break;
          case TYPE_NONE:         s.append ("<unspecified>"); break;
          
          case TYPE_INT_NATIVE:   s.append ("int"); break;
          case TYPE_INT:          s.append ("Int"); break;
          case TYPE_BOOL_NATIVE:  s.append ("bool"); break;
          case TYPE_STR:          s.append ("Str"); break;
          case TYPE_ARRAY:        s.append ("Array"); break;
          
          case TYPE_OBJECT:       s.append (bt.name); break;
          }
        
        if (i != this->types.size () - 1)
          s.append (" of ");
      }
    
    return s;
  }
  
  
  
  bool
  basic_type::operator== (const basic_type& other) const
  {
    if (this->type != other.type)
      return false;
    
    if (this->type == TYPE_OBJECT)
      {
        if (this->name != other.name)
          return false;
      }
    
    return true;
  }
  
  bool
  basic_type::operator!= (const basic_type& other) const
    { return !(this->operator== (other)); }
  
  
  bool
  type_info::operator== (const type_info& other) const
  {
    if (this->types.size () != other.types.size ())
      return false;
    
    for (unsigned int i = 0; i < this->types.size (); ++i)
      if (this->types[i] != other.types[i])
        return false;
    
    return true;
  }
  
  bool
  type_info::operator!= (const type_info& other) const
    { return !(this->operator== (other)); }
}

