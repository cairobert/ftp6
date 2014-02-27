#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "cftp.h"

bool CFTP::WriteToServer(const char *msg, bool bWait)
{
    if (IsConnected())
    {
        write(m_iCtlSock, msg, strlen(msg));
        write(m_iCtlSock, "\r\n", 2);
        if (IsInDebugMode())
            fprintf(stdout, "---> %s\n", msg);
        if (bWait)
            return WaitForResponse();
        return true;
    }
    return false;
}

void CFTP::ListFile(const char *fname)
{

    if (!DoJob(kList, fname))
        return;
    WriteToDst(m_iDataSock, STDOUT_FILENO, kList);
    close(m_iDataSock);
    m_iDataSock = CFTP_NO_CONN;
    WaitForResponse();              //*** directory sends ok
}

void CFTP::RecvFile(const char *fname)
{
    if (!DoJob(kRecv, fname))
        return;
    int fd = open(fname, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if( -1 == fd)
    {
        fprintf(stderr, "open %s: %s\n", fname, strerror(errno));
        Abor();
        return;
    }
    WriteToDst(m_iDataSock, fd, kRecv);
    close(fd);
    close(m_iDataSock);
    m_iDataSock = CFTP_NO_CONN;
    WaitForResponse();          //blablabla
}

void CFTP::SendFile(const char *fname)
{
    if (!DoJob(kSend, fname))
        return;
    int fd = open(fname, O_RDONLY);
    if ( -1 == fd)
    {
        Abor();
        fprintf(stderr, "send file failed: %s\n", strerror(errno));
        return;
    }

    WriteToDst(fd, m_iDataSock, kSend);
    close(fd);
    close(m_iDataSock);
    m_iDataSock = CFTP_NO_CONN;
    WaitForResponse();
}

bool CFTP::PasvConnect()
{
    if (IsConnected())
    {       
        int oflags = fcntl(m_iDataSock, F_GETFL, 0);
        fcntl(m_iDataSock, F_SETFL, oflags | O_NONBLOCK);
        int ret = connect(m_iDataSock, (struct sockaddr *)&m_ssData, sizeof(m_ssData));
        bool bRet = false;
        if (0 == ret)
        {
            bRet = true;
        }
        if ( ret < 0 )
        {
            if ( EINPROGRESS == errno)
            {
                /*******************************/
                bRet = PollWait(m_iDataSock, POLLOUT, CFTP_POLL_WAIT_TIME);
            }
        }
        fcntl(m_iDataSock, F_SETFL, oflags);
        return bRet;
    }
    return false;
}

bool CFTP::PasvSockInit()
{
    if (CFTP_NO_CONN != m_iDataSock)
    {
        close(m_iDataSock);
    }
    if ( -1 != (m_iDataSock = socket(GetFamily(), GetSockType(), GetProto())))      //socket() returns a file descriptor on success. On error, -1 is returned.
    {
        return true;
    }
    m_iDataSock = CFTP_NO_CONN;
    false;
}

bool CFTP::PasvWait()
{
    if (IsConnected())
    {
        if (PollWait(m_iCtlSock, POLLIN, CFTP_POLL_WAIT_TIME))
        {
            char buf[BUFSIZ] = {0};
            int nRead = read(m_iCtlSock, buf, BUFSIZ);
            if (nRead > 0)
            {
                if (IsSuccess(buf))
                    if (PasvSaveAddrAndPort(buf))
                        if (PasvSockInit())
                            return true;
            }
        }
    }
    return false;
}

bool CFTP::WaitForResponse(int waitTime)
{
    if (IsConnected())
    {
//        if (PollWait(m_iCtlSock, POLLIN, CFTP_POLL_WAIT_TIME))
        if (PollWait(m_iCtlSock, POLLIN, waitTime))
        {
            char buf[BUFSIZ] = {0};
            int nRead = read(m_iCtlSock, buf, BUFSIZ);
            if (0 == nRead || -1 == nRead)
                ServerTerminates();
            else
                return IsSuccess(buf);
        }
    }
    return false;
}

bool CFTP::PasvSaveAddrAndPort(const char *szAddrPort)
{
    if (AF_INET == GetFamily() && false == IsExtended())            //ipv4, not extened
        return PasvSaveAddrAndPort4NE(szAddrPort);
    else if (AF_INET == GetFamily() && true == IsExtended())
        return PasvSaveAddrAndPort4E(szAddrPort);
    else if (AF_INET6 == GetFamily())
        return PasvSaveAddrAndPort6(szAddrPort);
    return false;
}

bool CFTP::PortConnect()
{
    if (PollWait(m_iListenSock, POLLIN, CFTP_POLL_WAIT_TIME))
    {
        m_iDataSock = accept(m_iListenSock, NULL, NULL);
        return true;
    }
    return false;
}

bool CFTP::PortInit()
{
    if (IsConnected())
    {
        if ( CFTP_NO_CONN != m_iListenSock)
        {
            close(m_iListenSock);
            m_iListenSock = CFTP_NO_CONN;
        }
        struct addrinfo hint, *addr, *rp;
        memset(&hint, 0, sizeof(hint));
        hint.ai_family = GetFamily();
        hint.ai_socktype = GetSockType();
        hint.ai_protocol = GetProto();
        hint.ai_flags = AI_PASSIVE;

        if ( -1 == getaddrinfo(NULL, "0", &hint, &addr) )
        {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errno));
            return false;
        }

        for (rp  = addr; NULL != rp; rp = rp->ai_next)
        {
            if ( -1 == (m_iListenSock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)))
                continue;
            if ( 0 == bind(m_iListenSock, rp->ai_addr, rp->ai_addrlen) )
                break;
            close(m_iListenSock);
        }
        if ( NULL == rp)
        {
            m_iListenSock = CFTP_NO_CONN;       //file descriptor already closed
            freeaddrinfo(addr);
            return false;
        }
        struct sockaddr_storage ss;
        socklen_t len = sizeof(ss);
        if ( -1 == getsockname(m_iListenSock, (struct sockaddr *)&ss, &len) )
        {
            if (IsInDebugMode())
            {
                perror("getsockname");
            }
        }
        if ( AF_INET == GetFamily() )
        {
            struct sockaddr_in *addr = (struct sockaddr_in *)&ss;
            m_iListenPort = ntohs(addr->sin_port);
        }
        else
        {
            struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&ss;
            m_iListenPort = ntohs(addr->sin6_port);
        }
        listen(m_iListenSock, 1);
        freeaddrinfo(addr);
        return true;
    }
    return false;
}

void CFTP::ConnectToHost(const char *szHost)
{
    int port = m_iCtlPort;
    char buf[BUFSIZ] = {0};
    char szPort[BUFSIZ] = {0};
    if ( 2 == sscanf(szHost, "%s%s", buf, szPort) )
    {
        if ( 1 != sscanf(szPort, "%d", &port))
        {
            fprintf(stderr, "bad port number\n");
            return;
        }
        printf("prot = %d\n", port);
    }
    else
        snprintf(szPort, BUFSIZ, "%d", port);

    int iFam[2] = {AF_INET6, AF_INET};          //first try ipv6, then ipv4
    int i = 0;
    for(; i < 2; ++i)
    {
        struct addrinfo hint, *addr, *rp;
        memset(&hint, 0, sizeof(hint));
        hint.ai_family = iFam[i];
        hint.ai_protocol = GetProto();
        hint.ai_socktype = GetSockType();
        if ( 0 != getaddrinfo(buf, szPort, &hint, &addr) )
        {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errno));
            continue;
        }
        for (rp = addr; NULL != rp; rp = rp->ai_next)
        {
            if (-1 == (m_iCtlSock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)))
                continue;
            int oflags = fcntl(m_iCtlSock, F_GETFL, 0);
            fcntl(m_iCtlSock, F_SETFL, oflags | O_NONBLOCK);
            int ret = connect(m_iCtlSock, (struct sockaddr *) rp->ai_addr, rp->ai_addrlen);
            if (ret > 0)
            {
                fcntl(m_iCtlSock, F_SETFL, oflags);
                break;
            }
            else if ( 0 == ret )
            {
                fcntl(m_iCtlSock, F_SETFL, oflags);
                break;
            }
            if ( ret < 0)
                if (EINPROGRESS == errno)
                {
                    if (PollWait(m_iCtlSock, POLLOUT, CFTP_POLL_WAIT_TIME))
                    {
                        fcntl(m_iCtlSock, F_SETFL, oflags);
                        break;
                    }
                }
            fcntl(m_iCtlSock, F_SETFL, oflags);
            close(m_iCtlSock);
        }

        if (NULL == rp)
        {
            m_iCtlSock = CFTP_NO_CONN;
        }
        else
        {
            if (WaitForResponse())
            {
                m_iFamily = iFam[i];
                SendUserInfo();
                memset(&m_ss, 0, sizeof(m_ss));
                memcpy(&m_ss, rp->ai_addr, rp->ai_addrlen);
                freeaddrinfo(addr);
                break;
            }
            else
            {
                close(m_iCtlSock);
                m_iCtlSock = CFTP_NO_CONN;
            }
        }
        freeaddrinfo(addr);
    }
}

void CFTP::ServerTerminates()
{
    Reset();
}

void CFTP::CloseFtp()
{
    if (IsConnected())
    {
        Quit();
        Reset();
    }
}

void CFTP::QuitFtp()
{
    CloseFtp();
    m_bExit = true;
}

void CFTP::AbortFile(const char *fname)
{    
    Abor();    
}

bool CFTP::WriteToDst(int sfd, int dfd, TransType type)     //more robust
{
    if (PollWait(sfd, POLLIN, CFTP_POLL_WAIT_TIME))
    {
        int nRead = 0;
        int ttlRead = 0;
        char buf[BUFSIZ] = {0};
        struct timeval tm;
        gettimeofday(&tm, NULL);
        while( false == m_bExit && ( (nRead = read(sfd, buf, BUFSIZ)) > 0))
        {
            write(dfd, buf, nRead);
            ttlRead += nRead;
        }
        if (kList == type)
            return true;

        struct timeval tm2;         //send or received
        gettimeofday(&tm2, NULL);
        double ttlTime = tm2.tv_sec - 1 - tm.tv_sec + (tm2.tv_usec + 1000000 - tm.tv_usec ) * 1.0 / 1000000;
        double kbRead = ttlRead *1.0 / 1024;
        fprintf(stdout, "%ld bytes %s in %.2f secs (%.2f kB/s).\n", ttlRead, type == kSend ? "send" : "received",
                ttlTime, kbRead /ttlTime);
        return true;
    }
    return false;
}
