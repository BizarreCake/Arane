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

#include "compiler/signatures.hpp"
#include "common/types.hpp"
#include "parser/ast_store.hpp"
#include "compiler/asttools.hpp"
#include <memory>

#include <iostream> // DEBUG


namespace arane {
  
  signatures::signatures (ast_store& asts)
    : asts (asts)
  {
    
  }
  
  signatures::~signatures ()
  {
    for (auto s : this->subs)
      delete s;
  }
  
  
  
  /* 
   * Processes the specified AST program.
   */
  void
  signatures::process (ast_program *ast)
  {
    if (this->processed.find (ast) != this->processed.end ())
      return;
    
    this->processed.insert (ast);
    this->check (ast, "");
  }
  
  void
  signatures::check (ast_node *ast, const std::string& path)
  {
    switch (ast->get_type ())
      {
      case AST_PROGRAM:
        {
          ast_program *prog = static_cast<ast_program *> (ast);
          auto& stmts = prog->get_body ()->get_stmts ();
          for (auto stmt : stmts)
            this->check (stmt, path);
        }
        break;
      
      case AST_PACKAGE:
      case AST_MODULE:
        {
          ast_package *pack = static_cast<ast_package *> (ast);
          auto& stmts = pack->get_body ()->get_stmts ();
          for (auto stmt : stmts)
            this->check (stmt, path + pack->get_name () + "::");
        }
        break;
      
      case AST_SUB:
        {
          ast_sub *sub = static_cast<ast_sub *> (ast);
          this->check_sub (sub, path);
        }
        break;
      
      default: ;
      }
  }
  
  void
  signatures::check_sub (ast_sub *ast, const std::string& path)
  {
    std::unique_ptr<sigs::subroutine_info> inf { new sigs::subroutine_info () };
    
    inf->name = path + ast->get_name ();
    
    // parameters
    for (auto p : ast->get_params ())
      {
        auto expr = p.expr;
        switch (expr->get_type ())
          {
          case AST_IDENT:
            inf->params.push_back ({
              .name = (static_cast<ast_ident *> (expr))->get_name (),
              .ti = type_info::none (),
            });
            break;
          
          case AST_OF_TYPE:
            {
              ast_of_type *oft = static_cast<ast_of_type *> (expr);
              if (oft->get_expr ()->get_type () == AST_IDENT)
                {
                  inf->params.push_back ({
                    .name = (static_cast<ast_ident *> (oft->get_expr ()))->get_name (),
                    .ti = oft->get_typeinfo (),
                  });
                }
            }
            break;
          
          default: ;
          }
        
        // traits
        auto& param = inf->params.back ();
        for (const std::string& trait : expr->get_traits ())
          {
            if (trait == "copy")
              param.is_copy = true;
            else if (trait == "rw")
              param.is_rw = true;
            else
              {
                
              }
          }
      }
    
    inf->ret_ti = ast->get_return_type ();
    inf->uses_def_arr = ast::count_ident_uses (ast, AST_IDENT_ARRAY, "_");
    
    this->sub_map[inf->name] = this->subs.size ();
    this->subs.push_back (inf.release ());
  }
  
  
  
  /* 
   * Parses the specified AST tree.
   */
  void
  signatures::parse (ast_program *ast)
  {
    this->process (ast);
  }
  
  /* 
   * Parses the file located at the specified path.
   */
  void
  signatures::parse (const std::string& path)
  {
    error_tracker errs {5};
    try
      {
        this->parse (this->asts.parse (path, errs));
      }
    catch (const std::exception& ex)
      { }
  }
  
  
  
  /* 
   * Finds and returns the signature of the specified subroutine, or null
   * if not found.
   */
  sigs::subroutine_info*
  signatures::find_sub (const std::string& name)
  {
    auto itr = this->sub_map.find (name);
    if (itr == this->sub_map.end ())
      return nullptr;
    
    return this->subs[itr->second];
  }
}

