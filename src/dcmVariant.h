#ifndef DCMVARIANT_H
#define DCMVARIANT_H

#include "dyn.h"

enum
{
    V_NONE = 0, // 'n': nil / absent
    V_STRING,   // 's': must be a basic string
    V_ARRAY,    // 'a'
    V_MAP       // 'm'
};

struct dcmVariant;

typedef struct dcmVariant
{
    int type;
    union
    {
        char *s;
        struct dcmVariant **a;
        dynMap *m;
    };
} dcmVariant;

dcmVariant *dcmVariantCreate(int type);
void dcmVariantDestroy(dcmVariant *arg);
void dcmVariantClear(dcmVariant *arg);
void dcmVariantPrint(dcmVariant *v, int depth); // for debugging

#endif
