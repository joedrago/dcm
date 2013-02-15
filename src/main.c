#include "dyn.h"
#include "lua.h"
#include "dcmLuaState.h"
#include "dcmUtil.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if(argc > 1)
    {
        dcmLuaState *state = dcmLuaStateCreate();
        int len;
        char *script = dcmFileAlloc(argv[1], &len);
        if(script)
        {
            dcmLuaStateLoadScript(state, argv[1], script, len);
            if(state->error)
            {
                fprintf(stderr, "ERROR: %s\n", state->error);
            }
            free(script);
        }
        dcmLuaStateDestroy(state);
    }
    return 0;
}
