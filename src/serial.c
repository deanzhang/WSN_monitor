#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include "session.h"

#define FALSE  -1
#define TRUE   0

int speed_arr[] = {B115200, B57600, B38400, B19200, B9600, B4800, B2400, B1200, B300,
        B38400, B19200, B9600, B4800, B2400, B1200, B300, };
int name_arr[] = {115200, 57600, 38400,  19200,  9600,  4800,  2400,  1200,  300,
        38400,  19200,  9600, 4800, 2400, 1200,  300, };

/*void set_speed_old(int fd, int speed)
{
    int   i;
    int   status;
    struct termios   Opt;
    tcgetattr(fd, &Opt);
    for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++)
    {
        if  (speed == name_arr[i])
        {
            tcflush(fd, TCIOFLUSH);
            cfsetispeed(&Opt, speed_arr[i]);
            cfsetospeed(&Opt, speed_arr[i]);
            status = tcsetattr(fd, TCSANOW, &Opt);
            if  (status != 0)
                perror("tcsetattr fd1");
            return;
        }
        tcflush(fd,TCIOFLUSH);
   }
}*/

int set_speed(int fd, int speed)
{
    int   i;
    //int   status;
    struct termios   options;

    if  ( tcgetattr( fd,&options)  !=  0)
    {
        perror("SetupSerial 1");
        return(FALSE);
    }
    options.c_cflag &= ~CSIZE;
    //case 8:
    options.c_cflag |= CS8;
    //case 'n':
    //case 'N':
    options.c_cflag &= ~PARENB;
    options.c_iflag &= ~INPCK;
    //case 1:
    options.c_cflag &= ~CSTOPB;
    options.c_cc[VTIME] = 150; // 15 seconds
    options.c_cc[VMIN] = 0;

    for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++)
    {
        if  (speed == name_arr[i])
        {
            tcflush(fd, TCIOFLUSH);
            cfsetispeed(&options, speed_arr[i]);
            cfsetospeed(&options, speed_arr[i]);
            break;
        }
        //tcflush(fd,TCIOFLUSH);
    }
    if (tcsetattr(fd,TCSANOW,&options) != 0)
    {
        perror("SetupSerial 3");
        return (FALSE);
    }
    return (TRUE);
}

/*int set_Parity(int fd,int databits,int stopbits,int parity)
{
    struct termios options;
    if  ( tcgetattr( fd,&options)  !=  0)
    {
        perror("SetupSerial 1");
        return(FALSE);
    }
    options.c_cflag &= ~CSIZE;
    switch (databits)
    {
        case 7:
            options.c_cflag |= CS7;
            break;
        case 8:
            options.c_cflag |= CS8;
            break;
        default:
            fprintf(stderr,"Unsupported data size\n");
            return (FALSE);
    }
    switch (parity)
    {
        case 'n':
        case 'N':
            options.c_cflag &= ~PARENB;
            options.c_iflag &= ~INPCK;
            break;
        case 'o':
        case 'O':
            options.c_cflag |= (PARODD | PARENB);
            options.c_iflag |= INPCK;
            break;
        case 'e':
        case 'E':
            options.c_cflag |= PARENB;
            options.c_cflag &= ~PARODD;
            options.c_iflag |= INPCK;
            break;
        case 'S':
        case 's': 
            options.c_cflag &= ~PARENB;
            options.c_cflag &= ~CSTOPB;
            break;
        default:
            fprintf(stderr,"Unsupported parity\n");
            return (FALSE);
    }

    switch (stopbits)
    {
        case 1:
            options.c_cflag &= ~CSTOPB;
            break;
        case 2:
            options.c_cflag |= CSTOPB;
            break;
        default:
            fprintf(stderr,"Unsupported stop bits\n");
            return (FALSE);
    }
    if (parity != 'n')
        options.c_iflag |= INPCK;
    options.c_cc[VTIME] = 150; // 15 seconds
    options.c_cc[VMIN] = 0;

    tcflush(fd,TCIFLUSH);
    if (tcsetattr(fd,TCSANOW,&options) != 0)
    {
        perror("SetupSerial 3");
        return (FALSE);
    }
    return (TRUE);
}*/

uint8_t checksum(uint8_t *buf, int len)
{
    int i;
    uint8_t sum = 0;
    for (i = 0; i < len; ++i)
    {
        sum ^= buf[i];
    }
    return sum;
}

int phase(uint8_t *buff, int nread)
{
    int i;
    uint32_t lost = 0;
    msg_head_t *head = NULL;
    msg_tail_t *tail = NULL;
    terminal_t *s;
    for (i = 0; i < nread; ++i)
    {
        if (buff[i] == HEAD_SYNC)
        {
            head = (msg_head_t *)&buff[i];
            if (head->len > nread - 2)
            {
                continue;
            }
            tail = (msg_tail_t *)&buff[i + head->len];
            if ((tail->tail != TAIL_SYNC) || (tail->xor_sum != checksum(&buff[i + 2], head->len - 2)))
            {
                continue;
            }
            s = find_terminal(head->long_addr);
            if (s == NULL)
            {
                s = new_terminal(head->long_addr[8], head->short_addr, head->seq);
            }
            else
            {
                gettimeofday(&(s->tv_last), NULL);
                ++(s->msg_count);
                lost = head->seq - s->seq - 1;
                if (lost != 0)
                {
                    s->msg_lost = lost;
                }
            }
            terminal_print(s);
            printf("Got msg:len:%d seq:%d type:0x%X from:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X(L)--%04X(S) xor_sum:%02X(%02X)\n", head->len, head->seq, head->type, head->long_addr[0], head->long_addr[1], head->long_addr[2], head->long_addr[3], head->long_addr[4], head->long_addr[5], head->long_addr[6], head->long_addr[7], head->temp_addr, tail->xor_sum, checksum(&buff[i + 2], head->len - 2));
            return 0;
        }
    }
    return -1;
}

int recv_printf(uint8_t *buff, int nread)
{
    int i = 0;
    for( ; i < nread; ++i)
        printf(" %02X", buff[i]);
    printf(" |%d\n", nread);
    return 0;
}

int main(int argc, char **argv)
{
    int fd, fd_in;
    int epollfd, nfds;
    int arg_opt, n;
    int nread;
    int baud_rate = 115200;
    uint8_t buff[512];
    char dev[20] ="/dev/ttyS1";

    #define MAX_EVENTS 5
    struct epoll_event ev, events[MAX_EVENTS];

    while ((arg_opt = getopt(argc, argv, "d:b:h")) != -1)
    {
        switch (arg_opt)
        {
            case 'd':
                sprintf(dev, "%s", optarg);
                break;
            case 'b':
                baud_rate = atoi(optarg);
                break;
            case 'h':
            default: /* '?' */
                printf("USEAGE: %s -b baud_rate -d device\n", argv[0]);
                return 0;
        }
    }

    fd = open(dev, O_RDWR);
    if (fd > 0)
    {
        fcntl(fd, F_SETFL, FNDELAY);
        if (set_speed(fd, baud_rate) == FALSE)
        {
            printf("Set Parity Error\n");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        perror("Open Serial");
        exit(0);
    }

    epollfd = epoll_create(MAX_EVENTS);
    if (epollfd == -1)
    {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        perror("epoll_ctl: fd");
        exit(EXIT_FAILURE);
    }
    fd_in = STDIN_FILENO;
    fcntl(fd_in, F_SETFL, FNDELAY);
    ev.events = EPOLLIN;
    ev.data.fd = fd_in;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd_in, &ev) == -1)
    {
        perror("epoll_ctl: fd_in");
        exit(EXIT_FAILURE);
    }

    while(1)
    {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            perror("epoll_pwait");
            exit(EXIT_FAILURE);
        }
        if (nfds == 0)
        {
            continue;
        }

        for (n = 0; n < nfds; ++n)
        {
            if (events[n].data.fd == fd)
            {
                if ((nread = read(fd, buff, 512)) > 0)
                {
                    //recv_printf(buff, nread);
                    phase(buff, nread);
                }
            }
            if (events[n].data.fd == fd_in)
            {
                if ((nread = read(fd_in, buff, 512)) > 0)
                    write(fd, buff, nread);
            }
        }

    }
    close(fd);
    exit(0);
}
