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

#ifndef _ARANE__RUNTIME__BIGINT__H_
#define _ARANE__RUNTIME__BIGINT__H_

#include <string>


namespace arane {
  
  /* 
   * Arbitrary-precision integer.
   */
  class big_int
  {
    unsigned int *limbs;
    unsigned int size;
    unsigned int cap;
    bool neg;
    
  public:
    /* 
     * Constructs a new big integer initialized as zero.
     */
    big_int ();
    big_int (long long val);
    
    big_int (const big_int& other);
    
    ~big_int ();
    
  private:
    /* 
     * Resizes the internal buffer used to hold the integer.
     */
    void extend_capacity (unsigned int ncap);
    
    /* 
     * Resets the number of bytes used by the integer.
     */
    void resize (unsigned int size);
    
    void resize_to_fit ();
    
  private:
    unsigned int divmod (unsigned int other);
    
    // multiplication algorithms:
    void umul_classic (const big_int& other);
    void umul_karatsuba (const big_int& other);
    
  private:
    /* 
     * Unsigned arithmetic:
     */
    
    void uadd (const big_int& other);
    void uadd (unsigned int other);
    void usub (const big_int& other);
    void usub (unsigned int other);
    void umul (const big_int& other);
    void umul (unsigned int other);
    
    int ucmp (const big_int& other);
    int ucmp (const unsigned long long other);
    
  public:
    /* 
     * Signed arithmetic:
     */
    void add (const big_int& other);
    void add (int other);
    void sub (const big_int& other);
    void sub (int other);
    void mul (const big_int& other);
    void mul (int other);
    
    void shift_left_by_places (unsigned int count);
    void shift_right_by_places (unsigned int count);
    void keep_places (unsigned int count);
    
    void set (const big_int& other);
    void set (long long val);
    
    bool is_zero ();
    int cmp (big_int& other);
    int cmp (long long other);
    
  public:
    void to_str (std::string& out);
    std::string str ();
  };
}

#endif

