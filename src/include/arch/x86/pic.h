// pic.h
#pragma once
#include "includes.h"

#define MASTER_PIC_BASE             0x0020
#define SLAVE_PIC_BASE              0x00A0
#define PIC_CMD_PORT_OFFSET         0x0001

#define PIC_CMD_END_INTERRUPT       0x0020
#define PIC_CMD_INITIALIZE          0x0011

#define PIC_IRQ_OFFSET_1              32
#define PIC_IRQ_OFFSET_2              40

extern void pic_end_interrupt(int);
extern void pic_initialize(char, char);
extern void pic_set_mask(uint16_t);
extern uint16_t pic_get_mask(void);