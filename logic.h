#ifndef LOGIC_H
#define LOGIC_H

#include <mapper/mapper.h>

#define MAX_LIST 256

typedef struct _signal_ref
{
    void *data;
    int offset;
} t_signal_ref;

typedef struct _snapshot
{
    int id;
    float *inputs;
    float *outputs;
    struct _snapshot *next;
} t_snapshot, *snapshot;

typedef struct _ioValue
{
    int size;
    t_signal_ref mapperSignals[MAX_LIST];
    float value[MAX_LIST];
    float minima[MAX_LIST];
    float maxima[MAX_LIST];
    float multiplier[MAX_LIST];
//    float offset[MAX_LIST];
    bool init[MAX_LIST];
} t_ioValue;

typedef struct _implicitMapperData {
    mapper_admin admin;
    mapper_device device;
    mapper_monitor monitor;
    mapper_db db;
    mapper_timetag_t tt;
    bool ready;
    bool mute;
    bool newIn;
    bool newOut;
    int queryCount;
    t_ioValue input;
    t_ioValue output;
    int numSnapshots;
    snapshot snapshots;
    bool updateLabels;
} implicitMapperData_t, *implicitMapperData;

// function prototypes
void linkHandler(mapper_db_link lnk, mapper_db_action_t a, void *user_data);
void connectHandler(mapper_db_connection con, mapper_db_action_t a, void *user);
void initIO(t_ioValue *x, void *data);
void clearSnapshots(implicitMapperData data);
void randomizeDest(implicitMapperData data);
void takeSnapshot(implicitMapperData data);
void queryTimeout(implicitMapperData data);


#endif // LOGIC_H
