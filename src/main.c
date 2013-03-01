#include "dyn.h"
#include "dcmVariant.h"
#include "dcmBaseLua.h"

#include "lua.h"
#include "lstate.h"
#include "lualib.h"
#include "lauxlib.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef WIN32
#include <windows.h> // for GetCurrentDirectory()
#else
#include <unistd.h> // for getcwd()
#endif

// ---------------------------------------------------------------------------
// Path helpers

const char *dcmWorkingDir()
{
#ifdef WIN32
    static char currentDir[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, currentDir);
    return currentDir;
#else
    char cwd[512];
    return getcwd(cwd, 512);
#endif
}

int dcmDirExists(const char *path)
{
    int ret = 0;

#ifdef WIN32
    DWORD dwAttrib = GetFileAttributes(path);
    if(dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
    {
        ret = 1;
    }
#else
#error fix this
#endif

    return ret;
}

void dcmMkdir(const char *path)
{
    if(!dcmDirExists(path))
    {
        printf("Creating directory: %s\n", path);

#ifdef WIN32
        CreateDirectory(path, NULL);
#else
#error fix this
#endif

    }
}

// ---------------------------------------------------------------------------
// File reading

char *dcmFileAlloc(const char *filename, int *outputLen)
{
    char *data;
    int len;
    int bytesRead;

    FILE *f = fopen(filename, "rb");
    if(!f)
    {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    len = (int)ftell(f);
    if(len < 1)
    {
        fclose(f);
        return NULL;
    }

    fseek(f, 0, SEEK_SET);
    data = (char *)calloc(1, len + 1);
    bytesRead = fread(data, 1, len, f);
    if(bytesRead != len)
    {
        free(data);
        data = NULL;
    }

    fclose(f);
    *outputLen = len;
    return data;
}

// ---------------------------------------------------------------------------
// Script loading

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

static int dcmLoadScript(lua_State *L, const char *name, const char *script, int len)
{
    int err;
    struct dcmScriptInfo info;
    info.script = script;
    info.len = len;
    err = lua_load(L, dcmLoadScriptReader, &info, name);
    if(err == 0)
    {
        err = lua_pcall(L, 0, LUA_MULTRET, 0);
        if(err == 0)
        {
            return 1;
        }
        else
        {
            // failed to run chunk
            printf("ERROR: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    else
    {
        // failed to load chunk
        printf("ERROR: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    return 0;
}

// ---------------------------------------------------------------------------

#ifdef WIN32
#define PROPER_SLASH '\\'
#define PROPER_CURRENT "\\."
#define PROPER_PARENT "\\.."
#else
#define PROPER_SLASH '/'
#define PROPER_CURRENT "/."
#define PROPER_PARENT "/.."
#endif

static int isAbsolutePath(const char *s)
{
#ifdef WIN32
    if((strlen(s) >= 3) && (s[1] == ':') && (s[2] == PROPER_SLASH))
    {
        char driveLetter = tolower(s[0]);
        if((driveLetter >= 'a') && (driveLetter <= 'z'))
        {
            return 1;
        }
    }
#endif
    if(s[0] == PROPER_SLASH)
    {
        return 1;
    }
    return 0;
}

static void dsCleanupSlashes(char **ds)
{
    char *c;
    char *head;
    char *tail;
    int wasSlash = 0;
    if(!dsLength(ds))
    {
        return;
    }

    // Remove all trailing slashes
    c = *ds + (dsLength(ds) - 1);
    for(; c >= *ds; --c)
    {
        if((*c == '/') || (*c == '\\'))
        {
            *c = 0;
        }
        else
        {
            break;
        }
    }

    // Remove all duplicate slashes, fix slash direction
    head = *ds;
    tail = *ds;
    for(; *tail; ++tail)
    {
        int isSlash = ((*tail == '/') || (*tail == '\\')) ? 1 : 0;
        if(isSlash)
        {
            if(!wasSlash)
            {
                *head = PROPER_SLASH;
                ++head;
            }
        }
        else
        {
            *head = *tail;
            ++head;
        }
        wasSlash = isSlash;
    }
    *head = 0;

    // Clue in the dynString that it was manually altered
    dsCalcLength(ds);
}

static void dsSquashDotDirs(char **ds)
{
    char *dot;
    char *prevSlash;
    while((dot = strstr(*ds, PROPER_PARENT)) != NULL)
    {
        if(dot == *ds)
        {
            // bad path
            return;
        }
        prevSlash = dot - 1;
        dot += strlen(PROPER_PARENT);
        while((prevSlash != *ds) && (*prevSlash != PROPER_SLASH))
        {
            --prevSlash;
        }
        if(prevSlash != *ds)
        {
            memmove(prevSlash, dot, strlen(dot)+1);
        }
    }
    while((dot = strstr(*ds, PROPER_CURRENT)) != NULL)
    {
        if(dot == *ds)
        {
            // bad path
            return;
        }
        prevSlash = dot;
        dot += strlen(PROPER_CURRENT);
        if(prevSlash != *ds)
        {
            memmove(prevSlash, dot, strlen(dot)+1);
        }
    }
    dsCalcLength(ds);
}

void dcmCanonicalizePath(char **dspath, const char *curDir)
{
    char *temp = NULL;
    if(!curDir || !strlen(curDir))
    {
        curDir = ".";
    }

    if(isAbsolutePath(*dspath))
    {
        dsCopy(&temp, *dspath);
    }
    else
    {
        dsPrintf(&temp, "%s/%s", curDir, *dspath);
    }

    dsCleanupSlashes(&temp);
    dsSquashDotDirs(&temp);

    dsCopy(dspath, temp);
    dsDestroy(&temp);
}

static const char * product(dynMap *vars, const char *in, char **out, char end)
{
    char *leftovers = NULL;
    char *varname = NULL;
    const char *c = in;
    const char *varval;

    int uppercase;
    const char *colon;
    const char *nextBackslash;
    const char *nextOpenBrace;
    const char *nextCloseBrace;

    dsCopy(&leftovers, "");
    for(; *c && *c != end; ++c)
    {
        if(*c == '{')
        {
            // Append any additional text we've seen to each entry in list we have so far
            dsConcat(out, leftovers);
            dsCopy(&leftovers, "");

            ++c;

            uppercase = 0;
            colon = strchr(c, ':');
            nextBackslash = strchr(c, '\\');
            nextOpenBrace = strchr(c, '{');
            nextCloseBrace = strchr(c, '}');
            if( colon
            && (!nextBackslash  || colon < nextBackslash)
            && (!nextOpenBrace  || colon < nextOpenBrace)
            && (!nextCloseBrace || colon < nextCloseBrace))
            {
                for(; c != colon; ++c)
                {
                    switch(*c)
                    {
                        case 'u': 
                            uppercase = 1;
                            break;
                    }
                }
                ++c;
            }

            // Now look up all variables contained in the {}
            c = product(vars, c, &varname, '}');
            varval = dmGetS2P(vars, varname);
            if(varval)
            {
                dsConcat(out, varval);
            }

            dsDestroy(&varname);
        }
        else
        {
            // Just append 
            dsConcatLen(&leftovers, c, 1);
        }
    }

    // If there was stuff trailing at the end, be sure to grab it
    dsConcat(out, leftovers);
    dsDestroy(&leftovers);
    return c;
}

// ---------------------------------------------------------------------------
// Lua dcm functions

int dcm_canonicalize(lua_State *L, struct dcmVariant *args)
{
    char *temp = NULL;
    dsCopy(&temp, args->a[0]->s);
    dcmCanonicalizePath(&temp, args->a[1]->s);
    lua_pushstring(L, temp);
    dsDestroy(&temp);
    return 1;
}

int dcm_interp(lua_State *L, struct dcmVariant *args)
{
    // TODO: error checking of any kind
    const char *template = args->a[0]->s;
    char *out = NULL;
    char *basename = NULL;
    dynMap *argMap = args->a[1]->m;
    dynMap *vars = dmCreate(DKF_STRING, 0);

    if(dmHasS(argMap, "path"))
    {
        dcmVariant *variant = dmGetS2P(argMap, "path");
        const char *path = variant->s;
        if(path)
        {
            char *dotLoc;
            char *slashLoc1;
            char *slashLoc2;
            char *basenameLoc;
            dsCopy(&basename, path);
            dotLoc = strrchr(basename, '.');
            slashLoc1 = strrchr(basename, '/');
            slashLoc2 = strrchr(basename, '\\');
            if(!slashLoc1)
            {
                slashLoc1 = slashLoc2;
            }
            if(slashLoc2)
            {
                if(slashLoc1 < slashLoc2)
                {
                    slashLoc1 = slashLoc2;
                }
            }
            if(dotLoc)
            {
                *dotLoc = 0;
            }
            if(slashLoc1)
            {
                basenameLoc = slashLoc1 + 1;
            }
            else
            {
                basenameLoc = basename;
            }
            dmGetS2P(vars, "BASENAME") = basenameLoc;
        }
    }

    product(vars, template, &out, 0);

    if(dmHasS(argMap, "root"))
    {
        dcmVariant *variant = dmGetS2P(argMap, "root");
        const char *root = variant->s;
        if(root)
        {
            dcmCanonicalizePath(&out, root);
        }
    }

    lua_pushstring(L, out);
    dsDestroy(&out);
    dsDestroy(&basename);
    return 1;
}

int dcm_die(lua_State *L, struct dcmVariant *args)
{
    const char *error = "<unknown>";
    if(args->a[0]->s)
    {
        error = args->a[0]->s;
    }
    luaL_error(L, error);
    exit(-1);
    return 0;
}

int dcm_read(lua_State *L, struct dcmVariant *args)
{
    int ret = 0;
    int len = 0;
    char *text = dcmFileAlloc(args->a[0]->s, &len);
    if(text)
    {
        if(len)
        {
            lua_pushstring(L, text);
            ++ret;
        }
        free(text);
    }
    return ret;
}

int dcm_write(lua_State *L, struct dcmVariant *args)
{
    char *err = NULL;
    const char *filename = args->a[0]->s;
    const char *text = args->a[1]->s;
    FILE *f = fopen(filename, "wb");
    dsCopy(&err, "");
    if(f)
    {
        int len = strlen(text);
        fwrite(text, 1, len, f);
        fclose(f);
    }
    lua_pushstring(L, err);
    dsDestroy(&err);
    return 1;
}

int dcm_mkdir_for_file(lua_State *L, struct dcmVariant *args)
{
    const char *path = args->a[0]->s;
    char *dirpath = NULL;
    char *slashLoc1;
    char *slashLoc2;
    dsCopy(&dirpath, path);
    slashLoc1 = strrchr(dirpath, '/');
    slashLoc2 = strrchr(dirpath, '\\');
    if(!slashLoc1)
    {
        slashLoc1 = slashLoc2;
    }
    if(slashLoc2)
    {
        if(slashLoc1 < slashLoc2)
        {
            slashLoc1 = slashLoc2;
        }
    }
    if(slashLoc1)
    {
        *slashLoc1 = 0;
        dcmMkdir(dirpath);
    }
    dsDestroy(&dirpath);
    return 0;
}

// ---------------------------------------------------------------------------
// Lua dcm function hooks

static int unimplemented(lua_State *L)
{
    dcmVariant *variant = dcmVariantFromArgs(L);
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
    int ret; \
    dcmVariant *args = dcmVariantFromArgs(L); \
    ret = CONTEXTFUNC(L, args); \
    dcmVariantDestroy(args); \
    return ret; \
}

LUA_CONTEXT_IMPLEMENT_FUNC(interp, dcm_interp);
LUA_CONTEXT_IMPLEMENT_FUNC(canonicalize, dcm_canonicalize);
LUA_CONTEXT_IMPLEMENT_FUNC(die, dcm_die);
LUA_CONTEXT_IMPLEMENT_FUNC(read, dcm_read);
LUA_CONTEXT_IMPLEMENT_FUNC(write, dcm_write);
LUA_CONTEXT_IMPLEMENT_FUNC(mkdir_for_file, dcm_mkdir_for_file);

static const luaL_Reg dcmFuncs[] =
{
    LUA_CONTEXT_DECLARE_FUNC(interp),
    LUA_CONTEXT_DECLARE_FUNC(canonicalize),
    LUA_CONTEXT_DECLARE_FUNC(die),
    LUA_CONTEXT_DECLARE_FUNC(read),
    LUA_CONTEXT_DECLARE_FUNC(write),
    LUA_CONTEXT_DECLARE_FUNC(mkdir_for_file),
    {NULL, NULL}
};

static void dcmPrepareLua(lua_State *L, int argc, char **argv)
{
    int isUNIX = 0;
    int isWIN32 = 0;

#ifdef __unix__
    isUNIX = 1;
#endif

#ifdef WIN32
    isWIN32 = 1;
#endif

    luaL_openlibs(L);

    luaL_register(L, "dcm", dcmFuncs);

    lua_pushboolean(L, isUNIX);
    lua_setfield(L, -2, "unix");
    lua_pushboolean(L, isWIN32);
    lua_setfield(L, -2, "win32");
    lua_pushstring(L, dcmWorkingDir());
    lua_setfield(L, -2, "cwd");
    lua_pushstring(L, argv[0]);
    lua_setfield(L, -2, "cmd");

    lua_newtable(L);
    {
        int i;
        for(i = 1; i < argc; ++i)
        {
            lua_pushinteger(L, i);
            lua_pushstring(L, argv[i]);
            lua_settable(L, -3);
        }
    }
    lua_setfield(L, -2, "args");

    lua_pop(L, 1); // pop "dcm"
    dcmLoadScript(L, "dcmBase", dcmBaseLuaData, dcmBaseLuaSize);
}

// ---------------------------------------------------------------------------
// Main

int main(int argc, char **argv)
{
    lua_State *L = luaL_newstate();
    dcmPrepareLua(L, argc, argv);
    return 0;
}
