#ifndef SESSION_H
#define SESSION_H

#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define HEAD_SYNC 0xFE
#define TAIL_SYNC 0xEF

#pragma pack(1)

typedef struct msg_head {
    uint8_t head;
    uint8_t len;
    uint16_t seq;
    uint8_t control;
    uint8_t type;
    uint8_t long_addr[8];
    uint16_t temp_addr;
} msg_head_t;


typedef struct  msg_tail
{
    uint8_t xor_sum;
    uint8_t tail;
} msg_tail_t;

#pragma pack()



#endif  //#define SESSION_H