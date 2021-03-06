/*
Copyright (c) 2013 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include <limits>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>
#include "util/scoped_map.h"
#include "util/exception.h"
#include "util/scoped_set.h"
#include "util/sexpr/options.h"
#include "util/interrupt.h"
#include "kernel/context.h"
#include "kernel/for_each_fn.h"
#include "kernel/find_fn.h"
#include "kernel/occurs.h"
#include "kernel/kernel.h"
#include "kernel/free_vars.h"
#include "kernel/replace_fn.h"
#include "library/context_to_lambda.h"
#include "library/placeholder.h"
#include "library/io_state_stream.h"
#include "frontends/lean/notation.h"
#include "frontends/lean/pp.h"
#include "frontends/lean/frontend.h"
#include "frontends/lean/coercion.h"
#include "frontends/lean/frontend_elaborator.h"

#ifndef LEAN_DEFAULT_PP_MAX_DEPTH
#define LEAN_DEFAULT_PP_MAX_DEPTH std::numeric_limits<unsigned>::max()
#endif

#ifndef LEAN_DEFAULT_PP_MAX_STEPS
#define LEAN_DEFAULT_PP_MAX_STEPS std::numeric_limits<unsigned>::max()
#endif

#ifndef LEAN_DEFAULT_PP_NOTATION
#define LEAN_DEFAULT_PP_NOTATION true
#endif

#ifndef LEAN_DEFAULT_PP_IMPLICIT
#define LEAN_DEFAULT_PP_IMPLICIT false
#endif

#ifndef LEAN_DEFAULT_PP_COERCION
#define LEAN_DEFAULT_PP_COERCION false
#endif

#ifndef LEAN_DEFAULT_PP_EXTRA_LETS
#define LEAN_DEFAULT_PP_EXTRA_LETS true
#endif

#ifndef LEAN_DEFAULT_PP_ALIAS_MIN_WEIGHT
#define LEAN_DEFAULT_PP_ALIAS_MIN_WEIGHT 20
#endif

#ifndef LEAN_DEFAULT_PP_DEFINITION_VALUE
#define LEAN_DEFAULT_PP_DEFINITION_VALUE true
#endif

namespace lean {
static format g_Type_fmt      = highlight_builtin(format("Type"));
static format g_lambda_n_fmt  = highlight_keyword(format("\u03BB"));
static format g_Pi_n_fmt      = highlight_keyword(format("\u2200"));
static format g_lambda_fmt    = highlight_keyword(format("fun"));
static format g_Pi_fmt        = highlight_keyword(format("forall"));
static format g_arrow_n_fmt   = highlight_keyword(format("\u2192"));
static format g_arrow_fmt     = highlight_keyword(format("->"));
static format g_exists_n_fmt  = highlight_keyword(format("\u2203"));
static format g_exists_fmt    = highlight_keyword(format("exists"));
static format g_ellipsis_n_fmt= highlight(format("\u2026"));
static format g_ellipsis_fmt  = highlight(format("..."));
static format g_let_fmt       = highlight_keyword(format("let"));
static format g_in_fmt        = highlight_keyword(format("in"));
static format g_pair_fmt      = highlight_keyword(format("pair"));
static format g_proj1_fmt     = highlight_keyword(format("proj1"));
static format g_proj2_fmt     = highlight_keyword(format("proj2"));
static format g_assign_fmt    = highlight_keyword(format(":="));
static format g_geq_fmt       = format("\u2265");
static format g_lift_fmt      = highlight_keyword(format("lift"));
static format g_inst_fmt      = highlight_keyword(format("inst"));
static format g_sig_fmt       = highlight_keyword(format("sig"));
static format g_heq_fmt       = highlight_keyword(format("=="));
static format g_cartesian_product_fmt   = highlight_keyword(format("#"));
static format g_cartesian_product_n_fmt = highlight_keyword(format("\u2A2F"));

static name g_pp_max_depth        {"lean", "pp", "max_depth"};
static name g_pp_max_steps        {"lean", "pp", "max_steps"};
static name g_pp_implicit         {"lean", "pp", "implicit"};
static name g_pp_notation         {"lean", "pp", "notation"};
static name g_pp_extra_lets       {"lean", "pp", "extra_lets"};
static name g_pp_alias_min_weight {"lean", "pp", "alias_min_weight"};
static name g_pp_coercion         {"lean", "pp", "coercion"};
static name g_pp_def_value        {"lean", "pp", "definition_value"};

RegisterUnsignedOption(g_pp_max_depth, LEAN_DEFAULT_PP_MAX_DEPTH, "(lean pretty printer) maximum expression depth, after that it will use ellipsis");
RegisterUnsignedOption(g_pp_max_steps, LEAN_DEFAULT_PP_MAX_STEPS, "(lean pretty printer) maximum number of visited expressions, after that it will use ellipsis");
RegisterBoolOption(g_pp_implicit,  LEAN_DEFAULT_PP_IMPLICIT, "(lean pretty printer) display implicit parameters");
RegisterBoolOption(g_pp_notation,  LEAN_DEFAULT_PP_NOTATION, "(lean pretty printer) disable/enable notation (infix, mixfix, postfix operators and unicode characters)");
RegisterBoolOption(g_pp_coercion,  LEAN_DEFAULT_PP_COERCION, "(lean pretty printer) display coercions");
RegisterBoolOption(g_pp_extra_lets,  LEAN_DEFAULT_PP_EXTRA_LETS, "(lean pretty printer) introduce extra let expressions when displaying shared terms");
RegisterUnsignedOption(g_pp_alias_min_weight,  LEAN_DEFAULT_PP_ALIAS_MIN_WEIGHT, "(lean pretty printer) mimimal weight (approx. size) of a term to be considered a shared term");
RegisterBoolOption(g_pp_def_value, LEAN_DEFAULT_PP_DEFINITION_VALUE, "(lean pretty printer) display definition/theorem value (i.e., the actual definition)");

unsigned get_pp_max_depth(options const & opts)        { return opts.get_unsigned(g_pp_max_depth, LEAN_DEFAULT_PP_MAX_DEPTH); }
unsigned get_pp_max_steps(options const & opts)        { return opts.get_unsigned(g_pp_max_steps, LEAN_DEFAULT_PP_MAX_STEPS); }
bool     get_pp_implicit(options const & opts)         { return opts.get_bool(g_pp_implicit, LEAN_DEFAULT_PP_IMPLICIT); }
bool     get_pp_notation(options const & opts)         { return opts.get_bool(g_pp_notation, LEAN_DEFAULT_PP_NOTATION); }
bool     get_pp_coercion(options const & opts)         { return opts.get_bool(g_pp_coercion, LEAN_DEFAULT_PP_COERCION); }
bool     get_pp_extra_lets(options const & opts)       { return opts.get_bool(g_pp_extra_lets, LEAN_DEFAULT_PP_EXTRA_LETS); }
unsigned get_pp_alias_min_weight(options const & opts) { return opts.get_unsigned(g_pp_alias_min_weight, LEAN_DEFAULT_PP_ALIAS_MIN_WEIGHT); }
bool     get_pp_def_value(options const & opts)        { return opts.get_bool(g_pp_def_value, LEAN_DEFAULT_PP_DEFINITION_VALUE); }

// =======================================
// Prefixes for naming local aliases (auxiliary local decls)
static name g_a("a");
static name g_b("b");
static name g_c("c");
// =======================================

/**
   \brief Return a fresh name for the given abstraction or let.
   By fresh, we mean a name that is not used for any constant in
   abst_body(e).

   The resultant name is based on abst_name(e).
*/
name get_unused_name(expr const & e) {
    lean_assert(is_abstraction(e) || is_let(e));
    name const & n = is_abstraction(e) ? abst_name(e) : let_name(e);
    name n1    = n;
    unsigned i = 1;
    expr const & b = is_abstraction(e) ? abst_body(e) : let_body(e);
    while (occurs(n1, b)) {
        n1 = name(n, i);
        i++;
    }
    return n1;
}

/**
   \brief Replace free variable \c 0 in \c a with the name \c n.

   \remark Metavariable context is ignored.
*/
expr replace_var_with_name(expr const & a, name const & n) {
    expr c = mk_constant(n);
    return replace(a, [=](expr const & m, unsigned offset) -> expr {
            if (is_var(m)) {
                unsigned vidx = var_idx(m);
                if (vidx >= offset)
                    return vidx == offset ? c : mk_var(vidx - 1);
            }
            return m;
        });
}

bool is_notation_decl(object const & obj) {
    return dynamic_cast<notation_declaration const *>(obj.cell());
}

bool is_coercion_decl(object const & obj) {
    return dynamic_cast<coercion_declaration const *>(obj.cell());
}

bool is_alias_decl(object const & obj) {
    return dynamic_cast<alias_declaration const *>(obj.cell());
}

bool supported_by_pp(object const & obj) {
    return obj.kind() != object_kind::Neutral || is_notation_decl(obj) || is_coercion_decl(obj) || is_alias_decl(obj) || is_set_opaque(obj);
}

/** \brief Functional object for pretty printing expressions */
class pp_fn {
    typedef scoped_map<expr, name, expr_hash_alloc, expr_eqp> local_aliases;
    typedef std::vector<std::pair<name, format>>              local_aliases_defs;
    typedef scoped_set<name, name_hash, name_eq>              local_names;
    ro_environment   m_env;
    // State
    local_aliases      m_local_aliases;
    local_aliases_defs m_local_aliases_defs;
    local_names        m_local_names;
    unsigned           m_num_steps;
    name               m_aux;
    expr_map<unsigned> m_num_occs;
    // Configuration
    unsigned           m_indent;
    unsigned           m_max_depth;
    unsigned           m_max_steps;
    bool               m_implict;          //!< if true show implicit arguments
    bool               m_unicode;          //!< if true use unicode chars
    bool               m_coercion;         //!< if true show coercions
    bool               m_notation;         //!< if true use notation
    bool               m_extra_lets;       //!< introduce extra let-expression to cope with sharing.
    unsigned           m_alias_min_weight; //!< minimal weight for creating an alias

    // Create a scope for local definitions
    struct mk_scope {
        pp_fn &            m_fn;
        unsigned           m_old_size;
        expr_map<unsigned> m_num_occs;

        void update_num_occs(expr const & e) {
            buffer<expr> todo;
            todo.push_back(e);
            while (!todo.empty()) {
                expr e = todo.back();
                todo.pop_back();
                unsigned & n = m_num_occs[e];
                n++;
                // we do not visit other composite expressions such as Let, Lambda and Pi, since they create new scopes
                if (n == 1 && is_app(e)) {
                    for (unsigned i = 0; i < num_args(e); i++)
                        todo.push_back(arg(e, i));
                }
            }
        }

        mk_scope(pp_fn & fn, expr const & e):m_fn(fn), m_old_size(fn.m_local_aliases_defs.size()) {
            m_fn.m_local_aliases.push();
            update_num_occs(e);
            swap(m_fn.m_num_occs, m_num_occs);
        }
        ~mk_scope() {
            lean_assert(m_old_size <= m_fn.m_local_aliases_defs.size());
            m_fn.m_local_aliases.pop();
            m_fn.m_local_aliases_defs.resize(m_old_size);
            swap(m_fn.m_num_occs, m_num_occs);
        }
    };

    bool has_several_occs(expr const & e) const {
        auto it = m_num_occs.find(e);
        if (it != m_num_occs.end())
            return it->second > 1;
        else
            return false;
    }

    format nest(unsigned i, format const & f) { return ::lean::nest(i, f); }

    typedef std::pair<format, unsigned> result;

    bool is_coercion(expr const & e) {
        return is_app(e) && num_args(e) == 2 && ::lean::is_coercion(m_env, arg(e, 0));
    }

    /**
       \brief Return true iff \c e is an atomic operation.
    */
    bool is_atomic(expr const & e) {
        if (auto aliased_list = get_aliased(m_env, e)) {
            if (m_unicode || std::any_of(aliased_list->begin(), aliased_list->end(), [](name const & a) { return a.is_safe_ascii(); }))
                return true;
        }
        switch (e.kind()) {
        case expr_kind::Var: case expr_kind::Constant: case expr_kind::Type:
            return true;
        case expr_kind::Value:
            return to_value(e).is_atomic_pp(m_unicode, m_coercion);
        case expr_kind::MetaVar:
            return !metavar_lctx(e);
        case expr_kind::App:
            if (!m_coercion && is_coercion(e))
                return is_atomic(arg(e, 1));
            else
                return false;
        case expr_kind::Lambda: case expr_kind::Pi: case expr_kind::Let:
        case expr_kind::Sigma: case expr_kind::Pair: case expr_kind::Proj:
        case expr_kind::HEq:
            return false;
        }
        return false;
    }

    result mk_result(format const & fmt, unsigned depth) {
        return mk_pair(fmt, depth);
    }

    result pp_ellipsis() {
        return mk_result(m_unicode ? g_ellipsis_n_fmt : g_ellipsis_fmt, 1);
    }

    result pp_var(expr const & e) {
        unsigned vidx = var_idx(e);
        return mk_result(compose(format("#"), format(vidx)), 1);
    }

    bool has_implicit_arguments(name const & n) const {
        return ::lean::has_implicit_arguments(m_env, n) && m_local_names.find(n) == m_local_names.end();
    }

    result pp_value(expr const & e) {
        value const & v = to_value(e);
        if (has_implicit_arguments(v.get_name())) {
            return mk_result(format(get_explicit_version(m_env, v.get_name())), 1);
        } else {
            return mk_result(v.pp(m_unicode, m_coercion), 1);
        }
    }

    result pp_constant(expr const & e) {
        name const & n = const_name(e);
        if (is_placeholder(e)) {
            return mk_result(format("_"), 1);
        } else if (is_exists_fn(e)) {
            // use alias when exists is used as a function symbol
            return mk_result(format("Exists"), 1);
        } else if (has_implicit_arguments(n)) {
            return mk_result(format(get_explicit_version(m_env, n)), 1);
        } else {
            optional<object> obj = m_env->find_object(const_name(e));
            if (obj && obj->is_builtin() && obj->get_name() == const_name(e)) {
                // e is a constant that is referencing a builtin object.
                return pp_value(obj->get_value());
            } else {
                return mk_result(format(n), 1);
            }
        }
    }

    result pp_type(expr const & e) {
        if (e == Type()) {
            return mk_result(g_Type_fmt, 1);
        } else {
            return mk_result(paren(format{g_Type_fmt, space(), ::lean::pp(ty_level(e), m_unicode)}), 1);
        }
    }

    /**
       \brief Pretty print given expression and put parenthesis around
       it IF the pp of the expression is not a simple name.
    */
    result pp_child_with_paren(expr const & e, unsigned depth) {
        result r = pp(e, depth + 1);
        if (is_name(r.first)) {
            // We do not add a parenthis if the format object is just
            // a name. This can happen when \c e is a complicated
            // expression, but an alias is created for it.
            return r;
        } else {
            return mk_result(paren(r.first), r.second);
        }
    }

    /**
       \brief Pretty print given expression and put parenthesis around
       it if it is not atomic.
    */
    result pp_child(expr const & e, unsigned depth) {
        if (is_atomic(e))
            return pp(e, depth + 1);
        else
            return pp_child_with_paren(e, depth);
    }

    bool is_exists_expr(expr const & e) {
        return is_app(e) && arg(e, 0) == mk_exists_fn() && num_args(e) == 3;
    }

    /**
       \brief Collect nested quantifiers, and instantiate
       variables with unused names. Store in \c r the selected names
       and associated domains. Return the body of the sequence of
       nested quantifiers.
    */
    expr collect_nested_quantifiers(expr const & e, buffer<std::pair<name, expr>> & r) {
        lean_assert(is_exists_expr(e));
        if (is_lambda(arg(e, 2))) {
            expr lambda = arg(e, 2);
            name n1 = get_unused_name(lambda);
            m_local_names.insert(n1);
            r.emplace_back(n1, abst_domain(lambda));
            expr b  = replace_var_with_name(abst_body(lambda), n1);
            if (is_exists_expr(b))
                return collect_nested_quantifiers(b, r);
            else
                return b;
        } else {
            // Quantifier is not in normal form. That is, it might be
            //   (exists t p)  where p is not a lambda
            //   abstraction
            // So, we put it in normal form
            //   (exists t (fun x : t, p x))
            expr new_body = mk_lambda("x", arg(e, 1), mk_app(lift_free_vars(arg(e, 2), 1), mk_var(0)));
            expr normal_form = mk_app(arg(e, 0), arg(e, 1), new_body);
            return collect_nested_quantifiers(normal_form, r);
        }
    }

    /** \brief Auxiliary function for pretty printing exists formulas */
    result pp_exists(expr const & e, unsigned depth) {
        buffer<std::pair<name, expr>> nested;
        expr b = collect_nested_quantifiers(e, nested);
        format head;
        if (m_unicode)
            head = g_exists_n_fmt;
        else
            head = g_exists_fmt;
        format sep  = comma();
        expr domain0 = nested[0].second;
        // TODO(Leo): the following code is very similar to pp_abstraction
        if (std::all_of(nested.begin() + 1, nested.end(), [&](std::pair<name, expr> const & p) { return p.second == domain0; })) {
            // Domain of all binders is the same
            format names    = pp_bnames(nested.begin(), nested.end(), false);
            result p_domain = pp_scoped_child(domain0, depth);
            result p_body   = pp_scoped_child(b, depth);
            format sig      = format{names, space(), colon(), space(), p_domain.first};
            format r_format = group(nest(m_indent, format{head, space(), sig, sep, line(), p_body.first}));
            return mk_result(r_format, p_domain.second + p_body.second + 1);
        } else {
            auto it  = nested.begin();
            auto end = nested.end();
            unsigned r_weight = 1;
            // PP binders in blocks (names ... : type)
            bool first = true;
            format bindings;
            while (it != end) {
                auto it2 = it;
                ++it2;
                while (it2 != end && it2->second == it->second) {
                    ++it2;
                }
                result p_domain = pp_scoped_child(it->second, depth);
                r_weight += p_domain.second;
                format block = group(nest(m_indent, format{lp(), pp_bnames(it, it2, true), space(), colon(), space(), p_domain.first, rp()}));
                if (first) {
                    bindings = block;
                    first = false;
                } else {
                    bindings += compose(line(), block);
                }
                it = it2;
            }
            result p_body   = pp_scoped_child(b, depth);
            format r_format = group(nest(m_indent, format{head, space(), group(bindings), sep, line(), p_body.first}));
            return mk_result(r_format, r_weight + p_body.second);
        }
    }

    operator_info find_op_for(expr const & e) const {
        if (is_constant(e) && m_local_names.find(const_name(e)) != m_local_names.end())
            return operator_info();
        else
            return ::lean::find_op_for(m_env, e, m_unicode);
    }

    /**
       \brief Return the operator associated with \c e.
       Return the null operator if there is none.

       We say \c e has an operator associated with it, if:

       1) \c e is associated with an operator.

       2) It is an application, and the function is associated with an
       operator.
    */
    operator_info get_operator(expr const & e) {
        operator_info op = find_op_for(e);
        if (op)
            return op;
        else if (is_app(e))
            return find_op_for(arg(e, 0));
        else
            return operator_info();
    }

    /**
       \brief Return the precedence of the given expression
    */
    unsigned get_operator_precedence(expr const & e) {
        operator_info op = get_operator(e);
        if (op) {
            return op.get_precedence();
        } else if (is_arrow(e)) {
            return g_arrow_precedence;
        } else if (is_cartesian(e)) {
            return g_cartesian_product_precedence;
        } else if (is_lambda(e) || is_pi(e) || is_let(e) || is_exists(e) || is_sigma(e) || is_dep_pair(e)) {
            return 0;
        } else if (is_heq(e)) {
            return g_heq_precedence;
        } else {
            return g_app_precedence;
        }
    }

    /**
       \brief Return true iff the given expression has the given fixity.
    */
    bool has_fixity(expr const & e, fixity fx) {
        operator_info op = get_operator(e);
        if (op) {
            return op.get_fixity() == fx;
        } else if (is_arrow(e)) {
            return fixity::Infixr == fx;
        } else {
            return false;
        }
    }

    /**
        \brief Pretty print the child of an infix, prefix, postfix or
        mixfix operator. It will add parethesis when needed.
    */
    result pp_mixfix_child(operator_info const & op, expr const & e, unsigned depth) {
        if (is_atomic(e)) {
            return pp(e, depth + 1);
        } else {
            if (op.get_precedence() < get_operator_precedence(e))
                return pp(e, depth + 1);
            else
                return pp_child_with_paren(e, depth);
        }
    }

    /**
        \brief Pretty print the child of an associative infix
        operator. It will add parethesis when needed.
    */
    result pp_infix_child(operator_info const & op, expr const & e, unsigned depth, fixity fx) {
        if (is_atomic(e)) {
            return pp(e, depth + 1);
        } else {
            unsigned e_prec = get_operator_precedence(e);
            if (op.get_precedence() < e_prec)
                return pp(e, depth + 1);
            else if (op.get_precedence() == e_prec && has_fixity(e, fx))
                return pp(e, depth + 1);
            else
                return pp_child_with_paren(e, depth);
        }
    }

    result mk_infix(operator_info const & op, result const & lhs, result const & rhs) {
        unsigned r_weight = lhs.second + rhs.second + 1;
        format   r_format = group(format{lhs.first, space(), format(op.get_op_name()), line(), rhs.first});
        return mk_result(r_format, r_weight);
    }

    /**
       \brief Wrapper for accessing the explicit arguments of an
       application and its function.

       \remark If show_implicit is true, then we show the explicit
       arguments even if the function has implicit arguments.

       \remark We also show the implicit arguments, if the
       application does not have the necessary number of
       arguments.

       \remark When we expose the implicit arguments, we use the
       explicit version of the function. So, the user can
       understand the pretty printed term.
    */
    struct application {
        expr const & m_app;
        expr         m_f;
        std::vector<bool> const * m_implicit_args;
        bool         m_notation_enabled;

        static bool has_implicit_arguments(pp_fn const & owner, expr const & f) {
            return
                (is_constant(f) && owner.has_implicit_arguments(const_name(f))) ||
                (is_value(f)    && owner.has_implicit_arguments(to_value(f).get_name()));
        }

        application(expr const & e, pp_fn const & owner, bool show_implicit):m_app(e) {
            ro_environment const & env = owner.m_env;
            expr const & f = arg(e, 0);
            if (has_implicit_arguments(owner, f)) {
                name const & n = is_constant(f) ? const_name(f) : to_value(f).get_name();
                m_implicit_args = &(get_implicit_arguments(env, n));
                if (show_implicit || num_args(e) - 1 < m_implicit_args->size()) {
                    // we are showing implicit arguments, thus we do
                    // not need the bit-mask for implicit arguments
                    m_implicit_args = nullptr;
                    // we use the explicit name of f, to make it clear
                    // that we are exposing implicit arguments
                    m_f = mk_constant(get_explicit_version(env, n));
                    m_notation_enabled = false;
                } else {
                    m_f = f;
                    m_notation_enabled = true;
                }
            } else {
                m_f = f;
                m_implicit_args = nullptr;
                m_notation_enabled = true;
            }
        }

        unsigned get_num_args() const {
            if (m_implicit_args) {
                unsigned r = 0;
                for (unsigned i = 0; i < num_args(m_app) - 1; i++) {
                    // Remark: we need the test i >= m_implicit_args because the application
                    // m_app may contain more arguments than the declaration of m_f.
                    // Example:
                    // m_f was declared as
                    //     Pi {A : Type} (a : A) : A
                    // Thus m_implicit_args has size 2, and contains {true, false}
                    // indicating that the first argument is implicit.
                    // Then, the actuall application is:
                    //    f (Int -> Int) g 10
                    // Assuming g has type Int -> Int.
                    // This application is fine and has type Int.
                    // We should not print the argument (Int -> Int) since it is
                    // implicit.
                    // We should view the application above as:
                    //   (f (Int -> Int) g) 10
                    // So, the arguments at position >= m_implicit_args->size()
                    // are explicit by default.
                    if (i >= m_implicit_args->size() || !(*m_implicit_args)[i])
                        r++;
                }
                return r;
            } else {
                return num_args(m_app) - 1;
            }
        }

        expr const & get_arg(unsigned i) const {
            lean_assert(i < get_num_args());
            if (m_implicit_args) {
                unsigned n = num_args(m_app);
                for (unsigned j = 1; j < n; j++) {
                    // See comment in get_num_args()
                    if (j - 1 >= m_implicit_args->size() || !(*m_implicit_args)[j-1]) {
                        // explicit argument found
                        if (i == 0)
                            return arg(m_app, j);
                        --i;
                    }
                }
                lean_unreachable(); // LCOV_EXCL_LINE
            } else {
                return arg(m_app, i + 1);
            }
        }

        expr const & get_function() const {
            return m_f;
        }

        bool notation_enabled() const {
            return m_notation_enabled;
        }
    };

    /** \brief Return true if the application \c app has the number of arguments expected by the operator \c op. */
    bool has_expected_num_args(application const & app, operator_info const & op) {
        switch (op.get_fixity()) {
        case fixity::Infix: case fixity::Infixl: case fixity::Infixr:
            return app.get_num_args() == 2;
        case fixity::Prefix: case fixity::Postfix:
            return app.get_num_args() == 1;
        case fixity::Mixfixl: case fixity::Mixfixr:
            return app.get_num_args() == length(op.get_op_name_parts());
        case fixity::Mixfixc:
            return app.get_num_args() == length(op.get_op_name_parts()) - 1;
        case fixity::Mixfixo:
            return app.get_num_args() == length(op.get_op_name_parts()) + 1;
        }
        lean_unreachable(); // LCOV_EXCL_LINE
    }

    /**
       \brief Pretty print an application.
    */
    result pp_app(expr const & e, unsigned depth) {
        if (!m_coercion && is_coercion(e))
            return pp(arg(e, 1), depth);
        application app(e, *this, m_implict || has_metavar(e));
        operator_info op;
        if (m_notation && app.notation_enabled() && (op = get_operator(e)) && has_expected_num_args(app, op)) {
            result   p_arg;
            format   r_format;
            unsigned sz;
            unsigned r_weight = 1;
            switch (op.get_fixity()) {
            case fixity::Infix:
                return mk_infix(op, pp_mixfix_child(op, app.get_arg(0), depth), pp_mixfix_child(op, app.get_arg(1), depth));
            case fixity::Infixr:
                return mk_infix(op, pp_mixfix_child(op, app.get_arg(0), depth), pp_infix_child(op, app.get_arg(1), depth, fixity::Infixr));
            case fixity::Infixl:
                return mk_infix(op, pp_infix_child(op, app.get_arg(0), depth, fixity::Infixl),  pp_mixfix_child(op, app.get_arg(1), depth));
            case fixity::Prefix:
                p_arg = pp_infix_child(op, app.get_arg(0), depth, fixity::Prefix);
                sz  = op.get_op_name().size();
                return mk_result(group(format{format(op.get_op_name()), nest(sz+1, format{line(), p_arg.first})}),
                                 p_arg.second + 1);
            case fixity::Postfix:
                p_arg = pp_mixfix_child(op, app.get_arg(0), depth);
                return mk_result(group(format{p_arg.first, space(), format(op.get_op_name())}),
                                 p_arg.second + 1);
            case fixity::Mixfixr: case fixity::Mixfixo: {
                // _ ID ... _ ID
                // _ ID ... _ ID _
                list<name> parts = op.get_op_name_parts();
                auto it = parts.begin();
                unsigned num = app.get_num_args();
                for (unsigned i = 0; i < num; i++) {
                    result p_arg = pp_mixfix_child(op, app.get_arg(i), depth);
                    if (i == num - 1) {
                        if (op.get_fixity() == fixity::Mixfixo)
                            r_format += p_arg.first;
                        else
                            r_format += format{p_arg.first, space(), format(*it)};
                    } else {
                        r_format += format{p_arg.first, space(), format(*it), line()};
                        ++it;
                    }
                    r_weight += p_arg.second;
                }
                return mk_result(group(r_format), r_weight);
            }
            case fixity::Mixfixl: case fixity::Mixfixc: {
                // ID _ ... _
                // ID _ ... _ ID
                list<name> parts = op.get_op_name_parts();
                auto it = parts.begin();
                unsigned num = app.get_num_args();
                for (unsigned i = 0; i < num; i++) {
                    result p_arg = pp_mixfix_child(op, app.get_arg(i), depth);
                    unsigned sz  = it->size();
                    if (i > 0) r_format += space();
                    r_format    += format{format(*it), nest(sz+1, format{line(), p_arg.first})};
                    r_weight    += p_arg.second;
                    ++it;
                }
                if (it != parts.end()) {
                    // it is Mixfixc
                    r_format += format{space(), format(*it)};
                }
                return mk_result(group(r_format), r_weight);
            }}
            lean_unreachable(); // LCOV_EXCL_LINE
        } else if (m_notation && is_exists_expr(e)) {
            return pp_exists(e, depth);
        } else {
            // standard function application
            expr const & f  = app.get_function();
            result p;
            bool is_const = is_constant(f) && !is_exists_fn(f);
            if (is_const)
                p = mk_result(format(const_name(f)), 1);
            else if (is_choice(f))
                p = pp_child(f, depth);
            else if (is_value(f))
                p = mk_result(to_value(f).pp(m_unicode, m_coercion), 1);
            else
                p = pp_child(f, depth);
            bool simple     = is_const && const_name(f).size() <= m_indent + 4;
            unsigned indent = simple ? const_name(f).size()+1 : m_indent;
            format   r_format = p.first;
            unsigned r_weight = p.second;
            unsigned num      = app.get_num_args();
            for (unsigned i = 0; i < num; i++) {
                result p_arg = pp_child(app.get_arg(i), depth);
                r_format += format{i == 0 && simple ? space() : line(), p_arg.first};
                r_weight += p_arg.second;
            }
            return mk_result(group(nest(indent, r_format)), r_weight);
        }
    }

    /**
       \brief Collect nested Lambdas (or Pis), and instantiate
       variables with unused names. Store in \c r the selected names
       and associated domains. Return the body of the sequence of
       Lambda (of Pis).

       \remark The argument B is only relevant when processing
       condensed definitions. \see pp_abstraction_core.
    */
    std::pair<expr, optional<expr>> collect_nested(expr const & e, optional<expr> T, expr_kind k, buffer<std::pair<name, expr>> & r) {
        if (e.kind() == k && (!T || is_abstraction(*T))) {
            name n1    = get_unused_name(e);
            m_local_names.insert(n1);
            r.emplace_back(n1, abst_domain(e));
            expr b = replace_var_with_name(abst_body(e), n1);
            if (T)
                T = some_expr(replace_var_with_name(abst_body(*T), n1));
            return collect_nested(b, T, k, r);
        } else {
            return mk_pair(e, T);
        }
    }

    result pp_scoped_child(expr const & e, unsigned depth, unsigned prec = 0) {
        if (is_atomic(e)) {
            return pp(e, depth + 1, true);
        } else {
            mk_scope s(*this, e);
            result r = pp(e, depth + 1, true);
            if (m_local_aliases_defs.size() == s.m_old_size) {
                if (prec <= get_operator_precedence(e))
                    return r;
                else
                    return mk_result(paren(r.first), r.second);
            } else {
                format r_format   = g_let_fmt;
                unsigned r_weight = 2;
                unsigned begin    = s.m_old_size;
                unsigned end      = m_local_aliases_defs.size();
                for (unsigned i = begin; i < end; i++) {
                    auto b = m_local_aliases_defs[i];
                    name const & n = b.first;
                    format beg = i == begin ? space() : line();
                    format sep = i < end - 1 ? comma() : format();
                    r_format += nest(3 + 1, format{beg, format(n), space(), g_assign_fmt, nest(n.size() + 1 + 2 + 1, format{space(), b.second, sep})});
                    // we do not store the alias definitin real weight. We know it is at least m_alias_min_weight
                    r_weight += m_alias_min_weight + 1;
                }
                r_format += format{line(), g_in_fmt, space(), nest(2 + 1, r.first)};
                r_weight += r.second;
                return mk_pair(group(r_format), r_weight);
            }
        }
    }

    result pp_arrow_child(expr const & e, unsigned depth) {
        return pp_scoped_child(e, depth, g_arrow_precedence + 1);
    }

    result pp_arrow_body(expr const & e, unsigned depth) {
        return pp_scoped_child(e, depth, g_arrow_precedence);
    }

    result pp_cartesian_child(expr const & e, unsigned depth) {
        return pp_scoped_child(e, depth, g_cartesian_product_precedence + 1);
    }

    result pp_cartesian_body(expr const & e, unsigned depth) {
        return pp_scoped_child(e, depth, g_cartesian_product_precedence);
    }

    template<typename It>
    format pp_bnames(It const & begin, It const & end, bool use_line) {
        auto it = begin;
        format r = format(it->first);
        ++it;
        for (; it != end; ++it) {
            r += compose(use_line ? line() : space(), format(it->first));
        }
        return r;
    }

    bool is_implicit(std::vector<bool> const * implicit_args, unsigned arg_pos) {
        return implicit_args && arg_pos < implicit_args->size() && (*implicit_args)[arg_pos];
    }

    /**
       \brief Auxiliary method for computing where Pi can be pretty printed as an arrow.
       Examples:
       Pi x : Int, Pi y : Int, Int        ===> return 0
       Pi A : Type, Pi x : A, Pi y : A, A ===> return 1
       Pi A : Type, Pi x : Int, A         ===> return 1
       Pi A : Type, Pi x : Int, x > 0     ===> return UINT_MAX (there is no tail that can be printed as a arrow)

       If \c e is not Pi, it returns UINT_MAX
    */
    unsigned get_arrow_starting_at(expr e) {
        if (!is_pi(e))
            return std::numeric_limits<unsigned>::max();
        unsigned pos = 0;
        while (is_pi(e)) {
            expr e2 = abst_body(e);
            unsigned num_vars = 1;
            bool ok = true;
            while (true) {
                if (has_free_var(e2, 0, num_vars)) {
                    ok = false;
                    break;
                }
                if (is_pi(e2)) {
                    e2 = abst_body(e2);
                } else {
                    break;
                }
            }
            if (ok) {
                return pos;
            }
            e = abst_body(e);
            pos++;
        }
        return std::numeric_limits<unsigned>::max();
    }

    /**
       \brief Pretty print Lambdas, Pis and compact definitions.
       When T != 0, it is a compact definition.
       A compact definition is of the form

       Defintion Name Pi(x : A), B := Lambda (x : A), C

       it is printed as

       Defintion Name (x : A) : B := C

       This method only generates the
          (x : A) : B := C
       for compact definitions.

       \remark if T != 0, then T is Pi(x : A), B
    */
    result pp_abstraction_core(expr const & e, unsigned depth, optional<expr> T,
                               std::vector<bool> const * implicit_args = nullptr) {
        if (is_arrow(e) && !implicit_args) {
            lean_assert(!T);
            result p_lhs    = pp_arrow_child(abst_domain(e), depth);
            result p_rhs    = pp_arrow_body(abst_body(e), depth);
            format r_format = group(format{p_lhs.first, space(), m_unicode ? g_arrow_n_fmt : g_arrow_fmt, line(), p_rhs.first});
            return mk_result(r_format, p_lhs.second + p_rhs.second + 1);
        } else if (is_cartesian(e) && !implicit_args) {
            lean_assert(!T);
            result p_lhs    = pp_cartesian_child(abst_domain(e), depth);
            result p_rhs    = pp_cartesian_body(abst_body(e), depth);
            format r_format = group(format{p_lhs.first, space(), m_unicode ? g_cartesian_product_n_fmt : g_cartesian_product_fmt,
                        line(), p_rhs.first});
            return mk_result(r_format, p_lhs.second + p_rhs.second + 1);
        } else {
            unsigned arrow_starting_at = get_arrow_starting_at(e);
            buffer<std::pair<name, expr>> nested;
            auto p = collect_nested(e, T, e.kind(), nested);
            expr b = p.first;
            T = p.second;
            unsigned head_indent = m_indent;
            format head;
            if (!T && !implicit_args) {
                if (m_unicode) {
                    head = is_lambda(e) ? g_lambda_n_fmt : (is_pi(e) ? g_Pi_n_fmt : g_sig_fmt);
                    head_indent = is_sigma(e) ? 4 : 2;
                } else {
                    head = is_lambda(e) ? g_lambda_fmt : (is_pi(e) ? g_Pi_fmt : g_sig_fmt);
                    head_indent = is_pi(e) ? 3 : 4;
                }
            }
            format body_sep;
            if (T) {
                format T_f = pp_scoped_child(*T, 0).first;
                body_sep = format{space(), colon(), space(), T_f, space(), g_assign_fmt};
            } else if (implicit_args) {
                // This is a little hack to pretty print Variable and
                // Axiom declarations that contain implicit arguments
                body_sep = compose(space(), colon());
            } else {
                body_sep = comma();
            }
            if (!nested.empty() &&
                std::all_of(nested.begin() + 1, nested.end(), [&](std::pair<name, expr> const & p) {
                        return p.second == nested[0].second; }) &&
                !implicit_args) {
                // Domain of all binders is the same
                expr domain0    = nested[0].second;
                format names    = pp_bnames(nested.begin(), nested.end(), false);
                result p_domain = pp_scoped_child(domain0, depth);
                result p_body   = pp_scoped_child(b, depth);
                format sig      = format{names, space(), colon(), space(), p_domain.first};
                if (T)
                    sig = format{lp(), sig, rp()};
                format r_format = group(nest(head_indent, format{head, space(), sig, body_sep, line(), p_body.first}));
                return mk_result(r_format, p_domain.second + p_body.second + 1);
            } else {
                auto it  = nested.begin();
                auto end = nested.end();
                unsigned r_weight = 1;
                unsigned arg_pos  = 0;
                // PP binders in blocks (names ... : type)
                bool first = true;
                format bindings;
                while (it != end) {
                    auto it2 = it;
                    ++it2;
                    bool implicit = is_implicit(implicit_args, arg_pos);
                    ++arg_pos;
                    if (!implicit_args && arg_pos > arrow_starting_at) {
                        // The rest is an arrow
                        // We do not use arrow pp when implicit_args marks are used.
                        format block;
                        bool first_domain = true;
                        for (; it != end; ++it) {
                            result p_domain = pp_arrow_child(it->second, depth);
                            r_weight += p_domain.second;
                            if (first_domain) {
                                first_domain = false;
                                block = p_domain.first;
                            } else {
                                block += format{space(), m_unicode ? g_arrow_n_fmt : g_arrow_fmt, line(), p_domain.first};
                            }
                        }
                        result p_body = pp_arrow_child(b, depth);
                        r_weight += p_body.second;
                        block += format{space(), m_unicode ? g_arrow_n_fmt : g_arrow_fmt, line(), p_body.first};
                        block = group(block);
                        format r_format = group(nest(head_indent, format{head, space(), group(bindings), body_sep, line(), block}));
                        return mk_result(r_format, r_weight);
                    }
                    // Continue with standard encoding
                    while (it2 != end && it2->second == it->second && implicit == is_implicit(implicit_args, arg_pos)) {
                        ++it2;
                        ++arg_pos;
                    }
                    result p_domain = pp_scoped_child(it->second, depth);
                    r_weight += p_domain.second;
                    format const & par_open  = implicit ? lcurly() : lp();
                    format const & par_close = implicit ? rcurly() : rp();
                    format block = group(nest(m_indent, format{par_open, pp_bnames(it, it2, true), space(), colon(), space(), p_domain.first, par_close}));
                    if (first) {
                        bindings = block;
                        first = false;
                    } else {
                        bindings += compose(line(), block);
                    }
                    it = it2;
                }
                result p_body   = pp_scoped_child(b, depth);
                format r_format = group(nest(head_indent, format{head, space(), group(bindings), body_sep, line(), p_body.first}));
                return mk_result(r_format, r_weight + p_body.second);
            }
        }
    }

    result pp_abstraction(expr const & e, unsigned depth) {
        return pp_abstraction_core(e, depth, none_expr());
    }

    expr collect_nested_let(expr const & e, buffer<std::tuple<name, optional<expr>, expr>> & bindings) {
        if (is_let(e)) {
            name n1    = get_unused_name(e);
            m_local_names.insert(n1);
            bindings.emplace_back(n1, let_type(e), let_value(e));
            expr b = replace_var_with_name(let_body(e), n1);
            return collect_nested_let(b, bindings);
        } else {
            return e;
        }
    }

    result pp_let(expr const & e, unsigned depth) {
        buffer<std::tuple<name, optional<expr>, expr>> bindings;
        expr body = collect_nested_let(e, bindings);
        unsigned r_weight = 2;
        format r_format = g_let_fmt;
        unsigned sz = bindings.size();
        for (unsigned i = 0; i < sz; i++) {
            auto b = bindings[i];
            name const & n = std::get<0>(b);
            format beg = i == 0 ? space() : line();
            format sep = i < sz - 1 ? comma() : format();
            result p_def = pp_scoped_child(std::get<2>(b), depth+1);
            optional<expr> const & type = std::get<1>(b);
            if (type) {
                result p_type = pp_scoped_child(*type, depth+1);
                r_format += nest(3 + 1, compose(beg, group(format{format(n), space(),
                                    colon(), nest(n.size() + 1 + 1 + 1, compose(space(), p_type.first)), space(), g_assign_fmt,
                                    nest(m_indent, format{line(), p_def.first, sep})})));
                r_weight += p_type.second + p_def.second;
            } else {
                r_format += nest(3 + 1, format{beg, format(n), space(), g_assign_fmt, nest(n.size() + 1 + 2 + 1, format{space(), p_def.first, sep})});
                r_weight += p_def.second;
            }
        }
        result p_body = pp_scoped_child(body, depth+1);
        r_weight += p_body.second;
        r_format += format{line(), g_in_fmt, space(), nest(2 + 1, p_body.first)};
        return mk_pair(group(r_format), r_weight);
    }

    result pp_choice(expr const & e, unsigned depth) {
        lean_assert(is_choice(e));
        unsigned num = get_num_choices(e);
        format r_format;
        unsigned r_weight = 0;
        for (unsigned i = 0; i < num; i++) {
            if (i > 0)
                r_format += format{space(), format("|"), line()};
            expr const & c = get_choice(e, i);
            result p_c     = pp_child(c, depth);
            r_weight      += p_c.second;
            r_format      += p_c.first;
        }
        return mk_result(r_format, r_weight+1);
    }

    result pp_metavar(expr const & a, unsigned depth) {
        format mv_fmt = compose(format("?"), format(metavar_name(a)));
        if (metavar_lctx(a)) {
            format ctx_fmt;
            bool first = true;
            unsigned r_weight = 1;
            for (local_entry const & e : metavar_lctx(a)) {
                format e_fmt;
                if (e.is_lift()) {
                    e_fmt = format{g_lift_fmt, colon(), format(e.s()), space(), format(e.n())};
                } else {
                    lean_assert(e.is_inst());
                    auto p_e = pp_child_with_paren(e.v(), depth);
                    r_weight += p_e.second;
                    e_fmt = format{g_inst_fmt, colon(), format(e.s()), space(), nest(m_indent, p_e.first)};
                }
                if (first) {
                    ctx_fmt = e_fmt;
                    first = false;
                } else {
                    ctx_fmt += format{comma(), line(), e_fmt};
                }
            }
            return mk_result(group(compose(mv_fmt, nest(m_indent, format{lsb(), ctx_fmt, rsb()}))), r_weight);
        } else {
            return mk_result(mv_fmt, 1);
        }
    }

    result pp_pair(expr a, unsigned depth) {
        buffer<expr> args;
        unsigned indent   = 5;
        format r_format   = g_pair_fmt;
        unsigned r_weight = 1;

        auto f_r = pp_child(pair_first(a), depth);
        auto s_r = pp_child(pair_second(a), depth);
        r_format += nest(indent, compose(line(), f_r.first));
        r_format += nest(indent, compose(line(), s_r.first));
        r_weight += f_r.second + s_r.second;

        expr t = pair_type(a);
        if (!is_cartesian(t)) {
            auto t_r = pp_child(t, depth);
            r_format += nest(indent, compose(line(), format{colon(), space(), t_r.first}));
            r_weight += t_r.second;
        }
        return result(group(r_format), r_weight);
    }

    result pp_proj(expr a, unsigned depth) {
        unsigned i = 0;
        bool first = proj_first(a);
        while (is_proj(proj_arg(a)) && !proj_first(proj_arg(a))) {
            a = proj_arg(a);
            i++;
        }
        auto arg_r = pp_child(proj_arg(a), depth);
        unsigned indent   = 6;
        format r_format   = first ? g_proj1_fmt : g_proj2_fmt;
        unsigned r_weight = 1 + arg_r.second;;
        if (i > 0)
            r_format += format{space(), format(i)};
        r_format += nest(indent, compose(line(), arg_r.first));
        return result(group(r_format), r_weight);
    }

    result pp_heq(expr const & a, unsigned depth) {
        result p_lhs    = pp_child(heq_lhs(a), depth);
        result p_rhs    = pp_child(heq_rhs(a), depth);
        format r_format = group(format{p_lhs.first, space(), g_heq_fmt, line(), p_rhs.first});
        return mk_result(r_format, p_lhs.second + p_rhs.second + 1);
    }

    result pp(expr const & e, unsigned depth, bool main = false) {
        check_system("pretty printer");
        if (!is_atomic(e) && (m_num_steps > m_max_steps || depth > m_max_depth)) {
            return pp_ellipsis();
        } else {
            m_num_steps++;
            if (auto aliased_list = get_aliased(m_env, e)) {
                if (m_unicode) {
                    return mk_result(format(head(*aliased_list)), 1);
                } else {
                    for (auto n : *aliased_list) {
                        if (n.is_safe_ascii())
                            return mk_result(format(n), 1);
                    }
                }
            }
            if (m_extra_lets && has_several_occs(e)) {
                auto it = m_local_aliases.find(e);
                if (it != m_local_aliases.end())
                    return mk_result(format(it->second), 1);
            }
            result r;
            if (is_choice(e)) {
                return pp_choice(e, depth);
            } else {
                switch (e.kind()) {
                case expr_kind::Var:        r = pp_var(e);                break;
                case expr_kind::Constant:   r = pp_constant(e);           break;
                case expr_kind::Value:      r = pp_value(e);              break;
                case expr_kind::App:        r = pp_app(e, depth);         break;
                case expr_kind::Lambda:
                case expr_kind::Sigma:
                case expr_kind::Pi:         r = pp_abstraction(e, depth); break;
                case expr_kind::Type:       r = pp_type(e);               break;
                case expr_kind::Let:        r = pp_let(e, depth);         break;
                case expr_kind::MetaVar:    r = pp_metavar(e, depth);     break;
                case expr_kind::HEq:        r = pp_heq(e, depth);         break;
                case expr_kind::Pair:       r = pp_pair(e, depth);        break;
                case expr_kind::Proj:       r = pp_proj(e, depth);        break;
                }
            }
            if (!main && m_extra_lets && has_several_occs(e) && r.second > m_alias_min_weight) {
                name new_aux = name(m_aux, m_local_aliases_defs.size()+1);
                m_local_aliases.insert(e, new_aux);
                m_local_aliases_defs.emplace_back(new_aux, r.first);
                return mk_result(format(new_aux), 1);
            }
            return r;
        }
    }

    void set_options(options const & opts) {
        m_indent           = get_pp_indent(opts);
        m_max_depth        = get_pp_max_depth(opts);
        m_max_steps        = get_pp_max_steps(opts);
        m_implict          = get_pp_implicit(opts);
        m_unicode          = get_pp_unicode(opts);
        m_coercion         = get_pp_coercion(opts);
        m_notation         = get_pp_notation(opts);
        m_extra_lets       = get_pp_extra_lets(opts);
        m_alias_min_weight = get_pp_alias_min_weight(opts);
    }

    bool uses_prefix(expr const & e, name const & prefix) {
        return static_cast<bool>(find(e, [&](expr const & e) {
                    return
                        (is_constant(e) && is_prefix_of(prefix, const_name(e)))   ||
                        (is_abstraction(e) && is_prefix_of(prefix, abst_name(e))) ||
                        (is_let(e) && is_prefix_of(prefix, let_name(e)));
                }));
    }

    name find_unused_prefix(expr const & e) {
        if (!uses_prefix(e, g_a)) {
            return g_a;
        } else if (!uses_prefix(e, g_b)) {
            return g_b;
        } else {
            unsigned i = 1;
            name n(g_c, i);
            while (uses_prefix(e, n)) {
                i++;
                n = name(g_c, i);
            }
            return n;
        }
    }

    void init(expr const & e) {
        m_local_aliases.clear();
        m_local_aliases_defs.clear();
        m_num_steps = 0;
        m_aux = find_unused_prefix(e);
    }

public:
    pp_fn(ro_environment const & env, options const & opts):
        m_env(env) {
        set_options(opts);
        m_num_steps   = 0;
    }

    format operator()(expr const & e) {
        init(e);
        return pp_scoped_child(e, 0).first;
    }

    format pp_definition(expr const & v, expr const & t, std::vector<bool> const * implicit_args) {
        init(mk_app(v, t));
        return pp_abstraction_core(v, 0, some_expr(t), implicit_args).first;
    }

    format pp_pi_with_implicit_args(expr const & e, std::vector<bool> const & implicit_args) {
        init(e);
        return pp_abstraction_core(e, 0, none_expr(), &implicit_args).first;
    }

    void register_local(name const & n) {
        m_local_names.insert(n);
    }
};

class pp_formatter_cell : public formatter_cell {
    ro_environment m_env;

    format pp(expr const & e, options const & opts) {
        pp_fn fn(m_env, opts);
        return fn(e);
    }

    format pp(context const & c, expr const & e, bool include_e, options const & opts) {
        pp_fn fn(m_env, opts);
        unsigned indent = get_pp_indent(opts);
        format r;
        bool first = true;
        expr c2   = context_to_lambda(c, e);
        while (is_fake_context(c2)) {
            check_interrupted();
            name n1 = get_unused_name(c2);
            fn.register_local(n1);
            format entry = format(n1);
            optional<expr> domain = fake_context_domain(c2);
            optional<expr> val    = fake_context_value(c2);
            if (domain)
                entry += format{space(), colon(), space(), fn(*domain)};
            if (val)
                entry += format{space(), g_assign_fmt, nest(indent, format{line(), fn(*val)})};
            if (first) {
                r = group(entry);
                first = false;
            } else {
                r += format{comma(), line(), group(entry)};
            }
            c2 = replace_var_with_name(fake_context_rest(c2), n1);
        }
        if (include_e) {
            if (!first) {
                bool unicode     = get_pp_unicode(opts);
                format turnstile = unicode ? format("\u22A2") /* ⊢ */ : format("|-");
                r += format{line(), turnstile, space(), fn(c2)};
            } else {
                r = fn(c2);
            }
            return group(r);
        } else {
            return group(r);
        }
    }

    format pp_definition(char const * kwd, name const & n, expr const & t, expr const & v, options const & opts) {
        unsigned indent = get_pp_indent(opts);
        bool def_value  = get_pp_def_value(opts);
        format def_fmt;
        if (def_value) {
            def_fmt = format{highlight_command(format(kwd)), space(), format(n), space(), colon(), space(),
                             pp(t, opts), space(), g_assign_fmt, line(), pp(v, opts)};
        } else {
            // suppress the actual definition
            def_fmt = format{highlight_command(format(kwd)), space(), format(n), space(), colon(), space(), pp(t, opts)};
        }
        return group(nest(indent, def_fmt));
    }

    format pp_compact_definition(char const * kwd, name const & n, expr const & t, expr const & v, options const & opts) {
        expr it1 = t;
        expr it2 = v;
        while (is_pi(it1) && is_lambda(it2)) {
            check_interrupted();
            if (abst_domain(it1) != abst_domain(it2))
                return pp_definition(kwd, n, t, v, opts);
            it1 = abst_body(it1);
            it2 = abst_body(it2);
        }
        if (!is_lambda(v) || is_pi(it1)) {
            return pp_definition(kwd, n, t, v, opts);
        } else {
            lean_assert(is_lambda(v));
            std::vector<bool> const * implicit_args = nullptr;
            if (has_implicit_arguments(m_env, n))
                implicit_args = &(get_implicit_arguments(m_env, n));
            pp_fn fn(m_env, opts);
            format def_fmt;
            bool def_value  = get_pp_def_value(opts);
            if (def_value)
                def_fmt = fn.pp_definition(v, t, implicit_args);
            else if (implicit_args)
                def_fmt = fn.pp_pi_with_implicit_args(t, *implicit_args);
            else
                def_fmt = format{space(), colon(), space(), pp(t, opts)};
            return format{highlight_command(format(kwd)), space(), format(n), def_fmt};
        }
    }

    format pp_uvar_cnstr(object const & obj, options const & opts) {
        bool unicode = get_pp_unicode(opts);
        return format{highlight_command(format(obj.keyword())), space(), format(obj.get_name()), space(), format(unicode ? "\u2265" : ">="), space(), ::lean::pp(obj.get_cnstr_level(), unicode)};
    }

    format pp_postulate(object const & obj, options const & opts) {
        char const * kwd = obj.keyword();
        name const & n = obj.get_name();
        format r = format{highlight_command(format(kwd)), space(), format(n)};
        if (has_implicit_arguments(m_env, n)) {
            pp_fn fn(m_env, opts);
            r += fn.pp_pi_with_implicit_args(obj.get_type(), get_implicit_arguments(m_env, n));
        } else {
            r += format{space(), colon(), space(), pp(obj.get_type(), opts)};
        }
        return r;
    }

    format pp_builtin_set(object const & obj, options const &) {
        char const * kwd = obj.keyword();
        name const & n = obj.get_name();
        return format{highlight_command(format(kwd)), space(), format(n)};
    }

    format pp_definition(object const & obj, options const & opts) {
        if (is_explicit(m_env, obj.get_name())) {
            // Hide implicit arguments when pretty printing the
            // explicit version on an object.
            // We do that because otherwise it looks like a recursive definition.
            options new_opts = update(opts, g_pp_implicit, false);
            return pp_compact_definition(obj.keyword(), obj.get_name(), obj.get_type(), obj.get_value(), new_opts);
        } else {
            return pp_compact_definition(obj.keyword(), obj.get_name(), obj.get_type(), obj.get_value(), opts);
        }
    }

    format pp_notation_decl(object const & obj, options const & opts) {
        notation_declaration const & n = *static_cast<notation_declaration const *>(obj.cell());
        expr const & d = n.get_expr();
        format d_fmt   = is_constant(d) ? format(const_name(d)) : pp(d, opts);
        return format{::lean::pp(n.get_op()), space(), colon(), space(), d_fmt};
    }

    format pp_coercion_decl(object const & obj, options const & opts) {
        unsigned indent = get_pp_indent(opts);
        coercion_declaration const & n = *static_cast<coercion_declaration const *>(obj.cell());
        expr const & c = n.get_coercion();
        return group(format{highlight_command(format(n.keyword())), nest(indent, format({line(), pp(c, opts)}))});
    }

    format pp_alias_decl(object const & obj, options const & opts) {
        alias_declaration const & alias_decl = *static_cast<alias_declaration const *>(obj.cell());
        name const & n = alias_decl.get_alias_name();
        expr const & d = alias_decl.get_expr();
        format d_fmt   = is_constant(d) ? format(const_name(d)) : pp(d, opts);
        return format{highlight_command(format(alias_decl.keyword())), space(), ::lean::pp(n), space(), colon(), space(), d_fmt};
    }

    format pp_set_opaque(object const & obj) {
        return format{highlight_command(format(obj.keyword())), space(), format(get_set_opaque_id(obj)), space(),
                format(get_set_opaque_flag(obj) ? "true" : "false")};
    }
public:
    pp_formatter_cell(ro_environment const & env):
        m_env(env) {
    }

    virtual format operator()(expr const & e, options const & opts) {
        return pp(e, opts);
    }

    virtual format operator()(context const & c, options const & opts) {
        return pp(c, Type(), false, opts);
    }

    virtual format operator()(context const & c, expr const & e, bool format_ctx, options const & opts) {
        if (format_ctx) {
            return pp(c, e, true, opts);
        } else {
            pp_fn fn(m_env, opts);
            expr c2   = context_to_lambda(c, e);
            while (is_fake_context(c2)) {
                check_interrupted();
                name n1 = get_unused_name(c2);
                fn.register_local(n1);
                expr const & rest = fake_context_rest(c2);
                c2 = replace_var_with_name(rest, n1);
            }
            return fn(c2);
        }
    }

    virtual format operator()(object const & obj, options const & opts) {
        switch (obj.kind()) {
        case object_kind::UVarConstraint:   return pp_uvar_cnstr(obj, opts);
        case object_kind::Postulate:        return pp_postulate(obj, opts);
        case object_kind::Definition:       return pp_definition(obj, opts);
        case object_kind::Builtin:          return pp_postulate(obj, opts);
        case object_kind::BuiltinSet:       return pp_builtin_set(obj, opts);
        case object_kind::Neutral:
            if (is_notation_decl(obj)) {
                return pp_notation_decl(obj, opts);
            } else if (is_coercion_decl(obj)) {
                return pp_coercion_decl(obj, opts);
            } else if (is_alias_decl(obj)) {
                return pp_alias_decl(obj, opts);
            } else if (is_set_opaque(obj)) {
                return pp_set_opaque(obj);
            } else {
                // If the object is not notation or coercion
                // declaration, then the object was created in
                // different frontend, and we ignore it.
                return format("Unknown neutral object");
            }
        }
        lean_unreachable(); // LCOV_EXCL_LINE
    }

    virtual format operator()(ro_environment const & env, options const & opts) {
        format r;
        bool first = true;
        auto it  = env->begin_objects();
        auto end = env->end_objects();
        for (; it != end; ++it) {
            check_interrupted();
            auto obj = *it;
            if (supported_by_pp(obj)) {
                if (first) first = false; else r += line();
                r += operator()(obj, opts);
            }
        }
        return r;
    }

    virtual optional<ro_environment> get_environment() const { return optional<ro_environment>(m_env); }
};

formatter mk_pp_formatter(ro_environment const & env) {
    return mk_formatter(pp_formatter_cell(env));
}
}
