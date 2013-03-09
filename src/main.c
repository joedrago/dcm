#include "dyn.h"
#include "dcmBaseEk.h"

#include "ek.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef WIN32
#include <windows.h>   // for GetCurrentDirectory()
#else
#include <unistd.h>    // for getcwd()
#include <sys/types.h> // for S_* defines
#include <sys/stat.h>  // for mkdir()
#include <dirent.h>    // for opendir()
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
    static char cwd[512];
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
    DIR *p = opendir(path);
    if(p != NULL)
    {
        ret = 1;
        closedir(p);
    }
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
        mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
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
// Eureka dcm functions

static ekU32 dcm_canonicalize(struct ekContext *E, ekU32 argCount)
{
    char *temp = NULL;

    ekValue *path = NULL;
    ekValue *root = NULL;
    if(!ekContextGetArgs(E, argCount, "ss", &path, &root))
    {
        return ekContextArgsFailure(E, argCount, "dcm.canonicalize([string] path, [string] root)");
    }

    dsCopy(&temp, ekStringSafePtr(&path->stringVal));
    dcmCanonicalizePath(&temp, ekStringSafePtr(&root->stringVal));
    ekContextPushValue(E, ekValueCreateString(E, temp));
    dsDestroy(&temp);

    ekValueRemoveRef(E, path);
    ekValueRemoveRef(E, root);
    return 1;
}


static ekU32 dcm_interp(struct ekContext *E, ekU32 argCount)
{
    ekValue *templateVal = NULL;
    ekValue *args = NULL;
    if(!ekContextGetArgs(E, argCount, "sm", &templateVal, &args))
    {
        return ekContextArgsFailure(E, argCount, "dcm.interp([string] template, [map] args)");
    }
    else
    {
        // TODO: error checking of any kind
        const char *template = ekStringSafePtr(&templateVal->stringVal);
        char *out = NULL;
        char *basename = NULL;
        dynMap *vars = dmCreate(DKF_STRING, 0);

        if(ekObjectGetRef(E, args->objectVal, "path", ekFalse))
        {
            ekValue **ref = ekObjectGetRef(E, args->objectVal, "path", ekFalse);
            const char *path = ekStringSafePtr(&((*ref)->stringVal));
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

        if(ekObjectGetRef(E, args->objectVal, "root", ekFalse))
        {
            ekValue **ref = ekObjectGetRef(E, args->objectVal, "root", ekFalse);
            const char *root = ekStringSafePtr(&((*ref)->stringVal));
            if(root)
            {
                dcmCanonicalizePath(&out, root);
            }
        }

        ekContextPushValue(E, ekValueCreateString(E, out));

        dsDestroy(&out);
        dsDestroy(&basename);
        ekValueRemoveRef(E, templateVal);
        ekValueRemoveRef(E, args);
    }
    return 1;
}

static ekU32 dcm_die(struct ekContext *E, ekU32 argCount)
{
    const char *error = "<unknown>";
    ekValue *s = NULL;
    if(!ekContextGetArgs(E, argCount, "|s", &s))
    {
        return ekContextArgsFailure(E, argCount, "dcm.die([string] reason)");
    }

    if(s)
    {
        error = ekStringSafePtr(&s->stringVal);
    }

    printf("dcm.die: %s\n", error);
    exit(-1);

    if(s)
    {
        ekValueRemoveRef(E, s);
    }
    return 0;
}

static ekU32 dcm_read(struct ekContext *E, ekU32 argCount)
{
    ekValue *filename = NULL;
    int pushed = 0;
    int len = 0;
    char *text = NULL;
    if(!ekContextGetArgs(E, argCount, "s", &filename))
    {
        return ekContextArgsFailure(E, argCount, "dcm.read([string] filename)");
    }

    text = dcmFileAlloc(ekStringSafePtr(&filename->stringVal), &len);
    if(text)
    {
        if(len)
        {
            ekContextPushValue(E, ekValueCreateString(E, text));
            pushed = 1;
        }
        free(text);
    }
    if(!pushed)
    {
        ekContextPushValue(E, &ekValueNull);
    }
    ekValueRemoveRef(E, filename);
    return 1;
}

static ekU32 dcm_write(struct ekContext *E, ekU32 argCount)
{
    ekValue *filename = NULL;
    ekValue *textVal = NULL;
    int len = 0;
    FILE *f;
    char *err = NULL;
    const char *text;
    if(!ekContextGetArgs(E, argCount, "ss", &filename, &textVal))
    {
        return ekContextArgsFailure(E, argCount, "dcm.write([string] filename, [string] text)");
    }

    dsCopy(&err, "");
    text = ekStringSafePtr(&textVal->stringVal);

    f = fopen(ekStringSafePtr(&filename->stringVal), "wb");
    if(f)
    {
        int len = strlen(text);
        fwrite(text, 1, len, f);
        fclose(f);
    }

    ekContextPushValue(E, ekValueCreateString(E, err));

    dsDestroy(&err);
    ekValueRemoveRef(E, filename);
    ekValueRemoveRef(E, textVal);
    return 1;
}

static ekU32 dcm_mkdir_for_file(struct ekContext *E, ekU32 argCount)
{
    ekValue *filename = NULL;
    if(!ekContextGetArgs(E, argCount, "s", &filename))
    {
        return ekContextArgsFailure(E, argCount, "dcm.read([string] filename)");
    }
    else
    {
        const char *path = ekStringSafePtr(&filename->stringVal);
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

        ekValueRemoveRef(E, filename);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Lua dcm function hooks

static void dcmPrepare(ekContext *E, int argc, char **argv)
{
    ekValue *dcm;
    ekValue *v;
    char *error = NULL;
    int isUNIX = 0;
    int isWIN32 = 0;

#ifdef __unix__
    isUNIX = 1;
#endif

#ifdef WIN32
    isWIN32 = 1;
#endif

    dcm = ekValueCreateObject(E, NULL, 0, ekFalse);

    // variables
    ekValueObjectSetMember(E, dcm, "unix", ekValueCreateInt(E, isUNIX));
    ekValueObjectSetMember(E, dcm, "win32", ekValueCreateInt(E, isWIN32));
    ekValueObjectSetMember(E, dcm, "cwd", ekValueCreateString(E, dcmWorkingDir()));
    ekValueObjectSetMember(E, dcm, "cmd", ekValueCreateString(E, argv[0]));

    // functions
    ekValueObjectSetMember(E, dcm, "interp", ekValueCreateCFunction(E, dcm_interp));
    ekValueObjectSetMember(E, dcm, "canonicalize", ekValueCreateCFunction(E, dcm_canonicalize));
    ekValueObjectSetMember(E, dcm, "die", ekValueCreateCFunction(E, dcm_die));
    ekValueObjectSetMember(E, dcm, "read", ekValueCreateCFunction(E, dcm_read));
    ekValueObjectSetMember(E, dcm, "write", ekValueCreateCFunction(E, dcm_write));
    ekValueObjectSetMember(E, dcm, "mkdir_for_file", ekValueCreateCFunction(E, dcm_mkdir_for_file));

    // cmdline args
    v = ekValueCreateArray(E);
    {
        int i;
        for(i = 1; i < argc; ++i)
        {
            ekValueArrayPush(E, v, ekValueCreateString(E, argv[i]));
        }
    }
    ekValueObjectSetMember(E, dcm, "args", v);

    ekContextAddGlobal(E, "dcm", dcm);

    ekContextEval(E, dcmBaseEkData, 0);
    if(ekContextGetError(E))
    {
        error = strdup(ekContextGetError(E));
    }
    ekContextDestroy(E);
    if(error)
    {
        printf("VM Bailed out: %s\n", error);
    }
    free(error);
}


// ---------------------------------------------------------------------------
// Main

int main(int argc, char **argv)
{
    ekContext *E = ekContextCreate(NULL);
    dcmPrepare(E, argc, argv);
    return 0;
}
