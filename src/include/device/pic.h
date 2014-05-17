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

extern void pic_end_interrupt(bool);
extern void pic_initialize(char, char);
extern void irq_mask(unsigned char,bool);
extern bool get_irq_mask_status(unsigned char);
extern bool get_irq_mask(unsigned char);
extern void set_all_irq_status(bool);
extern unsigned short get_irq_mask_all(void);
extern void set_irq_mask_all(unsigned short);