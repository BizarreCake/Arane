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
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/errors.hpp"


namespace p6 {
  
  error_tracker::error_tracker (int max_errors)
  {
    this->max_errors = max_errors;
    this->error_count = 0;
    this->deaf = this->deaf_ex = false;
  }
  
  
  
  void
  error_tracker::info (error_stage stage, const std::string& what, int ln, int col)
  {
    if (this->deaf) return;
    this->entries.push_back ({ stage, ET_INFO, ln, col, what });
  }
  
  void
  error_tracker::warning (error_stage stage, const std::string& what, int ln, int col)
  {
    if (this->deaf) return;
    this->entries.push_back ({ stage, ET_WARNING, ln, col, what });
  }
  
  void
  error_tracker::error (error_stage stage, const std::string& what, int ln, int col)
  {
    if (!this->deaf)
      this->entries.push_back ({ stage, ET_ERROR, ln, col, what });
      
    if (!this->deaf_ex)
      if (++this->error_count >= this->max_errors)
        throw compilation_error ();
  }
  
  
  
  /* 
   * Sets whether the tracker should ignore calls to the logging functions.
   */
  void
  error_tracker::silence (bool on, bool throw_on_errors)
  {
    this->deaf = on;
    this->deaf_ex = !throw_on_errors;
  }
} 

