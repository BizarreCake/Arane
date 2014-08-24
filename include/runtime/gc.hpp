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

#ifndef _ARANE__RUNTIME__GC__H_
#define _ARANE__RUNTIME__GC__H_

#include "runtime/value.hpp"
#include <unordered_set>
#include <deque>


namespace arane {
  
  class virtual_machine;
  
  
  namespace gc {
    
// number of values in a page.
#define GC_PAGE_SIZE    1024
    
    constexpr unsigned int
    free_bitmap_size (unsigned page_size)
    {
      return
        (page_size >> 3) + // a bit for every object
        (page_size >> 6);  // an auxiliary bitmap
    }
    
    /* 
     * Individual values are grouped into pages or "blocks" which are tracked
     * by the garbage collector.
     */
    struct heap_page
    {
      p_value objs[GC_PAGE_SIZE];
      
      // pointer to previous and next page in the chain.
      heap_page *prev, *next;
      
      unsigned char free_bitmap[free_bitmap_size (GC_PAGE_SIZE)];
    };
    
    
    /* 
     * The state of the garbage collector (and not of an object).
     */
    enum gc_state
    {
      GCS_NONE,
      GCS_MARK,
      GCS_SWEEP,
    };
  }
  
  
  /* 
   * 
   */
  class garbage_collector
  {
    virtual_machine& vm;
    
    gc::gc_state state;
    unsigned int alloc_count;
    unsigned int total_alloc_count;
    
    long long total_ext_bytes;
    long long ext_bytes;
    long long last_ext_bytes;
    long long last_alloc_count;
    long long inc_count;
    
    // the current "meaning" of white objects.
    unsigned char curr_white;
    
    gc::heap_page *pages;
    gc::heap_page *to_sweep;
    std::deque<p_value *> grays;
    unsigned int page_count;
    unsigned int total_page_count;
    
    // debug:
    unsigned int counter;
    unsigned int marked;
    unsigned int marked_roots;
    
  public:
    /* 
     * Constructs a garbage collector on top of the specified runtime stack.
     * The given stack will function as the collector's "root" set.
     */
    garbage_collector (virtual_machine& vm);
    
    ~garbage_collector ();
    
  private:
    /* 
     * Allocates a new empty heap page.
     */
    gc::heap_page* alloc_page ();
    
    /* 
     * Destroys the page and all the objects inside it.
     */
    void free_page (gc::heap_page *page);
    
    /* 
     * Links the specified heap page to the rest.
     */
    void link_page (gc::heap_page *page);
    
    /* 
     * Removes the specified heap page from the page list.
     */
    void unlink_page (gc::heap_page *page);
    
    
    /* 
     * Reclaims memory used by the specified object.
     */
    void delete_object (p_value& val);
    
  private:
    /* 
     * Prepares the root set by marking the root objects (stack, globals, etc...)
     */
    void mark_roots ();
    
    /* 
     * Marks as many as "limit" objects.
     * Returns true if there's still more work to be done.
     */
    bool incremental_mark (unsigned int limit);
    
    /* 
     * Marks the children of the specified value.
     */
    void mark_children (p_value *val);
    
    /* 
     * Inserts the specified object into the gray set.
     */
    void paint_gray (p_value *val);
    
    
    
    /* 
     * Reclaims all dead objects in the specified page.
     * Returns true if after sweeping the page is empty and can be reclaimed.
     */
    bool sweep_page (gc::heap_page *page);
    
    /* 
     * Sweeps a little bit.
     */
    bool incremental_sweep (unsigned int limit);
    
  public:
    /* 
     * Notifies the collector that the specified amount of bytes have been
     * allocated outside of the GC (.e.g. dynamic strings, arrays, etc...).
     */
    void notify_increase (unsigned int count);
    
    /* 
     * Notifies the collector that the specified amount of bytes have been
     * released outside of the GC (.e.g. from dynamic strings, arrays, etc...).
     */
    void notify_decrease (unsigned int count);
    
  public:
    /* 
     * Performs a full garbage collection.
     */
    void collect ();
    
    /* 
     * Performs one unit/cycle of work.
     */
    void work ();
    
    
    /* 
     * Allocates and returns a new object.
     */
    p_value* alloc (bool protect);
    p_value* alloc_copy (p_value& other, bool protect);
  };
}

#endif

