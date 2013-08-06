/*
Copyright (c) 2013 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#pragma once
#include "expr.h"

namespace lean {
/**
   \brief Return unit if <tt>num_args == 0<\tt>, args[0] if <tt>num_args == 1<\tt>, and
   <tt>(op args[0] (op args[1] (op ... )))<\tt>
*/
expr mk_bin_op(expr const & op, expr const & unit, unsigned num_args, expr const * args);
expr mk_bin_op(expr const & op, expr const & unit, std::initializer_list<expr> const & l);

/** \brief Return (Type m)  m >= bottom + Offset */
expr mk_type_m();
#define TypeM mk_type_m()

/** \brief Return (Type u)  u >= m + Offset */
expr mk_type_u();
#define TypeU mk_type_u()

/** \brief Return the Lean Boolean type. */
expr mk_bool_type();
#define Bool mk_bool_type()
/** \brief Return true iff \c e is the Lean Boolean type. */
bool is_bool_type(expr const & e);

/** \brief Create a Lean Boolean value (true/false) */
expr mk_bool_value(bool v);
#define True  mk_bool_value(true)
#define False mk_bool_value(false)
/** \brief Return true iff \c e is a Lean Boolean value. */
bool is_bool_value(expr const & e);
/**
    \brief Convert a Lean Boolean value into a C++ Boolean value.
    \pre is_bool_value(e)
*/
bool to_bool(expr const & e);
/** \brief Return true iff \c e is the Lean true value. */
bool is_true(expr const & e);
/** \brief Return true iff \c e is the Lean false value. */
bool is_false(expr const & e);

/** \brief Return the Lean If-Then-Else operator. It has type: pi (A : Type), bool -> A -> A -> A */
expr mk_if_fn();
/** \brief Return true iff \c e is the Lean If-Then-Else operator */
bool is_if_fn(expr const & e);

/** \brief Return the term (if A c t e) */
inline expr mk_if(expr const & A, expr const & c, expr const & t, expr const & e) { return mk_app(mk_if_fn(), A, c, t, e); }
inline expr If(expr const & A, expr const & c, expr const & t, expr const & e) { return mk_if(A, c, t, e); }
/** \brief Return the term (if bool c t e) */
inline expr mk_bool_if(expr const & c, expr const & t, expr const & e) { return mk_if(mk_bool_type(), c, t, e); }
inline expr bIf(expr const & c, expr const & t, expr const & e) { return mk_bool_if(c, t, e); }

/** \brief Return the Lean and operator */
expr mk_and_fn();
/** \brief Return true iff \c e is the Lean and operator. */
bool is_and_fn(expr const & e);

/** \brief Return (and e1 e2) */
inline expr mk_and(expr const & e1, expr const & e2) { return mk_app(mk_and_fn(), e1, e2); }
inline expr mk_and(unsigned num_args, expr const * args) { return mk_bin_op(mk_and_fn(), True, num_args, args); }
inline expr And(expr const & e1, expr const & e2) { return mk_and(e1, e2); }
inline expr And(std::initializer_list<expr> const & l) { return mk_and(l.size(), l.begin()); }

/** \brief Return the Lean or operator */
expr mk_or_fn();
bool is_or_fn(expr const & e);

/** \brief Return (or e1 e2) */
inline expr mk_or(expr const & e1, expr const & e2) { return mk_app(mk_or_fn(), e1, e2); }
inline expr mk_or(unsigned num_args, expr const * args) { return mk_bin_op(mk_or_fn(), False, num_args, args); }
inline expr Or(expr const & e1, expr const & e2) { return mk_or(e1, e2); }
inline expr Or(std::initializer_list<expr> const & l) { return mk_or(l.size(), l.begin()); }

/** \brief Return the Lean not operator */
expr mk_not_fn();
bool is_not_fn(expr const & e);

/** \brief Return (not e) */
inline expr mk_not(expr const & e) { return mk_app(mk_not_fn(), e); }
inline expr Not(expr const & e) { return mk_not(e); }

/** \brief Return the Lean forall operator. It has type: <tt>Pi (A : Type), (A -> bool) -> Bool<\tt> */
expr mk_forall_fn();
/** \brief Return true iff \c e is the Lean forall operator */
bool is_forall_fn(expr const & e);
/** \brief Return the term (forall A P), where A is a type and P : A -> bool */
inline expr mk_forall(expr const & A, expr const & P) { return mk_app(mk_forall_fn(), A, P); }
inline expr Forall(expr const & A, expr const & P) { return mk_forall(A, P); }

/** \brief Return the Lean exists operator. It has type: <tt>Pi (A : Type), (A -> Bool) -> Bool<\tt> */
expr mk_exists_fn();
/** \brief Return true iff \c e is the Lean exists operator */
bool is_exists_fn(expr const & e);
/** \brief Return the term (exists A P), where A is a type and P : A -> bool */
inline expr mk_exists(expr const & A, expr const & P) { return mk_app(mk_exists_fn(), A, P); }
inline expr Exists(expr const & A, expr const & P) { return mk_exists(A, P); }


expr mk_refl_fn();
bool is_refl_fn(expr const & e);
inline expr Refl(expr const & A, expr const & a) { return mk_app(mk_refl_fn(), A, a); }
expr mk_subst_fn();
bool is_subst_fn(expr const & e);
inline expr Subst(expr const & A, expr const & P, expr const & a, expr const & b, expr const & H1, expr const & H2) { return mk_app({mk_subst_fn(), A, P, a, b, H1, H2}); }
expr mk_symm_fn();
bool is_symm_fn(expr const & e);
inline expr Symm(expr const & A, expr const & a, expr const & b, expr const & H) { return mk_app(mk_symm_fn(), A, a, b, H); }
expr mk_trans_fn();
bool is_trans_fn(expr const & e);
expr mk_congr_fn();
bool is_congr_fn(expr const & e);
expr mk_eq_mp_fn();
bool is_eq_mp_fn(expr const & e);
inline expr EqMP(expr const & a, expr const & b, expr const & H1, expr const & H2) { return mk_app(mk_eq_mp_fn(), a, b, H1, H2); }
expr mk_truth();
bool is_truth(expr const & e);
#define Truth mk_truth()
expr mk_ext_fn();
bool is_ext_fn(expr const & e);
expr mk_foralle_fn();
bool is_foralle_fn(expr const & e);
expr mk_foralli_fn();
bool is_foralli_fn(expr const & e);
expr mk_domain_inj_fn();
bool is_domain_inj_fn(expr const & e);
expr mk_range_inj_fn();
bool is_range_inj_fn(expr const & e);

class environment;
/** \brief Initialize the environment with basic builtin declarations and axioms */
void add_basic_theory(environment & env);

/**
   \brief Helper macro for defining constants such as bool_type, int_type, int_add, etc.
*/
#define MK_BUILTIN(Name, ClassName)                                     \
expr mk_##Name() {                                                      \
    static thread_local expr r = mk_value(*(new ClassName()));          \
    return r;                                                           \
}                                                                       \
bool is_##Name(expr const & e) {                                        \
    return is_value(e) && to_value(e).kind() == ClassName::g_kind;      \
}

/**
   \brief Helper macro for generating "defined" constants.
*/
#define MK_CONSTANT(Name, NameObj)                              \
static name Name ## _name = NameObj;                            \
expr mk_##Name() {                                              \
    static thread_local expr r = mk_constant(Name ## _name);    \
    return r ;                                                  \
}                                                               \
bool is_##Name(expr const & e) {                                \
    return is_constant(e) && const_name(e) == Name ## _name;    \
}
}