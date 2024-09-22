#ifndef _SOCKET_H
#define _SOCKET_H

/*
    Attempts to open a socket to the specified location.

    @param address    Address to connect to
    @param port       Port number to connect to
    @param timeout_ms Number of milliseconds to wait before timing out

    @returns The socket file descriptor, or -1 on error.
*/
int socket_connect(const char* address, uint16_t port, int timeout_ms);

/*
    Reads data from a socket. Returns once enough data has been read to
    completely fill the specified buffer, or an error has occurred.

    @param sock    File descriptor of socket to read from
    @param out_buf [output] Buffer to store receieved data in
    @param buf_len Length of receive buffer, out_buf

    @returns Whether or not all of the data could be read from the socket.
*/
bool socket_read(int sock, uint8_t* out_buf, size_t buf_len);

/*
    Writes data to a socket. Returns once all of the data in the specified
    buffer has been written, or an error has occurred.

    @param sock    File descriptor of socket to write to
    @param buf     Buffer to write to the socket
    @param buf_len Length of send buffer, buf

    @returns Whether or not all of the data could be written to the socket.
*/
bool socket_write(int sock, const uint8_t* buf, size_t buf_len);

#endif
