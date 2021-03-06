/*
Copyright (c) 2013 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include <algorithm>
#include <limits>
#include "kernel/free_vars.h"
#include "kernel/replace_fn.h"
#include "kernel/metavar.h"
#include "kernel/instantiate.h"

namespace lean {
template<bool ClosedSubst>
expr instantiate_core(expr const & a, unsigned s, unsigned n, expr const * subst, optional<ro_metavar_env> const & menv) {
    return replace(a, [=](expr const & m, unsigned offset) -> expr {
            if (is_var(m)) {
                unsigned vidx = var_idx(m);
                if (vidx >= offset + s) {
                    if (vidx < offset + s + n) {
                        if (ClosedSubst)
                            return subst[n - (vidx - s - offset) - 1];
                        else
                            return lift_free_vars(subst[n - (vidx - s - offset) - 1], offset, menv);
                    } else {
                        return mk_var(vidx - n);
                    }
                } else {
                    return m;
                }
            } else if (is_metavar(m)) {
                expr r = m;
                for (unsigned i = 0; i < n; i++) {
                    expr v = ClosedSubst ? subst[i] : lift_free_vars(subst[i], offset + n - i - 1, menv);
                    r = add_inst(r, offset + s + n - i - 1, v, menv);
                }
                return r;
            } else {
                return m;
            }
        });
}

expr instantiate_with_closed(expr const & a, unsigned n, expr const * s, optional<ro_metavar_env> const & menv) {
    lean_assert(std::all_of(s, s+n, [&](expr const & e) { return !has_free_var(e, 0, std::numeric_limits<unsigned>::max(), menv); }));
    return instantiate_core<true>(a, 0, n, s, menv);
}
expr instantiate_with_closed(expr const & e, unsigned n, expr const * s, ro_metavar_env const & menv) { return instantiate_with_closed(e, n, s, some_ro_menv(menv)); }
expr instantiate_with_closed(expr const & e, unsigned n, expr const * s) { return instantiate_with_closed(e, n, s, none_ro_menv()); }
expr instantiate_with_closed(expr const & e, std::initializer_list<expr> const & l) { return instantiate_with_closed(e, l.size(), l.begin()); }
expr instantiate_with_closed(expr const & e, expr const & s, optional<ro_metavar_env> const & menv) { return instantiate_with_closed(e, 1, &s, menv); }
expr instantiate_with_closed(expr const & e, expr const & s) { return instantiate_with_closed(e, 1, &s); }
expr instantiate_with_closed(expr const & e, expr const & s, ro_metavar_env const & menv) { return instantiate_with_closed(e, s, some_ro_menv(menv)); }

expr instantiate(expr const & a, unsigned s, unsigned n, expr const * subst, optional<ro_metavar_env> const & menv) {
    return instantiate_core<false>(a, s, n, subst, menv);
}
expr instantiate(expr const & e, unsigned n, expr const * s, optional<ro_metavar_env> const & menv) { return instantiate(e, 0, n, s, menv); }
expr instantiate(expr const & e, unsigned n, expr const * s, ro_metavar_env const & menv) { return instantiate(e, n, s, some_ro_menv(menv)); }
expr instantiate(expr const & e, unsigned n, expr const * s) { return instantiate(e, n, s, none_ro_menv()); }
expr instantiate(expr const & e, std::initializer_list<expr> const & l) {  return instantiate(e, l.size(), l.begin()); }
expr instantiate(expr const & e, unsigned i, expr const & s, optional<ro_metavar_env> const & menv) { return instantiate(e, i, 1, &s, menv); }
expr instantiate(expr const & e, unsigned i, expr const & s, ro_metavar_env const & menv) { return instantiate(e, i, 1, &s, some_ro_menv(menv)); }
expr instantiate(expr const & e, unsigned i, expr const & s) { return instantiate(e, i, 1, &s, none_ro_menv()); }
expr instantiate(expr const & e, expr const & s, optional<ro_metavar_env> const & menv) { return instantiate(e, 1, &s, menv); }
expr instantiate(expr const & e, expr const & s, ro_metavar_env const & menv) { return instantiate(e, s, some_ro_menv(menv)); }
expr instantiate(expr const & e, expr const & s) { return instantiate(e, s, none_ro_menv()); }

bool is_head_beta(expr const & t) {
    return is_app(t) && is_lambda(arg(t, 0));
}

expr apply_beta(expr f, unsigned num_args, expr const * args, optional<ro_metavar_env> const & menv) {
    if (!is_lambda(f)) {
        buffer<expr> new_args;
        new_args.push_back(f);
        new_args.append(num_args, args);
        return mk_app(new_args);
    } else {
        unsigned m = 1;
        while (is_lambda(abst_body(f)) && m < num_args) {
            f = abst_body(f);
            m++;
        }
        lean_assert(m <= num_args);
        expr r = instantiate(abst_body(f), m, args, menv);
        if (m == num_args) {
            return r;
        } else {
            buffer<expr> new_args;
            new_args.push_back(r);
            for (; m < num_args; m++)
                new_args.push_back(args[m]);
            return mk_app(new_args);
        }
    }
}
expr apply_beta(expr f, unsigned num_args, expr const * args, ro_metavar_env const & menv) { return apply_beta(f, num_args, args, some_ro_menv(menv)); }
expr apply_beta(expr f, unsigned num_args, expr const * args) { return apply_beta(f, num_args, args, none_ro_menv()); }

expr head_beta_reduce(expr const & t, optional<ro_metavar_env> const & menv) {
    if (!is_head_beta(t)) {
        return t;
    } else {
        return apply_beta(arg(t, 0), num_args(t) - 1, &arg(t, 1), menv);
    }
}
expr head_beta_reduce(expr const & t) { return head_beta_reduce(t, none_ro_menv()); }
expr head_beta_reduce(expr const & t, ro_metavar_env const & menv) { return head_beta_reduce(t, some_ro_menv(menv)); }

expr beta_reduce(expr t, optional<ro_metavar_env> const & menv) {
    auto f = [=](expr const & m, unsigned) -> expr {
        if (is_head_beta(m))
            return head_beta_reduce(m, menv);
        else
            return m;
    };
    while (true) {
        expr new_t = replace_fn<decltype(f)>(f)(t);
        if (new_t == t)
            return new_t;
        else
            t = new_t;
    }
}
expr beta_reduce(expr t, ro_metavar_env const & menv) { return beta_reduce(t, some_ro_menv(menv)); }
expr beta_reduce(expr t) { return beta_reduce(t, none_ro_menv()); }
}
