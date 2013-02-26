#include "dcmContext.h"
#include "dcmLuaState.h"
#include "dcmUtil.h"
#include "dcmVariant.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

dcmContext *dcmContextCreate(const char *srcRoot, const char *dstRoot)
{
    dcmContext *context = calloc(1, sizeof(dcmContext));
    daPushString(&context->srcPathStack, srcRoot);
    daPushString(&context->dstPathStack, dstRoot);
    context->luaState = dcmLuaStateCreate(context);
    if(context->luaState)
    {
        dcmLuaStateAddSubdir(context->luaState, ".");
    }
    else
    {
        dcmContextDestroy(context);
        context = NULL;
    }
    return context;
}

void dcmContextDestroy(dcmContext *context)
{
    if(context->luaState)
    {
        dcmLuaStateDestroy(context->luaState);
    }
    daDestroyStrings(&context->includes);
    daDestroyStrings(&context->defines);
    daDestroyStrings(&context->srcPathStack);
    daDestroyStrings(&context->dstPathStack);
    free(context);
}

const char *dcmContextSrcPath(dcmContext *context)
{
    return daTopString(&context->srcPathStack);
}

const char *dcmContextDstPath(dcmContext *context)
{
    return daTopString(&context->dstPathStack);
}

int dcmContextProject(dcmContext *context, struct dcmVariant *args)
{
//    printf("dcmContextProject:\n");
//    dcmVariantPrint(args, 1);
    return 0;
}

int dcmContextAddSubdirectory(dcmContext *context, struct dcmVariant *args)
{
    if((daSize(&args->a) == 1) && (args->a[0]->type == V_STRING))
    {
        dcmLuaStateAddSubdir(context->luaState, args->a[0]->s);
    }
    else
    {
        dcmLuaStateError(context->luaState, "add_subdirectory takes exactly one string argument");
    }
    return 0;
}

int dcmContextAddDefinitions(dcmContext *context, struct dcmVariant *args)
{
    int i;
    for(i = 0; i < daSize(&args->a); ++i)
    {
        dcmVariant *v = args->a[i];
        if(v->type == V_STRING)
        {
            daPushUniqueString(&context->defines, v->s);
        }
    }
    return 0;
}

int dcmContextIncludeDirectories(dcmContext *context, struct dcmVariant *args)
{
    int i;
    for(i = 0; i < daSize(&args->a); ++i)
    {
        dcmVariant *v = args->a[i];
        if(v->type == V_STRING)
        {
            dcmCanonicalizePath(&v->s, daTopString(&context->srcPathStack));
            daPushUniqueString(&context->includes, v->s);
        }
    }

/*
    printf("* Included dirs is now:\n");
    for(i = 0; i < daSize(&context->includes); ++i)
    {
        printf("  * %s\n", context->includes[i]);
    }
*/
    return 0;
}

// add_target("platform name", "rule name", "target name", { "relative file #1", ... })
int dcmContextAddTarget(dcmContext *context, struct dcmVariant *args)
{
    // TODO: error checking of any kind
    const char *platform = args->a[0]->s;
    const char *rule = args->a[1]->s;
    const char *target = args->a[2]->s;
    int i;

    printf("dcmContextAddTarget:\n");
    printf("* platform: %s\n", platform);
    printf("* rule: %s\n", rule);
    printf("* target: %s\n", target);
    return 0;
}

int dcmContextAddRule(dcmContext *context, struct dcmVariant *args)
{
    // TODO: error checking of any kind
    dynMap *m = args->a[0]->m;
    return 0;
}

const char * product(dynMap *vars, const char *in, char **out, char end)
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

            /*

            // Loop over all found variables and multiply the strings into a new list
            for(StringVector::iterator outer = out.begin(); outer != out.end(); ++outer)
            {
                for(StringVector::iterator inner = varname.begin(); inner != varname.end(); ++inner)
                {
                    StringVector &v = vars[*inner];
                    if(v.size())
                    {
                        for(StringVector::iterator vit = v.begin(); vit != v.end(); ++vit)
                        {
                            string val = *vit;
                            if(uppercase)
                            {
                                std::transform(val.begin(), val.end(), val.begin(), ::toupper);
                            }
                            temp.push_back(*outer + val);
                        }
                    }
                    else
                    {
                        temp.push_back(*outer);
                    }
                }
            }
            out.swap(temp);
            */

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



int dcmContextInterp(dcmContext *context, struct dcmVariant *args)
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
    dcmLuaStatePushString(context->luaState, out);
    dsDestroy(&out);
    dsDestroy(&basename);
    return 1;
}
