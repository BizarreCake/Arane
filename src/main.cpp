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

#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <string>
#include "interpreter.hpp"


int
main (int argc, char *argv[])
{
  std::vector<std::string> files;  
  
  for (int i = 1; i < argc; ++i)
    {
      char *arg = argv[i];
      if (arg[0] == '-')
        {
          if (arg[1] == '-')
            {
              if (std::strcmp (arg + 2, "version") == 0)
                {
                  std::cout << "Arane 1.0.1 20140827" << std::endl;
                  return 0;
                }
            }
          else
            {
              if (std::strcmp (arg + 1, "e") == 0)
                {
                  if ((i + 1) >= argc)
                    {
                      std::cout << "expected string after -e option" << std::endl;
                      return -1;
                    }
                  
                  std::istringstream ss { argv[++i] };
                  arane::interpreter interp {};
                  return interp.interpret (ss);
                }
            }
        }
      else
        {
          files.push_back (arg);
        }
    }
  
  if (files.empty ())
    {
      std::cout << "arane: error: no input files" << std::endl;
      return -1;
    }
  
  arane::interpreter interp {};
  return interp.interpret (files[0]);
}

