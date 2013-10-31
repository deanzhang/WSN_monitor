#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <ncurses.h>
#include "session.h"

WINDOW *my_win;
WINDOW *main_win;
int error = 0;

int speed_arr[] = {B115200, B57600, B38400, B19200, B9600, B4800, B2400, B1200, B300,
        B38400, B19200, B9600, B4800, B2400, B1200, B300, };
int name_arr[] = {115200, 57600, 38400,  19200,  9600,  4800,  2400,  1200,  300,
        38400,  19200,  9600, 4800, 2400, 1200,  300, };

int set_speed(int fd, int speed)
{
    int   i;
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
    int y, x;
    uint32_t lost = 0;
    int respon_flag = 0;
    msg_head_t *head = NULL;
    msg_tail_t *tail = NULL;
    terminal_t *s;
    getyx(my_win, y, x);
    for (i = 0; i < nread; ++i)
    {
        if (buff[i] == HEAD_SYNC)
        {
            head = (msg_head_t *)&buff[i];
            if (head->len > (nread - i - 2))
            {
                mvprintw(0, 20, "ERRORS:%d", ++error);
                recv_printf(1, 20, buff + i, nread - i, COLOR_PAIR(1));
                continue;
            }
            tail = (msg_tail_t *)&buff[i + head->len];
            if ((tail->tail != TAIL_SYNC) || (tail->xor_sum != checksum(&buff[i + 2], head->len - 2)))
            {
                mvprintw(0, 20, "ERRORS:%d", ++error);
                recv_printf(1, 20, buff + i, nread - i, COLOR_PAIR(1));
                continue;
            }
            if ((head->control & 0x80) != 0)
            {
                respon_flag = 1;
            }
            s = find_terminal(head->long_addr);
            if (s == NULL)
            {
                s = new_terminal(head->long_addr, head->temp_addr, head->seq);
            }
            else
            {
                gettimeofday(&(s->tv_last), NULL);
                ++(s->msg_count);
                lost = head->seq - s->seq - 1;
                if (lost != 0)
                {
                    s->msg_lost += lost;
                }
                s->seq = head->seq;
            }
            mvwprintw(my_win, 0, x, "Got msg:len:%d seq:%d type:%s\nFRM:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X(L)--%04X(S)\nXOR:%02X\n", head->len, head->seq, (head->type > 0x20)?msg_type[head->type - 29]:msg_type[head->type], head->long_addr[0], head->long_addr[1], head->long_addr[2], head->long_addr[3], head->long_addr[4], head->long_addr[5], head->long_addr[6], head->long_addr[7], head->temp_addr, tail->xor_sum);
            refresh();
            i += head->len;
            //return 0;
        }
    }
    return -1;
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

    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_BLUE, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);
    init_pair(5, COLOR_BLACK, COLOR_WHITE);

    my_win = newwin(4, 40, (LINES - 10), (COLS - 40));
    main_win = newwin((LINES -15), (COLS - 41), 6, 0);
    //box(main_win, 0 , 0);
    //attron(COLOR_PAIR(5));
    printw("Welcome\n");
    mvhline(5, 0, ACS_HLINE, COLS);
    mvvline(6, (COLS - 41), ACS_VLINE, LINES -6);
    mvaddch(5, (COLS - 41), ACS_TTEE);
    //attroff(COLOR_PAIR(5));
    wattron(main_win, COLOR_PAIR(5));
    mvwprintw(main_win, 0, 0, "TEM-L_ADDR-S_ADDR-Name1-Name2-P_X-P_Y\tMSG-(SUM)-(LST)");
    wattroff(main_win, COLOR_PAIR(5));
    refresh();

    while(1)
    {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, 1000);
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
                    if(nread > 20)
                        recv_printf(LINES - 4, 0, buff, nread, COLOR_PAIR(3));
                    phase(buff, nread);
                }
            }
            if (events[n].data.fd == fd_in)
            {
                if ((nread = read(fd_in, buff, 512)) > 0)
                    write(fd, buff, nread);
            }
        }
        terminal_print(main_win, 1, 0);
        wrefresh(main_win);
        wrefresh(my_win);

    }
    endwin();
    close(fd);
    exit(0);
}
