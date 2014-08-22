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

#include "runtime/gc.hpp"
#include "runtime/vm.hpp"

#include <iostream> // DEBUG


namespace p6 {
  
#define GC_MARK_LIMIT         2048
#define GC_SWEEP_LIMIT          12
#define GC_ALLOC_THRESHOLD     448


//#define GC_DEBUG
#ifdef GC_DEBUG
# define GC_IF_DEBUG(s) \
  s
#else
# define GC_IF_DEBUG(S) ;
#endif
  
  namespace {
    
    enum: unsigned char
    {
      /* 
       * Because this is an incremental GC, should new objects be allocated
       * in between the phases, they could be deleted even though they might be
       * live objects.
       * 
       * To prevent this, there are two types of white objects - one
       * representing white objects that are canditates to be reclaimed in the
       * current GC cycle, and another white type for newly created objects.
       * 
       * Following how it's done in mruby, instead of toggling the type of
       * every white object individually, the meaning of what type of white
       * object is what is toggled instead, which is much cheaper to do.
       */
      GC_WHITE_A = 0,
      GC_WHITE_B = 1,
      
      GC_GRAY,
      GC_BLACK,
    };
    
    
    static inline unsigned char
    _opposite_white (unsigned char type)
    {
      return type ^ 1;
    }
  }
  
  
  
  /* 
   * Constructs a garbage collector on top of the specified runtime stack.
   * The given stack will function as the collector's "root" set.
   */
  garbage_collector::garbage_collector (virtual_machine& vm)
    : vm (vm)
  {
    this->pages = nullptr;
    this->page_count = 0;
    this->total_page_count = 0;
    
    // create the initial page
    auto page = this->alloc_page ();
    this->link_page (page);
    
    this->state = gc::GCS_NONE;
    this->curr_white = GC_WHITE_A;
    this->alloc_count = 0;
    this->total_alloc_count = 0;
    this->counter = 0;
    
    this->total_ext_bytes = this->ext_bytes = this->last_ext_bytes = 0;
    this->inc_count = 0;
  }
  
  garbage_collector::~garbage_collector ()
  {
    /*
    std::cout << "total_ext_bytes: " << this->total_ext_bytes << std::endl;
    std::cout << "avg increase: " << (this->total_ext_bytes / this->inc_count) << std::endl;
    std::cout << "page count: " << this->page_count << std::endl;
    std::cout << "total page count: " << this->total_page_count << std::endl;
    std::cout << "total alloc count: " << this->total_alloc_count << std::endl;
    */
    auto page = this->pages;
    while (page)
      {
        auto next = page->next;
        this->free_page (page);
        page = next;
      }
  }
  
  
  
  /* 
   * Page manipulation:
   */
//------------------------------------------------------------------------------
  
  /* 
   * Allocates a new empty heap page.
   */
  gc::heap_page*
  garbage_collector::alloc_page ()
  {
    gc::heap_page *page = new gc::heap_page;
    
    page->prev = nullptr;
    page->next = nullptr;
    
    // mark all objects in the page as free
    for (unsigned int i = 0; i < sizeof page->free_bitmap; ++i)
      page->free_bitmap[i] = 0xFF;
    
    return page;
  }
  
  /* 
   * Links the specified heap page to the rest.
   */
  void
  garbage_collector::link_page (gc::heap_page *page)
  {
    page->next = this->pages;
    if (this->pages)
      this->pages->prev = page;
    this->pages = page;
    ++ this->page_count;
    ++ this->total_page_count;
  }
  
  /* 
   * Removes the specified heap page from the page list.
   */
  void
  garbage_collector::unlink_page (gc::heap_page *page)
  {
    if (page->prev)
      page->prev->next = page->next;
    if (page->next)
      page->next->prev = page->prev;
    if (this->pages == page)
      this->pages = page->next;
    -- this->page_count;
  }
  
  
  
  static bool
  _page_space_available (gc::heap_page *page)
  {
    unsigned int aux_size = GC_PAGE_SIZE >> 6;
    for (unsigned int i = 0; i < aux_size; ++i)
      if (page->free_bitmap[i])
        return true;
    return false;
  }
  
  
  
  // counts consecutive zero bits on the right (index of first set lsb).
  static int
  _find_lsb (unsigned int v)
  {
    static const int _table[] = {
      0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 
      31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
    };
    return _table[(unsigned int)(((v & -v) * 0x077CB531U)) >> 27];
  }
  
  // or -1 if not found
  static int
  _next_free_object_index (gc::heap_page *page)
  {
    const unsigned int aux_size = GC_PAGE_SIZE >> 6;
    
    for (unsigned int i = 0; i < aux_size; ++i)
      if (page->free_bitmap[i])
        {
          unsigned int aux_ind = _find_lsb (page->free_bitmap[i]);
          
          unsigned char fb = page->free_bitmap[aux_size + (i << 3) + aux_ind];
          return (i << 6) | (aux_ind << 3) | _find_lsb (fb);
        }
    
    return -1;
  }
  
  // marks the object located at the specified index as used (not free).
  static void
  _mark_used (gc::heap_page *page, int index)
  {
    const unsigned int aux_size = GC_PAGE_SIZE >> 6;
    if ((page->free_bitmap[aux_size + (index >> 3)] &= ~(1 << (index & 7)))
      == 0)
      page->free_bitmap[index >> 6] &= ~(1 << ((index >> 3) & 7));
  }
  
  static void
  _mark_free (gc::heap_page *page, int index)
  {
    const unsigned int aux_size = GC_PAGE_SIZE >> 6;
    if ((page->free_bitmap[aux_size + (index >> 3)] |= (1 << (index & 7)))
      != 0)
      page->free_bitmap[index >> 6] |= (1 << ((index >> 3) & 7));
  }
  
  
  
//------------------------------------------------------------------------------
  
  
  
  /* 
   * Marking:
   */
//------------------------------------------------------------------------------
  
  /* 
   * Inserts the specified object into the gray set.
   */
  void
  garbage_collector::paint_gray (p_value *val)
  {
    if (!val)
      return;
    if (val->gc_state == GC_GRAY)
      return;
    
    ++ this->marked;
    val->gc_state = GC_GRAY;
    this->grays.push_back (val);
  }
  
  
  
  /* 
   * Prepares the root set by marking the root objects (stack, globals, etc...)
   */
  void
  garbage_collector::mark_roots ()
  {
    // 
    // Stack.
    // 
    int sp = this->vm.sp;
    for (int i = 0; i < sp; ++i)
      {
        p_value& val = this->vm.stack[i];
        
        /*
        if (val.type == PERL_REF)
          {
            if (val.val.ref)
              {
                if (val.val.ref->is_gc)
                  {
                    val.gc_state = GC_BLACK;
                    val.val.ref->gc_state = GC_GRAY;
                    this->grays.push_back (val.val.ref);
                    ++ this->marked_roots;
                  }
              }
          }
        */
        
        //if (false)
        if ((val.type == PERL_REF) && val.val.ref && val.val.ref->is_gc)
          {
            val.gc_state = GC_BLACK;
            val.val.ref->gc_state = GC_GRAY;
            this->grays.push_back (val.val.ref);
            ++ this->marked_roots;
          }
      }
    
    // 
    // Globals.
    // 
    for (auto p : this->vm.globs)
      {
        p_value& val = p.second;
        if ((val.type == PERL_REF) && val.val.ref && val.val.ref->is_gc)
          {
            val.gc_state = GC_BLACK;
            val.val.ref->gc_state = GC_GRAY;
            this->grays.push_back (val.val.ref);
          }
      }
  }
  
  
  /* 
   * Marks the children of the specified value.
   */
  void
  garbage_collector::mark_children (p_value *val)
  {
    switch (val->type)
      {
      case PERL_REF:
        this->paint_gray (val->val.ref);
        break;
      
      case PERL_ARRAY:
        {
          auto& data = val->val.arr;
          for (unsigned int i = 0; i < data.len; ++i)
            {
              auto& v = data.data[i];
              if ((v.type == PERL_REF) && v.val.ref && v.val.ref->is_gc)
                this->paint_gray (v.val.ref);
            }
        }
        break;
      
      
      default: ;
      }
  }
  
  
  /* 
   * Marks as many as "limit" objects.
   * Returns true if there's still more work to be done.
   */
  bool
  garbage_collector::incremental_mark (unsigned int limit)
  {
    unsigned int marked = 0;
    
    while (!this->grays.empty () && (marked++ < limit))
      {
        p_value *val = this->grays.back ();
        this->grays.pop_back ();
        
        this->mark_children (val);
        
        // blacken the object as all of its children have been marked.
        val->gc_state = GC_BLACK;
      }
    
    return !this->grays.empty ();
  }
  
//------------------------------------------------------------------------------
  
  
  
  /* 
   * Sweeping:
   */
//------------------------------------------------------------------------------
  
  /* 
   * Reclaims memory used by the specified object.
   */
  void
  garbage_collector::delete_object (p_value& val)
  {
    switch (val.type)
      {
      case PERL_ARRAY:
        delete[] val.val.arr.data;
        this->ext_bytes -= val.val.arr.cap * sizeof (p_value);
        break;
      
      case PERL_DSTR:
        delete[] val.val.str.data;
        this->ext_bytes -= val.val.str.cap;
        break;
      
      default: ;
      }
  }
  
  
  /* 
   * Destroys the page and all the objects inside it.
   */
  void
  garbage_collector::free_page (gc::heap_page *page)
  {
    const unsigned int aux_size = GC_PAGE_SIZE >> 6;
    unsigned int bitmap_bytes = GC_PAGE_SIZE >> 3;
    for (unsigned int i = 0; i < bitmap_bytes; ++i)
      {
        unsigned char bval = ~page->free_bitmap[aux_size + i];
        while (bval) // check for used objects
          {
            unsigned int obj_index = (i << 3) | _find_lsb (bval);
            bval &= ~(1 << (obj_index & 7));
            
            p_value& val = page->objs[obj_index];
            this->delete_object (val);
          }
      }
    
    delete page;
  }
  

  
  /* 
   * Reclaims all dead objects in the specified page.
   * Returns true if after sweeping the page is empty and can be reclaimed.
   */
  bool
  garbage_collector::sweep_page (gc::heap_page *page)
  {
    bool dead_page = true;
   
    GC_IF_DEBUG(std::cout << "    GC SWEEP PAGE" << std::endl;)
    
    const unsigned int aux_size = GC_PAGE_SIZE >> 6;
    unsigned int bitmap_bytes = GC_PAGE_SIZE >> 3;
    for (unsigned int i = 0; i < bitmap_bytes; ++i)
      {
        unsigned char bval = ~page->free_bitmap[aux_size + i];
        while (bval) // check for used objects
          {
            unsigned int obj_index = (i << 3) | _find_lsb (bval);
            bval &= ~(1 << (obj_index & 7));
            
            GC_IF_DEBUG(std::cout << "      USED OBJECT @#" << obj_index << std::endl;)
            p_value& val = page->objs[obj_index];
            if ((val.gc_state == this->curr_white) && !val.gc_protect)
              {
                // dead object
                
                GC_IF_DEBUG(std::cout << "        DEAD" << std::endl;)
                this->delete_object (val);
                _mark_free (page, obj_index);
              }
            else
              {
                dead_page = false;
                
                // toggle type of white color for next cycle
                val.gc_state = _opposite_white (this->curr_white);
              }
          }
      }
    
    return dead_page;
  }
  
  /* 
   * Sweeps a little bit.
   */
  bool
  garbage_collector::incremental_sweep (unsigned int limit)
  {
    unsigned int sweeped = 0;
    auto page = this->to_sweep;
    while (page && (sweeped++ < limit))
      {
        if (this->sweep_page (page))
          {
            // page is now empty
            
            auto next = page->next;
            
            this->unlink_page (page);
            delete page;
            
            page = next;
          }
        else
          page = page->next;
      }
    
    
    return page != nullptr;
  }
  
//------------------------------------------------------------------------------
  
  /* 
   * Notifies the collector that the specified amount of bytes have been
   * allocated outside of the GC (.e.g. dynamic strings, arrays, etc...).
   */
  void
  garbage_collector::notify_increase (unsigned int count)
  {
    this->ext_bytes += count;
    this->total_ext_bytes += count;
    ++ this->inc_count;
  }
  
  /* 
   * Notifies the collector that the specified amount of bytes have been
   * released outside of the GC (.e.g. from dynamic strings, arrays, etc...).
   */
  void
  garbage_collector::notify_decrease (unsigned int count)
  {
    this->ext_bytes -= count;
  }
  
  
  
  
  /* 
   * Performs a full garbage collection.
   */
  void
  garbage_collector::collect ()
  {
    // finish minor GC work
    while (this->state != gc::GCS_NONE)
      {
        this->work ();
      } 
    
    GC_IF_DEBUG(std::cout << "GC COLLECT" << std::endl;)
    
    // major GC
    do
      {
        this->work ();
      }
    while (this->state != gc::GCS_NONE);
  }
  
  /* 
   * Performs one unit/cycle of work.
   */
  void
  garbage_collector::work ()
  {
    switch (this->state)
      {
      case gc::GCS_NONE:
        GC_IF_DEBUG(std::cout << "  GC INC MARK START: [sp: " << this->vm.sp << "]" << std::endl;)
        this->marked = 0;
        this->marked_roots = 0;
        this->mark_roots ();
        this->state = gc::GCS_MARK;
        
        // flip current white color
        this->curr_white = _opposite_white (this->curr_white);
        break;
      
      case gc::GCS_MARK:
        GC_IF_DEBUG(std::cout << "  GC INC MARK" << std::endl;)
        if (!this->incremental_mark (GC_MARK_LIMIT))
          {
            // marking phase over
            this->to_sweep = this->pages;
            this->state = gc::GCS_SWEEP;
            GC_IF_DEBUG(std::cout << "  GC INC SWEEP START [marked " << this->marked << ", " << this->marked_roots << " roots]" << std::endl;)
          }
        break;
      
      case gc::GCS_SWEEP:
        GC_IF_DEBUG(std::cout << "  GC INC SWEEP" << std::endl;)
        if (!this->incremental_sweep (GC_SWEEP_LIMIT))
          {
            // sweeping phase over
            this->state = gc::GCS_NONE;
          }
        break;
      }
  }
  
  
  
  /* 
   * Allocates and returns a new object.
   */
  p_value*
  garbage_collector::alloc (bool protect)
  {
    ++ this->alloc_count;
    ++ this->total_alloc_count;
    long long ext_inc = this->ext_bytes - this->last_ext_bytes;
    long long allocs_since = this->alloc_count - this->last_alloc_count;
    
    // 
    // Collect if necessary.
    // 
    enum {
      NOTHING, CYCLE, FULL
    } act = NOTHING;
    if ((ext_inc > 67108864) && (allocs_since > 32)) // 64MB
      {
        act = FULL;
      }
    else if (this->alloc_count >= GC_ALLOC_THRESHOLD)
      {
        act = CYCLE;
      }
    if (act != NOTHING)
      {
        if (act == CYCLE)
          this->work ();
        else if (act == FULL)
          this->collect ();
          
        this->alloc_count = 0;
        this->last_alloc_count = this->alloc_count;
        this->last_ext_bytes = this->ext_bytes;
      }
    
    auto page = this->pages;
    if (!page || !_page_space_available (page))
      {
        // create new page
        page = this->alloc_page ();
        this->link_page (page);
      }
    
    unsigned int free_index = _next_free_object_index (page);
    GC_IF_DEBUG(std::cout << "GC ALLOC [" << free_index << "]" << std::endl;)
    p_value *val = &page->objs[free_index];
    _mark_used (page, free_index);
    
    val->is_gc = true;
    val->gc_state = _opposite_white (this->curr_white);
    val->gc_protect = protect;
    return val;
  }
  
  p_value*
  garbage_collector::alloc_copy (p_value& other, bool protect)
  {
    p_value *val = this->alloc (protect);
    
    *val = other;
    
    // restore GC fields
    val->is_gc = true;
    val->gc_state = _opposite_white (this->curr_white);
    val->gc_protect = protect;
    
    return val;
  }
}

