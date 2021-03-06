#include "session.h"

terminal_t *terminals = NULL;    /* important! initialize to NULL */
unsigned int num_users = 0;
ITEM *my_items[256] = {NULL};
MENU *my_menu = NULL;

terminal_t *new_terminal(uint8_t long_addr[8], uint16_t short_addr, uint16_t seq)
{
    terminal_t *s;

    s = malloc(sizeof(terminal_t));
    if(s == NULL)
        return NULL;
    memcpy(s->long_addr, long_addr, 8);
    s->short_addr = short_addr;
    s->seq = seq;
    sprintf(s->type, "UNKOWN");
    sprintf(s->name1, "name1");
    sprintf(s->name2, "name2");
    s->pos_x = 0;
    s->pos_y = 0;
    s->signal_lqi = 0;
    s->battery_state = 0;
    gettimeofday(&(s->tv_first), NULL);
    gettimeofday(&(s->tv_last), NULL);
    s->msg_count = 1;
    s->msg_error = 0;
    s->msg_lost = 0;
    sprintf(s->desc, "%02X..%02X-%04X %6u %6u %6d %6d %6d", s->long_addr[0], s->long_addr[7], s->short_addr, s->msg_count, s->msg_lost, s->battery_state, s->signal_lqi, (int)(s->tv_last.tv_sec - s->tv_first.tv_sec));
    if (num_users == 0)
       free_item(my_items[0]);
    my_items[num_users]= new_item(s->name1, s->desc);
    set_item_userptr(my_items[num_users], s);
    my_items[++num_users] = (ITEM *)NULL;
    if (num_users != 1)
        my_menu->nitems++;
    HASH_ADD(hh, terminals, long_addr, 8, s);
    return s;
}

#ifdef DEBUG
void debug_printf(char *buf, int len)
{
    int j = 0;
    for (;j < len; ++j)
    {
        printf("0x%02X ", (uint8_t)buf[j]);
    }
    printf("\n-----------\n");
}
#endif

MENU *terminal_print(WINDOW *win, int y, int x)
{
    char time_first[40] = {0};
    char time_last[40] = {0};
    terminal_t *s;

    num_users = HASH_COUNT(terminals);
    mvprintw(0, 10, "TEM_CNT:%d", num_users);

    wmove(win, y, x);
    for(s = terminals; s != NULL; s=s->hh.next)
    {
        ctime_r(&(s->tv_first.tv_sec), time_first);
        ctime_r(&(s->tv_last.tv_sec), time_last);
        sprintf(s->desc, "%02X..%02X-%04X %u %u %d %d %d", s->long_addr[0], s->long_addr[7], s->short_addr, s->msg_count, s->msg_lost, s->battery_state, s->signal_lqi, (int)(s->tv_last.tv_sec - s->tv_first.tv_sec));
        //wprintw(win, "%02X...%02X   %04x  %s  %s  %d   %d  %6u %6u %s %s\n", s->long_addr[0], s->long_addr[7], s->short_addr, s->name1, s->name2, s->pos_x, s->pos_y, s->msg_count, s->msg_lost, time_first, time_last);
    }
    return my_menu;
}

int recv_printf(int y, int x, uint8_t *buff, int nread, chtype color)
{
    int i = 0;
    move(y, x);
    attron(color);
    for( ; i < nread; ++i)
        printw(" %02X", buff[i]);
    attroff(color);
    printw(" |%d\n", nread);
    return 0;
}

terminal_t *find_terminal(uint8_t long_addr[8])
{
    terminal_t *s;

    HASH_FIND(hh,terminals, long_addr, 8, s);
    return s;
}

void delete_terminal(terminal_t *s)
{
    HASH_DEL(terminals, s);  /* s: pointer to deletee */
    free(s);               /* optional; it's up to you! */
}

int update_terminal(terminal_t *s, uint8_t long_addr[8], uint16_t short_addr)
{
    if(s == NULL)
        return -1;
    memcpy(s->long_addr, long_addr, 8);
    s->short_addr = short_addr;
    gettimeofday(&(s->tv_first), NULL);
    gettimeofday(&(s->tv_last), NULL);
    s->msg_count = 0;
    s->msg_error = 0;
    s->msg_lost = 0;
    return 0;
}

int update_name1(uint8_t long_addr[8], char *name)
{
    terminal_t *s;
    s = find_terminal(long_addr);
    if (s == NULL)
    {
        return -1;
    }
    snprintf(s->name1, 256, "%s", name);
    return 0;
}

int update_name2(uint8_t long_addr[8], char *name)
{
    terminal_t *s;
    s = find_terminal(long_addr);
    if (s == NULL)
    {
        return -1;
    }
    snprintf(s->name2, 256, "%s", name);
    return 0;
}
