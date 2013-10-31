#ifndef SESSION_H
#define SESSION_H

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "uthash.h"

#define HEAD_SYNC 0xFE
#define TAIL_SYNC 0xEF


#define TYPE_RAW    0x00
#define TYPE_ROUTE  0x01
#define TYPE_NORMAL 0x02

typedef struct  terminal
{
    uint8_t     long_addr[8];
    uint16_t    short_addr;
    uint16_t    seq;
    uint8_t     type;
    char        name1[256];
    char        name2[256];
    int32_t     pos_x;
    int32_t     pos_y;
    uint8_t     signal_lqi;
    uint8_t     battery_state;
    struct timeval tv_first;
    struct timeval tv_last;
    uint32_t    msg_count;
    uint32_t    msg_error;
    uint32_t    msg_lost;
    UT_hash_handle hh;         /* makes this structure hashable */
} terminal_t;

#pragma pack(1)

typedef struct msg_head
{
    uint8_t     head;
    uint8_t     len;
    uint16_t    seq;
    uint8_t     control;
    uint8_t     type;
    uint8_t     long_addr[8];
    uint16_t    temp_addr;
} msg_head_t;


typedef struct  msg_tail
{
    uint8_t     xor_sum;
    uint8_t     tail;
} msg_tail_t;

#pragma pack()


terminal_t *new_terminal(uint8_t long_addr[8], uint16_t short_addr, uint16_t seq);
terminal_t *find_terminal(uint8_t long_addr[8]);
void delete_terminal(terminal_t *s);
int update_terminal(terminal_t *s, uint8_t long_addr[8], uint16_t short_addr);
int update_name1(uint8_t long_addr[8], char *name);
int update_name2(uint8_t long_addr[8], char *name);
void terminal_print(WINDOW *win, int y, int x);
int recv_printf(int y, int x, uint8_t *buff, int nread, chtype color);


#endif  //#define SESSION_H
