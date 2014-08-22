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

#ifndef _P6__COMPILER__H_
#define _P6__COMPILER__H_

#include "parser/ast.hpp"
#include "common/errors.hpp"
#include "linker/module.hpp"
#include <deque>
#include <unordered_set>

#include "compiler/package.hpp"
#include "compiler/frame.hpp"
#include "compiler/sub.hpp"


namespace p6 {
  
  // forward decs:
  class code_generator;
  
  
  /* 
   * The compiler.
   * Takes an AST tree representing a module as input, and produces a compiled
   * module.
   */
  class compiler
  {
    friend class package;
    
    error_tracker& errs;
    std::deque<frame *> frms;
    std::deque<package *> packs;
    std::vector<package *> all_packs;
    
    module *mod;
    code_generator *cgen;
    std::unordered_map<std::string, unsigned int> data_str_map;
    
    std::vector<subroutine_use> sub_uses;
    
    std::unordered_set<std::string> deps;
    
  public:
    inline const std::unordered_set<std::string>&
    get_dependencies () const
      { return this->deps; }
    
  public:
    compiler (error_tracker& errs);
    ~compiler ();
    
  public:
    /* 
     * Compiles the specified AST tree into a module.
     * The returned module should be deleted once no longer in use.
     */
    module* compile (const std::string& mod_name, ast_program *program);
    
  private:
    void push_frame (frame_type type);
    void pop_frame ();
    frame& top_frame ();
    
    void push_package (package_type type, const std::string& name);
    void pop_package ();
    package& top_package ();

  private:
    /* 
     * Inserts all required relocation entries into the compiled module.
     */
    void mark_relocs ();
    
    /* 
     * Searches all packages in the compiled unit for the specified subroutine.
     * This function accepts a full path to the subroutine (<pack1>::...::<name>).
     */
    subroutine_info* find_sub (const std::string& path);

  private:
    /* 
     * Inserts the specified string into the module's data section and returns
     * its index.  If the same string has already been added, the previous
     * string's index will be returned instead.
     */
    unsigned int insert_string (const std::string& str);
    
  private:
    /* 
     * Compilation:
     */
    
    void compile_program (ast_program *program);
    
    void compile_package (ast_package *ast);
    
    void compile_module (ast_module *ast);
    
    
    void compile_stmt (ast_stmt *ast);
    
    void compile_expr_stmt (ast_expr_stmt *ast);
    
    void compile_block (ast_block *ast, bool create_frame = true);
    
    void compile_sub (ast_sub *ast);
    
    void compile_sub_call (ast_sub_call *ast);
    
    void compile_return (ast_return *ast);
    
    void compile_if (ast_if *ast);
    
    void compile_while (ast_while *ast);
    
    void compile_for (ast_for *ast);
    void compile_for_on_range (ast_for *ast);
    
    void compile_loop (ast_loop *ast);
    
    void compile_use (ast_use *ast);
    
    
    
    void compile_expr (ast_expr *ast);
    
    void compile_undef (ast_undef *ast);
    
    void compile_integer (ast_integer *ast);
    
    void compile_ident (ast_ident *ast);
    
    void compile_string (ast_string *ast);
    
    void compile_interp_string (ast_interp_string *ast);
    
    void compile_list (ast_list *ast);
    
    void compile_anonym_array (ast_anonym_array *ast);
    
    void compile_subscript (ast_subscript *ast);
    
    void compile_binop (ast_binop *ast);
    void compile_cmp_binop (ast_binop *ast);
    
    void compile_named_unop (ast_named_unop *ast);
    void compile_unop_my (ast_named_unop *ast);
    
    void compile_ref (ast_ref *ast);
    
    void compile_deref (ast_deref *ast);
    
    void compile_range (ast_range *ast);
    
    
    void compile_assign (ast_expr *lhs, ast_expr *rhs);
    void assign_to_ident (ast_ident *lhs, ast_expr *rhs);
    void assign_to_list (ast_list *lhs, ast_expr *rhs);
    void assign_to_subscript (ast_subscript *lhs, ast_expr *rhs);
    void assign_to_deref (ast_deref *lhs, ast_expr *rhs);
    void assign_in_stack (ast_expr *lhs);
    
    
    // special subroutines:
    void compile_sub_last (ast_sub_call *ast);
    void compile_sub_next (ast_sub_call *ast);
    void compile_sub_push (ast_sub_call *ast);
    void compile_sub_checkpoint (ast_sub_call *ast);
  };
}

#endif

