#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cftp.h"

//#define CFTP_NO_CONN (-1)
#define IAC 255
#define IP 244
#define DM 242

CFTP::cmd cmdArr[] = {
{"abort",                   &CFTP::UAbort,               "abort file transmission"           },
{"cd",                      &CFTP::UCd,                  "change remote working directory"   },
{"close",                   &CFTP::UClose,               "terminate ftp session"             },
{"debug",                   &CFTP::UDebug,               "toggle/set debugging mode"         },
{"extend",                  &CFTP::UExtend,              "use extended command"              },
{"get",                     &CFTP::UGet,                 "receive file"                      },
{"ipv4",                    &CFTP::UIpv4,                "restrict address usage to IPv4"    },
{"ipv6",                    &CFTP::UIpv6,                "restrict address usage to IPv6"    },
{"ls",                      &CFTP::UList,                "list contents of remote directory" },
{"open",                    &CFTP::UOpen,                "connect to a host"                 },
{"passive",                 &CFTP::UPasv,                "enter passive transfer mode"       },
{"put",                     &CFTP::UPut,                 "send file"                         },
{"pwd",                     &CFTP::UPwd,                 "print working directory on remote machine"},
{"quit",                    &CFTP::UQuit,                "terminate ftp session and exit"    },
{"?",                       &CFTP::UHelp,                "print local help information"      },
{NULL,                      NULL,                         NULL                               },
};

CFTP::CFTP()
{
    m_cmdArr = cmdArr;
    Init();
}

CFTP::~CFTP()
{

}

static const char *kszNoConn = "Not connected.";

static const char *ExtractOneFile(const char *fname)
{
    if ( NULL == fname )
        return "\0";
    static char name[BUFSIZ] = {0};
    const char *ptr = fname;
    for (; '\0' != *ptr; ++ptr)
    {
        if ( ' ' != *ptr)
            break;
    }
    const char *ptr2 = ptr + 1;
    for(; '\0' != *ptr2; ++ptr2)
    {
        if ( ' ' == *ptr2)
            break;
    }
    memset(name, 0 ,sizeof(name));
    strncpy(name, ptr, (ptr2 - ptr));
    return name;
}


void CFTP::UList(const char *fname)
{
    if (!IsConnected())
        fprintf(stderr, "%s\n", kszNoConn);
    else
        ListFile(ExtractOneFile(fname));
}

void CFTP::UClose(const char *arg)
{
    if (!IsConnected())
        fprintf(stderr, "%s\n", kszNoConn);
    else
        CloseFtp();
}

void CFTP::UCd(const char *dir)
{
    if (!IsConnected())
        fprintf(stderr, "%s\n", kszNoConn);
    else
    {
        const char *name = ExtractOneFile(dir);
        if (0 == strlen(name))
            fprintf(stderr, "Usage: cd directory\n");
        else
            Cd(name);
    }
}

void CFTP::UExtend(const char *arg)
{
    if (AF_INET6 == GetFamily())
        m_bExtend = true;
    else
        m_bExtend = !m_bExtend;
    printf("extende mode %s.\n", m_bExtend ? "on" : "off");
}

void CFTP::UQuit(const char *arg)
{
    QuitFtp();
}

void CFTP::UAbort(const char *arg)
{
    if (!IsConnected())
        fprintf(stderr, "%s\n", kszNoConn);
    else
        AbortFile(arg);
}

void CFTP::UGet(const char *fname)
{
    if (!IsConnected())
        fprintf(stderr, "%s\n", kszNoConn);
    else
    {
        const char *name = ExtractOneFile(fname);
        if (strlen(name) == 0)
            fprintf(stderr, "Usage: get filename\n");
        else
        {
            RecvFile(name);
        }
    }
}

void CFTP::UHelp(const char *arg)
{
    if ( NULL == arg || 0 == strlen(arg))
    {
        printf("Commands are:\n");
        for(const struct cmd *rp = m_cmdArr; NULL != rp->cmd_name; ++rp)
        {
            fprintf(stdout, "%s\t\t%s\n", rp->cmd_name, rp->str_help);
        }
        return;
    }
    for( const struct cmd *rp = m_cmdArr; NULL != rp->cmd_name; ++rp)
    {
        if ( 0 == strcmp(rp->cmd_name, arg))
        {
            fprintf(stdout, "%s\t\t%s\n", rp->cmd_name, rp->str_help);
            return;
        }
    }
    fprintf(stdout, "?Invalid command %s\n", arg);
}

void CFTP::UIpv4(const char *arg)
{
    if (SetFamily(AF_INET) )
        printf("Chosen addressing: IPv4.\n");
    else
        printf("addressing not changed.\n");
}

void CFTP::UIpv6(const char *arg)
{
    if (SetFamily(AF_INET6) )
        printf("Chosen addressing: IPv6.\n");
    else
        printf("addressing not changed.\n");
}

void CFTP::UPasv(const char *arg)
{
    m_bPasv = !m_bPasv;
    fprintf(stdout, "passive mode %s.\n", m_bPasv ? "on" : "off");
}

void CFTP::UPut(const char *fname)
{
    if (!IsConnected())
        fprintf(stderr, "%s\n", kszNoConn);
    else
    {
        const char *name = ExtractOneFile(fname);
        if ( 0 == strlen(name))
        {
            fprintf(stdout, "usage: put filename\n");
        }
        else
        {
            SendFile(name);
        }
    }
}
void CFTP::UOpen(const char *szHost)
{
    if (IsConnected())
        fprintf(stderr, "aleady connected. use close first\n");
    else
    {
        if ( 0 == strlen(szHost))
        {
            fprintf(stdout, "open hostname [port]\n");
            return;
        }
        ConnectToHost(szHost);
    }
}

void CFTP::UPwd(const char *fname)
{
    if (!IsConnected())
        fprintf(stderr, "%s\n", kszNoConn);
    else
        Pwd();
}

void CFTP::UDebug(const char *arg)
{
    m_bDebug = !m_bDebug;
    fprintf(stdout, "Debugging %s.\n", m_bDebug ? "on" : "off");
}

bool CFTP::Abor()
{
    char buf1[BUFSIZ] = { IAC, IP, IAC, '\0'};
    send(m_iCtlSock, buf1, strlen(buf1), 0);
    char buf2[BUFSIZ] = { DM, 'A', 'B', 'O', 'R' ,'\r', '\n',  '\0'};
    send(m_iCtlSock, buf2, strlen(buf2), MSG_OOB);
    if (IsInDebugMode())
        fprintf(stdout, "---> ABOR\r\n");
    return WaitForResponse(3 * CFTP_POLL_WAIT_TIME);
}

bool CFTP::Cd(const char *dir)
{
    char buf[BUFSIZ] = {'C', 'W', 'D', ' '};
    strncat(buf, dir, strlen(dir));
    return WriteToServer(buf);
}

bool CFTP::List(const char *fname)
{
    char buf[BUFSIZ] = { 'L', 'I', 'S', 'T', ' '};
    strncat(buf, fname, strlen(fname));
    return WriteToServer(buf);
}

bool CFTP::Pass(const char *pass)
{
    char buf[BUFSIZ] = {'P', 'A', 'S', 'S', ' '};
    strncat(buf, pass, strlen(pass));
    return WriteToServer(buf);
}

bool CFTP::EPasv()          //ipv4 or ipv6
{
    char buf[BUFSIZ] = {'E', 'P', 'S', 'V', ' '};
    if (AF_INET == GetFamily())
        buf[ strlen(buf)] = '1';
    else
        buf[strlen(buf)] = '2';
    return WriteToServer(buf, false);
}

bool CFTP::NEPasv()         //ipv4 only
{
    char buf[BUFSIZ] = {'P', 'A', 'S', 'V'};
    return WriteToServer(buf, false);
}

bool CFTP::Pasv()
{
    if (IsExtended())
    {
        if (!EPasv())
            return false;
    }
    else
    {
        if (!NEPasv())
            return false;
    }
    return PasvWait() && PasvConnect();
}

bool CFTP::NEPort(int port)
{
    char buf[BUFSIZ] = {'P', 'O', 'R', 'T', ' '};
    const char *addr = GetLocalAddress();
    if (NULL == addr)
        return false;
    int a1, a2, a3, a4;
    sscanf(addr, "%d.%d.%d.%d", &a1, &a2, &a3, &a4);
    int size = strlen(buf);
    for( const char *p = addr; '\0' != *p; ++p)
    {
        if ('.' != *p)
            buf[size++] = *p;
        else
            buf[size++] = ',';
    }
    buf[size++] = ',';
    sprintf(buf + strlen(buf), "%d,%d", port / 256, port % 256);
    return WriteToServer(buf);
}

bool CFTP::EPort(int port)
{
    char buf[BUFSIZ] = {'E', 'P', 'R', 'T'};
    static char delim = '|';

    int size = strlen(buf);
    buf[size++] = ' ';
    buf[size++] = delim;
    if (AF_INET == GetFamily())
    {
        buf[ size++ ] = '1';
    }
    else
    {
        buf[ size++ ] = '2';
    }
    buf[size++] = delim;
    const char *s = GetLocalAddress();
    strncat(buf + size, s,  strlen(s));
    size += strlen(s);
    buf[size++] = delim;
    snprintf(buf + size, BUFSIZ - size, "%d", port);
    return WriteToServer(buf);
}


bool CFTP::Port()
{
    if (PortInit())
    {
        if (IsExtended())
            return EPort(GetListenPort());
        else
            return NEPort(GetListenPort());
    }
    return false;
}

bool CFTP::Pwd()
{
    char buf[BUFSIZ] = {'P', 'W', 'D'};
    return WriteToServer(buf);
}

bool CFTP::Quit()
{
    char buf[BUFSIZ] = { 'Q', 'U', 'I', 'T', '\0'};
    return WriteToServer(buf);
}

bool CFTP::Retr(const char *fname)
{
    char buf[BUFSIZ] = {'R', 'E', 'T', 'R', ' '};
    strncat(buf, fname, strlen(fname));
    return WriteToServer(buf);
}

bool CFTP::Stor(const char *fname)
{
    char buf[BUFSIZ] = {'S', 'T', 'O', 'R', ' '};
    strncat(buf, fname, strlen(fname));
    return WriteToServer(buf);
}

bool CFTP::Syst()
{
    char buf[BUFSIZ] = {'S', 'Y', 'S', 'T'};
    return WriteToServer(buf);
}

bool CFTP::Type(const char type)
{
    char buf[BUFSIZ] = {'T', 'Y', 'P', 'E', ' ', type};
    return WriteToServer(buf);
}

bool CFTP::User(const char *user)
{
    char buf[BUFSIZ] = {'U', 'S', 'E', 'R', ' '};
    strncat(buf, user, strlen(user));
    return WriteToServer(buf);
}
