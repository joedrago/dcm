#ifndef DCMLUASTATE_H
#define DCMLUASTATE_H

typedef struct lua_State lua_State;

typedef struct dcmLuaState
{
    lua_State *L;
    char *error;
} dcmLuaState;

dcmLuaState *dcmLuaStateCreate();
void dcmLuaStateDestroy(dcmLuaState *state);
int dcmLuaStateError(dcmLuaState *state, const char *error);
int dcmLuaStateLoadScript(dcmLuaState *state, const char *name, const char *script, int len);

#endif
