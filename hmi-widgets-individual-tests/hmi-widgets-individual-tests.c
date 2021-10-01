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

#define PLUGIN_URI "http://moddevices.com/plugins/mod-devel/hmi-widgets-individual-tests"

typedef enum {
    PARAMETER_BYPASS_LED = 0,
    PARAMETER_FOOT_LED,
    PARAMETER_FOOT_LABEL,
    PARAMETER_KNOB_LABEL,
    PARAMETER_KNOB_INDICATOR,
    PARAMETER_KNOB_VALUE,
    PARAMETER_KNOB_UNIT,
    PARAMETER_COUNT
} Parameters;

typedef struct {
    // Parameter stuff
    float* ports[PARAMETER_COUNT];
    float last[PARAMETER_COUNT];
    LV2_HMI_Addressing addressings[PARAMETER_COUNT];
    LV2_HMI_AddressingInfo infos[PARAMETER_COUNT];

    // Features
    LV2_URID_Map* map;
    LV2_Log_Logger logger;
    LV2_HMI_WidgetControl* hmi;

    // Internal state
    int sample_counter;
    int sample_overflow;
    bool reset;
} Control;

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
    Control* self = (Control*)calloc(1, sizeof(Control));

    // Get host features
    const char* missing = lv2_features_query(
            features,
            LV2_LOG__log,           &self->logger.log, false,
            LV2_URID__map,          &self->map,        true,
            LV2_HMI__WidgetControl, &self->hmi,        true,
            NULL);

    lv2_log_logger_set_map(&self->logger, self->map);

    if (missing) {
        lv2_log_error(&self->logger, "Missing feature <%s>\n", missing);
        free(self);
        return NULL;
    }

    self->sample_overflow = (int)rate * 60; // 60s, 1 minute

    return (LV2_Handle)self;
}

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
    Control* self = (Control*)instance;

    self->ports[port] = (float*)data;
}

static void
activate(LV2_Handle instance)
{
    Control* self = (Control*) instance;

    self->sample_counter = 0;
}

static LV2_HMI_LED_Colour
led_based_on_numaddrs(int numaddrs)
{
    return numaddrs > LV2_HMI_LED_Colour_White ? LV2_HMI_LED_Colour_White : numaddrs;
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
    Control* self = (Control*) instance;

    bool usedbypass = false;
    bool usedfootled = false;

    for (unsigned i = 0; i < n_samples; ++i, ++self->sample_counter)
    {
        if (self->sample_counter != self->sample_overflow && ! self->reset)
            continue;

        self->sample_counter = 0;
        self->reset = false;

        int numaddrs = 0;

        for (int j=0; j<PARAMETER_COUNT; ++j)
            if (self->addressings[j])
                ++numaddrs;

        for (int j=0; j<PARAMETER_COUNT; ++j)
        {
            if (! self->addressings[j])
                continue;

            const float value = *self->ports[j];
            LV2_HMI_Addressing addr = self->addressings[j];
            // LV2_HMI_AddressingInfo* info = &self->infos[j];

            switch (j)
            {
            case PARAMETER_BYPASS_LED:
                usedbypass = true;
                self->hmi->set_led(self->hmi->handle, addr,
                                   value > 0.5f ? led_based_on_numaddrs(numaddrs) : LV2_HMI_LED_Colour_Off, 0, 0);
                break;
            case PARAMETER_FOOT_LED:
                usedfootled = true;
                self->hmi->set_led(self->hmi->handle, addr, led_based_on_numaddrs(numaddrs),
                                   value > 0.5f ? 900 : 100, value > 0.5f ? 100 : 900);
                break;
            case PARAMETER_FOOT_LABEL:
                self->hmi->set_label(self->hmi->handle, addr, "x-label-x");
                break;
            case PARAMETER_KNOB_LABEL:
                self->hmi->set_label(self->hmi->handle, addr, "x-knob-x");
                break;
            case PARAMETER_KNOB_INDICATOR:
                self->hmi->set_indicator(self->hmi->handle, addr, (float)numaddrs / PARAMETER_COUNT);
                break;
            case PARAMETER_KNOB_VALUE:
                self->hmi->set_value(self->hmi->handle, addr, "x-value-x");
                break;
            case PARAMETER_KNOB_UNIT:
                self->hmi->set_unit(self->hmi->handle, addr, "x-unit-x");
                break;
            }
        }
    }

    for (int i=0; i<PARAMETER_COUNT; ++i)
    {
        const float value = *self->ports[i];

        if (self->last[i] == value)
            continue;

        self->last[i] = value;

        if (! self->addressings[i])
            continue;

        int numaddrs = 0;
        for (int j=0; j<PARAMETER_COUNT; ++j)
            if (self->addressings[j])
                ++numaddrs;

        LV2_HMI_Addressing addr = self->addressings[i];

        if (i == PARAMETER_BYPASS_LED && ! usedbypass)
            self->hmi->set_led(self->hmi->handle, addr,
                               value > 0.5f ? led_based_on_numaddrs(numaddrs) : LV2_HMI_LED_Colour_Off, 0, 0);

        if (i == PARAMETER_FOOT_LED && ! usedfootled)
            self->hmi->set_led(self->hmi->handle, addr, led_based_on_numaddrs(numaddrs),
                               value > 0.5f ? 900 : 100, value > 0.5f ? 100 : 900);
    }
}

static void
deactivate(LV2_Handle instance)
{
}

static void
cleanup(LV2_Handle instance)
{
    Control* self = (Control*) instance;

    for (int i=0; i<PARAMETER_COUNT; ++i)
        free((void*)self->infos[i].label);

    free(self);
}

static void
addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info)
{
    Control* self = (Control*) handle;

    self->infos[index] = *info;
    self->infos[index].label = strdup(info->label);

    self->addressings[index] = addressing;

    self->reset = true;
}

static void
unaddressed(LV2_Handle handle, uint32_t index)
{
    Control* self = (Control*) handle;

    self->addressings[index] = NULL;

    free((void*)self->infos[index].label);
    memset(&self->infos[index], 0, sizeof(self->infos[index]));

    self->reset = true;
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
