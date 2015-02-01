// ps2_controller.h

#pragma once
#include "includes.h"

#define PS2_CTRL_DATA_PORT        0x60
#define PS2_CTRL_CMD_STAT_PORT    0x64

#define PS2_STA_OUTPUT_FULL       (1<<0)
#define PS2_STA_INPUT_FULL        (1<<1)
#define PS2_STA_SELFTEST_PASS     (1<<2) 
#define PS2_STA_DATA_TYPE_FLAG    (1<<3)
#define PS2_STA_TIMEOUT_ERR       (1<<6)
#define PS2_STA_PARITY_ERR        (1<<7)

#define PS2_CMD_READ_CCB          0x20
#define PS2_CMD_WRITE_CCB         0x60
#define PS2_CMD_DISABLE_PORT2     0xA7
#define PS2_CMD_ENABLE_PORT2      0xA8
#define PS2_CMD_TEST_PORT2        0xA9
#define PS2_CMD_TEST_CONTROLLER   0xAA
#define PS2_CMD_TEST_PORT1        0xAB
#define PS2_CMD_DISABLE_PORT1     0xAD
#define PS2_CMD_ENABLE_PORT1      0xAE
#define PS2_CMD_WRITE_PORT2       0xD4

#define PS2_CCB_PORT1_INT         (1<<0)
#define PS2_CCB_PORT2_INT         (1<<1)
#define PS2_CCB_SYS_FLAG          (1<<2)
#define PS2_CCB_PORT1_CLK         (1<<4)
#define PS2_CCB_PORT2_CLK         (1<<5)
#define PS2_CCB_PORT_TRANS        (1<<6)

#define PS2_RESP_TPORT_CLK_LOW    0x01
#define PS2_RESP_TPORT_CLK_HIGH   0x02
#define PS2_RESP_TPORT_DAT_LOW    0x03
#define PS2_RESP_TPORT_DAT_HIGH   0x04

#define PS2_RESP_SUCCESS          0xFA
#define PS2_RESP_FAILURE          0xFC

#define PS2_RESP_TIMEOUT          5000


extern void ps2_controller_init();
extern bool ps2_send_byte(unsigned char, bool);
extern bool ps2_send_command(unsigned char);
extern unsigned char ps2_receive_byte(bool);
extern void ps2_set_interrupt_status(bool);
extern uint16_t ps2_get_ident_bytes(bool);
extern unsigned char ps2_wait_for_input();

struct ps2_data {
	uint8_t data;
	bool port;
};
