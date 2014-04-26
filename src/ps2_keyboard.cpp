// ps2_keyboard.cpp - ps2 keyboard driver
#include "includes.h"
#include "ps2_controller.h"
#include "ps2_keyboard.h"
#include "pic.h"
#include "irq.h"
#include "vga.h"

bool shift_stat, alt_stat, ctrl_stat;
char current_state;

ps2_keypress* keystroke_buffer[256];
int keystroke_buffer_offset = 0;

void ps2_keyboard_irq_handler() {}

ps2_keypress* convert_scancode(bool f0, bool e0, char code_end) {
    unsigned char keycode = 0;
    ps2_keypress* kp = new ps2_keypress;
    if(e0)
        keycode = extd_scancodes[code_end];
    else
        keycode = base_scancodes[code_end];
    if(keycode == KEY_Lctrl || keycode == KEY_Rctrl) {
        ctrl_stat = !f0;
    }
    if(keycode == KEY_Lshift || keycode == KEY_Rshift) {
        shift_stat = !f0;
    }
    if(keycode == KEY_Lalt || keycode == KEY_Ralt) {
        alt_stat = !f0;
    }
    kp->key = keycode;
    kp->shift = shift_stat;
    kp->ctrl = ctrl_stat;
    kp->alt = alt_stat;
    kp->released = f0;
    return kp;
}

ps2_keypress* ps2_keyboard_read_keystroke_from_buffer() {
    if(keystroke_buffer_offset == 0)
        return NULL;
    
    // disable interrupts here
    
    ps2_keypress* ks;
    ks = keystroke_buffer[0];
    for(int i=0;i<keystroke_buffer_offset;i++)
        keystroke_buffer[i] = keystroke_buffer[i+1];
    keystroke_buffer[keystroke_buffer_offset] = NULL;
    keystroke_buffer_offset--;
    
    // enable interrupts here
    
    return ks;
}

void ps2_keyboard_initialize() {
    uint16_t port1_ident = ps2_get_ident_bytes(false);
    uint16_t port2_ident = ps2_get_ident_bytes(true);
    if(port1_ident == 0xAB83 || port1_ident == 0xAB41 || port1_ident == 0xABC1) {
        terminal_writestring("Initializing port 1 keyboard.\n");
        ps2_send_byte(0xF0, false); // 0xF0 - Set scancode set
        ps2_send_byte(0xF4, false); // 0xF4 - Enable scanning
    }
    if(port2_ident == 0xAB83 || port2_ident == 0xAB41 || port2_ident == 0xABC1) {
        terminal_writestring("Initializing port 2 keyboard.\n");
        ps2_send_byte(0xF0, true); // 0xF0 - Set scancode set
        ps2_send_byte(0xF4, true); // 0xF4 - Enable scanning
    }
}