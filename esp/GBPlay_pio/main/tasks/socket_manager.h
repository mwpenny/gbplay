#ifndef _SOCKET_MANAGER_H
#define _SOCKET_MANAGER_H

/*
    Maintains a socket connection to the backend server.
*/
void task_socket_manager_start(int core, int priority);

#endif
