#include "dcmSnapshotMap.h"

#include "dyn.h"

#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------

static int cloneEntry(dynMap *dm, dynMapEntry *e, dynMap *newMap)
{
    // Note: reuses e->keyStr as raw key ptr (owned by sm->allocations)
    dmGetS2P(newMap, e->keyStr) = dmEntryDefaultData(e)->valuePtr;
}

dcmSnapshot *dcmSnapshotCreate(dcmSnapshot *prev)
{
    dcmSnapshot *snapshot = calloc(1, sizeof(dcmSnapshot));
    snapshot->m = dmCreate(DKF_STRING | DKF_UNOWNED_KEYS, 0);
    if(prev)
    {
        snapshot->generation = prev->generation + 1;
        dmIterate(prev->m, cloneEntry, snapshot->m);
    }
    else
    {
        snapshot->generation = 0;
    }
    return snapshot;
}

void dcmSnapshotDestroy(dcmSnapshot *snapshot)
{
    dmDestroy(snapshot->m, NULL);
    free(snapshot);
}

const char *dcmSnapshotGetString(dcmSnapshot *snapshot, const char *key)
{
    // avoid calling dmGetS2P before we know the entry is present to avoid
    // accidentally stashing off a pointer to key permanently.
    if(dmHasS(snapshot->m, key))
    {
        return dmGetS2P(snapshot->m, key);
    }
    return "";
}

// ---------------------------------------------------------------------------

dcmSnapshotMap *dcmSnapshotMapCreate()
{
    dcmSnapshotMap *sm = calloc(1, sizeof(dcmSnapshotMap));
    dcmSnapshot *snapshot = dcmSnapshotCreate(NULL);
    daPush(&sm->snapshots, snapshot);
    return sm;
}

void dcmSnapshotMapDestroy(dcmSnapshotMap *sm)
{
    daDestroy(&sm->snapshots, dcmSnapshotDestroy);
    daDestroy(&sm->allocations, free);
    free(sm);
}

static dcmSnapshot *dcmSnapshotMapTop(dcmSnapshotMap *sm)
{
    return sm->snapshots[daSize(&sm->snapshots) - 1];
}

dcmSnapshot *dcmSnapshotMapFreeze(dcmSnapshotMap *sm)
{
    dcmSnapshot *snapshot = dcmSnapshotMapTop(sm);
    snapshot->frozen = 1;
    return snapshot;
}

const char *dcmSnapshotMapGetString(dcmSnapshotMap *sm, const char *key)
{
    return dcmSnapshotGetString(dcmSnapshotMapTop(sm), key);
}

void dcmSnapshotMapSetString(dcmSnapshotMap *sm, const char *key, const char *val)
{
    dcmSnapshot *snapshot = dcmSnapshotMapTop(sm);
    if(snapshot->frozen)
    {
        dcmSnapshot *newSnapshot = dcmSnapshotCreate(snapshot);
        daPush(&sm->snapshots, newSnapshot);
        snapshot = newSnapshot;
    }

    if(!dmHasS(snapshot->m, key))
    {
        key = strdup(key);
        daPush(&sm->allocations, key);
    }
    dmGetS2P(snapshot->m, key) = strdup(val);
    daPush(&sm->allocations, val);
}
