#pragma once

#include <stdint.h>
#include "pkgi_style.h"

typedef enum {
    VIEW_HOME = 0,
    VIEW_CATALOG = 1,
    VIEW_DETAILS = 2,
    VIEW_FILTER_MENU = 3
} StoreView;

typedef enum {
    TAB_HOME = 0,
    TAB_PS3 = 1,
    TAB_PS2 = 2,
    TAB_PS1 = 3,
    TAB_PSP = 4,
    TAB_EXTRAS = 5,
    TAB_COUNT = 6
} StoreTab;

void ui_modern_init(void);
void ui_modern_update(uint32_t buttons_down, uint32_t buttons_pressed);
void ui_modern_render(void);
int ui_modern_wants_exit(void);

// Navigation
void ui_modern_set_view(StoreView view);
void ui_modern_open_details(uint32_t record_index);
