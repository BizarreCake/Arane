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

#include "runtime/bigint.hpp"
#include <cstring>
#include <algorithm>

#include <iostream> // DEBUG


namespace arane {
  
  /* 
   * Constructs a new big integer initialized as zero.
   */
  big_int::big_int ()
  {
    this->limbs = new unsigned int[4];
    this->size = 0;
    this->cap = 4;
    this->neg = false;
  }
  
  big_int::big_int (long long val)
  {
    this->limbs = new unsigned int[4];
    this->cap = 4;
    
    // TODO: handle edge case (when val == -2^63)
    if ((this->neg = (val < 0)))
      val = -val;
    this->size = val ? ((val >> 32) ? 2 : 1) : 0;
    
    this->limbs[0] = val & 0xFFFFFFFFU;
    this->limbs[1] = val >> 32;
  }
  
  big_int::big_int (const big_int& other)
  {
    this->size = other.size;
    this->cap = other.cap;
    this->limbs = new unsigned int [other.cap];
    this->neg = other.neg;
    std::memcpy (this->limbs, other.limbs, 4 * other.size);
  }
  
  big_int::~big_int ()
  {
    delete[] this->limbs;
  }
  
  
  
  /* 
   * Resizes the internal buffer used to hold the integer.
   */
  void
  big_int::extend_capacity (unsigned int ncap)
  {
    if (ncap <= this->cap)
      return;
    
    unsigned int *nlimbs = new unsigned int [ncap];
    for (unsigned int i = 0; i < this->size; ++i)
      nlimbs[i] = this->limbs[i];
    
    delete[] this->limbs;
    this->limbs = nlimbs;
    this->cap = ncap;
  }
  
  /* 
   * Resets the number of bytes used by the integer.
   */
  void
  big_int::resize (unsigned int size)
  {
    if (size > this->cap)
      {
        unsigned int ncap = this->cap * 15 / 10;
        if (size > ncap)
          ncap += size;
        this->extend_capacity (ncap);
      }
    
    for (unsigned int i = this->size; i < size; ++i)
      this->limbs[i] = 0;
    this->size = size;
  }
  
  void
  big_int::resize_to_fit ()
  {
    int i = this->size - 1;
    for (; i >= 0; --i)
      {
        if (this->limbs[i])
          break;
      }
    this->size = i + 1;
  }
  
  
  
  void
  big_int::set (const big_int& other)
  {
    this->extend_capacity (other.cap);
    std::memcpy (this->limbs, other.limbs, other.size * 4);
    
    this->size = other.size;
    this->cap = other.cap;
    this->neg = other.neg;
  }
  
  void
  big_int::set (long long val)
  {
    std::memset (this->limbs, 0, this->size * 4);
    
    // TODO: handle edge case (when val == -2^63)
    if ((this->neg = (val < 0)))
      val = -val;
    
    this->limbs[0] = val & 0xFFFFFFFFU;
    this->limbs[1] = val >> 32;
    
    this->size = val ? ((val >> 32) ? 2 : 1) : 0;
  }
  
  
  bool
  big_int::is_zero ()
  {
    for (unsigned int i = 0; i < this->size; ++i)
      if (this->limbs[i])
        return false;
    
    return true;
  }
  
  
  int
  big_int::ucmp (const big_int& other)
  {
    if (this->size > other.size)
      return 1;
    else if (this->size < other.size)
      return -1;
    
    for (int i = this->size - 1; i >= 0; --i)
      {
        if (this->limbs[i] > other.limbs[i])
          return 1;
        else if (this->limbs[i] < other.limbs[i])
          return -1;
      }
    
    return 0;
  }
  
  int
  big_int::ucmp (unsigned long long other)
  {
    unsigned int osize = other ? ((other >> 32) ? 2 : 1) : 0;
    if (this->size > osize)
      return 1;
    else if (this->size < osize)
      return -1;
    
    if (osize == 2)
      {
        if (this->limbs[1] > (other >> 32))
          return 1;
        else if (this->limbs[1] < (other >> 32))
          return -1;
      }
    
    if (this->limbs[0] > (other & 0xFFFFFFFFU))
      return 1;
    else if (this->limbs[0] < (other & 0xFFFFFFFFU))
      return -1;
    
    return 0;
  }
  
  
  
  /* 
   * Arithmetic:
   */
//------------------------------------------------------------------------------
  
  void
  big_int::uadd (const big_int& other)
  {
    if (other.size > this->size)
      this->resize (other.size);
    
    unsigned int r, carry = 0;
    for (unsigned int i = 0; i < other.size; ++i)
      {
        r = this->limbs[i] + other.limbs[i] + carry;
        carry = (r < this->limbs[i]);
        this->limbs[i] = r;
      }
    
    // left over carry
    if (carry > 0)
      {
        this->resize (other.size + 1);
        this->limbs[other.size] = carry;
      }
  }
  
  void
  big_int::uadd (unsigned int other)
  {
    if (this->size == 0)
      {
        if (!other)
          return;
        this->resize (1);
      }
    
    unsigned int r, carry = 0;
    
    r = this->limbs[0] + other + carry;
    carry = (r < this->limbs[0]);
    this->limbs[0] = r;
    
    for (unsigned int i = 1; carry && (i < this->size); ++i)
      {
        r = this->limbs[i] + carry;
        carry = (r < this->limbs[i]);
        this->limbs[i] = r;
      }
    
    // left over carry
    if (carry > 0)
      {
        this->resize (this->size + 1);
        this->limbs[this->size - 1] = carry;
      }
  }
  
  
  
  void
  big_int::usub (const big_int& other)
  {
    for (unsigned int i = 0; i < other.size; ++i)
      {
        if (this->limbs[i] < other.limbs[i])
          {
            for (unsigned int j = 1; j < this->size; ++j)
              {
                if (this->limbs[j] > 0)
                  {
                    -- this->limbs[j];
                    break;
                  }
                else
                  this->limbs[j] = ~this->limbs[j];
              }
            this->limbs[i] = (unsigned int)(
              0x100000000ULL - other.limbs[i] + this->limbs[i]);
          }
        else
          this->limbs[i] -= other.limbs[i];
      }
    
    this->resize_to_fit ();
  }
  
  void
  big_int::usub (unsigned int other)
  {
    if (this->size == 0)
      {
        if (!other)
          return;
         
        // shouldn't happen
      }
    
    if (this->limbs[0] >= other)
      this->limbs[0] -= other;
    else
      {
        for (unsigned int i = 1; i < this->size; ++i)
          if (this->limbs[i])
            {
              -- this->limbs[i];
              break;
            }
          else
            this->limbs[i] = ~this->limbs[i];
         this->limbs[0] = (unsigned int)(
           0x100000000ULL - other + this->limbs[0]);
      }
    
    this->resize_to_fit ();
  }
  
  
  
  void
  big_int::umul (unsigned int other)
  {
    unsigned long long r;
    unsigned int c = 0;
    for (unsigned int i = 0; i < this->size; ++i)
      {
        r = (unsigned long long)this->limbs[i] * other + c;
        c = r >> 32;
        this->limbs[i] = r;
      }
    if (c > 0)
      {
        this->resize (this->size + 1);
        this->limbs[this->size - 1] = c;
      }
  }
  
  void
  big_int::umul (const big_int& other)
  {
    this->umul_classic (other);
  }
  
  
  void
  big_int::umul_classic (const big_int& other)
  {
    big_int orig = *this;
    this->set (0);
    for (unsigned int i = 0; i < other.size; ++i)
      {
        big_int part = orig;
        part.umul (other.limbs[i]);
        part.shift_left_by_places (i);
        this->add (part);
      }
  }
  
  void
  big_int::umul_karatsuba (const big_int& other)
  {
    /* 
     * TODO: Will be usable once negative big integers are implemented.
     */
    
    /*
    unsigned int m = this->size / 2;
    if (m > other.size)
      m = other.size;
    
    // A = a1:a0
    big_int a0 = *this;
    a0.resize (m);
    big_int a1 = *this;
    a1.shift_right_by_places (m);
    
    // B = b1:b1
    big_int b0 = other;
    b0.resize (m);
    big_int b1 = other;
    b1.shift_right_by_places (m);
    
    // z0 = a0 * b0
    big_int z0 = a0;
    z0.mul (b0);
    
    // z2 = a1 * b1
    big_int z2 = a1;
    z2.mul (b1);
    
    // z1 (as a0)
    a0.add (a1);
    b0.add (b1);
    a0.mul (b0);
    if (a0.cmp (z2) < 0)
      std::cout << "AA" << std::endl;
    a0.sub (z2);
    if (a0.cmp (z0) < 0)
      std::cout << "BB" << std::endl;
    a0.sub (z0);
    
    this->set (0);
    this->add (z0);
    
    a0.shift_left_by_places (m);
    this->add (a0);
    
    z2.shift_left_by_places (m * 2);
    this->add (z2);
    */
  }
  
  
  
  void
  big_int::shift_left_by_places (unsigned int count)
  {
    this->resize (this->size + count);
    
    std::memmove (this->limbs + count, this->limbs, (this->size - count) * 4);
    std::memset (this->limbs, 0, 4 * count);
  }
  
  void
  big_int::shift_right_by_places (unsigned int count)
  {
    if (this->size < count)
      this->resize (0);
    else
      {
        this->resize (this->size - count);
        std::memmove (this->limbs, this->limbs + count, this->size * 4);
      }
  }
  
  
  
  unsigned int
  big_int::divmod (unsigned int other)
  {
    unsigned long long v, r, c = 0;
    unsigned int start = 0;
    
    int i;
    for (i = this->size - 1; i >= 0; --i)
      {
        if (this->limbs[i])
          {
            start = i;
            if (this->limbs[start] < other)
              {
                // take two digits
                c = this->limbs[start];
                this->limbs[start] = 0;
                -- this->size;
                -- start;
              }
            break;
          }
      }
    if (i < 0)
      return 0;
    
    for (int i = start; i >= 0; --i)
      {
        v = ((c << 32) | this->limbs[i]);
        r = v / other;
        c = v - (r * other);
        
        this->limbs[i] = r;
      }
    
    return c;
  }
  
//------------------------------------------------------------------------------
  

  
  /* 
   * Signed arithmetic:
   */
//------------------------------------------------------------------------------
  
  void
  big_int::add (const big_int& other)
  {
    static void* _table[] = { &&l00, &&l01, &&l10, &&l11 };
    goto *_table[(this->neg << 1) | other.neg];
    
  l00:
    // pos & pos
    this->uadd (other);
    return;
    
  l01:
    // pos & neg
    if (this->ucmp (other) >= 0)
      this->usub (other);
    else
      {
        big_int c = *this;
        this->set (other);
        this->usub (c);
        this->neg = true;
      }
    return;
  
  l10:
    // neg & pos
    if (this->ucmp (other) >= 0)
      this->usub (other);
    else
      {
        big_int c = *this;
        this->set (other);
        this->usub (c);
        this->neg = false;
      }
    return;
  
  l11:
    // neg & neg
    this->uadd (other);
    return;
  }
  
  void
  big_int::add (int other)
  {
    static void* _table[] = { &&l00, &&l01, &&l10, &&l11 };
    goto *_table[(this->neg << 1) | (other < 0)];
    
  l00:
    // pos & pos
    this->uadd (other);
    return;
    
  l01:
    // pos & neg
    if (this->ucmp (other) >= 0)
      this->usub (other);
    else
      {
        big_int c = *this;
        this->set (other);
        this->usub (c);
        this->neg = true;
      }
    return;
  
  l10:
    // neg & pos
    if (this->ucmp (other) >= 0)
      this->usub (other);
    else
      {
        big_int c = *this;
        this->set (other);
        this->usub (c);
        this->neg = false;
      }
    return;
  
  l11:
    // neg & neg
    this->uadd (other);
    return;
  }
  
  void
  big_int::sub (const big_int& other)
  {
    static void* _table[] = { &&l00, &&l01, &&l10, &&l11 };
    goto *_table[(this->neg << 1) | other.neg];
    int c;
    
  l00:
    // pos & pos
    c = this->ucmp (other);
    if (c >= 0)
      this->usub (other);
    else
      {
        big_int copy = *this;
        this->set (other);
        this->usub (copy);
        this->neg = true;
      }
    return;
    
  l01:
    // pos & neg
    this->uadd (other);
    return;
  
  l10:
    // neg & pos
    this->uadd (other);
    return;
  
  l11:
    // neg & neg
    c = this->ucmp (other);
    if (c >= 0)
      this->usub (other);
    else
      {
        big_int copy = *this;
        this->set (other);
        this->usub (copy);
        this->neg = false;
      }
    return;
  }
  
  void
  big_int::sub (int other)
  {
    static void* _table[] = { &&l00, &&l01, &&l10, &&l11 };
    goto *_table[(this->neg << 1) | (other < 0)];
    int c;
    
  l00:
    // pos & pos
    c = this->ucmp (other);
    if (c >= 0)
      this->usub (other);
    else
      {
        big_int copy = *this;
        this->set (other);
        this->usub (copy);
        this->neg = true;
      }
    return;
    
  l01:
    // pos & neg
    this->uadd (other);
    return;
  
  l10:
    // neg & pos
    this->uadd (other);
    return;
  
  l11:
    // neg & neg
    c = this->ucmp (other);
    if (c >= 0)
      this->usub (other);
    else
      {
        big_int copy = *this;
        this->set (other);
        this->usub (copy);
        this->neg = false;
      }
    return;
  }
  
  void
  big_int::mul (const big_int& other)
  {
    this->umul (other);
    this->neg = this->neg ^ other.neg;
  }
  
  void
  big_int::mul (int other)
  {
    this->umul (other);
    this->neg = this->neg ^ (other < 0);
  }
  
  
  
  int
  big_int::cmp (big_int& other)
  {
    static void* _table[] = { &&l00, &&l01, &&l10, &&l11 };
    goto *_table[(this->neg << 1) | other.neg];
    
  l00:
    // pos & pos
    return this->ucmp (other);
    
  l01:
    // pos & neg
    return 1;
  
  l10:
    // neg & pos
    return -1;
  
  l11:
    // neg & neg
    return -this->ucmp (other);
  }
  
  int
  big_int::cmp (long long other)
  {
    static void* _table[] = { &&l00, &&l01, &&l10, &&l11 };
    goto *_table[(this->neg << 1) | (other < 0)];
    
  l00:
    // pos & pos
    return this->ucmp (other);
    
  l01:
    // pos & neg
    return 1;
  
  l10:
    // neg & pos
    return -1;
  
  l11:
    // neg & neg
    return -this->ucmp (other);
  }
    
  
//------------------------------------------------------------------------------
  
  
  void
  big_int::to_str (std::string& out)
  {
    if (this->is_zero ())
      {
        out.push_back ('0');
        return;
      }
    
    if (this->neg)
      out.push_back ('-');
  
    big_int copy = *this;
    while (!copy.is_zero ())
      {
        unsigned int d = copy.divmod (10);
        out.push_back (d + '0');
      }
    
    std::reverse (out.begin () + (this->neg ? 1 : 0), out.end ());
  }
  
  std::string
  big_int::str ()
  {
    std::string s;
    this->to_str (s);
    return s;
  }
}

