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

#ifndef _ARANE__COMMON__ERRORS__H_
#define _ARANE__COMMON__ERRORS__H_

#include <exception>
#include <string>
#include <vector>


namespace arane {
  
  /* 
   * Thrown by the error tracker when too many errors accumulate.
   */
  class compilation_error: public std::exception
  {
  public:
    compilation_error ()
      : std::exception ()
      { }
  };
  
  
  enum error_type
  {
    ET_INFO,
    ET_WARNING,
    ET_ERROR,
  };
  
  enum error_stage
  {
    ES_LEXER,
    ES_PARSER,
    ES_COMPILER,
    ES_LINKER,
  };
  
  
  
  class error_tracker
  {
  public:
    struct entry
    {
      error_stage stage;
      error_type type;
      int ln, col;
      std::string text;
    };
    
  private:
    std::vector<entry> entries;
    int error_count;
    int max_errors;
    bool deaf, deaf_ex;
    
  public:
    inline const std::vector<entry>& get_entries () const { return this->entries; }
    inline bool got_errors () const { return !this->entries.empty (); }
    inline size_t count () const { return this->entries.size (); }
    
  public:
    error_tracker (int max_errors);
    
  public:
    void info (error_stage stage, const std::string& what, int ln = -1, int col = -1);
    void warning (error_stage stage, const std::string& what, int ln = -1, int col = -1);
    void error (error_stage stage, const std::string& what, int ln = -1, int col = -1);
    
    
    /* 
     * Sets whether the tracker should ignore calls to the logging functions.
     */
    void silence (bool on, bool throw_on_errors);
  };
}

#endif

