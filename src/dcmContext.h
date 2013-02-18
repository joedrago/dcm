#ifndef DCMCONTEXT_H
#define DCMCONTEXT_H

struct dcmLuaState;
struct dcmVariant;

typedef struct dcmContext
{
    struct dcmLuaState *luaState;
    char **includes;
    char **defines;

    char **srcPathStack;
    char **dstPathStack;
} dcmContext;

dcmContext *dcmContextCreate();
void dcmContextDestroy(dcmContext *context);

const char *dcmContextSrcPath(dcmContext *context);
const char *dcmContextDstPath(dcmContext *context);

// Functions called by Lua
void dcmContextProject(dcmContext *context, struct dcmVariant *args);
void dcmContextAddSubdirectory(dcmContext *context, struct dcmVariant *args);
void dcmContextAddDefinitions(dcmContext *context, struct dcmVariant *args);
void dcmContextIncludeDirectories(dcmContext *context, struct dcmVariant *args);

#endif
