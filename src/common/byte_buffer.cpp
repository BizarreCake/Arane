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

#include "common/byte_buffer.hpp"
#include <cstring>
#include <malloc.h>
#include <new>


namespace arane {
  
  /* 
   * Constructs a new byte buffer with the specified initial capacity.
   */
  byte_buffer::byte_buffer (unsigned int init_cap)
  {
    this->data = (unsigned char *)malloc (init_cap);
    this->cap = init_cap;
    this->size = this->pos = 0;
  }
  
  byte_buffer::~byte_buffer ()
  {
    free (this->data);
  }
  
  
  
  /* 
   * Expands the byte buffer in order to accommodate the specified amount
   * of bytes.
   */
  void
  byte_buffer::expand (unsigned int new_cap)
  {
    if (this->cap >= new_cap)
      return;
    
    void* tmp = realloc (this->data, new_cap);
    if (!tmp)
      throw std::bad_alloc ();
    
    this->data = (unsigned char *)tmp;
    this->cap = new_cap;
  }
  
  
  
  void
  byte_buffer::put_byte (unsigned char val)
  {
    if ((this->size + 1) > this->cap)
      this->expand (this->cap * 2);
    
    this->data[this->pos ++] = val;
    if (this->pos > this->size)
      this->size = this->pos;
  }

  void
  byte_buffer::put_short (unsigned short val)
  {
    if ((this->size + 2) > this->cap)
      {
        unsigned int ncap = this->cap * 2;
        if ((this->size + 2) > ncap)
          ncap *= 2;
        this->expand (ncap);
      }
    
    this->data[this->pos + 0] = val & 0xFF;
    this->data[this->pos + 1] = val >> 8;
    this->pos += 2;
    if (this->pos > this->size)
      this->size = this->pos;
  }
  
  void
  byte_buffer::put_int (unsigned int val)
  {
    if ((this->size + 4) > this->cap)
      {
        unsigned int ncap = this->cap * 2;
        if ((this->size + 4) > ncap)
          ncap *= 2;
        this->expand (ncap);
      }
    
    this->data[this->pos + 0] = val & 0xFF;
    this->data[this->pos + 1] = (val >> 8) & 0xFF;
    this->data[this->pos + 2] = (val >> 16) & 0xFF;
    this->data[this->pos + 3] = (val >> 24) & 0xFF;
    this->pos += 4;
    if (this->pos > this->size)
      this->size = this->pos;
  }
  
  void
  byte_buffer::put_long (unsigned long long val)
  {
    if ((this->size + 8) > this->cap)
      {
        unsigned int ncap = this->cap * 2;
        if ((this->size + 8) > ncap)
          ncap *= 2;
        this->expand (ncap);
      }
    
    this->data[this->pos + 0] = val & 0xFF;
    this->data[this->pos + 1] = (val >> 8) & 0xFF;
    this->data[this->pos + 2] = (val >> 16) & 0xFF;
    this->data[this->pos + 3] = (val >> 24) & 0xFF;
    this->data[this->pos + 4] = (val >> 32) & 0xFF;
    this->data[this->pos + 5] = (val >> 40) & 0xFF;
    this->data[this->pos + 6] = (val >> 48) & 0xFF;
    this->data[this->pos + 7] = (val >> 56) & 0xFF;
    this->pos += 8;
    if (this->pos > this->size)
      this->size = this->pos;
  }
  
  void
  byte_buffer::put_bytes (const unsigned char *arr, unsigned int len)
  {
    if ((this->size + len) > this->cap)
      {
        unsigned int ncap = this->cap * 2;
        if ((this->size + len) > ncap)
          ncap += len;
        this->expand (ncap);
      }
    
    std::memcpy (this->data + this->pos, arr, len);
    this->pos += len;
    if (this->pos > this->size)
      this->size = this->pos;
  }
  
  
  
  void
  byte_buffer::push ()
  {
    this->pos_stack.push (this->pos);
  }
  
  void
  byte_buffer::pop ()
  {
    this->pos = this->pos_stack.top ();
    this->pos_stack.pop ();
  }
}

