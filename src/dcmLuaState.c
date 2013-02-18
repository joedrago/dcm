#include "dcmLuaState.h"

#include "dyn.h"

#include "dcmBaseLua.h"
#include "dcmUtil.h"
#include "dcmVariant.h"

#include "lua.h"
#include "lstate.h"
#include "lualib.h"
#include "lauxlib.h"

#include <stdlib.h>
#include <string.h>

static dcmVariant *dcmLuaStateIndexToVariant(dcmLuaState *state, int index)
{
    lua_State *L = state->L;
    dcmVariant *ret = NULL;
    dcmVariant *child;
    const char *s = NULL;
    size_t l;
    char temp[16];

    int type = lua_type(L, index);

    switch (type)
    {
        //LUA_TNIL
        case LUA_TBOOLEAN:
            ret = dcmVariantCreate(V_STRING);
            s = (lua_toboolean(L, index)) ? "true" : "false";
            dsCopy(&ret->s, s);
            break;
        //LUA_TLIGHTUSERDATA
        case LUA_TNUMBER:
            ret = dcmVariantCreate(V_STRING);
            sprintf(temp, "%d", (int)lua_tonumber(L, index));
            dsCopy(&ret->s, temp);
            break;
        case LUA_TSTRING:
            ret = dcmVariantCreate(V_STRING);
            s = lua_tolstring(L, index, &l);  /* get result */
            dsCopy(&ret->s, s);
            break;
        case LUA_TTABLE:
            lua_pushnil(L);  /* first key */
            while (lua_next(L, index))
            {
                // uses 'key' (at index -2) and 'value' (at index -1)

                if(ret == NULL)
                {
                    // Lazily create this so a table can either be a map or an array
                    if(lua_type(L, -2) == LUA_TSTRING)
                    {
                        ret = dcmVariantCreate(V_MAP);
                    }
                    else
                    {
                        ret = dcmVariantCreate(V_ARRAY);
                    }
                }

                if(ret->type == V_MAP)
                {
                    if(lua_type(L, -1) == LUA_TSTRING)
                    {
                        s = lua_tolstring(L, -1, &l);  /* get result */
                    }
                    else if(lua_type(L, -1) == LUA_TNUMBER)
                    {
                        sprintf(temp, "%d", (int)lua_tonumber(L, -1));
                        s = temp;
                    }

                    if(s != NULL)
                    {
                        child = dcmLuaStateIndexToVariant(state, lua_gettop(L) - 1);
                        dmGetS2P(ret->m, s) = child;
                    }
                }
                else
                {
                    child = dcmLuaStateIndexToVariant(state, lua_gettop(L) - 1);
                    daPush(&ret->a, child);
                }


                // printf("    * %s - %s\n",
                //        lua_typename(L, lua_type(L, -2)),
                //        lua_typename(L, lua_type(L, -1)));

                // removes 'value'; keeps 'key' for next iteration
                lua_pop(L, 1);
            }

            break;
            //LUA_TTABLE
            //LUA_TFUNCTION
            //LUA_TUSERDATA
            //LUA_TTHREAD
    };

    if(!ret)
    {
        ret = dcmVariantCreate(V_NONE);
    }
    return ret;
}

static dcmVariant *dcmLuaStateArgsToVariant(dcmLuaState *state)
{
    lua_State *L = state->L;
    int argCount = lua_gettop(L);
    int i;
    dcmVariant *ret = dcmVariantCreate(V_ARRAY);
    dcmVariant *child;

    for (i=1; i <= argCount; ++i)
    {
        child = dcmLuaStateIndexToVariant(state, i);
        daPush(&ret->a, child);
    }
    return ret;
}

#define PRINTDEPTH(DEPTH) { int d; for(d = 0; d < (DEPTH); ++d) printf("  "); }
static void printVariant(dcmVariant *v, int depth);

int printVariantMap(dynMap *dm, dynMapEntry *e, void *userData)
{
    int *depth = (int *)userData;
    dcmVariant *v = dmEntryDefaultData(e)->valuePtr;
    PRINTDEPTH(*depth + 1);
    printf("* key: %s\n", e->keyStr);
    printVariant(v, *depth + 2);
    return 1;
}

static void printVariant(dcmVariant *v, int depth)
{
    int i;
    switch (v->type)
    {
        case V_NONE:
            PRINTDEPTH(depth);
            printf("* nil\n");
            break;
        case V_STRING:
            PRINTDEPTH(depth);
            printf("* string: %s\n", v->s);
            break;
        case V_ARRAY:
            PRINTDEPTH(depth);
            printf("* array: %d element(s)\n", (int)daSize(&v->a));
            for(i = 0; i < daSize(&v->a); ++i)
            {
                printVariant(v->a[i], depth + 1);
            }
            break;
        case V_MAP:
            PRINTDEPTH(depth);
            printf("* map: %d element(s)\n", v->m->count);
            dmIterate(v->m, printVariantMap, &depth);
            break;
    };
}

static int whatever(lua_State *L)
{
    dcmLuaState *state = (dcmLuaState *)L->dcmstate;
    dcmVariant *variant = dcmLuaStateArgsToVariant(L->dcmstate);
    printVariant(variant, 0);
    dcmVariantDestroy(variant);
    return 0;
}

static const luaL_Reg dcmGlobalFuncs[] =
{
    {"print", whatever},
    {NULL, NULL}
};

static void dcmLuaStateRegisterGlobals(dcmLuaState *state)
{
    lua_pushglobaltable(state->L);
    lua_pushglobaltable(state->L);
    lua_setfield(state->L, -2, "_G");
    luaL_setfuncs(state->L, dcmGlobalFuncs, 0);
}

dcmLuaState *dcmLuaStateCreate()
{
    dcmLuaState *state = calloc(1, sizeof(dcmLuaState));
    state->L = luaL_newstate();
    state->L->dcmstate = state;
    luaopen_base(state->L);
    dcmLuaStateRegisterGlobals(state);
    dcmLuaStateLoadScript(state, "dcmBase", dcmBaseLuaData, dcmBaseLuaSize);
    if(state->error)
    {
        fprintf(stderr, "ERROR (Base): %s\n", state->error);
        dcmLuaStateDestroy(state);
        state = NULL;
    }
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
        dcmLuaStateError(state, lua_tostring(state->L, -1));
        lua_pop(state->L, 1);
    }
    return 0;
}

int dcmLuaStateAddSubdir(dcmLuaState *state, const char *dir)
{
    int ret = 1;
    int len;
    char *makeFilename = NULL;
    char *script;

    dsPrintf(&makeFilename, "%s/Makefile.lua", dir);
    script = dcmFileAlloc(makeFilename, &len);
    if(script)
    {
        dcmLuaStateLoadScript(state, makeFilename, script, len);
        if(state->error)
        {
            fprintf(stderr, "ERROR: %s\n", state->error);
        }
        free(script);
    }
    else
    {
        ret = 0;
    }
    dsDestroy(&makeFilename);
    return ret;
}
