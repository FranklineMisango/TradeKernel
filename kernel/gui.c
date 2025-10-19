#include "gui.h"
#include "mm/memory.h"

// Global GUI state
static gui_state_t gui_state;

// Initialize GUI system
void gui_init(void) {
    memset(&gui_state, 0, sizeof(gui_state_t));
    gui_state.next_window_id = 1;
}

// Create a new window
window_t* gui_create_window(int x, int y, int width, int height, const char* title) {
    window_t* window = (window_t*)kmalloc(sizeof(window_t));
    if (!window) return NULL;
    
    window->id = gui_state.next_window_id++;
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    strncpy(window->title, title, sizeof(window->title) - 1);
    window->title[sizeof(window->title) - 1] = '\0';
    window->visible = 0;
    window->focused = 0;
    window->widgets = NULL;
    window->next = gui_state.windows;
    
    gui_state.windows = window;
    return window;
}

// Destroy a window and all its widgets
void gui_destroy_window(window_t* window) {
    if (!window) return;
    
    // Remove from window list
    if (gui_state.windows == window) {
        gui_state.windows = window->next;
    } else {
        window_t* prev = gui_state.windows;
        while (prev && prev->next != window) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = window->next;
        }
    }
    
    // Destroy all widgets
    widget_t* widget = window->widgets;
    while (widget) {
        widget_t* next = widget->next;
        if (widget->text) kfree(widget->text);
        kfree(widget);
        widget = next;
    }
    
    kfree(window);
}

// Show a window
void gui_show_window(window_t* window) {
    if (!window) return;
    window->visible = 1;
    gui_focus_window(window);
    gui_draw_window(window);
}

// Hide a window
void gui_hide_window(window_t* window) {
    if (!window) return;
    window->visible = 0;
    gui_redraw_all();
}

// Focus a window
void gui_focus_window(window_t* window) {
    if (!window) return;
    
    // Unfocus all windows
    window_t* w = gui_state.windows;
    while (w) {
        w->focused = 0;
        w = w->next;
    }
    
    // Focus the specified window
    window->focused = 1;
    gui_state.focused_window = window;
    
    // Move window to front (end of list)
    if (gui_state.windows != window) {
        // Remove from current position
        window_t* prev = gui_state.windows;
        while (prev && prev->next != window) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = window->next;
        }
        
        // Add to end
        window->next = NULL;
        w = gui_state.windows;
        if (w) {
            while (w->next) w = w->next;
            w->next = window;
        } else {
            gui_state.windows = window;
        }
    }
    
    gui_redraw_all();
}

// Create a button widget
widget_t* gui_create_button(window_t* window, int x, int y, int width, int height, const char* text, void (*callback)(widget_t*)) {
    if (!window) return NULL;
    
    widget_t* widget = (widget_t*)kmalloc(sizeof(widget_t));
    if (!widget) return NULL;
    
    widget->type = WIDGET_BUTTON;
    widget->x = x;
    widget->y = y;
    widget->width = width;
    widget->height = height;
    widget->text = kmalloc(strlen(text) + 1);
    if (widget->text) {
        strcpy(widget->text, text);
    }
    widget->active = 0;
    widget->callback = callback;
    widget->next = window->widgets;
    
    window->widgets = widget;
    return widget;
}

// Create a label widget
widget_t* gui_create_label(window_t* window, int x, int y, const char* text) {
    if (!window) return NULL;
    
    widget_t* widget = (widget_t*)kmalloc(sizeof(widget_t));
    if (!widget) return NULL;
    
    widget->type = WIDGET_LABEL;
    widget->x = x;
    widget->y = y;
    widget->width = strlen(text);
    widget->height = 1;
    widget->text = kmalloc(strlen(text) + 1);
    if (widget->text) {
        strcpy(widget->text, text);
    }
    widget->active = 0;
    widget->callback = NULL;
    widget->next = window->widgets;
    
    window->widgets = widget;
    return widget;
}

// Create a checkbox widget
widget_t* gui_create_checkbox(window_t* window, int x, int y, const char* text, int checked) {
    if (!window) return NULL;
    
    widget_t* widget = (widget_t*)kmalloc(sizeof(widget_t));
    if (!widget) return NULL;
    
    widget->type = WIDGET_CHECKBOX;
    widget->x = x;
    widget->y = y;
    widget->width = strlen(text) + 4; // [ ] + space + text
    widget->height = 1;
    widget->text = kmalloc(strlen(text) + 1);
    if (widget->text) {
        strcpy(widget->text, text);
    }
    widget->active = checked;
    widget->callback = NULL;
    widget->next = window->widgets;
    
    window->widgets = widget;
    return widget;
}

// Draw a window
void gui_draw_window(window_t* window) {
    if (!window || !window->visible) return;
    
    int x = window->x;
    int y = window->y;
    int width = window->width;
    int height = window->height;
    
    // Draw title bar
    vga_set_color(GUI_COLOR_TITLE_BAR_FG, GUI_COLOR_TITLE_BAR_BG);
    for (int i = 0; i < width; i++) {
        vga_set_cursor(x + i, y);
        vga_putchar(' ');
    }
    vga_set_cursor(x + 1, y);
    vga_write_string(window->title);
    
    // Draw window border
    vga_set_color(GUI_COLOR_BORDER_FG, GUI_COLOR_WINDOW_BG);
    for (int i = 0; i < width; i++) {
        vga_set_cursor(x + i, y + height - 1);
        vga_putchar('-');
    }
    for (int i = 1; i < height - 1; i++) {
        vga_set_cursor(x, y + i);
        vga_putchar('|');
        vga_set_cursor(x + width - 1, y + i);
        vga_putchar('|');
    }
    vga_set_cursor(x, y);
    vga_putchar('+');
    vga_set_cursor(x + width - 1, y);
    vga_putchar('+');
    vga_set_cursor(x, y + height - 1);
    vga_putchar('+');
    vga_set_cursor(x + width - 1, y + height - 1);
    vga_putchar('+');
    
    // Clear window interior
    vga_set_color(GUI_COLOR_WINDOW_FG, GUI_COLOR_WINDOW_BG);
    for (int i = 1; i < height - 1; i++) {
        for (int j = 1; j < width - 1; j++) {
            vga_set_cursor(x + j, y + i);
            vga_putchar(' ');
        }
    }
    
    // Draw widgets
    widget_t* widget = window->widgets;
    while (widget) {
        gui_draw_widget(widget, window);
        widget = widget->next;
    }
}

// Draw a widget
void gui_draw_widget(widget_t* widget, window_t* window) {
    if (!widget || !window) return;
    
    int wx = window->x + widget->x + 1; // +1 for border
    int wy = window->y + widget->y + GUI_TITLE_BAR_HEIGHT + 1; // +1 for title bar +1 for border
    
    switch (widget->type) {
        case WIDGET_BUTTON: {
            vga_color_t fg = widget->active ? GUI_COLOR_BUTTON_ACTIVE_FG : GUI_COLOR_BUTTON_FG;
            vga_color_t bg = widget->active ? GUI_COLOR_BUTTON_ACTIVE_BG : GUI_COLOR_BUTTON_BG;
            vga_set_color(fg, bg);
            
            // Draw button border
            for (int i = 0; i < widget->width; i++) {
                vga_set_cursor(wx + i, wy);
                vga_putchar(' ');
                vga_set_cursor(wx + i, wy + widget->height - 1);
                vga_putchar(' ');
            }
            for (int i = 0; i < widget->height; i++) {
                vga_set_cursor(wx, wy + i);
                vga_putchar(' ');
                vga_set_cursor(wx + widget->width - 1, wy + i);
                vga_putchar(' ');
            }
            
            // Draw button text
            int text_x = wx + (widget->width - strlen(widget->text)) / 2;
            int text_y = wy + widget->height / 2;
            vga_set_cursor(text_x, text_y);
            vga_write_string(widget->text);
            break;
        }
        
        case WIDGET_LABEL:
            vga_set_color(GUI_COLOR_WINDOW_FG, GUI_COLOR_WINDOW_BG);
            vga_set_cursor(wx, wy);
            vga_write_string(widget->text);
            break;
            
        case WIDGET_CHECKBOX:
            vga_set_color(GUI_COLOR_WINDOW_FG, GUI_COLOR_WINDOW_BG);
            vga_set_cursor(wx, wy);
            vga_putchar('[');
            vga_putchar(widget->active ? 'X' : ' ');
            vga_putchar(']');
            vga_putchar(' ');
            vga_write_string(widget->text);
            break;
            
        default:
            break;
    }
}

// Redraw all visible windows
void gui_redraw_all(void) {
    // Clear screen
    vga_clear();
    
    // Draw all visible windows (in order, focused window last)
    window_t* window = gui_state.windows;
    while (window) {
        if (window->visible) {
            gui_draw_window(window);
        }
        window = window->next;
    }
}

// Handle keyboard input for GUI
void gui_handle_input(char c) {
    // For now, just pass through to shell
    // Future: handle GUI-specific input
    (void)c;
}

// Placeholder for mouse input (future enhancement)
void gui_process_mouse(int x, int y, int button) {
    (void)x; (void)y; (void)button;
    // Future: handle mouse clicks on widgets
}