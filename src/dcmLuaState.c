#include "dcmLuaState.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include <stdlib.h>
#include <string.h>

dcmLuaState *dcmLuaStateCreate()
{
    dcmLuaState *state = calloc(1, sizeof(dcmLuaState));
    state->L = luaL_newstate();
    return state;
}

void dcmLuaStateDestroy(dcmLuaState *state)
{
    if(state->error)
    {
        free(state->error);
    }
    lua_close(state->L);
    free(state);
}

int dcmLuaStateError(dcmLuaState *state, const char *error)
{
    if(state->error)
    {
        free(state->error);
        state->error = NULL;
    }
    if(error)
    {
        state->error = strdup(error);
    }
}

struct dcmScriptInfo
{
    const char *script;
    int len;
};

static const char *dcmLoadScriptReader(lua_State *L, void *data, size_t *size)
{
    struct dcmScriptInfo *info = (struct dcmScriptInfo *)data;
    if(info->script && info->len)
    {
        *size = info->len;
        info->len = 0;
        return info->script;
    }
    return NULL;
}

int dcmLuaStateLoadScript(dcmLuaState *state, const char *name, const char *script, int len)
{
    struct dcmScriptInfo info;
    info.script = script;
    info.len = len;
    int err = lua_load(state->L, dcmLoadScriptReader, &info, name, "t");
    if(err == LUA_OK)
    {
        err = lua_pcall(state->L, 0, LUA_MULTRET, 0);
        if(err == LUA_OK)
        {
            return 1;
        }
        else
        {
            // failed to run chunk
            dcmLuaStateError(state, lua_tostring(state->L, -1));
            lua_pop(state->L, 1);
        }
    }
    else
    {
        // failed to load chunk
    }
    return 0;
}
