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

#ifndef _ARANE__COMPILER__H_
#define _ARANE__COMPILER__H_

#include "parser/ast_store.hpp"
#include "parser/ast.hpp"
#include "common/errors.hpp"
#include "linker/module.hpp"
#include "common/types.hpp"
#include <deque>
#include <unordered_set>
#include <queue>

#include "compiler/signatures.hpp"
#include "compiler/package.hpp"
#include "compiler/frame.hpp"
#include "compiler/sub.hpp"
#include "compiler/context.hpp"


namespace arane {
  
  // forward decs:
  class code_generator;
  
  
  
  using compile_callback = void (compiler::*) (ast_node *ast, void *extra);
  
  /* 
   * The compiler.
   * Takes an AST tree representing a module as input, and produces a compiled
   * module.
   */
  class compiler
  {
  private:
    /* 
     * A module relocation subject to some changes by the compiler.
     */
    struct c_reloc
    {
      relocation_type type;
      int src;
      int dest;
      unsigned char size;
      
      int src_add;    // added to the final source offset.
    };
    
    struct c_deferred
    {
      compilation_context *ctx;
      int ph_lbl;
      compile_callback cb;
      ast_node *ast;
      void *extra;
    };
    
  private:
    friend class package;
    
    error_tracker& errs;
    ast_store& asts;
    signatures sigs;
    
    std::deque<frame *> frms;
    std::vector<frame *> all_frms;
    
    std::deque<package *> packs;
    std::vector<package *> all_packs;
    
    std::deque<compilation_context *> ctxs;
    std::vector<compilation_context *> all_ctxs;
    compilation_context *orig_ctx;
    std::queue<c_deferred> dcomps;
    
    std::vector<c_reloc> relocs;
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
    compiler (error_tracker& errs, ast_store& asts);
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
    package& global_package ();
    
    
    compilation_context* save_context ();
    void restore_context (compilation_context *ctx);
    void push_context (compilation_context *ctx);
    void pop_context ();
    
    /* 
     * Defers a compilation to the end of the compilation phase, once the
     * entire AST tree has been analyzed.
     * 
     * TODO: Might be better to just run a light check on the AST tree for
     *       any necessary information instead of deferring compilation...
     */
    void compile_later (compilation_context *ctx, int ph_lbl,
      compile_callback cb, ast_node *ast, void *extra);

  private:
    /* 
     * Attempts to statically deduce the type of the specified expression given
     * the current state of the compiler.
     */
    type_info deduce_type (ast_expr *ast);
    bool deduce_type_of_ident (ast_ident *ast, type_info& ti);
    bool deduce_type_of_sub_call (ast_sub_call *ast, type_info& ti);
    bool deduce_type_of_binop (ast_binop *ast, type_info& ti);
    bool deduce_type_of_binop_add (ast_binop *ast, type_info& ti);
    bool deduce_type_of_binop_sub (ast_binop *ast, type_info& ti);
    bool deduce_type_of_binop_mul (ast_binop *ast, type_info& ti);
    bool deduce_type_of_binop_div (ast_binop *ast, type_info& ti);
    bool deduce_type_of_binop_mod (ast_binop *ast, type_info& ti);
    bool deduce_type_of_binop_cmp (ast_binop *ast, type_info& ti);
    bool deduce_type_of_binop_concat (ast_binop *ast, type_info& ti);
    bool deduce_type_of_conditional (ast_conditional *ast, type_info& ti);
    bool deduce_type_of_prefix (ast_prefix *ast, type_info& ti);
    bool deduce_type_of_postfix (ast_postfix *ast, type_info& ti);

  private:
    /* 
     * Deferred compilation:
     */
//------------------------------------------------------------------------------

    /* 
     * Handles work deferred to the end of the compilation phase.
     */
    void handle_deferred ();
    
//------------------------------------------------------------------------------
    
  private:
    
    /* 
     * Inserts all required relocation entries into the compiled module.
     * 
     * Must be called once the code section will no longer be rearranged.
     */
    void mark_relocs ();
    
    /* 
     * Searches all packages in the compiled unit for the specified subroutine.
     * This function accepts a full path to the subroutine (<pack1>::...::<name>).
     */
    subroutine_info* find_sub (const std::string& path);

  private:
    /* 
     * Inserts a relocation.
     * Because the code generator may rearrange the contents of the code
     * section in any way it likes, raw offsets should not be used until the
     * end of the compilation phase, but labels instead.
     * 
     * If the relocation's destination lies in the code section, the third
     * parameter (`dest') should hold a label, otherwise a raw offset.
     */
    void insert_reloc (relocation_type type, int src_lbl, int dest,
      unsigned char size, int src_add = 0);
    
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
    void enforce_return_type (ast_expr *expr);
    
    void compile_if (ast_if *ast);
    
    void compile_while (ast_while *ast);
    
    void compile_for (ast_for *ast);
    void compile_for_on_range (ast_for *ast);
    
    void compile_loop (ast_loop *ast);
    
    void compile_use (ast_use *ast);
    
    
    
    void compile_expr (ast_expr *ast);
    
    void compile_undef (ast_undef *ast);
    
    void compile_integer (ast_integer *ast);
    
    void compile_bool (ast_bool *ast);
    
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
    
    void compile_conditional (ast_conditional *ast);
    
    void compile_prefix (ast_prefix *ast);
    void compile_prefix_inc (ast_prefix *ast);
    void compile_prefix_dec (ast_prefix *ast);
    void compile_prefix_str (ast_prefix *ast);
    
    void compile_postfix (ast_postfix *ast);
    void compile_postfix_inc (ast_postfix *ast);
    void compile_postfix_dec (ast_postfix *ast);
    
    
    void compile_assign (ast_expr *lhs, ast_expr *rhs);
    void assign_to_ident (ast_ident *lhs, ast_expr *rhs);
    void assign_to_list (ast_list *lhs, ast_expr *rhs);
    void assign_to_subscript (ast_subscript *lhs, ast_expr *rhs);
    void assign_to_deref (ast_deref *lhs, ast_expr *rhs);
    void assign_in_stack (ast_expr *lhs, bool keep_result);
    void enforce_assignment_type (const type_info& lhs_type, ast_expr *rhs);
    
    
    // special subroutines:
    void compile_sub_last (ast_sub_call *ast);
    void compile_sub_next (ast_sub_call *ast);
    void compile_sub_checkpoint (ast_sub_call *ast);
  };
}

#endif

