#include "dyn.h"
#include "lua.h"
#include "dcmContext.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if(argc > 2)
    {
        dcmContext *context = dcmContextCreate(argv[1], argv[2]);
        if(context)
        {
            dcmContextDestroy(context);
        }
    }
    return 0;
}
