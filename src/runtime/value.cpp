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

#include "runtime/value.hpp"
#include "runtime/vm.hpp"
#include "runtime/bigint.hpp"
#include <stdexcept>
#include <cstring>
#include <sstream>
#include <iomanip>

#include <iostream> // DEBUG


namespace arane {
  
  static std::string
  _get_type_name_from_ptype (p_basic_type type)
  {
    switch (type)
      {
      case PTYPE_INT_NATIVE:   return "int";
      case PTYPE_INT:          return "Int";
      case PTYPE_BOOL_NATIVE:  return "bool";
      case PTYPE_STR:          return "Str";
      case PTYPE_ARRAY:        return "Array";
      
      default: return "<invalid type>";
      }
  }
  
  static std::string
  _get_type_name_from_value (p_value& val)
  {
    switch (val.type)
      {
      case PERL_UNDEF:      return "undef";
      case PERL_INT:        return "int";
      case PERL_BOOL:       return "bool";
      
      case PERL_REF:
        {
          if (!val.val.ref)
            return "null";
          switch (val.val.ref->type)
            {
            case PERL_ARRAY:    return "Array";
            case PERL_DSTR:     return "Str";
            case PERL_CSTR:     return "Str";
            case PERL_BIGINT:   return "Int";
            
            default: return "<invalid type>";
            }
        }
        break;
      
      default: return "<invalid type>";
      }
  }
  
  /* 
   * Attempts to cast the specified value into a compatible type.
   */
  p_value
  p_value_to_compatible (p_value& a, p_basic_type *types, int count,
    virtual_machine& vm)
  {
    if (count != 1)
      throw std::runtime_error ("not implemented");
    
#define THROW_TYPE_ERROR \
  throw type_error ( \
    "cannot cast value of type `" + _get_type_name_from_value (a) + \
    "' to type `" + _get_type_name_from_ptype (etyp) + "'");
    
    p_basic_type etyp = types[0];
    switch (etyp)
      {
      // to int
      case PTYPE_INT_NATIVE:
        switch (a.type)
          {
          // int -> int
          case PERL_INT:
            return a;
          
          default:
            THROW_TYPE_ERROR
          }
        break;
      
      // to Int
      case PTYPE_INT:
        switch (a.type)
          {
          // int -> Int
          case PERL_INT:
            {
              p_value *data = vm.get_gc ().alloc (true);
              data->type = PERL_BIGINT;
              data->val.bint = new big_int (a.val.i64);
              
              p_value res;
              res.type = PERL_REF;
              res.val.ref = data;
              return res;
            }
            break;
          
          case PERL_REF:
            switch (a.val.ref->type)
              {
              // Int -> Int
              case PERL_BIGINT:
                return a;
              
              default:
                THROW_TYPE_ERROR
              }
            break;
          
          default:
            THROW_TYPE_ERROR
          }
        break;
      
      // to bool
      case PTYPE_BOOL_NATIVE:
        switch (a.type)
          {
          case PERL_BOOL:
            return a;
          
          default:
            THROW_TYPE_ERROR
          }
        break;
      
      // to Str
      case PTYPE_STR:
        switch (a.type)
          {
          case PERL_REF:
            switch (a.val.ref->type)
              {
              case PERL_DSTR:
                return a;
              
              default:
                THROW_TYPE_ERROR
              }
            break;
          
          case PERL_CSTR:
            return a;
          
          default:
            THROW_TYPE_ERROR
          }
        break;
      
      default:
        THROW_TYPE_ERROR
      }
  }
  
  
  
  /* 
   * Removes GC protection from the specified value.
   */
  void
  p_value_unprotect (p_value *val)
  {
    val->gc_protect = false;
  }
  
  
  
  /* 
   * Performs a deep copy.
   */
  void
  p_value_copy (p_value& dest, p_value& src)
  {
    switch (src.type)
      {
      default:
        dest = src;
        break;
      }
  }
  
  static const char*
  _type_to_str (p_value_type type)
  {
    switch (type)
      {
      case PERL_REF: return "REF";
      case PERL_ARRAY: return "ARRAY";
      
      default: return "SCALAR";
      }
  }
  
  /* 
   * Returns a textual representation of the specified value.
   */  
  std::string
  p_value_str (p_value& val)
  {
    if (val.type == PERL_REF)
      {
        p_value *ref = val.val.ref;
        if (!ref)
          return "REF(0)";
        
        switch (val.val.ref->type)
          {
          case PERL_DSTR:
          case PERL_ARRAY:
          case PERL_BIGINT:
            return p_value_str (*ref);
            
          default:
            {
              std::ostringstream ss;
              if (ref->type == PERL_REF && ref->val.ref)
                {
                  switch (ref->val.ref->type)
                    {
                    case PERL_ARRAY: ss << "ARRAY"; break;
                    case PERL_DSTR: ss << "SCALAR"; break;
                    
                    default:
                      ss << _type_to_str (ref->type);
                      break;
                    }
                }
              else
                ss << _type_to_str (ref->type);
              ss << '(';
              ss << std::hex << "0x" << (void *)ref;
              ss << ')';
              return ss.str ();
            }
          }
      }
    
    switch (val.type)
      {
      case PERL_BOOL:
        return std::string (val.val.bl ? "True" : "False");
        
      case PERL_CSTR:
        return std::string (val.val.cstr.data);
      
      case PERL_DSTR:
        return std::string (val.val.str.data);
      
      case PERL_INT:
        {
          std::ostringstream ss;
          ss << val.val.i64;
          return ss.str ();
        }
        
      case PERL_BIGINT:
        {
          std::string str;
          val.val.bint->to_str (str);
          return str;
        }
        
      case PERL_ARRAY:
        {
          std::string str;
          for (unsigned int i = 0; i < val.val.arr.len; ++i)
            {
              str.append (p_value_str (val.val.arr.data[i]));
              if (i != val.val.arr.len - 1)
                str.push_back (' ');
            }
          return str;
        }
        
      default:
        return "";
      }
  }
  
  /*
   * Checks whether the contents of the two specified values are references
   * to the same data.
   */
  bool
  p_value_ref_eq (p_value& a, p_value& b)
  {
    return a.val.ref == b.val.ref;
  }
  
  
  
  long long
  p_value_array_length (p_value& arr)
  {
    if (arr.type == PERL_REF)
      return p_value_array_length (*arr.val.ref);
    else if (arr.type != PERL_ARRAY)
      return 0;
    
    return arr.val.arr.len;
  }
  
  
  
  /* 
   * Checks for equality between the two specified values.
   */
  bool
  p_value_eq (p_value& a, p_value& b)
  {
    switch (a.type)
      {
      case PERL_BOOL:
        switch (b.type)
          {
          case PERL_BOOL:
            return a.val.bl == b.val.bl;
            
          case PERL_INT:
            return a.val.bl == (b.val.i64 ? true : false);
          
          case PERL_REF:
            switch (b.type)
              {
              case PERL_BIGINT:
                return a.val.bl == !b.val.ref->val.bint->is_zero ();
              
              default:
                return false;
              }
            break;
          
          default:
            return false;
          }
        break;
      
      case PERL_INT:
        switch (b.type)
          {
          case PERL_BOOL:
            return (a.val.i64 ? true : false) == b.val.bl;
          
          case PERL_INT:
            return a.val.i64 == b.val.i64;
          
          case PERL_REF:
            switch (b.val.ref->type)
              {
              case PERL_BIGINT:
                return (b.val.ref->val.bint->cmp (a.val.i64) == 0);
              
              default:
                return false;
              }
            break;
          
          default:
            return false;
          }
        break;
      
      case PERL_CSTR:
        switch (b.type)
          {
          case PERL_CSTR:
            return (std::strcmp (a.val.cstr.data, b.val.cstr.data) == 0);
          
          case PERL_DSTR:
            return (std::strcmp (a.val.cstr.data, b.val.str.data) == 0);
          
          default:
            return false;
          }
        break;
      
      case PERL_REF:
        switch (a.val.ref->type)
          {
          case PERL_DSTR:
            return p_value_eq (b, *a.val.ref);
          
          case PERL_BIGINT:
            switch (b.type)
              {
              case PERL_BOOL:
                return !a.val.ref->val.bint->is_zero () == b.val.bl;
              
              case PERL_INT:
                return (a.val.ref->val.bint->cmp (b.val.i64) == 0);
              
              case PERL_REF:
                switch (b.val.ref->type)
                  {
                  case PERL_BIGINT:
                    return (a.val.ref->val.bint->cmp (*b.val.ref->val.bint) == 0);
                  
                  default:
                    return false;
                  }
                break;
              
              default:
                return false;
              }
            break;
          
          default:
            return false;
          }
        break;
      
      default:
        return a.type == b.type;
      }
  }
  
  bool
  p_value_lt (p_value& a, p_value& b)
  {
    switch (a.type)
      {
      case PERL_INT:
        switch (b.type)
          {
          case PERL_INT:
            return a.val.i64 < b.val.i64;
          
          case PERL_REF:
            switch (b.val.ref->type)
              {
              case PERL_BIGINT:
                return (b.val.ref->val.bint->cmp (a.val.i64) >= 0);
              
              default:
                return false;
              }
            break;
          
          default:
            return false;
          }
        break;
      
      case PERL_REF:
        switch (a.val.ref->type)
          {
          case PERL_DSTR:
            return p_value_eq (b, *a.val.ref);
          
          case PERL_BIGINT:
            switch (b.type)
              {
              case PERL_INT:
                return (a.val.ref->val.bint->cmp (b.val.i64) < 0);
              
              case PERL_REF:
                switch (b.val.ref->type)
                  {
                  case PERL_BIGINT:
                    return (a.val.ref->val.bint->cmp (*b.val.ref->val.bint) < 0);
                  
                  default:
                    return false;
                  }
                break;
              
              default:
                return false;
              }
            break;
          
          default:
            return false;
          }
        break;
      
      default:
        return false;
      }
  }
  
  bool
  p_value_lte (p_value& a, p_value& b)
  {
    switch (a.type)
      {
      case PERL_INT:
        switch (b.type)
          {
          case PERL_INT:
            return a.val.i64 <= b.val.i64;
          
          case PERL_REF:
            switch (b.val.ref->type)
              {
              case PERL_BIGINT:
                return (b.val.ref->val.bint->cmp (a.val.i64) > 0);
              
              default:
                return false;
              }
            break;
          
          default:
            return false;
          }
        break;
      
      case PERL_REF:
        switch (a.val.ref->type)
          {
          case PERL_DSTR:
            return p_value_eq (b, *a.val.ref);
          
          case PERL_BIGINT:
            switch (b.type)
              {
              case PERL_INT:
                return (a.val.ref->val.bint->cmp (b.val.i64) <= 0);
              
              case PERL_REF:
                switch (b.val.ref->type)
                  {
                  case PERL_BIGINT:
                    return (a.val.ref->val.bint->cmp (*b.val.ref->val.bint) <= 0);
                  
                  default:
                    return false;
                  }
                break;
              
              default:
                return false;
              }
            break;
          
          default:
            return false;
          }
        break;
      
      default:
        return a.type == b.type;
      }
  }
  
  bool
  p_value_gt (p_value& a, p_value& b)
  {
    switch (a.type)
      {
      case PERL_INT:
        switch (b.type)
          {
          case PERL_INT:
            return a.val.i64 > b.val.i64;
          
          case PERL_REF:
            switch (b.val.ref->type)
              {
              case PERL_BIGINT:
                return (b.val.ref->val.bint->cmp (a.val.i64) <= 0);
              
              default:
                return false;
              }
            break;
          
          default:
            return false;
          }
        break;
      
      case PERL_REF:
        switch (a.val.ref->type)
          {
          case PERL_DSTR:
            return p_value_eq (b, *a.val.ref);
          
          case PERL_BIGINT:
            switch (b.type)
              {
              case PERL_INT:
                return (a.val.ref->val.bint->cmp (b.val.i64) > 0);
              
              case PERL_REF:
                switch (b.val.ref->type)
                  {
                  case PERL_BIGINT:
                    return (a.val.ref->val.bint->cmp (*b.val.ref->val.bint) > 0);
                  
                  default:
                    return false;
                  }
                break;
              
              default:
                return false;
              }
            break;
          
          default:
            return false;
          }
        break;
      
      default:
        return false;
      }
  }
  
  bool
  p_value_gte (p_value& a, p_value& b)
  {
    switch (a.type)
      {
      case PERL_INT:
        switch (b.type)
          {
          case PERL_INT:
            return a.val.i64 >= b.val.i64;
          
          case PERL_REF:
            switch (b.val.ref->type)
              {
              case PERL_BIGINT:
                return (b.val.ref->val.bint->cmp (a.val.i64) < 0);
              
              default:
                return false;
              }
            break;
          
          default:
            return false;
          }
        break;
      
      case PERL_REF:
        switch (a.val.ref->type)
          {
          case PERL_DSTR:
            return p_value_eq (b, *a.val.ref);
          
          case PERL_BIGINT:
            switch (b.type)
              {
              case PERL_INT:
                return (a.val.ref->val.bint->cmp (b.val.i64) >= 0);
              
              case PERL_REF:
                switch (b.val.ref->type)
                  {
                  case PERL_BIGINT:
                    return (a.val.ref->val.bint->cmp (*b.val.ref->val.bint) >= 0);
                  
                  default:
                    return false;
                  }
                break;
              
              default:
                return false;
              }
            break;
          
          default:
            return false;
          }
        break;
      
      default:
        return a.type == b.type;
      }
  }
  
  
  bool
  p_value_is_false (p_value& val)
  {
    if (val.type == PERL_REF)
      return p_value_is_false (*val.val.ref);
    
    switch (val.type)
      {
      case PERL_INT:
        return (val.val.i64 == 0);
      
      default:
        return false;
      }
  }
  
  
  
  /* 
   * Arithmetic:
   */
  
  p_value
  p_value_add (p_value& a, p_value& b, virtual_machine& vm)
  {
    p_value res;
    
    switch (a.type)
      {
      case PERL_INT:
        switch (b.type)
          {
          // int + int
          case PERL_INT:
            res.type = PERL_INT;
            res.val.i64 = a.val.i64 + b.val.i64;
            break;
          
          case PERL_REF:
            switch (b.val.ref->type)
              {
              // int + Int
              case PERL_BIGINT:
                {
                  p_value *data = vm.get_gc ().alloc (true);
                  data->type = PERL_BIGINT;
                  data->val.bint = new big_int (a.val.i64);
                  data->val.bint->add (*b.val.ref->val.bint);
                  
                  res.type = PERL_REF;
                  res.val.ref = data;
                }
                break;
              
              default:
                res.type = PERL_UNDEF;
                break;
              }
            break;
          
          default:
            res.type = PERL_UNDEF;
            break;
          }
        break;
      
      case PERL_REF:
        switch (a.val.ref->type)
          {
          case PERL_BIGINT:
            switch (b.type)
              {
              // Int + int
              case PERL_INT:
                {
                  p_value *data = vm.get_gc ().alloc (true);
                  data->type = PERL_BIGINT;
                  data->val.bint = new big_int (*a.val.ref->val.bint);
                  data->val.bint->add (b.val.i64);
                  
                  res.type = PERL_REF;
                  res.val.ref = data;
                }
                break;
              
              case PERL_REF:
                switch (b.val.ref->type)
                  {
                  // Int + Int
                  case PERL_BIGINT:
                    {
                      p_value *data = vm.get_gc ().alloc (true);
                      data->type = PERL_BIGINT;
                      data->val.bint = new big_int (*a.val.ref->val.bint);
                      data->val.bint->add (*b.val.ref->val.bint);
                      
                      res.type = PERL_REF;
                      res.val.ref = data;
                    }
                    break;
                  
                  default:
                    res.type = PERL_UNDEF;
                    break;
                  }
                break;
              
              default:
                res.type = PERL_UNDEF;
                break;
              }
            break;
          
          default:
            res.type = PERL_UNDEF;
            break;
          }
        break;
      
      default:
        res.type = PERL_UNDEF;
        break;
      }
    
    return res;
  }
  
  p_value
  p_value_sub (p_value& a, p_value& b, virtual_machine& vm)
  {
    p_value res;
    
    switch (a.type)
      {
      case PERL_INT:
        switch (b.type)
          {
          // int + int
          case PERL_INT:
            res.type = PERL_INT;
            res.val.i64 = a.val.i64 - b.val.i64;
            break;
          
          case PERL_REF:
            switch (b.val.ref->type)
              {
              // int + Int
              case PERL_BIGINT:
                {
                  p_value *data = vm.get_gc ().alloc (true);
                  data->type = PERL_BIGINT;
                  data->val.bint = new big_int (a.val.i64);
                  data->val.bint->sub (*b.val.ref->val.bint);
                  
                  res.type = PERL_REF;
                  res.val.ref = data;
                }
                break;
              
              default:
                res.type = PERL_UNDEF;
                break;
              }
            break;
          
          default:
            res.type = PERL_UNDEF;
            break;
          }
        break;
      
      case PERL_REF:
        switch (a.val.ref->type)
          {
          case PERL_BIGINT:
            switch (b.type)
              {
              // Int + int
              case PERL_INT:
                {
                  p_value *data = vm.get_gc ().alloc (true);
                  data->type = PERL_BIGINT;
                  data->val.bint = new big_int (*a.val.ref->val.bint);
                  data->val.bint->sub (b.val.i64);
                  
                  res.type = PERL_REF;
                  res.val.ref = data;
                }
                break;
              
              case PERL_REF:
                switch (b.val.ref->type)
                  {
                  // Int + Int
                  case PERL_BIGINT:
                    {
                      p_value *data = vm.get_gc ().alloc (true);
                      data->type = PERL_BIGINT;
                      data->val.bint = new big_int (*a.val.ref->val.bint);
                      data->val.bint->sub (*b.val.ref->val.bint);
                      
                      res.type = PERL_REF;
                      res.val.ref = data;
                    }
                    break;
                  
                  default:
                    res.type = PERL_UNDEF;
                    break;
                  }
                break;
              
              default:
                res.type = PERL_UNDEF;
                break;
              }
            break;
          
          default:
            res.type = PERL_UNDEF;
            break;
          }
        break;
      
      default:
        res.type = PERL_UNDEF;
        break;
      }
    
    return res;
  }
  
  p_value
  p_value_mul (p_value& a, p_value& b, virtual_machine& vm)
  {
    p_value res;
    
    switch (a.type)
      {
      case PERL_INT:
        switch (b.type)
          {
          // int + int
          case PERL_INT:
            res.type = PERL_INT;
            res.val.i64 = a.val.i64 * b.val.i64;
            break;
          
          case PERL_REF:
            switch (b.val.ref->type)
              {
              // int + Int
              case PERL_BIGINT:
                {
                  p_value *data = vm.get_gc ().alloc (true);
                  data->type = PERL_BIGINT;
                  data->val.bint = new big_int (a.val.i64);
                  data->val.bint->mul (*b.val.ref->val.bint);
                  
                  res.type = PERL_REF;
                  res.val.ref = data;
                }
                break;
              
              default:
                res.type = PERL_UNDEF;
                break;
              }
            break;
          
          default:
            res.type = PERL_UNDEF;
            break;
          }
        break;
      
      case PERL_REF:
        switch (a.val.ref->type)
          {
          case PERL_BIGINT:
            switch (b.type)
              {
              case PERL_INT:
                {
                  p_value *data = vm.get_gc ().alloc (true);
                  data->type = PERL_BIGINT;
                  data->val.bint = new big_int (*a.val.ref->val.bint);
                  data->val.bint->mul (b.val.i64);
                  
                  res.type = PERL_REF;
                  res.val.ref = data;
                }
                break;
              
              case PERL_REF:
                switch (b.val.ref->type)
                  {
                  case PERL_BIGINT:
                    {
                      p_value *data = vm.get_gc ().alloc (true);
                      data->type = PERL_BIGINT;
                      data->val.bint = new big_int (*a.val.ref->val.bint);
                      data->val.bint->mul (*b.val.ref->val.bint);
                      
                      res.type = PERL_REF;
                      res.val.ref = data;
                    }
                    break;
                  
                  default:
                    res.type = PERL_UNDEF;
                    break;
                  }
                break;
              
              default:
                res.type = PERL_UNDEF;
                break;
              }
            break;
          
          default:
            res.type = PERL_UNDEF;
            break;
          }
        break;
      
      default:
        res.type = PERL_UNDEF;
        break;
      }
    
    return res;
  }
  
  p_value
  p_value_div (p_value& a, p_value& b, virtual_machine& vm)
  {
    p_value res;
    
    switch (a.type)
      {
      case PERL_INT:
        switch (b.type)
          {
          // Int + Int
          case PERL_INT:
            res.type = PERL_INT;
            if (b.val.i64 == 0)
              throw std::runtime_error ("division by zero");
            res.val.i64 = a.val.i64 / b.val.i64;
            break;
          
          default:
            res.type = PERL_UNDEF;
            break;
          }
        break;
      
      default:
        res.type = PERL_UNDEF;
        break;
      }
    
    return res;
  }
  
  p_value
  p_value_mod (p_value& a, p_value& b, virtual_machine& vm)
  {
    p_value res;
    
    switch (a.type)
      {
      case PERL_INT:
        switch (b.type)
          {
          // Int + Int
          case PERL_INT:
            res.type = PERL_INT;
            if (b.val.i64 == 0)
              throw std::runtime_error ("division by zero");
            res.val.i64 = a.val.i64 % b.val.i64;
            break;
          
          default:
            res.type = PERL_UNDEF;
            break;
          }
        break;
      
      default:
        res.type = PERL_UNDEF;
        break;
      }
    
    return res;
  }
  
  
  
  p_value
  p_value_to_str (p_value& val, virtual_machine& vm)
  {
    std::string str = p_value_str (val);
    
    unsigned int cap = str.length () + 11;
    p_value *data = vm.get_gc ().alloc (true);
    data->type = PERL_DSTR;
    data->val.str.data = new char [cap];
    data->val.str.len = str.length ();
    data->val.str.cap = cap;
    std::strcpy (data->val.str.data, str.c_str ());
    vm.get_gc ().notify_increase (cap);
    
    p_value res;
    res.type = PERL_REF;
    res.val.ref = data;
    return res;
  }
  
  
  
  static long long
  _to_int (p_value& val)
  {
    if (val.type == PERL_REF)
      return _to_int (*val.val.ref);
    
    switch (val.type)
      {
      case PERL_INT:
        return val.val.i64;
      
      case PERL_CSTR:
        {
          std::istringstream ss { val.val.cstr.data };
          long long n;
          ss >> n;
          return n;
        }
        break;
      
      case PERL_DSTR:
        {
          std::istringstream ss { val.val.str.data };
          long long n;
          ss >> n;
          return n;
        }
        break;
      
      case PERL_ARRAY:
        // return number of elements
        return val.val.arr.len;
      
      default:
        return 0;
      }
  }
  
  p_value
  p_value_to_int (p_value& val, virtual_machine& vm)
  {
    p_value res;
    res.type = PERL_INT;
    res.val.i64 = _to_int (val);
    return res;
  }
  
  
  
  p_value
  p_value_to_big_int (p_value& val, virtual_machine& vm)
  {
    p_value *data = nullptr;
    switch (val.type)
      {
      case PERL_INT:
        data = vm.get_gc ().alloc (true);
        data->type = PERL_BIGINT;
        data->val.bint = new big_int (val.val.i64);
        break;
      
      case PERL_REF:
        switch (val.val.ref->type)
          {
          case PERL_BIGINT:
            return val;
          
          default:
            data = vm.get_gc ().alloc (true);
            data->type = PERL_BIGINT;
            data->val.bint = new big_int ();
            break;
          }
        break;
      
      default:
        data = vm.get_gc ().alloc (true);
        data->type = PERL_BIGINT;
        data->val.bint = new big_int ();
        break;
      }
    
    p_value res;
    res.type = PERL_REF;
    res.val.ref = data;
    return res;
  }
  
  
  
  p_value
  p_value_to_bool (p_value& val, virtual_machine& vm)
  {
    p_value res;
    res.type = PERL_BOOL;
    res.val.bl = true;
    
    switch (val.type)
      {
      case PERL_INT:
        res.val.bl = val.val.i64;
        break;
      
      case PERL_BOOL:
        res.val.bl = val.val.bl;
        break;
      
      case PERL_REF:
        switch (val.val.ref->type)
          {
          case PERL_BIGINT:
            res.val.bl = !val.val.ref->val.bint->is_zero ();
            break;
          
          case PERL_CSTR:
            res.val.bl = val.val.ref->val.cstr.len;
            break;
          
          case PERL_DSTR:
            res.val.bl = val.val.ref->val.str.len;
            break;
          
          case PERL_ARRAY:
            res.val.bl = val.val.ref->val.arr.len;
            break;
          
          default: ;
          }
        break;
      
      case PERL_UNDEF:
        res.val.bl = false;
        break;
      
      default: ;
      }
    
    return res;
  }
  
  
  
  p_value
  p_value_concat (p_value& a, p_value& b, virtual_machine& vm)
  {
    std::string str;
    str.append (p_value_str (a));
    str.append (p_value_str (b));
    
    unsigned int cap = str.length () + 11;
    p_value *data = vm.get_gc ().alloc (true);
    data->type = PERL_DSTR;
    data->val.str.data = new char [cap];
    data->val.str.len = str.length ();
    data->val.str.cap = cap;
    std::strcpy (data->val.str.data, str.c_str ());
    vm.get_gc ().notify_increase (cap);
    
    p_value res;
    res.type = PERL_REF;
    res.val.ref = data;
    return res;
  }
} 

