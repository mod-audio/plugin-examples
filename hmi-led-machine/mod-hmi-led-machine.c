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
#include "circular_buffer.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLUGIN_URI "http://moddevices.com/plugins/mod-devel/mod-hmi-led-machine"

float map(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

typedef enum {
    CVINP = 0,
    AUDIOIN,
    TOGGLE,
    COLOR,
    LED_MODE,
    BLINK_MODE,
    ON_TIME,
    OFF_TIME,
    SYNC_BLINK_MODE,
    BRIGHTNESS_MODE,
    BRIGHTNESS,
    BRIGHTNESS_LIST,
    UPDATE
} PortIndex;

typedef struct {
    // Input signals
    float* cv_input;
    float* audio_input;

    //controls
    float* toggle;
    float* color;
    float* led_mode;
    float* blink_mode;
    float* on_time;
    float* off_time;
    float* blink_sync;
    float* brighness_mode;
    float* brighness;
    float* brightness_list;

    //this control sends the update, so we dont keep spamming
    float *update;

    //used to calculate audio power
    ringbuffer_t buffer;

    // Features
    LV2_URID_Map*  map;
    LV2_Log_Logger logger;
    LV2_HMI_WidgetControl* hmi;

    // HMI Widgets stuff
    LV2_HMI_Addressing toggle_addressing;
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
        case CVINP:
            self->cv_input = (float*)data;
            break;
        case AUDIOIN:
            self->audio_input = (float*)data;
            break;
        case TOGGLE:
            self->toggle = (float*)data;
            break;
        case COLOR:
            self->color = (float*)data;
            break;
        case LED_MODE:
            self->led_mode = (float*)data;
            break;
        case BLINK_MODE:
            self->blink_mode = (float*)data;
            break;
        case ON_TIME:
            self->on_time = (float*)data;
            break;
        case OFF_TIME:
            self->off_time = (float*)data;
            break;
        case SYNC_BLINK_MODE:
            self->blink_sync = (float*)data;
            break;
        case BRIGHTNESS_MODE:
            self->brighness_mode = (float*)data;
            break;
        case BRIGHTNESS:
            self->brighness = (float*)data;
            break;
        case BRIGHTNESS_LIST:
            self->brightness_list = (float*)data;
            break;
        case UPDATE:
            self->update = (float*)data;
            break;
    }
}

static void
activate(LV2_Handle instance)
{
    Control* self = (Control*) instance;
    ringbuffer_clear(&self->buffer, 128);
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
    Control* self = (Control*) instance;

    uint16_t q;
    float audio_buffer_pwr = 0;
    //keep up with buffer
    for (q = 0; q < n_samples; q++)
        audio_buffer_pwr = ringbuffer_push_and_calculate_power(&self->buffer, self->audio_input[q]);

    //do we need to send a new command?
    if (((uint8_t)*self->update) && (self->toggle_addressing != NULL)) {
        self->colour = (uint8_t)*self->color;

        //brightness command
        if ((uint8_t)*self->led_mode) {
            //manual brightness with control
            if ((uint8_t)*self->brighness_mode == 0) {
                self->hmi->set_led_with_brightness(self->hmi->handle, self->toggle_addressing, self->colour, (uint8_t)*self->brighness);
            }
            //manual brightness with list
            else if ((uint8_t)*self->brighness_mode == 1) {
                switch((uint8_t)*self->brightness_list) {
                    //low
                    case 0:
                        self->hmi->set_led_with_brightness(self->hmi->handle, self->toggle_addressing, self->colour, LV2_HMI_LED_Brightness_Low);
                    break;

                    //mid
                    case 1:
                        self->hmi->set_led_with_brightness(self->hmi->handle, self->toggle_addressing, self->colour, LV2_HMI_LED_Brightness_Mid);
                    break;

                    //full
                    case 2:
                        self->hmi->set_led_with_brightness(self->hmi->handle, self->toggle_addressing, self->colour, LV2_HMI_LED_Brightness_High);
                    break;
                } 
            }
            //other brightness is monitored, happens below
        }
        //blink command
        else {
            //synced blink led
            if ((uint8_t)*self->blink_mode) {
                switch((uint8_t)*self->blink_sync) {
                    //slow
                    case 0:
                        self->hmi->set_led_with_blink(self->hmi->handle, self->toggle_addressing, self->colour, LV2_HMI_LED_Blink_Slow, 0);
                    break;

                    //mid
                    case 1:
                        self->hmi->set_led_with_blink(self->hmi->handle, self->toggle_addressing, self->colour, LV2_HMI_LED_Blink_Mid, 0);
                    break;

                    //fast
                    case 2:
                        self->hmi->set_led_with_blink(self->hmi->handle, self->toggle_addressing, self->colour, LV2_HMI_LED_Blink_Fast, 0);
                    break;
                }
            }
            //manual led blink
            else {
                self->hmi->set_led_with_blink(self->hmi->handle, self->toggle_addressing, self->colour, (uint16_t)*self->on_time, (uint16_t)*self->off_time);
            }
        }
    }

    //are we monitoring a signal? then change LED too
    //only once a buffer now, this is just to demo
    if (self->toggle_addressing != NULL) {
        uint8_t brightness_val = 100;
        //audio
        if ((uint8_t)*self->brighness_mode == 2) {
            brightness_val = map(audio_buffer_pwr, 0, 1, 0, 100);
        }
        //cv
        else if ((uint8_t)*self->brighness_mode == 3) {
            brightness_val = map(self->cv_input[0], 0, 10, 0, 100);
        }

        if (((uint8_t)*self->brighness_mode >= 2) && ((uint8_t)*self->led_mode))
            self->hmi->set_led_with_brightness(self->hmi->handle, self->toggle_addressing, self->colour, brightness_val);
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

    if (index == 2)
        self->toggle_addressing = addressing;
}

static void
unaddressed(LV2_Handle handle, uint32_t index)
{
    Control* self = (Control*) handle;

    if (index == 2)
        self->toggle_addressing = NULL;
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
