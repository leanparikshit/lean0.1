/*
Copyright (c) 2013 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#pragma once
#include <vector>
#include "environment.h"
#include "context.h"
#include "name_set.h"

namespace lean {
/** \brief Create a metavariable with the given index */
expr mk_metavar(unsigned idx);

/** \brief Return true iff the given expression is a metavariable */
bool is_metavar(expr const & n);

/** \brief Return the index of the given metavariable */
unsigned metavar_idx(expr const & n);

/** \brief Return true iff the given expression contains a metavariable */
bool has_metavar(expr const & e);

/**
   \brief Metavariable environment. It is used for solving
   unification constraints generated by expression elaborator
   module. The elaborator compute implicit arguments that were not
   provided by the user.
*/
class metavar_env {
    enum class state { Unprocessed, Processing, Processed };
    struct cell {
        expr     m_expr;
        context  m_context;
        unsigned m_find;
        unsigned m_rank;
        state    m_state;
        cell(expr const & e, context const & ctx, unsigned find);
        /*
          Basic properties for metavariable contexts:
          1) A metavariable does not occur in its own context.

          2) If a metavariable ?m1 occurs in context ctx2 of
          metavariable ?m2, then context ctx1 of ?m1 must be a prefix of ctx2.
          This is by construction.
          Here is an example:
             (fun (A : Type) (?m1 : A) (?m2 : B), C)
             The context of ?m1 is [A : Type]
             The context of ?m2 is [A : Type, ?m1 : A]

          Remark: these conditions are not enforced by this module.
        */
    };
    environment const & m_env;
    std::vector<cell>   m_cells;
    unsigned            m_max_depth;
    unsigned            m_depth;
    // If m_available_definitions != nullptr, then only the
    // definitions in m_available_definitions are unfolded during unification.
    name_set const *    m_available_definitions;
    volatile bool       m_interrupted;

    bool is_root(unsigned midx) const;
    unsigned root_midx(unsigned midx) const;
    cell & root_cell(unsigned midx);
    cell const & root_cell(unsigned midx) const;
    cell & root_cell(expr const & m);
    cell const & root_cell(expr const & m) const;
    friend struct root_fn;
    unsigned new_rank(unsigned r1, unsigned r2);
    void assign_term(expr const & m, expr const & s);
    [[noreturn]] void failed_to_unify();
    void ensure_same_length(context & ctx1, context & ctx2);
    void unify_ctx_prefix(context const & ctx1, context const & ctx2);
    void unify_ctx_entries(context const & ctx1, context const & ctx2);
    bool is_simple_ho_match_core(expr const & e1, expr const & e2, context const & ctx);
    bool is_simple_ho_match(expr const & e1, expr const & e2, context const & ctx);
    void unify_core(expr const & e1, expr const & e2, context const & ctx);

public:
    metavar_env(environment const & env, name_set const * available_defs, unsigned max_depth);
    metavar_env(environment const & env, name_set const * available_defs);
    metavar_env(environment const & env);

    /** \brief Create a new meta-variable with the given context. */
    expr mk_metavar(context const & ctx = context());

    /**
       \brief Return true iff the given metavariable representative is
       assigned.

       \pre is_metavar(m)
    */
    bool is_assigned(expr const & m) const;

    /**
        \brief If the given expression is a metavariable, then return the root
        of the equivalence class. Otherwise, return itself.
    */
    expr const & root(expr const & e) const;

    /**
       \brief Assign m <- s
    */
    void assign(expr const & m, expr const & s);

    /**
        \brief Return true iff \c e1 is structurally equal to \c e2
        modulo the union find table.
    */
    bool is_modulo_eq(expr const & e1, expr const & e2);

    /**
       \brief Replace the metavariables occurring in \c e with the
       substitutions in this metaenvironment.
    */
    expr instantiate_metavars(expr const & e);

    /**
        \brief Return true iff the given expression is an available
        definition.
    */
    bool is_definition(expr const & e);

    /**
       \brief Return the definition of the given expression in the
       environment m_env.
    */
    expr const & get_definition(expr const & e);

    /**
       \brief Check if \c e1 and \c e2 can be unified in the given
       metavariable environment. The environment may be updated with
       new assignments. An exception is throw if \c e1 and \c e2 can't
       be unified.
    */
    void unify(expr const & e1, expr const & e2, context const & ctx = context());

    /**
       \brief Return the context associated with the given
       meta-variable.

       \pre is_metavar(m)
    */
    context const & get_context(expr const & m);

    /**
       \brief Clear/Reset the state.
    */
    void clear();

    void set_interrupt(bool flag);

    void display(std::ostream & out) const;

    bool check_invariant() const;
};
}