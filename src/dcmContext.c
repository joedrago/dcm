#include "dcmContext.h"
#include "dcmLuaState.h"
#include "dcmUtil.h"
#include "dcmVariant.h"

#include <stdlib.h>
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

void dcmContextProject(dcmContext *context, struct dcmVariant *args)
{
//    printf("dcmContextProject:\n");
//    dcmVariantPrint(args, 1);
}

void dcmContextAddSubdirectory(dcmContext *context, struct dcmVariant *args)
{
    if((daSize(&args->a) == 1) && (args->a[0]->type == V_STRING))
    {
        dcmLuaStateAddSubdir(context->luaState, args->a[0]->s);
    }
    else
    {
    }
}

void dcmContextAddDefinitions(dcmContext *context, struct dcmVariant *args)
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
}

void dcmContextIncludeDirectories(dcmContext *context, struct dcmVariant *args)
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
}
