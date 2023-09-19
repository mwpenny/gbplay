#ifndef _NETWORK_MANAGER_H
#define _NETWORK_MANAGER_H

/*
    The network manager tries to ensure a Wi-Fi connection.
    When the device is not connected to a network, the manager will:

      1. Try to reconnect to the previous network, if it was saved
      2. Try to reconnect to in-range saved networks, prioritized by RSSI

    Whenever the manager fails to connect, the network in question will be
    blocked for 5 minutes. This also happens when the user initiates the
    disconnect, and in that case the manager will not try to reconnect to any
    network (since the decision to disconnect was intentional). Similarly,
    user-initiated connections override the decisions of the manager.

    When a connection is established, the list of blocked networks is
    cleared and the manager will not try to find another network.

    The task is suspended when not trying to reconnect.
*/
void task_network_manager_start(int core, int priority);

#endif
