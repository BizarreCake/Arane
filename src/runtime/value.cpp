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

#include "runtime/value.hpp"
#include <stdexcept>
#include <cstring>
#include <sstream>
#include <iomanip>


namespace p6 {
  
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
      case PERL_INT:
        switch (b.type)
          {
          case PERL_INT:
            return a.val.i64 == b.val.i64;
          
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
        if (a.val.ref->type == PERL_DSTR)
          return p_value_eq (b, *a.val.ref);
      
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
  p_value_add (p_value& a, p_value& b)
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
            res.val.i64 = a.val.i64 + b.val.i64;
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
  p_value_sub (p_value& a, p_value& b)
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
            res.val.i64 = a.val.i64 - b.val.i64;
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
  p_value_mul (p_value& a, p_value& b)
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
            res.val.i64 = a.val.i64 * b.val.i64;
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
  p_value_div (p_value& a, p_value& b)
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
  p_value_mod (p_value& a, p_value& b)
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
  p_value_to_str (p_value& val)
  {
    std::string str = p_value_str (val);
    
    p_value *data = new p_value;
    data->type = PERL_DSTR;
    data->val.str.data = new char [str.length () + 1];
    data->val.str.len = str.length ();
    std::strcpy (data->val.str.data, str.c_str ());
    
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
  p_value_to_int (p_value& val)
  {
    p_value res;
    res.type = PERL_INT;
    res.val.i64 = _to_int (val);
    return res;
  }
  
  
  p_value
  p_value_concat (p_value& a, p_value& b)
  {
    std::string str;
    str.append (p_value_str (a));
    str.append (p_value_str (b));
    
    p_value *data = new p_value;
    data->type = PERL_DSTR;
    data->val.str.data = new char [str.length () + 1];
    data->val.str.len = str.length ();
    std::strcpy (data->val.str.data, str.c_str ());
    
    p_value res;
    res.type = PERL_REF;
    res.val.ref = data;
    return res;
  }
} 

