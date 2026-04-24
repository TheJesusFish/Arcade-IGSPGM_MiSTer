#include "gui.h"
#include "input.h"
#include "tilemap.h"
#include "util.h"

#define GUI_TYPE_NULL 0
#define GUI_TYPE_U8 1
#define GUI_TYPE_U16 2
#define GUI_TYPE_BITS16 3
#define GUI_TYPE_BOOL 4
#define GUI_TYPE_BUTTON 5

typedef struct
{
    char label[15];
    uint8_t type;
    uint16_t value;
    uint16_t override;
} GUIEntry;

typedef struct
{
    uint16_t start_magic;
    uint16_t lock;
    uint16_t count;

    GUIEntry entries[32];

    uint16_t end_magic;
} GUIData;

__attribute__ ((section(".gui_buffer"))) __attribute__((used))
GUIData gui_data;


static u16 active_id = 0;
static u16 last_id = 0;
static u16 cur_x, cur_y, cur_id;
static u16 label_width, max_label_width;
static u16 dirty_x, dirty_y;

#define ACTIVE_COLOR 1
#define INACTIVE_COLOR 0
#define LABEL_COLOR 0

#define MAGIC 0xAB7D

static void end_element()
{
    last_id = cur_id;
    cur_y++;
    dirty_y = cur_y;
    cur_id = (cur_id + 0x100) & 0xff00;
    
    u16 x = text_get_x();
    if (x > dirty_x) dirty_x = x;
}

static void with_label(const char *label)
{
    text_color(LABEL_COLOR);
    text_cursor(cur_x, cur_y);
    u16 width = text(label);
    if (width > label_width)
    {
        label_width = width;
    }

    text_cursor(cur_x + max_label_width + 2, cur_y);
}

static void no_label()
{
    text_cursor(cur_x + max_label_width + 2, cur_y);
}

static bool gui_entry(u8 type, const char *label, void *value)
{
    GUIEntry *entry = &gui_data.entries[gui_data.count];
    gui_data.count++;
    
    if (strcmp(label, entry->label) || type != entry->type)
    {
        entry->type = type;
        strncpy(entry->label, label, sizeof(entry->label));
        switch(type)
        {
            case GUI_TYPE_BITS16:
            case GUI_TYPE_U16:
            case GUI_TYPE_BUTTON:
                entry->value = entry->override = *(uint16_t *)value;
                break;
            case GUI_TYPE_U8:
                entry->value = entry->override = *(uint8_t *)value;
                break;
            case GUI_TYPE_BOOL:
                entry->value = entry->override = *(bool *)value;
                break;
            default:
                entry->value = entry->override = 0;
                break;
        }
    }
    else
    {
        if( entry->value != entry->override )
        {
            switch(type)
            {
                case GUI_TYPE_BITS16:
                case GUI_TYPE_U16:
                case GUI_TYPE_BUTTON:
                    *(uint16_t *)value = entry->value = entry->override;
                    break;
                case GUI_TYPE_U8:
                    *(uint8_t *)value = entry->value = entry->override;
                    entry->value = entry->override = *(uint8_t *)value;
                    break;
                case GUI_TYPE_BOOL:
                    *(bool *)value = entry->value = entry->override;
                    entry->value = entry->override = *(bool *)value;
                    break;
                default:
                    break;
            }
            return true;
        }

        switch(type)
        {
            case GUI_TYPE_BITS16:
            case GUI_TYPE_U16:
            case GUI_TYPE_BUTTON:
                entry->value = entry->override = *(uint16_t *)value;
                break;
            case GUI_TYPE_U8:
                entry->value = entry->override = *(uint8_t *)value;
                break;
            case GUI_TYPE_BOOL:
                entry->value = entry->override = *(bool *)value;
                break;
            default:
                break;
        }
    }
    return false;
}

void gui_begin(u16 x, u16 y)
{
    cur_x = x;
    cur_y = y;
    cur_id = 0;
    max_label_width = label_width;
    label_width = 0;

    gui_data.start_magic = MAGIC;
    gui_data.end_magic = MAGIC;
    gui_data.count = 0;
    gui_data.lock = 0xffff;

    text_clear(cur_x, cur_y, dirty_x - cur_x, dirty_y - cur_y);


    if (input_pressed(DOWN))
    {
        active_id = (active_id + 0x100) & 0xff00;
        if (active_id > last_id) active_id = 0;
    }
    else if (input_pressed(UP))
    {
        active_id = (active_id - 0x100) & 0xff00;
        if (active_id > last_id) active_id = last_id & 0xff00;
    }

    text_color(0);
    dirty_x = 0;
}

bool gui_button(const char *name)
{
    uint16_t gui_value = 0;
    bool pressed = gui_entry(GUI_TYPE_BUTTON, name, &gui_value) && gui_value;
    
    no_label();
    
    if (cur_id == active_id)
    {
        text_color(ACTIVE_COLOR);
        pressed |= input_pressed(BTN1);
    }
    else
    {
        text_color(INACTIVE_COLOR);
    }
    
    text(name);

    end_element();
    return pressed;
}

bool gui_toggle(const char *label, bool *value)
{
    bool pressed = gui_entry(GUI_TYPE_BOOL, label, value);

    with_label(label);
    if (cur_id == active_id)
    {
        text_color(ACTIVE_COLOR);
        if (input_pressed(BTN1))
        {
            *value = !(*value);
            pressed = true;
        }
    }
    else
    {
        text_color(INACTIVE_COLOR);
    }
        
    if (*value)
    {
        text("ON ");
    }
    else
    {
        text("OFF");
    }

    end_element();
    return pressed;
}

bool gui_bits16(const char *label, uint16_t *value)
{
    bool pressed = gui_entry(GUI_TYPE_BITS16, label, value);
    u8 idx = active_id & 0xff;
    u16 active_bit = 0;
    with_label(label);
    if ((cur_id & 0xff00) == (active_id & 0xff00))
    {
        pressed = input_pressed(BTN1);
        if (input_pressed(LEFT)) idx--;
        if (input_pressed(RIGHT)) idx++;
        idx &= 0xf;
        active_id = (active_id & 0xff00) | idx;
        active_bit = 0x8000 >> idx;
        if (pressed) *value ^= active_bit;
    }
    
    u16 bit = 0x8000;
    while(bit != 0)
    {
        if (bit == active_bit)
            text_color(ACTIVE_COLOR);
        else
            text_color(INACTIVE_COLOR);

        if (*value & bit)
        {
            text("1");
        }
        else
        {
            text("0");
        }
        bit >>= 1;
    }

    end_element();
    return pressed;
}

bool gui_bits16_func(const char *label, u16 (*getter)(), void (*setter)(u16))
{
    u16 value = getter();
    if (gui_bits16(label, &value))
    {
        setter(value);
        return true;
    }
    return false;
}

bool gui_u16(const char *label, uint16_t *value)
{
    bool pressed = gui_entry(GUI_TYPE_U16, label, value);

    u16 active_bit = 0;
    with_label(label);
    if (cur_id == active_id)
    {
        if (input_pressed(LEFT))
        {
            *value = *value - 1;
            pressed = true;
        }
        if (input_pressed(RIGHT))
        {
            *value = *value + 1;
            pressed = true;
        }
        text_color(ACTIVE_COLOR);
    }
    else
    {
        text_color(INACTIVE_COLOR);
    }
    
    textf("%04X", *value);

    end_element();

    return pressed;
}

bool gui_u16_func(const char *label, u16 (*getter)(), void (*setter)(u16))
{
    u16 value = getter();
    if (gui_u16(label, &value))
    {
        setter(value);
        return true;
    }
    return false;
}

bool gui_u8(const char *label, u8 *value, u8 min, u8 max)
{
    bool pressed = gui_entry(GUI_TYPE_U8, label, value);

    u16 active_bit = 0;
    with_label(label);
    if (cur_id == active_id)
    {
        if (input_pressed(LEFT))
        {
            if (*value > min)
                *value = *value - 1;
            pressed = true;
        }
        if (input_pressed(RIGHT))
        {
            if (*value < max)
                *value = *value + 1;
            pressed = true;
        }
        text_color(ACTIVE_COLOR);
    }
    else
    {
        text_color(INACTIVE_COLOR);
    }
    
    textf("%02X", *value);

    end_element();
    return pressed;
}

void gui_end()
{
    text_color(0);
    gui_data.lock = 0x0000;
}


