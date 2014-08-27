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

#include "common/utils.hpp"


namespace arane {
  
  namespace utils {
    
    /* 
     * Strips package names from the specified path (e.g. Turns `Foo::bar' into
     * just `bar').
     */
    std::string
    strip_packages (const std::string& path)
    {
      std::string::size_type pos = path.rfind ("::");
      if (pos == std::string::npos)
        return path;
      
      return path.substr (pos + 2);
    }
    
    
    
    /* 
     * Turns a name such as 'Foo::Bar' in
     *     use Foo::Bar;
     * into /Foo/Bar.pm
     */    
    std::string
    module_name_to_path (const std::string& name)
    {
      std::string path;
      
      // replace :: with /
      for (size_t i = 0; i < name.length (); ++i)
        {
          if (name[i] == ':' && (i != name.length () - 1) && name[i + 1] == ':')
            {
              ++ i;
              path.push_back ('/');
            }
          else
            path.push_back (name[i]);
        }
      
      path.append (".pm");
      return path;
    }
  }
}

