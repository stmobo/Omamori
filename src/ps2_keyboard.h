// ps2_keyboard.h

#define KB_RESPONSE_SELFTEST_PASS           0xAA
#define KB_RESPONSE_ECHO                    0xEE
#define KB_RESPONSE_ACK                     0xFA
#define KB_RESPONSE_SELFTEST_FAIL_1         0xFC
#define KB_RESPONSE_SELFTEST_FAIL_2         0xFD
#define KB_RESPONSE_RESEND                  0xFE
#define KB_RESPONSE_ERROR_1                 0x00
#define KB_RESPONSE_ERROR_2                 0xFF

#define KB_STATE_NONE                       0x00
#define KB_STATE_WAITING_FOR_RESP           0x01
#define KB_STATE_WAITING_FOR_SCANCODE       0x02
#define KB_STATE_RETRY_1                    0x03
#define KB_STATE_RETRY_2                    0x04
#define KB_STATE_RETRY_3                    0x05

// scancodes:
#define KEY_F1               0xA0
#define KEY_F2               0xA1
#define KEY_F3               0xA2
#define KEY_F4               0xA3
#define KEY_F5               0xA4
#define KEY_F6               0xA5
#define KEY_F7               0xA6
#define KEY_F8               0xA7
#define KEY_F9               0xA8
#define KEY_F10              0xA9
#define KEY_F11              0xAA
#define KEY_F12              0xAB
#define KEY_Tab              0xAC
#define KEY_Bksp             0xAD
#define KEY_Enter            0xAE
#define KEY_Escape           0xAF

#define KEY_CurLeft          0xD6
#define KEY_CurDown          0xD7
#define KEY_CurRight         0xD8
#define KEY_CurUp            0xD9
#define KEY_Lctrl            0xDA
#define KEY_Lshift           0xDB
#define KEY_Lalt             0xDC
#define KEY_Rctrl            0xDD
#define KEY_Rshift           0xDE
#define KEY_Ralt             0xDF

#define KEY_Insert           0xC0
#define KEY_Home             0xC1
#define KEY_PgUp             0xC2
#define KEY_Delete           0xC3
#define KEY_End              0xC4
#define KEY_PgDn             0xC5
#define KEY_CapsLock         0xC6
#define KEY_NumbLock         0xC7
#define KEY_ScrlLock         0xC8

#define KEY_FwSlash          0xE0
#define KEY_Asterisk         0xE1
#define KEY_Dash             0xE2
#define KEY_Plus             0xE3
#define KEY_KeypadEnter      0xE4
#define KEY_KeypadPeriod     0xE5
#define KEY_0                0xE6
#define KEY_1                0xE7
#define KEY_2                0xE8
#define KEY_3                0xE9
#define KEY_4                0xEA
#define KEY_5                0xEB
#define KEY_6                0xEC
#define KEY_7                0xED
#define KEY_8                0xEE
#define KEY_9                0xEF

#define KEY_WWWSearch        0x01
#define KEY_PrevTrk          0x02
#define KEY_WWW Fav          0x03
#define KEY_Lgui             0x04
#define KEY_WWWRfsh          0x05
#define KEY_VolDown          0x06
#define KEY_Mute             0x07
#define KEY_Rgui             0x08
#define KEY_WWWStop          0x09
#define KEY_Calc             0x0A
#define KEY_Apps             0x0B
#define KEY_WWWFwd           0x0C
#define KEY_VolUp            0x0D
#define KEY_PlayPause        0x0E
#define KEY_Power            0x0F
#define KEY_WWWBack          0x10
#define KEY_WWWHome          0x11
#define KEY_Stop             0x12
#define KEY_Sleep            0x13
#define KEY_MyComp           0x14
#define KEY_Email            0x15
#define KEY_NextTrk          0x16
#define KEY_MediaSel         0x17
#define KEY_Wake             0x18

// everything else uses its ASCII value (no caps / shift).

// for most scan codes:
// Hit code     : <base>
// Release code : 0xF0 <base>


// scancode to keycode lookup tables
// "basic" scancodes - these are all the one-byte "hit" codes.
const unsigned char base_scancodes[] = { 0xFF, // 8 rows of 16 + 3 for the final row
    // 1     2     3     4     5     6     7     8     9     A     B     C     D     E     F     0
    0xA0, 0xFF, 0xA4, 0xA2, 0xA0, 0xA1, 0xAB, 0xFF, 0xA9, 0xA7, 0xA5, 0xA3, 0xAC,  '`', 0xFF, 0xFF, // 0x01 - 0x10
    0xDC, 0xDB, 0xFF, 0xFF,  'q',  '1', 0xFF, 0xFF, 0xFF,  'z',  's',  'a',  'w',  '2', 0xFF, 0xFF, // 0x11 - 0x20
     'c',  'x',  'd',  'e',  '4',  '3', 0xFF, 0xFF,  ' ',  'v',  'f',  't',  'r',  '5', 0xFF, 0xFF, // 0x21 - 0x30
     'n',  'b',  'h',  'g',  'y',  '6', 0xFF, 0xFF, 0xFF,  'm',  'j',  'u',  '7',  '8', 0xFF, 0xFF, // 0x31 - 0x40
     ',',  'k',  'i',  'o',  '0',  '9', 0xFF, 0xFF,  '.',  '/',  'l',  ';',  'p',  '-', 0xFF, 0xFF, // 0x41 - 0x50
    0xFF, '\'', 0xFF,  '[',  '=', 0xFF, 0xFF, 0xC6, 0xDE, 0xAE,  ']', 0xFF, '\\', 0xFF, 0xFF, 0xFF, // 0x51 - 0x60
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xAC, 0xFF, 0xFF, 0xE7, 0xFF, 0xEA, 0xED, 0xFF, 0xFF, 0xFF, 0xE6, // 0x61 - 0x70
    0xE5, 0xE8, 0xEB, 0xEC, 0xEE, 0xAF, 0xC7, 0xAA, 0xE3, 0xE9, 0xE2, 0xE1, 0xEF, 0xC8, 0xFF, 0xFF, // 0x71 - 0x80
    0xFF, 0xFF, 0xA6 };                                                                             // 0x81 - 0x83
      
// "extended" scancodes - these handle the multimedia and "ACPI" keys, as well as a few other keys.
const unsigned char extd_scancodes[] = { 0xFF,
    // 1     2     3     4     5     6     7     8     9     A     B     C     D     E     F     0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  0x01, // 0x01 - 0x10
    0xDF, 0xFF, 0xFF, 0xDD, 0x02, 0xFF, 0xFF, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x04,  0x05, // 0x11 - 0x20
    0x06, 0xFF, 0x07, 0xFF, 0xFF, 0xFF, 0x08, 0x09, 0xFF, 0xFF, 0x0A, 0xFF, 0xFF, 0xFF, 0x0B,  0x0C, // 0x21 - 0x30
    0xFF, 0x0D, 0xFF, 0x0E, 0xFF, 0xFF, 0x0F, 0x10, 0xFF, 0x11, 0x12, 0xFF, 0xFF, 0xFF, 0x13,  0x14, // 0x31 - 0x40
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x15, 0xFF, 0xE0, 0xFF, 0xFF, 0x16, 0xFF, 0xFF,  0x17, // 0x41 - 0x50
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE4, 0xFF, 0xFF, 0xFF, 0x18, 0xFF,  0xFF, // 0x51 - 0x60
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC4, 0xFF, 0xD6, 0xC1, 0xFF, 0xFF, 0xFF,  0xC0, // 0x61 - 0x70
    0xC3, 0xD7, 0xFF, 0xD8, 0xD9, 0xFF, 0xFF, 0xFF, 0xFF, 0xC5, 0xFF, 0xFF, 0xC2 };                  // 0x71 - 0x7D

typedef struct ps2_keypress {
    bool shift;
    bool ctrl;
    bool alt;
    bool released;
    bool is_ascii;
    unsigned char key;
} ps2_keypress;

extern void ps2_keyboard_initialize(void);
extern ps2_keypress* ps2_keyboard_get_keystroke(void);
