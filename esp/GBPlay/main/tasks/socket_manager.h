#ifndef _SOCKET_MANAGER_H
#define _SOCKET_MANAGER_H

extern int sock;
extern char rx_buffer[16];
extern volatile bool socket_data;

void task_socket_manager_start();

bool socket_send_data(const char*);

bool connect_socket();

#endif
