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

#ifndef _ARANE__BYTE_BUFFER__H_
#define _ARANE__BYTE_BUFFER__H_

#include <stack>


namespace arane {
  
  /* 
   * A convenient wrapper around a dynamic byte array.
   */
  class byte_buffer
  {
    unsigned char *data;
    unsigned int size;
    unsigned int pos;
    unsigned int cap;
    
    std::stack<unsigned int> pos_stack;
    
  public:
    inline const unsigned char* get_data () const { return this->data; }
    inline unsigned int get_size () const { return this->size; }
    inline unsigned int get_capacity () const { return this->cap; }
    inline unsigned int get_pos () const { return this->pos; }
    
    void set_pos (unsigned int pos) { this->pos = pos; }
    
  public:
    /* 
     * Constructs a new byte buffer with the specified initial capacity.
     */
    byte_buffer (unsigned int init_cap = 64);
    
    ~byte_buffer ();
    
  private:
    /* 
     * Expands the byte buffer in order to accommodate the specified amount
     * of bytes.
     */
    void expand (unsigned int new_cap);
    
  public:
    void put_byte (unsigned char val);
    void put_short (unsigned short val);
    void put_int (unsigned int val);
    void put_long (unsigned long long val);
    
    void put_bytes (const unsigned char *arr, unsigned int len);
    
  public:
    void push ();
    void pop ();
  };
}

#endif

