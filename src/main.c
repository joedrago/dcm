#include "dyn.h"
#include "lua.h"
#include "dcmVariant.h"
#include "dcmLuaState.h"
#include "dcmUtil.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if(argc > 1)
    {
        dcmLuaState *state = dcmLuaStateCreate();
        if(state)
        {
            dcmLuaStateAddSubdir(state, argv[1]);
            dcmLuaStateDestroy(state);
        }
    }
    {
        dcmVariant *arg, *p;

        arg = dcmVariantCreate(V_MAP);

        p = dcmVariantCreate(V_STRING);
        dsCopy(&p->s, "sup sup");
        dmGetS2P(arg->m, "sup") = p;

        dcmVariantDestroy(arg);
    }
    return 0;
}
