#include "dcmVariant.h"

#include <stdlib.h>
#include <stdio.h>

dcmVariant *dcmVariantCreate(int type)
{
    dcmVariant *arg = calloc(1, sizeof(dcmVariant));
    arg->type = type;
    if(arg->type == V_MAP)
    {
        arg->m = dmCreate(DKF_STRING, 0);
    }
    return arg;
}

void dcmVariantDestroy(dcmVariant *arg)
{
    dcmVariantClear(arg);
    free(arg);
}

void dcmVariantClear(dcmVariant *arg)
{
    switch (arg->type)
    {
        case V_STRING:
            dsDestroy(&arg->s);
            break;
        case V_ARRAY:
            daDestroy(&arg->a, dcmVariantDestroy);
            break;
        case V_MAP:
            dmDestroy(arg->m, dcmVariantDestroy);
            break;
    };
}

#define PRINTDEPTH(DEPTH) { int d; for(d = 0; d < (DEPTH); ++d) printf("  "); }

static int dcmVariantPrintMap(dynMap *dm, dynMapEntry *e, void *userData)
{
    int *depth = (int *)userData;
    dcmVariant *v = dmEntryDefaultData(e)->valuePtr;
    PRINTDEPTH(*depth + 1);
    printf("* key: %s\n", e->keyStr);
    dcmVariantPrint(v, *depth + 2);
    return 1;
}

void dcmVariantPrint(dcmVariant *v, int depth)
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
                dcmVariantPrint(v->a[i], depth + 1);
            }
            break;
        case V_MAP:
            PRINTDEPTH(depth);
            printf("* map: %d element(s)\n", v->m->count);
            dmIterate(v->m, dcmVariantPrintMap, &depth);
            break;
    };
}
