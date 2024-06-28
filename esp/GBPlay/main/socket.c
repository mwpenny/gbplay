#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <esp_log.h>
#include <esp_timer.h>

#include "socket.h"

static bool _set_socket_is_blocking(int sock, bool is_blocking)
{
    int flags = fcntl(sock, F_GETFL);
    if (flags < 0)
    {
        ESP_LOGE(__func__, "Unable to get socket flags: errno %d", errno);
        return false;
    }

    int new_flags = is_blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    if (fcntl(sock, F_SETFL, new_flags) < 0)
    {
        ESP_LOGE(__func__, "Unable to set socket flags: errno %d", errno);
        return false;
    }

    return true;
}

static int _get_socket_error(int sock)
{
    int sock_error = 0;
    socklen_t sock_error_len = sizeof(sock_error);

    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &sock_error, &sock_error_len) < 0)
    {
        ESP_LOGE(__func__, "Failed to get socket error code: errno %d", errno);
        sock_error = errno;
    }

    return sock_error;
}

static bool _wait_for_socket_connect(int sock, int timeout_ms)
{
    int64_t cutoff_time = esp_timer_get_time() + (timeout_ms * 1000);
    bool success = true;

    while (success)
    {
        int ms_to_wait = (cutoff_time - esp_timer_get_time()) / 1000;
        if (ms_to_wait <= 0)
        {
            errno = ETIMEDOUT;
            success = false;
            break;
        }

        struct pollfd fds[] = {{
            .fd = sock,
            .events = POLLOUT  // Writable
        }};
        int rc = poll(fds, 1, timeout_ms);

        if (rc == 0)
        {
            // poll() timed out
            errno = ETIMEDOUT;
            success = false;
        }
        else if (rc < 0 && errno != EINTR)
        {
            // poll() failed
            ESP_LOGE(
                __func__,
                "Failed waiting for socket to connect: errno %d",
                errno
            );
            success = false;
        }
        else if (rc > 0)
        {
            // poll() signaled socket as writable. Check for success.
            int sock_error = _get_socket_error(sock);
            if (sock_error == 0)
            {
                // Connected
                break;
            }

            ESP_LOGE(__func__, "Socket unable to connect: errno %d", sock_error);
            success = false;
        }
    }

    if (errno == ETIMEDOUT)
    {
        ESP_LOGE(
            __func__,
            "Timed out waiting for socket connection after %d ms",
            timeout_ms
        );
    }

    return success;
}

static bool _connect_socket(int sock, struct addrinfo* address, int timeout_ms)
{
    // Temporarily switch to non-blocking so we can control the timeout
    if (!_set_socket_is_blocking(sock, false))
    {
        ESP_LOGE(__func__, "Unable to set socket to non-blocking");
        return false;
    }

    bool success = true;
    if (connect(sock, address->ai_addr, address->ai_addrlen) < 0)
    {
        if (errno != EINPROGRESS)
        {
            ESP_LOGE(__func__, "Socket unable to connect: errno %d", errno);
            success = false;
        }
        else
        {
            success = _wait_for_socket_connect(sock, timeout_ms);
        }
    }

    if (!_set_socket_is_blocking(sock, true))
    {
        ESP_LOGE(__func__, "Unable to set socket to blocking");
        success = false;
    }

    return success;
}

static int _create_socket(struct addrinfo* address, int timeout_ms)
{
    assert(address->ai_protocol == IPPROTO_TCP);

    int sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (sock < 0)
    {
        ESP_LOGE(__func__, "Unable to create socket: errno %d", errno);
        return -1;
    }

    // Reduce latency
    int nodelay_value = 1;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay_value, sizeof(nodelay_value)) != 0)
    {
        ESP_LOGE(__func__, "Unable to set socket options: errno %d", errno);
        close(sock);
        return -1;
    }

    if (!_connect_socket(sock, address, timeout_ms))
    {
        close(sock);
        return -1;
    }

    return sock;
}

int socket_connect(const char* address, uint16_t port, int timeout_ms)
{
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char service[NI_MAXSERV] = {0};
    snprintf(service, sizeof(service), "%d", port);
    service[NI_MAXSERV - 1] = '\0';

    struct addrinfo* address_info = NULL;
    if (getaddrinfo(address, service, &hints, &address_info) != 0)
    {
        ESP_LOGE(__func__, "Could not get address info for %s:%d", address, port);
        return -1;
    }
    else
    {
        int sock = -1;
        for (struct addrinfo* a = address_info; a != NULL; a = a->ai_next)
        {
            sock = _create_socket(a, timeout_ms);
            if (sock >= 0)
            {
                break;
            }
        }

        freeaddrinfo(address_info);
        return sock;
    }
}

// TODO: detect unclean disconnect (read timeout)
bool socket_read(int sock, uint8_t* out_buf, size_t buf_len)
{
    ssize_t bytes_read = 0;

    while (bytes_read < buf_len)
    {
        ssize_t ret = recv(sock, out_buf + bytes_read, buf_len - bytes_read, 0);
        if (ret == 0)
        {
            ESP_LOGE(__func__, "Socket closed when reading: errno %d", errno);
            return false;
        }
        else if (ret == -1 && errno != EINTR)
        {
            ESP_LOGE(__func__, "Error reading socket data: errno %d", errno);
            return false;
        }
        else if (ret > 0)
        {
            bytes_read += ret;
        }
    }

    return true;
}

// TODO: detect unclean disconnect (write timeout)
bool socket_write(int sock, const uint8_t* buf, size_t buf_len)
{
    ssize_t bytes_written = 0;

    while (bytes_written < buf_len)
    {
        ssize_t ret = send(sock, buf + bytes_written, buf_len - bytes_written, 0);
        if (ret == 0)
        {
            ESP_LOGE(__func__, "Socket closed when writing: errno %d", errno);
            return false;
        }
        else if (ret == -1 && errno != EINTR)
        {
            ESP_LOGE(__func__, "Error writing socket data: errno %d", errno);
            return false;
        }
        else if (ret > 0)
        {
            bytes_written += ret;
        }
    }

    return true;
}
