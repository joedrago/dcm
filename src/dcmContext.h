#ifndef DCMCONTEXT_H
#define DCMCONTEXT_H

struct dcmLuaState;
struct dcmVariant;
struct dynMap;

typedef struct dcmContext
{
    struct dcmLuaState *luaState;
    char **includes;
    char **defines;

    char **srcPathStack;
    char **dstPathStack;

    struct dcmVariant *platforms;
} dcmContext;

dcmContext *dcmContextCreate();
void dcmContextDestroy(dcmContext *context);

const char *dcmContextSrcPath(dcmContext *context);
const char *dcmContextDstPath(dcmContext *context);

// Functions called by Lua
int dcmContextProject(dcmContext *context, struct dcmVariant *args);
int dcmContextAddSubdirectory(dcmContext *context, struct dcmVariant *args);
int dcmContextAddDefinitions(dcmContext *context, struct dcmVariant *args);
int dcmContextIncludeDirectories(dcmContext *context, struct dcmVariant *args);
int dcmContextAddTarget(dcmContext *context, struct dcmVariant *args);
int dcmContextAddRule(dcmContext *context, struct dcmVariant *args);
int dcmContextInterp(dcmContext *context, struct dcmVariant *args);

#endif
