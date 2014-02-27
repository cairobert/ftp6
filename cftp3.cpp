#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

#include "cftp.h"
#ifndef CFTP_NO_CONN
#define CFTP_NO_CONN (-1)
#endif
static const char *SkipSpace(const char *fname)
{
    if ( NULL == fname)
        return "\0";
    const char *ptr = fname;
    for(; '\0' != *ptr; ++ptr)
        if (' ' != *ptr)
            break;
    return ptr;
}

bool CFTP::IsExtended()
{
    return m_bExtend;
}

int CFTP::GetFamily()
{
    return m_iFamily;
}

bool CFTP::SetFamily(const int family)
{
    if (AF_INET == family || AF_INET6 == family)
    {
        if (IsConnected() && family != GetFamily())
        {
            fprintf(stdout, "only when not connecting to a host can you change to a different family.\n");
        }
        else
        {
            m_iFamily = family;
            m_bExtend = true;
            return true;
    }
    }
    return false;
}

int CFTP::GetListenPort()
{
    return m_iListenPort;
}

int CFTP::GetProto()
{
    return m_iProto;
}

int CFTP::GetSockType()
{
    return m_iSocktype;
}

bool CFTP::IsInDebugMode()
{
    return m_bDebug;
}

bool CFTP::IsInPasv()
{
    return m_bPasv;
}

bool CFTP::IsSuccess(const char *msg)
{
    int res = 0;
    if ( 1 == sscanf(msg, "%d", &res) )
    {
        fprintf(stdout, "%s", msg);
        if ( res >= 100 && res < 400)
        {
            return true;
        }
    }
    return false;
}

bool CFTP::IsConnected()
{
    return m_iCtlSock != CFTP_NO_CONN;
}

bool CFTP::IsToExit()
{
    return m_bExit;
}

void CFTP::Prompt()
{
    const char *str = "ftp6> ";
    write(STDOUT_FILENO, str, strlen(str) );
}

void CFTP::Run()
{
    while (!IsToExit())
    {
        Prompt();
        if (PollWait(STDIN_FILENO, POLLIN, -1))
        {
            char buf[BUFSIZ] = {0};
            int nRead = read(STDIN_FILENO, buf, BUFSIZ);
            if (0 == nRead)
            {
                QuitFtp();
                return;
            }
            buf[ strlen(buf) -1 ] = '\0';
            ExecCmd(buf);
        }
    }
}

void CFTP::ExecCmd(const char *msg)
{
    char szCmd[BUFSIZ] = {0};
    char arg[BUFSIZ] = {0};
    sscanf(msg, "%s%s", szCmd, arg);
    for ( const struct cmd *rp = m_cmdArr; NULL != rp->cmd_name; ++rp)
    {
        if ( 0 == strcmp(rp->cmd_name, szCmd))
        {
            (this->*(rp->mem_func))(SkipSpace(arg));
            return;
        }
    }
    fprintf(stdout, "?Invalid command\n");
}

void CFTP::Start(CFTP *pThis)
{
    if (NULL == pThis)
        return;
    pThis->Run();
}

bool CFTP::SendUserInfo()
{
    return User() && Pass();

}

const char *CFTP::GetLocalAddress()
{
    if (!IsConnected())
        return NULL;
    struct sockaddr_storage ss;
    socklen_t len = sizeof(ss);
    if ( -1 == getsockname(m_iCtlSock, (struct sockaddr *)&ss, &len))
        return NULL;
    char szAddr[BUFSIZ] = {0};
    if ( AF_INET == ss.ss_family )
    {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)&ss;
        inet_ntop(ss.ss_family, &addr4->sin_addr, szAddr, sizeof(szAddr));

    }
    else
    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&ss;
        inet_ntop(ss.ss_family, &addr6->sin6_addr, szAddr, sizeof(szAddr));
    }
    memset(&m_szLocalAddr, 0, sizeof(m_szLocalAddr));
    strncpy(m_szLocalAddr, szAddr, strlen(szAddr));
    return m_szLocalAddr;
}

void CFTP::Init()
{
    m_bDebug = false;
    m_bExtend = true;
    m_bPasv = true;
    m_iFamily = AF_INET6;
    m_iProto = IPPROTO_TCP;
    m_iSocktype = SOCK_STREAM;
    m_cTransType = 'A';

    m_iCtlPort = 21;
    m_iCtlSock = CFTP_NO_CONN;
    m_iDataSock = CFTP_NO_CONN;
    m_bExit = false;
    m_iListenSock = CFTP_NO_CONN;
    m_iListenPort = 0;

}

void CFTP::Reset()
{
    if (CFTP_NO_CONN !=  m_iCtlSock )
        close(m_iCtlSock);
    if ( CFTP_NO_CONN != m_iDataSock)
        close(m_iDataSock);
    if (CFTP_NO_CONN != m_iListenSock)
        close(m_iListenSock);

    m_iCtlPort = 21;
    m_iCtlSock = CFTP_NO_CONN;
    m_iDataSock = CFTP_NO_CONN;
    m_bExit = false;
    m_iListenSock = CFTP_NO_CONN;
    m_iListenPort = 0;    

}

bool CFTP::DoJob(TransType type, const char *fname)
{
    if ( type == kList )
    {
        if (!SwitchType('A') )
            return false;
    }
    else
    {
        if (!SwitchType('I'))
            return false;
    }

    if (IsInPasv())
    {
        if (!Pasv())
            return false;
    }
    else
    {
        if (!Port())
            return false;
    }
    switch(type)
    {
    case kList:
        if (!List(fname))
            return false;
        break;
    case kSend:
        if (!Stor(fname))
            return false;
        break;
    case kRecv:
        if (!Retr(fname))
            return false;
        break;
    default:
        return false;
    }
    if (IsInPasv())
        return  true;
    else
        return PortConnect();
}

bool CFTP::SwitchType(const char type)
{
    if (type == 'A' || type == 'I')
    {
        if (IsConnected())
        {
            if (m_cTransType != type)
            {
                m_cTransType = type;
                Type(m_cTransType);
            }
            return true;
        }
        else
            fprintf(stdout, "Not connected.\n");
    }
    return false;
}
