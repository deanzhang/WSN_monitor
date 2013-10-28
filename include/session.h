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
    char head;
    char len;
    uint16_t seq;
    char control;
    char type;
    char long_addr[8];
    uint16_t temp_addr;
} msg_head_t;


typedef struct  msg_tail
{
    char xor_sum;
    char tail;
} msg_tail_t;

#pragma pack()



#endif  //#define SESSION_H