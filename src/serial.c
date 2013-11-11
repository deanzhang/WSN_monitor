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

WINDOW *my_win;
WINDOW *main_win;
WINDOW *panel_win;
PANEL  *my_panel[3];

#define N_FIELDS 7

static struct {
    int h;
    int w;
    int y;
    int x;
    const char *title;
} field_attr[N_FIELDS] = {
    {
        1, 24, 1, 0, "Long Addr:"
    },
    {
        1, 18, 3, 2, "1st Name:"
    },
    {
        1, 18, 5, 2, "2rd Name:"
    },
    {
        1, 18, 7, 2, "TRM Type:"
    },
    {
        1, 8, 9, 2, "Positon:"
    },
    {
        1, 8, 9, 12, ""
    },
    {
        1, 20, 1, 10,
    }
};

FIELD *field[N_FIELDS];
FORM  *my_form;

extern unsigned int num_users;
extern ITEM *my_items[256];
extern MENU *my_menu;
int error = 0;
int new_session_flag = 0;

char *msg_type[] = {
    "Handshark",
    "Power UP",
    "Re-enter",
    "Acquire",
    "Position",
    "UNKOWN"
};

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
    int i, tail_index = 0;
    int y, x;
    uint32_t lost = 0;
    int respon_flag = 0;
    msg_head_t *head = NULL;
    msg_tail_t *tail = NULL;
    msg_parent_t *parent = NULL;
    terminal_t *s, *p;
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
                new_session_flag = 1;
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
            switch (head->type)
            {
                case 0x00:
                    if (buff[i + 16] != 0xAA)
                    {
                        mvprintw(0, 20, "ERRORS:%d", ++error);
                        mvprintw(LINES -1, 0, "ERRORS:Handshark not 0XAA!");
                    }
                    break;
                case 0x01:
                    //break; as the design want
                case 0x02:
                    gettimeofday(&(s->tv_first), NULL);
                    s->msg_count = 1;
                    s->msg_lost = 0;
                    parent = (msg_parent_t *)&buff[i + 16];
                    s->battery_state = parent->battery_state;

                    p = find_terminal(parent->long_addr);
                    if (p == NULL)
                    {
                        p = new_terminal(parent->long_addr, parent->temp_addr, head->seq);
                        new_session_flag = 1;
                        sprintf(p->type, "ROUTE");
                    }
                    p->signal_lqi = parent->signal_lqi;
                    break;
                case 0x20:
                    if (buff[i + 16] != 0x88 || buff[i + 17] != 0x88)
                    {
                        mvprintw(0, 20, "ERRORS:%d", ++error);
                        mvprintw(LINES -1, 0, "ERRORS:Acquire not 0X880X88!");
                    }
                    break;
                default:
                    mvprintw(0, 20, "ERRORS:%d", ++error);
                    mvprintw(LINES -1, 0, "ERRORS:Type unkown!");
            }
            mvwprintw(my_win, 0, x, "Got msg:len:%d seq:%d type:%s\nFRM:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X(L)--%04X(S)\nXOR:%02X\n", head->len, head->seq, (head->type >= 0x20)?msg_type[head->type - 29]:msg_type[head->type], head->long_addr[0], head->long_addr[1], head->long_addr[2], head->long_addr[3], head->long_addr[4], head->long_addr[5], head->long_addr[6], head->long_addr[7], head->temp_addr, tail->xor_sum);
            refresh();
            i += head->len;
            tail_index = i + 2;
            //return 0;
        }
    }
    if (tail_index < nread)
    {
        mvprintw(0, 20, "ERRORS:%d", ++error);
        recv_printf(1, 20, buff + tail_index, nread - tail_index, COLOR_PAIR(1));
    }
    return -1;
}

void print_in_middle(WINDOW *win, int starty, int startx, int width, char *string, chtype color)
{   int length, x, y;
    float temp;

    if(win == NULL)
        win = stdscr;
    getyx(win, y, x);
    if(startx != 0)
        x = startx;
    if(starty != 0)
        y = starty;
    if(width == 0)
        width = 80;

    length = strlen(string);
    temp = (width - length)/ 2;
    x = startx + (int)temp;
    wattron(win, color);
    mvwprintw(win, y, x, "%s", string);
    wattroff(win, color);
    refresh();
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
    char temp_buf[256] = {0};
    int i, hide = TRUE;
    ITEM *cur;
    terminal_t *se = NULL;
    char *valuelist[4] = {
    "NORMAL",
    "ROUTE",
    "MOVEABLE",
    NULL
    };

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
    read(fd, buff, 512);

    initscr();
    start_color();
    cbreak();
    noecho();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_BLUE, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);
    init_pair(5, COLOR_BLACK, COLOR_WHITE);
    
    my_win = newwin(4, 40, (LINES - 10), (COLS - 40));
    main_win = newwin((LINES -15), (COLS - 41), 6, 0);
    panel_win = newwin(20, 40, 10, 30);
    keypad(main_win, TRUE);
    box(panel_win, 0 , 0);

    my_panel[0] = new_panel(my_win);
    my_panel[1] = new_panel(main_win);
    //box(main_win, 0 , 0);
    //attron(COLOR_PAIR(5));
    printw("Welcome\n");
    mvhline(5, 0, ACS_HLINE, COLS);
    mvvline(6, (COLS - 41), ACS_VLINE, LINES -6);
    mvaddch(5, (COLS - 41), ACS_TTEE);
    wattron(main_win, COLOR_PAIR(5));
    mvwprintw(main_win, 0, 0, "TEM-L_ADDR-S_ADDR-Name1-Name2-P_X-P_Y\tMSG-(SUM)-(LST)");
    wattroff(main_win, COLOR_PAIR(5));

    field[0] = new_field(field_attr[0].h, field_attr[0].w, field_attr[0].y, field_attr[0].x, 0, 0);
    mvwprintw(panel_win, field_attr[0].y + 3, field_attr[0].x + 2, field_attr[0].title);
    field_opts_off(field[0], O_ACTIVE);
    for(i = 1; i < N_FIELDS - 1; ++i)
    {
        field[i] = new_field(field_attr[i].h, field_attr[i].w, field_attr[i].y, field_attr[i].x, 0, 0);
        mvwprintw(panel_win, field_attr[i].y + 3, field_attr[i].x + 2, field_attr[i].title);
        set_field_back(field[i], A_UNDERLINE);
        field_opts_off(field[i], O_AUTOSKIP);
    }
    set_field_type(field[1], TYPE_ALNUM, 4);
    set_field_type(field[2], TYPE_ALNUM, 4);
    set_field_type(field[3], TYPE_ENUM, valuelist, FALSE, TRUE);
    set_field_type(field[4], TYPE_INTEGER, 1, 0, 5000);
    set_field_type(field[5], TYPE_INTEGER, 1, 0, 5000);
    field[N_FIELDS - 1] = NULL;
    my_form = new_form(field);

    set_form_win(my_form, panel_win);
    set_form_sub(my_form, derwin(panel_win, 15, 26, 3, 13));
    print_in_middle(panel_win, 1, 14, 8, "Terminal info", COLOR_PAIR(2));

    my_panel[2] = new_panel(panel_win);
    hide_panel(my_panel[2]);
    update_panels();
    my_items[0] = new_item("        ", "-                                                  -");
    my_menu = new_menu((ITEM **)my_items);
    /* Set main window and sub window */
    set_menu_win(my_menu, main_win);
    set_menu_sub(my_menu, derwin(main_win, 6, 50, 1, 0));
    set_menu_format(my_menu, 5, 1);
                
    /* Set menu mark to the string " * " */
    set_menu_mark(my_menu, "*");
    post_menu(my_menu);
    doupdate();

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
                //if ((nread = read(fd_in, buff, 512)) > 0 && (buff[0] == 'q'))
                    //write(fd, buff, nread);
                //while((i = wgetch(main_win)) != KEY_F(1))
                if((hide == TRUE) && (i = wgetch(main_win)) != -1)
                {
                    cur = current_item(my_menu);
                    se = (terminal_t *)item_userptr(cur);
                    switch(i)
                    {
                        case KEY_DOWN:
                            menu_driver(my_menu, REQ_DOWN_ITEM);
                            break;
                        case KEY_UP:
                            menu_driver(my_menu, REQ_UP_ITEM);
                            break;
                        case KEY_NPAGE:
                            menu_driver(my_menu, REQ_SCR_DPAGE);
                            break;
                        case KEY_PPAGE:
                            menu_driver(my_menu, REQ_SCR_UPAGE);
                            break;
                        case 10:
                            show_panel(my_panel[2]);
                            post_form(my_form);
                            hide = FALSE;
                            keypad(main_win, FALSE);
                            keypad(panel_win, TRUE);
                            sprintf(temp_buf, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", se->long_addr[0], se->long_addr[1], se->long_addr[2], se->long_addr[3], se->long_addr[4], se->long_addr[5], se->long_addr[6], se->long_addr[7]);
                            set_field_buffer(field[0], 0, temp_buf);
                            set_field_buffer(field[1], 0, se->name1);
                            set_field_buffer(field[2], 0, se->name2);
                            set_field_buffer(field[3], 0, se->type);
                            sprintf(temp_buf, "%d(x)", se->pos_x);
                            set_field_buffer(field[4], 0, temp_buf);
                            sprintf(temp_buf, "%d(y)", se->pos_y);
                            set_field_buffer(field[5], 0, temp_buf);
                            form_driver(my_form, REQ_END_LINE);
                            break;
                        case 0x1b:
                        case 'q':
                            goto ending;
                            break;
                        default:
                            mvprintw(LINES - 6, 0, "POST_MENU error! %x", i);
                    }
                }
                if((hide == FALSE) && (i = wgetch(panel_win)) != -1)
                {
                    switch(i)
                    {
                        case 10:
                            form_driver(my_form, REQ_VALIDATION);
                            sprintf(se->name1, "%s", field_buffer(field[1], 0));
                            sprintf(se->name2, "%s", field_buffer(field[2], 0));
                            sprintf(se->type, "%s", field_buffer(field[3], 0));
                            se->pos_x = atoi(field_buffer(field[4], 0));
                            se->pos_y = atoi(field_buffer(field[5], 0));
                        case 0x1b:
                            unpost_form(my_form);
                            hide_panel(my_panel[2]);
                            mvvline(6, (COLS - 41), ACS_VLINE, LINES -6);
                            keypad(panel_win, FALSE);
                            keypad(main_win, TRUE);
                            hide = TRUE;
                            break;
                        case KEY_BACKSPACE:
                            form_driver(my_form, REQ_DEL_PREV);
                            break;
                        case KEY_DOWN:
                            form_driver(my_form, REQ_NEXT_FIELD);
                            form_driver(my_form, REQ_END_LINE);
                            break;
                        case KEY_UP:
                            form_driver(my_form, REQ_PREV_FIELD);
                            form_driver(my_form, REQ_END_LINE);
                            break;
                        default:
                            /* If this is a normal character, it gets */
                            /* Printed                */    
                            form_driver(my_form, i);
                            break;
                    }
                }
            }
        }
        if (hide == FALSE && se != NULL)
        {
            //mvwprintw(panel_win, 2, 2, "%02X..%02X N1:%s,N2:%s %d %d %6u", se->long_addr[0], se->long_addr[7], se->name1, se->name2, se->signal_lqi, se->battery_state, se->msg_count);
            //sprintf(temp_buf, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", se->long_addr[0], se->long_addr[1], se->long_addr[2], se->long_addr[3], se->long_addr[4], se->long_addr[5], se->long_addr[6], se->long_addr[7]);
            //set_field_buffer(field[0], 0, temp_buf);
        }
        unpost_menu(my_menu);
        terminal_print(main_win, 1, 0);
        if (new_session_flag == 1)
        {
            set_menu_format(my_menu, 5, 1);
            new_session_flag = 0;
        }
        post_menu(my_menu);
        update_panels();
        doupdate();

    }
ending:
    endwin();
    close(fd);
    exit(0);
}
