#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "lv2/log/log.h"
#include "lv2/log/logger.h"
#include "lv2/core/lv2.h"
#include "lv2/core/lv2_util.h"
#include "lv2/log/log.h"
#include "lv2/log/logger.h"

#include "lv2-hmi.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLUGIN_URI "http://moddevices.com/plugins/mod-devel/mod-log"

typedef enum {
    L_LEVEL = 0,
    L_TRIGGER,
} PortIndex;

typedef struct {
    // Control ports
    float* level;
    float* trigger;

    // Features
    LV2_URID_Map*  map;
    LV2_Log_Logger logger;
    LV2_HMI_WidgetControl* hmi;

    // Internal state
    int sample_counter;
    int sample_rate;

    // HMI Widgets stuff
    LV2_HMI_Addressing addressing;
    LV2_HMI_LED_Colour colour;
} Control;

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
    Control* self = (Control*)calloc(sizeof(Control), 1);

    // Get host features
    // clang-format off
    const char* missing = lv2_features_query(
            features,
            LV2_LOG__log,           &self->logger.log, false,
            LV2_URID__map,          &self->map,        true,
            LV2_HMI__WidgetControl, &self->hmi,        true,
            NULL);
    // clang-format on

    lv2_log_logger_set_map(&self->logger, self->map);

    if (missing) {
        lv2_log_error(&self->logger, "Missing feature <%s>\n", missing);
        free(self);
        return NULL;
    }

    self->sample_rate = (int)rate;
    self->colour = LV2_HMI_LED_Colour_Off;

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
        case L_TRIGGER:
            self->trigger = (float*)data;
            break;
    }
}

static void
activate(LV2_Handle instance)
{
    Control* self = (Control*) instance;

    self->sample_counter = 0;
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
    Control* self = (Control*) instance;

    for (unsigned i = 0; i < n_samples; ++i, ++self->sample_counter) {
        if (self->sample_counter == self->sample_rate) {
            /*
            lv2_log_error(&self->logger, "this is a error message!\n");
            lv2_log_warning(&self->logger, "this is a warning!\n");
            lv2_log_note(&self->logger, "this is a note!\n");
            */
            self->sample_counter = 0;

            if (self->addressing != NULL) {
                if (++self->colour > LV2_HMI_LED_Colour_White) {
                    self->colour = LV2_HMI_LED_Colour_Off;
                }
                lv2_log_trace(&self->logger, "HMI set color to %i", self->colour);
                if ((int)*self->trigger)
                    self->hmi->set_led(self->hmi->handle, self->addressing, self->colour, 100, 100);
                else
                    self->hmi->set_led(self->hmi->handle, self->addressing, self->colour, 0, 0);
            } else {
                lv2_log_trace(&self->logger, "HMI not addressed");
            }
        }
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

static void
addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info)
{
    Control* self = (Control*) handle;
    self->addressing = addressing;
}

static void
unaddressed(LV2_Handle handle, uint32_t index)
{
    if (index != 0)
        return;

    Control* self = (Control*) handle;
    self->addressing = NULL;
}

static const void*
extension_data(const char* uri)
{
    static const LV2_HMI_PluginNotification hmiNotif = {
        addressed,
        unaddressed,
    };
    if (!strcmp(uri, LV2_HMI__PluginNotification))
        return &hmiNotif;
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
