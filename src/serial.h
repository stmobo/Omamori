#pragma once
#include "includes.h"

#define COM1_BASE_PORT 0x03F8
#define COM2_BASE_PORT 0x02F8
#define COM3_BASE_PORT 0x03E8
#define COM4_BASE_PORT 0x02E8
#define LP_8N1         0x0003 // 8 data bits, no parity, 1 stop bit

#define DATA_BUF_OFFSET         (short)0
#define IER_OFFSET              (short)1
#define FCR_IIR_OFFSET          (short)2
#define LCR_OFFSET              (short)3
#define MCR_OFFSET              (short)4
#define LSR_OFFSET              (short)5
#define MSR_OFFSET              (short)6
#define SR_OFFSET               (short)7

#define IDENT_16750             1
#define IDENT_16550A            2
#define IDENT_16550             3
#define IDENT_16450             4
#define IDENT_8250              5

#define IER_RECV_DATA_AVAIL     0x0001
#define IER_THR_EMPTY           (1<<1)
#define IER_RECV_LINE_STAT      (1<<2)
#define IER_MODEM_STAT          (1<<3)
#define IER_SLEEP_MODE          (1<<4)
#define IER_LOW_POWER_MODE      (1<<5)

#define IIR_INT_NOT_PENDING     0x0001
#define IIR_INT_MODEM_STAT      0x0000
#define IIR_INT_THR_EMPTY       (1<<1)
#define IIR_INT_RECV_DATA_AVAIL (1<<2)
#define IIR_INT_RECV_LINE_STAT  (1<<1) | (1<<2)
#define IIR_INT_TIMEOUT_PENDING (1<<3) | (1<<2)
#define IIR_64BYTE_ENABLED      (1<<5)
#define IIR_NO_FIFO             0
#define IIR_FIFO_PRESENT        (1<<7)
#define IIR_FIFO_WORKING        (1<<6) | (1<<7)

#define FCR_FIFO_ON             0x0001
#define FCR_FIFO_CLEAR_RECV     (1<<1)
#define FCR_FIFO_CLEAR_TRANS    (1<<2)
#define FCR_FIFO_BUF_EXPAND     (1<<5)

#define FCR_FIFO_INT_TRIG_4x    (1<<6)
#define FCR_FIFO_INT_TRIG_8x    (1<<7)
#define FCR_FIFO_INT_TRIG_14x   (1<<6) | (1<<7)

#define MCR_DATA_TERM_READY     0x0001
#define MCR_REQUEST_TO_SEND     (1<<1)
#define MCR_AUX_OUT_1           (1<<2)
#define MCR_AUX_OUT_2           (1<<3)
#define MCR_LOOPBACK_MODE       (1<<4)
#define MCR_AUTO_FLOW_CONTROL   (1<<5)

#define LSR_DATA_READY          0x0001
#define LSR_OVERRUN_ERR         (1<<1)
#define LSR_PARITY_ERR          (1<<2)
#define LSR_FRAMING_ERR         (1<<3)
#define LSR_BREAK_INT           (1<<4)
#define LSR_EMPTY_TRANS_HOLD    (1<<5)
#define LSR_EMPTY_DATA_HOLD     (1<<6)
#define LSR_FIFO_ERR            (1<<7)

#define SERIAL_BUFFER_SIZE      2048

extern "C" {
    extern void set_divisor(short, short);
    extern void set_lp_settings(short, short);
    extern void initialize_serial(short, short);
    extern void serial_print_basic(char*);
    extern void serial_transmit(short,char);
    extern char serial_receive(short);
    extern char* serial_read();
    extern void serial_write(char*);
    extern void serial_enable_interrupts();
    extern void serial_disable_interrupts();
    extern int identify_uart(short);
    extern int write_uart_fifo(short);
}

extern bool serial_initialized;
extern void flush_serial_buffer(void*);