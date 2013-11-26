/*
Copyright (c) 2013 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include <sstream>
#include <lua.hpp>
#include "library/tactic/proof_builder.h"
#include "bindings/lua/util.h"
#include "bindings/lua/name.h"
#include "bindings/lua/expr.h"
#include "bindings/lua/metavar_env.h"
#include "bindings/lua/lref.h"

namespace lean {
DECL_UDATA(proof_map)

static int mk_proof_map(lua_State * L) {
    return push_proof_map(L, proof_map());
}

static int proof_map_len(lua_State * L) {
    lua_pushinteger(L, to_proof_map(L, 1).size());
    return 1;
}

static int proof_map_find(lua_State * L) {
    return push_expr(L, find(to_proof_map(L, 1), to_name_ext(L, 2)));
}

static int proof_map_insert(lua_State * L) {
    to_proof_map(L, 1).insert(to_name_ext(L, 2), to_expr(L, 3));
    return 0;
}

static int proof_map_erase(lua_State * L) {
    to_proof_map(L, 1).erase(to_name_ext(L, 2));
    return 0;
}

static const struct luaL_Reg proof_map_m[] = {
    {"__gc",            proof_map_gc}, // never throws
    {"__len",           safe_function<proof_map_len>},
    {"size",            safe_function<proof_map_len>},
    {"find",            safe_function<proof_map_find>},
    {"insert",          safe_function<proof_map_insert>},
    {"erase",           safe_function<proof_map_erase>},
    {0, 0}
};

DECL_UDATA(assignment);

static int mk_assignment(lua_State * L) {
    int nargs = lua_gettop(L);
    if (nargs == 0)
        return push_assignment(L, assignment(metavar_env()));
    else
        return push_assignment(L, assignment(to_metavar_env(L, 1)));
}

static int assignment_call(lua_State * L) {
    return push_expr(L, to_assignment(L, 1)(to_name_ext(L, 2)));
}

static const struct luaL_Reg assignment_m[] = {
    {"__gc",            assignment_gc}, // never throws
    {"__call",          safe_function<assignment_call>},
    {0, 0}
};

DECL_UDATA(proof_builder);

static int mk_proof_builder(lua_State * L) {
    luaL_checktype(L, 1, LUA_TFUNCTION); // user-fun
    lref ref(L, 1);
    return push_proof_builder(L,
                              mk_proof_builder([=](proof_map const & m, assignment const & a) -> expr {
                                      ref.push(); // push user-fun on the stack
                                      push_proof_map(L, m);
                                      push_assignment(L, a);
                                      pcall(L, 2, 1, 0);
                                      expr r = to_expr(L, -1);
                                      lua_pop(L, 1);
                                      return r;
                                  }));
}

static int proof_builder_call(lua_State * L) {
    return push_expr(L, to_proof_builder(L, 1)(to_proof_map(L, 2), to_assignment(L, 3)));
}

static const struct luaL_Reg proof_builder_m[] = {
    {"__gc",            proof_builder_gc}, // never throws
    {"__call",          safe_function<proof_builder_call>},
    {0, 0}
};

void open_proof_builder(lua_State * L) {
    luaL_newmetatable(L, proof_map_mt);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    setfuncs(L, proof_map_m, 0);

    SET_GLOBAL_FUN(proof_map_pred, "is_proof_map");
    SET_GLOBAL_FUN(mk_proof_map, "proof_map");

    luaL_newmetatable(L, assignment_mt);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    setfuncs(L, assignment_m, 0);

    SET_GLOBAL_FUN(assignment_pred, "is_assignment");
    SET_GLOBAL_FUN(mk_assignment, "assignment");

    luaL_newmetatable(L, proof_builder_mt);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    setfuncs(L, proof_builder_m, 0);

    SET_GLOBAL_FUN(proof_builder_pred, "is_proof_builder");
    SET_GLOBAL_FUN(mk_proof_builder, "proof_builder");
}
}