#include "dcmLuaState.h"

#include "dyn.h"

#include "dcmBaseLua.h"
#include "dcmContext.h"
#include "dcmUtil.h"
#include "dcmVariant.h"

#include "lua.h"
#include "lstate.h"
#include "lualib.h"
#include "lauxlib.h"

#include <stdlib.h>
#include <string.h>

static void dcmLuaStateUpdateGlobals(dcmLuaState *state);
static void dcmLuaStateRegisterGlobals(dcmLuaState *state);
static void dcmLuaStateRegisterGlobalVars(dcmLuaState *state);

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
        // Unimplemented:
        // LUA_TNIL
        // LUA_TLIGHTUSERDATA
        // LUA_TTABLE
        // LUA_TFUNCTION
        // LUA_TUSERDATA
        // LUA_TTHREAD

        case LUA_TBOOLEAN:
            ret = dcmVariantCreate(V_STRING);
            s = (lua_toboolean(L, index)) ? "true" : "false";
            dsCopy(&ret->s, s);
            break;
        case LUA_TNUMBER:
            ret = dcmVariantCreate(V_STRING);
            sprintf(temp, "%d", (int)lua_tonumber(L, index));
            dsCopy(&ret->s, temp);
            break;
        case LUA_TSTRING:
            ret = dcmVariantCreate(V_STRING);
            s = lua_tolstring(L, index, &l);
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
                    if(lua_type(L, -2) == LUA_TSTRING)
                    {
                        s = lua_tolstring(L, -2, &l);
                    }
                    else if(lua_type(L, -2) == LUA_TNUMBER)
                    {
                        sprintf(temp, "%d", (int)lua_tonumber(L, -2));
                        s = temp;
                    }

                    if(s != NULL)
                    {
                        child = dcmLuaStateIndexToVariant(state, lua_absindex(L, -1));
                        dmGetS2P(ret->m, s) = child;
                    }
                }
                else
                {
                    child = dcmLuaStateIndexToVariant(state, lua_absindex(L, -1));
                    daPush(&ret->a, child);
                }

                // removes 'value'; keeps 'key' for next iteration
                lua_pop(L, 1);
            }

            break;
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

dcmLuaState *dcmLuaStateCreate(struct dcmContext *context)
{
    dcmLuaState *state = calloc(1, sizeof(dcmLuaState));
    state->context = context;
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
    dcmLuaStateUpdateGlobals(state);
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

static void dcmLuaStateUpdateGlobals(dcmLuaState *state)
{
    const char *srcPath = dcmContextSrcPath(state->context);
    const char *dstPath = dcmContextDstPath(state->context);

    lua_pushglobaltable(state->L);

    lua_pushlstring(state->L, srcPath, strlen(srcPath)+1);
    lua_setfield(state->L, -2, "DCM_CURRENT_SOURCE_DIR");
    lua_pushlstring(state->L, dstPath, strlen(dstPath)+1);
    lua_setfield(state->L, -2, "DCM_CURRENT_BINARY_DIR");

    lua_pop(state->L, 1);
}

int dcmLuaStateAddSubdir(dcmLuaState *state, const char *dir)
{
    dcmContext *context = state->context;
    int ret = 1;
    int len;
    char *makeFilename = NULL;
    char *script;
    char *srcPath = NULL;
    char *dstPath = NULL;

    dsCopy(&srcPath, dir);
    dcmCanonicalizePath(&srcPath, dcmContextSrcPath(state->context));
    dsCopy(&dstPath, dir);
    dcmCanonicalizePath(&dstPath, dcmContextDstPath(state->context));
    dsPrintf(&makeFilename, "Makefile.lua");
    dcmCanonicalizePath(&makeFilename, srcPath);

    script = dcmFileAlloc(makeFilename, &len);
    if(script)
    {
        daPushString(&context->srcPathStack, srcPath);
        daPushString(&context->dstPathStack, dstPath);
        dcmLuaStateLoadScript(state, makeFilename, script, len);
        if(state->error)
        {
            fprintf(stderr, "ERROR: %s\n", state->error);
        }
        free(script);
        daPopString(&context->srcPathStack);
        daPopString(&context->dstPathStack);
    }
    else
    {
        ret = 0;
    }
    dsDestroy(&makeFilename);
    dsDestroy(&srcPath);
    dsDestroy(&dstPath);
    return ret;
}

// ---------------------------------------------------------------------------
// Lua global functions

static int unimplemented(lua_State *L)
{
    dcmVariant *variant = dcmLuaStateArgsToVariant(L->dcmstate);
    printf("UNIMPLEMENTED func called. Args:\n");
    dcmVariantPrint(variant, 1);
    dcmVariantDestroy(variant);
    return 0;
}

#define LUA_CONTEXT_DECLARE_STUB(NAME) { #NAME, unimplemented }
#define LUA_CONTEXT_DECLARE_FUNC(NAME) { #NAME, LuaFunc_ ## NAME }
#define LUA_CONTEXT_IMPLEMENT_FUNC(NAME, CONTEXTFUNC) \
static int LuaFunc_ ## NAME (lua_State *L) \
{ \
    dcmLuaState *state = L->dcmstate; \
    dcmVariant *args = dcmLuaStateArgsToVariant(state); \
    CONTEXTFUNC(state->context, args); \
    dcmVariantDestroy(args); \
    return 0; \
}

LUA_CONTEXT_IMPLEMENT_FUNC(project,             dcmContextProject);
LUA_CONTEXT_IMPLEMENT_FUNC(add_subdirectory,    dcmContextAddSubdirectory);
LUA_CONTEXT_IMPLEMENT_FUNC(add_definitions,     dcmContextAddDefinitions);
LUA_CONTEXT_IMPLEMENT_FUNC(include_directories, dcmContextIncludeDirectories);

static const luaL_Reg dcmGlobalFuncs[] =
{
    LUA_CONTEXT_DECLARE_STUB(print),
    LUA_CONTEXT_DECLARE_FUNC(project),
    LUA_CONTEXT_DECLARE_FUNC(add_subdirectory),
    LUA_CONTEXT_DECLARE_FUNC(add_definitions),
    LUA_CONTEXT_DECLARE_FUNC(include_directories),
    LUA_CONTEXT_DECLARE_FUNC(project),
    {NULL, NULL}
};

static void dcmLuaStateRegisterGlobals(dcmLuaState *state)
{
    int isUNIX = 0;
    int isWIN32 = 0;

#ifdef __unix__
    isUNIX = 1;
#endif

#ifdef WIN32
    isWIN32 = 1;
#endif

    lua_pushglobaltable(state->L);
    lua_pushglobaltable(state->L);
    lua_setfield(state->L, -2, "_G");
    luaL_setfuncs(state->L, dcmGlobalFuncs, 0);

    lua_pushboolean(state->L, isUNIX);
    lua_setfield(state->L, -2, "UNIX");
    lua_pushboolean(state->L, isWIN32);
    lua_setfield(state->L, -2, "WIN32");
}
