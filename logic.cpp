#include "logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
    return strcmp(mapper_signal_name(*(mapper_signal*)l),
                  mapper_signal_name(*(mapper_signal*)r));
}

void updateInputVectorPositions(implicitMapperData data)
{
    printf("updateInputVectorPositions()\n");
    int i, k = 0, count;
    int num_inputs = mapper_device_num_signals(data->device, MAPPER_DIR_INCOMING) - 1;

    // store input signal pointers
    mapper_signal sigs[num_inputs];
    mapper_signal *psig = mapper_device_signals(data->device, MAPPER_DIR_INCOMING);

    i = 0;
    while (psig) {
        if (*psig != data->dummy_input)
            sigs[i++] = *psig;
        psig = mapper_signal_query_next(psig);
    }

    // sort input signal pointer array
    qsort(sigs, num_inputs, sizeof(mapper_signal), compare_signal_names);

    // set offsets and user_data
    for (i = 0; i < num_inputs; i++) {
        data->input.mapperSignals[i].offset = k;
        mapper_signal_set_user_data(sigs[i], &data->input.mapperSignals[i]);
        k += mapper_signal_length(sigs[i]);
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
    int num_outputs = mapper_device_num_signals(data->device, MAPPER_DIR_OUTGOING) - 1;

    // store output signal pointers
    mapper_signal sigs[num_outputs];
    mapper_signal *psig = mapper_device_signals(data->device, MAPPER_DIR_OUTGOING);

    i = 0;
    while (psig) {
        if (*psig != data->dummy_output)
            sigs[i++] = *psig;
        psig = mapper_signal_query_next(psig);
    }

    // sort output signal pointer array
    qsort(sigs, num_outputs, sizeof(mapper_signal), compare_signal_names);

    // set offsets and user_data
    for (i = 0; i < num_outputs; i++) {
        data->output.mapperSignals[i].offset = k;
        mapper_signal_set_user_data(sigs[i], &data->output.mapperSignals[i]);
        k += mapper_signal_length(sigs[i]);
    }
    count = k < MAX_LIST ? k : MAX_LIST;
    if (count != data->output.size && data->numSnapshots) {
        printf("implicitMapper: output vector size has changed - resetting snapshots!");
        clearSnapshots(data);
    }
    data->output.size = count;
}

void inputHandler(mapper_signal sig, mapper_id instance_id, const void *value,
                  int count, mapper_timetag_t *time)
{
    t_signal_ref *ref = (t_signal_ref*)mapper_signal_user_data(sig);
    if (!ref) {
        printf("inputHandler: missing user_data for signal '%s'\n",
               mapper_signal_name(sig));
        return;
    }
    implicitMapperData data = (implicitMapperData)ref->data;
    t_ioValue *input = &data->input;

    int j, offset, recalc;
    char type = mapper_signal_type(sig);
    for (j=0; j < mapper_signal_length(sig); j++) {
        offset = ref->offset + j;
        recalc = 0;
        if (offset >= MAX_LIST) {
            printf("mapper: Maximum vector length exceeded!");
            break;
        }
        if (!value) {
            input->value[offset] = 0.f;
        }
        else if (type == 'f') {
            float *f = (float*)value;
            input->value[offset] = f[j];
        }
        else if (type == 'i') {
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

void queryHandler(mapper_signal sig,  mapper_id instance_id, const void *value,
                  int count, mapper_timetag_t *time)
{
    t_signal_ref *ref = (t_signal_ref*)mapper_signal_user_data(sig);
    if (!ref) {
        printf("queryHandler: missing user_data for signal '%s'\n",
               mapper_signal_name(sig));
        return;
    }
    implicitMapperData data = (implicitMapperData)ref->data;

    int j;
    char type = mapper_signal_type(sig);
    for (j = 0; j < mapper_signal_length(sig); j++) {
        if (ref->offset+j >= MAX_LIST) {
            printf("mapper: Maximum vector length exceeded!");
            break;
        }
        if (!value)
            continue;
        else if (type == 'f') {
            float *f = (float*)value;
            data->snapshots->outputs[ref->offset+j] = f[j];
        }
        else if (type == 'i') {
            int *i = (int*)value;
            data->snapshots->outputs[ref->offset+j] = (float)i[j];
        }
        else if (type == 'd') {
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
// -(map handler)------------------------------------------
void mapHandler(mapper_device dev, mapper_map map, mapper_record_event e)
{
    // if connected involves current generic signal, create a new generic signal
    implicitMapperData data = (implicitMapperData)mapper_device_user_data(dev);
    if (!data) {
        printf("error in map handler: user_data is NULL");
        return;
    }
    if (!data->ready) {
        printf("error in map handler: device not ready");
        return;
    }

    // retrieve devices and signals
    mapper_slot slot = mapper_map_slot(map, MAPPER_LOC_SOURCE, 0);
    mapper_signal src_sig = mapper_slot_signal(slot);
    mapper_device src_dev = mapper_signal_device(src_sig);
    slot = mapper_map_slot(map, MAPPER_LOC_DESTINATION, 0);
    mapper_signal dst_sig = mapper_slot_signal(slot);
    mapper_device dst_dev = mapper_signal_device(dst_sig);

    // sanity check: don't allow self-connections
    if (src_dev == dst_dev) {
        mapper_map_release(map);
        return;
    }

    char full_name[256];

    if (e == MAPPER_ADDED) {
        // check if applies to me
        if (src_dev == data->device) {
            snprintf(full_name, 256, "%s/%s", mapper_device_name(dst_dev),
                     mapper_signal_name(dst_sig));
            if (strcmp(mapper_signal_name(src_sig), full_name) == 0) {
                // <thisDev>:<dstDevName>/<dstSigName> -> <dstDev>:<dstSigName>
                return;
            }
            if (mapper_device_num_signals(data->device, MAPPER_DIR_OUTGOING) >= MAX_LIST) {
                printf("Max outputs reached!");
                return;
            }

            // unmap the generic signal
            mapper_map_release(map);

            // add a matching output signal
            int i, length = mapper_signal_length(dst_sig) ?: 1;
            char type = mapper_signal_type(dst_sig);
            void *min = mapper_signal_minimum(dst_sig);
            void *max = mapper_signal_maximum(dst_sig);

            float *minf = 0, *maxf = 0;
            if (type == 'f') {
                minf = (float*)min;
                maxf = (float*)max;
            }
            else {
                if (min) {
                    minf = (float*)alloca(length * sizeof(float));
                    if (type == 'i') {
                        int *mini = (int*)min;
                        for (i = 0; i < length; i++)
                            minf[i] = (float)mini[i];
                    }
                    else if (type == 'd') {
                        double *mind = (double*)min;
                        for (i = 0; i < length; i++)
                            minf[i] = (float)mind[i];
                    }
                    else
                        minf = 0;
                }
                if (max) {
                    maxf = (float*)alloca(length * sizeof(float));
                    if (type == 'i') {
                        int *maxi = (int*)max;
                        for (i = 0; i < length; i++)
                            maxf[i] = (float)maxi[i];
                    }
                    else if (type == 'd') {
                        double *maxd = (double*)max;
                        for (i = 0; i < length; i++)
                            maxf[i] = (float)maxd[i];
                    }
                    else
                        maxf = 0;
                }
            }
            src_sig = mapper_device_add_output_signal(data->device, full_name,
                                                      length, 'f', 0, minf, maxf);
            if (!src_sig) {
                printf("error creating new source signal!");
                return;
            }
            mapper_signal_set_callback(src_sig, queryHandler);

            // map the new signal
            map = mapper_map_new(1, &src_sig, 1, &dst_sig);
            mapper_map_set_mode(map, MAPPER_MODE_EXPRESSION);
            mapper_map_set_expression(map, "y=x");
            mapper_map_push(map);

            updateOutputVectorPositions(data);

            data->updateLabels = true;
        }
        else if (dst_dev == data->device) {
            snprintf(full_name, 256, "%s/%s", mapper_device_name(src_dev),
                     mapper_signal_name(src_sig));
            if (strcmp(mapper_signal_name(dst_sig), full_name) == 0) {
                // <srcDevName>:<srcSigName> -> <thisDev>:<srcDevName>/<srcSigName>
                return;
            }
            if (mapper_device_num_signals(data->device, MAPPER_DIR_INCOMING) >= MAX_LIST) {
                printf("Max inputs reached!");
                return;
            }
            // unmap the generic signal
            mapper_map_release(map);

            // add a matching input signal
            int i, length = mapper_signal_length(src_sig);
            char type = mapper_signal_type(src_sig);
            void *min = mapper_signal_minimum(src_sig);
            void *max = mapper_signal_maximum(src_sig);

            float *minf = 0, *maxf = 0;
            if (type == 'f') {
                minf = (float*)min;
                maxf = (float*)max;
            }
            else {
                if (min) {
                    minf = (float*)alloca(length * sizeof(float));
                    if (type == 'i') {
                        int *mini = (int*)min;
                        for (i = 0; i < length; i++)
                            minf[i] = (float)mini[i];
                    }
                    else if (type == 'd') {
                        double *mind = (double*)min;
                        for (i = 0; i < length; i++)
                            minf[i] = (float)mind[i];
                    }
                    else
                        minf = 0;
                }
                if (max) {
                    maxf = (float*)alloca(length * sizeof(float));
                    if (type == 'i') {
                        int *maxi = (int*)max;
                        for (i = 0; i < length; i++)
                            maxf[i] = (float)maxi[i];
                    }
                    else if (type == 'd') {
                        double *maxd = (double*)max;
                        for (i = 0; i < length; i++)
                            maxf[i] = (float)maxd[i];
                    }
                    else
                        maxf = 0;
                }
            }
            dst_sig = mapper_device_add_input_signal(data->device, full_name,
                                                     length, 'f', 0, minf, maxf,
                                                     inputHandler, 0);
            if (!dst_sig) {
                printf("error creating new destination signal!");
                return;
            }

            // map the new signal
            map = mapper_map_new(1, &src_sig, 1, &dst_sig);
            mapper_map_set_mode(map, MAPPER_MODE_EXPRESSION);
            mapper_map_set_expression(map, "y=x");
            mapper_map_push(map);

            updateInputVectorPositions(data);
            data->updateLabels = true;
        }
    }
    else if (e == MAPPER_REMOVED) {
        if (src_sig == data->dummy_input
                || src_sig == data->dummy_output
                || dst_sig == data->dummy_input
                || dst_sig == data->dummy_output)
            return;
        if (src_dev == data->device) {
            snprintf(full_name, 256, "%s/%s", mapper_device_name(dst_dev),
                     mapper_signal_name(dst_sig));
            if (strcmp(mapper_signal_name(src_sig), full_name) != 0)
                return;
            // remove signal
            mapper_device_remove_signal(data->device, src_sig);
            updateInputVectorPositions(data);
        }
        else if (dst_dev == data->device) {
            snprintf(full_name, 256, "%s/%s", mapper_device_name(src_dev),
                     mapper_signal_name(src_sig));
            if (strcmp(mapper_signal_name(dst_sig), full_name) != 0)
                return;
            // remove signal
            mapper_device_remove_signal(data->device, dst_sig);
            updateOutputVectorPositions(data);
        }
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
    if (!data->ready)
        return;

    mapper_timetag_now(&data->tt);
    mapper_device_start_queue(data->device, data->tt);
    mapper_signal *psig = mapper_device_signals(data->device, MAPPER_DIR_OUTGOING);
    t_ioValue *output = &data->output;

    while (psig) {
        if (*psig == data->dummy_output) {
            psig = mapper_signal_query_next(psig);
            continue;
        }
        t_signal_ref *ref = (t_signal_ref*)mapper_signal_user_data(*psig);
        int length = mapper_signal_length(*psig);
        float v[length];
        float *min = (float*)mapper_signal_minimum(*psig);
        float *max = (float*)mapper_signal_maximum(*psig);
        for (j = 0; j < length; j++) {
            rand_val = (float)rand() / (float)RAND_MAX;
            if (min && max) {
                printf("   using range %f : %f\n", min[j], max[j]);
                v[j] = rand_val * (max[j] - min[j]) + min[j];
            }
            else {
                // if ranges have not been declared, assume normalized between 0 and 1
                v[j] = rand_val;
            }
            output->value[ref->offset+j] = v[j];
            printf("   set val[%i] to %f (f)\n", ref->offset+j, output->value[ref->offset+j]);
        }
        mapper_signal_update(*psig, v, 1, data->tt);
        psig = mapper_signal_query_next(psig);
    }
    mapper_device_send_queue(data->device, data->tt);
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
    psig = mapper_device_signals(data->device, MAPPER_DIR_INCOMING);
    while (psig) {
        if (*psig != data->dummy_input) {
            const void *value = mapper_signal_value(*psig, 0);
            t_signal_ref *ref = (t_signal_ref*) mapper_signal_user_data(*psig);
            int length = mapper_signal_length(*psig);
            length = (ref->offset + length < MAX_LIST ? length : MAX_LIST - ref->offset);
            // we can simply use memcpy here since all our signals are type 'f'
            memcpy(&data->snapshots->inputs[ref->offset], value, length*sizeof(float));
        }
        psig = mapper_signal_query_next(psig);
    }

    mapper_timetag_now(&data->tt);
    mapper_device_start_queue(data->device, data->tt);

    // iterate through output signals and query the remote ends
    psig = mapper_device_signals(data->device, MAPPER_DIR_OUTGOING);
    while (psig) {
        if (*psig != data->dummy_output) {
            // query the remote value
            data->queryCount += mapper_signal_query_remotes(*psig, MAPPER_NOW);
        }
        psig = mapper_signal_query_next(psig);
    }

    mapper_device_send_queue(data->device, data->tt);
    printf("sent %i queries", data->queryCount);
}

void queryTimeout(implicitMapperData data)
{
    if (data->queryCount) {
        printf("query timeout! setting query count to 0 and outputting current values.");
        data->queryCount = 0;
    }
}
