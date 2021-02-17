#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "lv2/log/log.h"
#include "lv2/log/logger.h"
#include "lv2/core/lv2.h"
#include "lv2/core/lv2_util.h"
#include "lv2/log/log.h"
#include "lv2/log/logger.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLUGIN_URI "http://moddevices.com/plugins/mod-devel/mod-log"

typedef enum {
    L_LEVEL     = 0,
} PortIndex;

typedef struct {
    //Control port
    float* level;

    //Log feature
    LV2_URID_Map*  map;
    LV2_Log_Logger logger;

    //variables
    int sample_counter;
    int sample_rate;
} Control;

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
        double                    rate,
        const char*               bundle_path,
        const LV2_Feature* const* features)
{
    Control* self = (Control*)malloc(sizeof(Control));

    // Get host features
    // clang-format off
    const char* missing = lv2_features_query(
            features,
            LV2_LOG__log,         &self->logger.log, false,
            LV2_URID__map,        &self->map,        true,
            NULL);
    // clang-format on

    lv2_log_logger_set_map(&self->logger, self->map);
    if (missing) {
        lv2_log_error(&self->logger, "Missing feature <%s>\n", missing);
        free(self);
        return NULL;
    }

    self->sample_rate = (int)rate;
    self->sample_counter = 0;

    return (LV2_Handle)self;
}

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
    Control* self = (Control*)instance;

    switch ((PortIndex)port) {
        case L_LEVEL:
            self->level = (float*)data;
            break;
    }
}

static void
activate(LV2_Handle instance)
{
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
    Control* self = (Control*) instance;

    if (self->sample_counter >= self->sample_rate) {
        lv2_log_error(&self->logger, "this is a error message!\n");
        lv2_log_warning(&self->logger, "this is a warning!\n");
        lv2_log_note(&self->logger, "this is a note!\n");
        lv2_log_trace(&self->logger, "this is a trace!\n");
        self->sample_counter = 0;
    }

    for (unsigned i = 0; i < n_samples; i++) {
        self->sample_counter++;
    }

}

static void
deactivate(LV2_Handle instance)
{
}

static void
cleanup(LV2_Handle instance)
{
    free(instance);
}

static const void*
extension_data(const char* uri)
{
    return NULL;
}

static const LV2_Descriptor descriptor = {
    PLUGIN_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    switch (index) {
        case 0:  return &descriptor;
        default: return NULL;
    }
}
