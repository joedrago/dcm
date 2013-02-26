#ifndef DCMSNAPSHOTMAP_H
#define DCMSNAPSHOTMAP_H

struct dynMap;

typedef struct dcmSnapshot
{
    struct dynMap *m;
    int frozen;
    int generation;
} dcmSnapshot;

dcmSnapshot *dcmSnapshotCreate(dcmSnapshot *prev);
void dcmSnapshotDestroy(dcmSnapshot *snapshot);
const char *dcmSnapshotGetString(dcmSnapshot *snapshot, const char *key);

typedef struct dcmSnapshotMap
{
    dcmSnapshot **snapshots;
    char **allocations;
} dcmSnapshotMap;

dcmSnapshotMap *dcmSnapshotMapCreate();
void dcmSnapshotMapDestroy(dcmSnapshotMap *sm);
dcmSnapshot *dcmSnapshotMapFreeze(dcmSnapshotMap *sm);
const char *dcmSnapshotMapGetString(dcmSnapshotMap *sm, const char *key);
void dcmSnapshotMapSetString(dcmSnapshotMap *sm, const char *key, const char *val);

#endif
