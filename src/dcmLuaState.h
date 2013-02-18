#ifndef DCMLUASTATE_H
#define DCMLUASTATE_H

struct dcmContext;
struct lua_State;

typedef struct dcmLuaState
{
    struct lua_State *L;
    struct dcmContext *context;
    char *error;
} dcmLuaState;

dcmLuaState *dcmLuaStateCreate(struct dcmContext *context);
void dcmLuaStateDestroy(dcmLuaState *state);
int dcmLuaStateError(dcmLuaState *state, const char *error);
int dcmLuaStateLoadScript(dcmLuaState *state, const char *name, const char *script, int len);
int dcmLuaStateAddSubdir(dcmLuaState *state, const char *dir);

#endif
