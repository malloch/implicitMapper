#include "logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Helper function to check if the OSC prefix matches.  Like strcmp(),
 * returns 0 if they match (up to the second '/'), non-0 otherwise.
 * Also optionally returns a pointer to the remainder of str1 after
 * the prefix. */
static int osc_prefix_cmp(const char *str1, const char *str2,
                          const char **rest)
{
    if (str1[0]!='/') {
        return 0;
    }
    if (str2[0]!='/') {
        return 0;
    }

    // skip first slash
    const char *s1=str1+1, *s2=str2+1;

    while (*s1 && (*s1)!='/') s1++;
    while (*s2 && (*s2)!='/') s2++;

    int n1 = s1-str1, n2 = s2-str2;
    if (n1!=n2) return 1;

    if (rest)
        *rest = s1;

    return strncmp(str1, str2, n1);
}

void clearSnapshots(implicitMapperData data)
{
    while (data->snapshots) {
        snapshot temp = data->snapshots->next;
        free(data->snapshots->inputs);
        free(data->snapshots->outputs);
        data->snapshots = temp;
    }
    data->numSnapshots = 0;
    // clear state of implicit mapping engine
    // clear UI indicator
    data->newIn = data->newOut = true;
}

int compare_signal_names(const void *l, const void *r)
{
    mapper_db_signal l_props = msig_properties(*(mapper_signal*)l);
    mapper_db_signal r_props = msig_properties(*(mapper_signal*)r);
    return strcmp(l_props->name, r_props->name);
}

void updateInputVectorPositions(implicitMapperData data)
{
    int i, k = 0, count;

    // store input signal pointers
    mapper_signal sigs[mdev_num_inputs(data->device) - 1];
    mapper_signal *psig = mdev_get_inputs(data->device);
    // start counting at index 1 to ignore signal "/CONNECT_HERE"
    for (i = 1; i < mdev_num_inputs(data->device); i++) {
        sigs[i-1] = psig[i];
    }

    // sort input signal pointer array
    qsort(sigs, mdev_num_inputs(data->device) - 1,
          sizeof(mapper_signal), compare_signal_names);

    // set offsets and user_data
    for (i = 0; i < mdev_num_inputs(data->device) - 1; i++) {
        mapper_db_signal props = msig_properties(sigs[i]);
        data->input.mapperSignals[i].offset = k;
        props->user_data = &data->input.mapperSignals[i];
        k += props->length;
    }
    count = k < MAX_LIST ? k : MAX_LIST;
    if (count != data->input.size && data->numSnapshots) {
        printf("implicitmap: input vector size has changed - resetting snapshots!");
        clearSnapshots(data);
    }
    data->input.size = count;
}

void updateOutputVectorPositions(implicitMapperData data)
{
    int i, k = 0, count;

    // store output signal pointers
    mapper_signal sigs[mdev_num_outputs(data->device) - 1];
    mapper_signal *psig = mdev_get_outputs(data->device);
    // start counting at index 1 to ignore signal "/CONNECT_HERE"
    for (i = 1; i < mdev_num_outputs(data->device); i++) {
        sigs[i-1] = psig[i];
    }

    // sort output signal pointer array
    qsort(sigs, mdev_num_outputs(data->device) - 1,
          sizeof(mapper_signal), compare_signal_names);

    // set offsets and user_data
    for (i = 0; i < mdev_num_outputs(data->device) - 1; i++) {
        mapper_db_signal props = msig_properties(sigs[i]);
        data->output.mapperSignals[i].offset = k;
        props->user_data = &data->output.mapperSignals[i];
        k += props->length;
    }
    count = k < MAX_LIST ? k : MAX_LIST;
    if (count != data->output.size && data->numSnapshots) {
        printf("implicitMapper: output vector size has changed - resetting snapshots!");
        clearSnapshots(data);
    }
    data->output.size = count;
}

void inputHandler(mapper_signal sig, mapper_db_signal props,
                  int instance_id, void *value, int count,
                  mapper_timetag_t *time)
{
    t_signal_ref *ref = (t_signal_ref*)props->user_data;
    implicitMapperData data = (implicitMapperData)ref->data;
    t_ioValue *input = &data->input;

    int j, offset, recalc;
    for (j=0; j < props->length; j++) {
        offset = ref->offset + j;
        recalc = 0;
        if (offset >= MAX_LIST) {
            printf("mapper: Maximum vector length exceeded!");
            break;
        }
        if (!value) {
            input->value[offset] = 0.f;
        }
        else if (props->type == 'f') {
            float *f = (float*)value;
            input->value[offset] = f[j];
        }
        else if (props->type == 'i') {
            int *i = (int*)value;
            input->value[offset] = (float)i[j];
        }
        // check ranges
        if (!input->init[offset]) {
            input->minima[offset] = input->maxima[offset] = input->value[offset];
            input->multiplier[offset] = 0;
            input->init[offset] = true;
        }
        else if (input->value[offset] < input->minima[offset]) {
            input->minima[offset] = input->value[offset];
            recalc = 1;
        }
        else if (input->value[offset] > input->maxima[offset]) {
            input->maxima[offset] = input->value[offset];
            recalc = 1;
        }
        if (recalc) {
            input->multiplier[offset] = 1 / (input->maxima[offset] - input->minima[offset]);
        }
    }
    data->newIn = true;
}

void queryHandler(mapper_signal sig, mapper_db_signal props,
                  int instance_id, void *value, int count,
                  mapper_timetag_t *time)
{
    t_signal_ref *ref = (t_signal_ref*)props->user_data;
    implicitMapperData data = (implicitMapperData)ref->data;
    if (!data)
        printf("pointer problem! %p", data);

    int j;
    for (j = 0; j < props->length; j++) {
        if (ref->offset+j >= MAX_LIST) {
            printf("mapper: Maximum vector length exceeded!");
            break;
        }
        if (!value)
            continue;
        else if (props->type == 'f') {
            float *f = (float*)value;
            data->snapshots->outputs[ref->offset+j] = f[j];
        }
        else if (props->type == 'i') {
            int *i = (int*)value;
            data->snapshots->outputs[ref->offset+j] = (float)i[j];
        }
        else if (props->type == 'd') {
            double *d = (double*)value;
            data->snapshots->outputs[ref->offset+j] = (float)d[j];
        }
    }

    data->queryCount --;

    if (data->queryCount == 0) {
        data->newOut = true;
    }
}


// *********************************************************
// -(link handler)------------------------------------------
void linkHandler(mapper_db_link lnk, mapper_db_action_t a, void *user_data)
{
    implicitMapperData data = (implicitMapperData)user_data;
    if (!data) {
        printf("error in connect handler: user_data is NULL");
        return;
    }
    if (!data->ready) {
        printf("error in connect handler: device not ready");
        return;
    }
    if (a == MDB_NEW) {
        // do not allow self-links
        if (strcmp(lnk->src_name, mdev_name(data->device)) == 0 &&
            strcmp(lnk->dest_name, mdev_name(data->device)) == 0) {
            mapper_monitor_unlink(data->monitor, lnk->src_name, lnk->dest_name);
        }
    }
}

void connectHandler(mapper_db_connection con, mapper_db_action_t a, void *user)
{
    // if connected involves current generic signal, create a new generic signal
    implicitMapperData data = (implicitMapperData)user;
    if (!data) {
        printf("error in connect handler: user_data is NULL");
        return;
    }
    if (!data->ready) {
        printf("error in connect handler: device not ready");
        return;
    }
    const char *signalName = 0;
    switch (a) {
    case MDB_NEW: {
        // check if applies to me
        if (!osc_prefix_cmp(con->src_name, mdev_name(data->device),
                            &signalName)) {
            if (strcmp(signalName, con->dest_name) == 0)
                return;
            if (mdev_num_outputs(data->device) >= MAX_LIST) {
                printf("Max outputs reached!");
                return;
            }
            // disconnect the generic signal
            mapper_monitor_disconnect(data->monitor, con->src_name,
                                      con->dest_name);

            // add a matching output signal
            mapper_signal msig;
            char str[256];
            int length = con->dest_length ? : 1;
            if (con->dest_min && con->dest_max)
                msig = mdev_add_output(data->device, con->dest_name, length,
                                       'f', 0, con->dest_min, con->dest_max);
            else {
                float min[length], max[length];
                for (int i = 0; i < length; i++) {
                    min[i] = 0.f;
                    max[i] = 1.f;
                }
                msig = mdev_add_output(data->device, con->dest_name,
                                       length, 'f', 0, min, max);
            }
            if (!msig) {
                printf("msig doesn't exist!");
                return;
            }
            msig_set_callback(msig, queryHandler, 0);
            // connect the new signal
            msig_full_name(msig, str, 256);
            mapper_db_connection_t props;
            props.mode = MO_BYPASS;
            mapper_monitor_connect(data->monitor, str, con->dest_name,
                                   &props, CONNECTION_MODE);

            updateOutputVectorPositions(data);

            data->updateLabels = true;
        }
        else if (!osc_prefix_cmp(con->dest_name, mdev_name(data->device),
                                 &signalName)) {
            if (strcmp(signalName, con->src_name) == 0)
                return;
            if (mdev_num_inputs(data->device) >= MAX_LIST) {
                printf("Max inputs reached!");
                return;
            }
            // disconnect the generic signal
            mapper_monitor_disconnect(data->monitor, con->src_name,
                                      con->dest_name);

            // create a matching input signal
            mapper_signal msig;
            char str[256];
            int length = con->src_length ? : 1;
            if (con->src_min && con->src_max)
                msig = mdev_add_input(data->device, con->src_name, length,
                                      'f', 0, con->src_min, con->src_max,
                                      inputHandler, 0);
            else {
                float min[length], max[length];
                for (int i = 0; i < length; i++) {
                    min[i] = 0.f;
                    max[i] = 1.f;
                }
                msig = mdev_add_input(data->device, con->src_name, length,
                                      'f', 0, min, max, inputHandler, 0);
            }
            if (!msig)
                return;
            // connect the new signal
            mapper_db_connection_t props;
            props.mode = MO_BYPASS;
            msig_full_name(msig, str, 256);
            mapper_monitor_connect(data->monitor, con->src_name, str,
                                   &props, CONNECTION_MODE);

            updateInputVectorPositions(data);

            data->updateLabels = true;
        }
        break;
    }
    case MDB_MODIFY:
        break;
    case MDB_REMOVE: {
        mapper_signal msig;
        // check if applies to me
        if (!(osc_prefix_cmp(con->dest_name, mdev_name(data->device),
                             &signalName))) {
            if (strcmp(signalName, "/CONNECT_HERE") == 0)
                return;
            if (strcmp(signalName, con->src_name) != 0)
                return;
            // find corresponding signal
            if (!(msig = mdev_get_input_by_name(data->device, signalName, 0))) {
                printf("error: input signal %s not found!", signalName);
                return;
            }
            // remove it
            mdev_remove_input(data->device, msig);
            updateInputVectorPositions(data);
        }
        else if (!(osc_prefix_cmp(con->src_name, mdev_name(data->device),
                                  &signalName))) {
            if (strcmp(signalName, "/CONNECT_HERE") == 0)
                return;
            if (strcmp(signalName, con->dest_name) != 0)
                return;
            // find corresponding signal
            if (!(msig = mdev_get_output_by_name(data->device, signalName, 0))) {
                printf("error: output signal %s not found", signalName);
                return;
            }
            // remove it
            mdev_remove_output(data->device, msig);
            updateOutputVectorPositions(data);
        }
        break;
    }
    default:
        break;
    }
}

void initIO(t_ioValue *x, void *data)
{
    x->size = 0;

    for (int i = 0; i < MAX_LIST; i++) {
        x->value[i] = 0.f;
        x->mapperSignals[i].data = data;
        x->init[i] = false;
    }
}

void randomizeDest(implicitMapperData data)
{
    printf("randomizeDest()\n");
    int i, j;
    float rand_val;
    mapper_db_signal props;
    if (!data->ready)
        return;

    mdev_now(data->device, &data->tt);
    mdev_start_queue(data->device, data->tt);
    mapper_signal *psig = mdev_get_outputs(data->device);
    t_ioValue *output = &data->output;
    for (i = 1; i < mdev_num_outputs(data->device); i ++) {
        props = msig_properties(psig[i]);
        t_signal_ref *ref = (t_signal_ref*)props->user_data;
        if (props->type == 'f') {
            float v[props->length];
            float *min = (float*)props->minimum, *max = (float*)props->maximum;
            for (j = 0; j < props->length; j++) {
                rand_val = (float)rand() / (float)RAND_MAX;
                if (min && max) {
                    v[j] = rand_val * (max[j] - min[j]) + min[j];
                }
                else {
                    // if ranges have not been declared, assume normalized between 0 and 1
                    v[j] = rand_val;
                }
                output->value[ref->offset+j] = v[j];
                printf("   set val[%i] to %f (f)\n", ref->offset+j, output->value[ref->offset+j]);
            }
            msig_update(psig[i], v, 1, data->tt);
        }
        else if (props->type == 'i') {
            int v[props->length];
            int *min = (int*)props->minimum, *max = (int*)props->maximum;
            for (j = 0; j < props->length; j++) {
                rand_val = (float)rand() / (float)RAND_MAX;
                if (props->minimum && props->maximum) {
                    v[j] = (int) (rand_val * (max[j] - min[j]) + min[j]);
                }
                else {
                    // if ranges have not been declared, assume normalized between 0 and 1
                    v[j] = (int) rand_val;
                }
                output->value[ref->offset+j] = (float)v[j];
                printf("   set val[%i] to %f (i)\n", ref->offset+j, output->value[ref->offset+j]);
            }
            msig_update(psig[i], v, 1, data->tt);
        }
        else if (props->type == 'd') {
            double v[props->length];
            double *min = (double*)props->minimum, *max = (double*)props->maximum;
            for (j = 0; j < props->length; j++) {
                rand_val = (float)rand() / (float)RAND_MAX;
                if (props->minimum && props->maximum) {
                    v[j] = rand_val * (max[j] - min[j]) + min[j];
                }
                else {
                    // if ranges have not been declared, assume normalized between 0 and 1
                    v[j] = rand_val;
                }
                output->value[ref->offset+j] = (float)v[j];
                printf("   set val[%i] to %f (d)\n", ref->offset+j, output->value[ref->offset+j]);
            }
            msig_update(psig[i], v, 1, data->tt);
        }
    }
    mdev_send_queue(data->device, data->tt);
    // update ranges
    for (i = 0; i < output->size; i++) {
        bool recalc = false;
        // check ranges
        if (!output->init[i]) {
            output->minima[i] = output->maxima[i] = output->value[i];
            output->multiplier[i] = 0;
            output->init[i] = true;
        }
        else if (output->value[i] < output->minima[i]) {
            output->minima[i] = output->value[i];
            recalc = 1;
        }
        else if (output->value[i] > output->maxima[i]) {
            output->maxima[i] = output->value[i];
            recalc = 1;
        }
        if (recalc) {
            output->multiplier[i] = 1 / (output->maxima[i] - output->minima[i]);
        }
    }
    data->newOut = 1;
}

void takeSnapshot(implicitMapperData data)
{
    // if previous snapshot still in progress, output current snapshot status
    if (data->queryCount) {
        printf("still waiting for last snapshot");
        return;
    }

    int i, num;
    mapper_signal *psig;
    data->queryCount = 0;

    // allocate a new snapshot
    snapshot new_snapshot = (snapshot) malloc(sizeof(t_snapshot));
    new_snapshot->id = data->numSnapshots++;
    new_snapshot->next = data->snapshots;
    new_snapshot->inputs = (float*) calloc(data->input.size, sizeof(float));
    new_snapshot->outputs = (float*) calloc(data->output.size, sizeof(float));
    data->snapshots = new_snapshot;

    // iterate through input signals and store their current values
    psig = mdev_get_inputs(data->device);
    num = mdev_num_inputs(data->device);
    for (i = 1; i < num; i++) {
        mapper_db_signal props = msig_properties(psig[i]);
        void *value = msig_value(psig[i], 0);
        t_signal_ref *ref = (t_signal_ref*) props->user_data;
        int length = (ref->offset + props->length < MAX_LIST
                      ? props->length
                      : MAX_LIST - ref->offset);
        // we can simply use memcpy here since all our signals are type 'f'
        memcpy(&data->snapshots->inputs[ref->offset], value, length*sizeof(float));
    }

    // iterate through output signals and query the remote ends
    psig = mdev_get_outputs(data->device);
    num = mdev_num_outputs(data->device);
    mdev_now(data->device, &data->tt);
    mdev_start_queue(data->device, data->tt);
    for (i = 1; i < num; i++) {
        // query the remote value
        data->queryCount += msig_query_remotes(psig[i], MAPPER_NOW);
    }
    mdev_send_queue(data->device, data->tt);
    printf("sent %i queries", data->queryCount);
}

void queryTimeout(implicitMapperData data)
{
    if (data->queryCount) {
        printf("query timeout! setting query count to 0 and outputting current values.");
        data->queryCount = 0;
    }
}
