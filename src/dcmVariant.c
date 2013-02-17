#include "dcmVariant.h"

#include <stdlib.h>

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
            daDestroyPtr(&arg->a, dcmVariantDestroy);
            break;
        case V_MAP:
            dmDestroyPtr(arg->m, dcmVariantDestroy);
            break;
    };
}
