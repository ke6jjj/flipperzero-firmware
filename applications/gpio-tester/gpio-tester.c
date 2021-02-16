#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

typedef struct {
    const char* name;
    GpioPin pin;
} GpioItem;

const GpioItem GPIO_PINS[] = {
    {"1.2: PA7", {GPIOA, GPIO_PIN_7}},
    {"1.3: PA6", {GPIOA, GPIO_PIN_6}},
    {"1.4: PA4", {GPIOA, GPIO_PIN_4}},
    {"1.5: PB3", {GPIOB, GPIO_PIN_3}},
    {"1.6: PB2", {GPIOB, GPIO_PIN_2}},
    {"1.7: PC3", {GPIOC, GPIO_PIN_3}},

    {"2.7: PC1", {GPIOC, GPIO_PIN_1}},
    {"2.8: PC0", {GPIOC, GPIO_PIN_0}},
};

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    union {
        InputEvent input;
    } value;
    EventType type;
} AppEvent;

typedef struct {
    uint8_t gpio_index;
} State;

static void render_callback(Canvas* canvas, void* ctx) {
    State* state = (State*)acquire_mutex((ValueMutex*)ctx, 25);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "GPIO demo");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 25, GPIO_PINS[state->gpio_index].name);

    release_mutex((ValueMutex*)ctx, state);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    osMessageQueueId_t event_queue = ctx;

    AppEvent event;
    event.type = EventTypeKey;
    event.value.input = *input_event;
    osMessageQueuePut(event_queue, &event, 0, 0);
}

int32_t app_gpio_test(void* p) {
    osMessageQueueId_t event_queue = osMessageQueueNew(8, sizeof(AppEvent), NULL);
    furi_check(event_queue);

    State _state;
    _state.gpio_index = 0;

    ValueMutex state_mutex;
    if(!init_mutex(&state_mutex, &_state, sizeof(State))) {
        printf("[gpio-tester] cannot create mutex\r\n");
        return 255;
    }

    ViewPort* view_port = view_port_alloc();

    view_port_draw_callback_set(view_port, render_callback, &state_mutex);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // configure pin
    for(uint8_t i = 0; i < sizeof(GPIO_PINS) / sizeof(GPIO_PINS[0]); i++) {
        gpio_init((GpioPin*)&GPIO_PINS[i].pin, GpioModeOutputPushPull);
    }

    gpio_init((GpioPin*)&led_gpio[1], GpioModeOutputOpenDrain);

    AppEvent event;
    while(1) {
        osStatus_t event_status = osMessageQueueGet(event_queue, &event, NULL, osWaitForever);
        State* state = (State*)acquire_mutex_block(&state_mutex);

        if(event_status == osOK) {
            if(event.type == EventTypeKey) {
                if(event.value.input.type == InputTypeShort &&
                   event.value.input.key == InputKeyBack) {
                    printf("[gpio-tester] bye!\r\n");
                    // TODO remove all view_ports create by app
                    view_port_enabled_set(view_port, false);
                    return 0;
                }

                if(event.value.input.type == InputTypeShort &&
                   event.value.input.key == InputKeyRight) {
                    if(state->gpio_index < (sizeof(GPIO_PINS) / sizeof(GPIO_PINS[0]) - 1)) {
                        state->gpio_index++;
                    }
                }

                if(event.value.input.type == InputTypeShort &&
                   event.value.input.key == InputKeyLeft) {
                    if(state->gpio_index > 0) {
                        state->gpio_index--;
                    }
                }

                if(event.value.input.key == InputKeyOk) {
                    if(event.value.input.type == InputTypePress) {
                        gpio_write((GpioPin*)&GPIO_PINS[state->gpio_index].pin, true);
                        gpio_write((GpioPin*)&led_gpio[1], false);
                    } else if(event.value.input.type == InputTypeRelease) {
                        gpio_write((GpioPin*)&GPIO_PINS[state->gpio_index].pin, false);
                        gpio_write((GpioPin*)&led_gpio[1], true);
                    }
                }
            }
        }

        release_mutex(&state_mutex, state);
        view_port_update(view_port);
    }

    return 0;
}