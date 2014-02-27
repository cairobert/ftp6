#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cftp.h"

bool CFTP::PasvSaveAddrAndPort4NE(const char *szAddrPort)
{
    const char *s = strstr(szAddrPort, "(");
    int a1, a2, a3, a4, a5, a6;
    if ( 6 != sscanf(s+1, "%d,%d,%d,%d,%d,%d", &a1, &a2, &a3, &a4, &a5, &a6))
        return false;
    m_iReDataPort = a5 * 256 + a6;
    char szReAddr[BUFSIZ] = {0};
    snprintf(szReAddr, sizeof(szReAddr), "%d.%d.%d.%d", a1, a2, a3, a4);
    memset(&m_ssData, 0, sizeof(m_ssData));
    struct sockaddr_in *addr = (struct sockaddr_in *) &m_ssData;
    addr->sin_family = AF_INET;
    addr->sin_port = htons(m_iReDataPort);
    inet_pton(AF_INET, szReAddr, &addr->sin_addr );
    return true;
}

bool CFTP::PasvSaveAddrAndPort4E(const char *szAddrPort)
{
    const char *p = strstr(szAddrPort, "|||");
    for(; '\0' != *p; ++p)
        if (*p >= '0' && *p <= '9')
            break;
    if ('\0' == *p)
        return false;
    int port = 0;
    if ( 1 != sscanf(p, "%d", &port) )
        return false;
    m_iReDataPort = port;
    memset(&m_ssData, 0, sizeof(m_ssData));

    struct sockaddr_in *addr4 = (struct sockaddr_in *) &m_ss;
    struct sockaddr_in *addr = (struct sockaddr_in *) &m_ssData;
    addr->sin_family = AF_INET;
    addr->sin_port = htons(m_iReDataPort);
    memcpy(&addr->sin_addr, &addr4->sin_addr, sizeof(addr4->sin_addr));
    return true;
}

bool CFTP::PasvSaveAddrAndPort6(const char *szAddrPort)
{
    const char *p = strstr(szAddrPort, "|||");
    for(; '\0' != *p; ++p)
    {
        if (*p >= '0' && *p <= '9')
            break;
    }
    if ('\0' == *p)    
        return false;
    int port = 0;
    if ( 1 != sscanf(p, "%d", &port) )
        return false;
    m_iReDataPort = port;
    memset(&m_ssData, 0, sizeof(m_ssData));

    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) &m_ss;
    struct sockaddr_in6 *addr = (struct sockaddr_in6 *) &m_ssData;
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(m_iReDataPort);
    memcpy(&addr->sin6_addr, &addr6->sin6_addr, sizeof(addr6->sin6_addr));
    return true;
}

bool CFTP::PollWait(const int waitFd, const int waitType, const int waitTime)
{
    if (POLLIN != waitType && POLLOUT != waitType)
        return false;

    struct pollfd fdarr[1];
    memset(fdarr, 0, sizeof(fdarr));
    fdarr[0].fd = waitFd;
    fdarr[0].events = waitType;
    int ret = poll(fdarr, 1, waitTime);
    if ( 0 == ret)
    {
        fprintf(stdout, "poll wait timeout.\n");
    }
    else if (ret < 0)
    {
        perror("poll");
    }
    else if ( ret > 0 && (waitType == POLLIN) && (fdarr[0].revents & (waitType | POLLERR)))
        return true;
    else if ( ret > 0 && (waitType == POLLOUT) && (fdarr[0].revents & (waitType | POLLERR)))
    {
        int val;
        socklen_t len = sizeof(val);

        if (-1 == getsockopt(waitFd, SOL_SOCKET, SO_ERROR, &val, &len))
            perror("getsockopt");
        else if (0 != val)
            perror("error on socket");
        else
            return true;
    }
    return false;
}
