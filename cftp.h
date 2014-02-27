#ifndef CFTP_H
#define CFTP_H

#include <netdb.h>
#include <stdio.h>

#ifndef CFTP_POLL_WAIT_TIME
#define CFTP_POLL_WAIT_TIME 3000
#endif

#define CFTP_NO_CONN (-1)

class CFTP;
typedef void (CFTP::*pCftpMemFunc)(const char *);

class CFTP
{
public:
    CFTP();
    virtual ~CFTP();

    typedef enum TransType
    {
        kSend, kRecv, kList
    }TransType;

    struct cmd
    {
        const char *cmd_name;
        pCftpMemFunc mem_func;
        const char *str_help;
    };

    static void Start(CFTP *pThis);
    void Init();
    bool IsConnected();
    void ListFile(const char *fname = NULL);
    void CloseFtp();
    void QuitFtp();
    void AbortFile(const char *fname = NULL);
    void SendFile(const char *fname = NULL);
    void RecvFile(const char *fname = NULL);
    bool SwitchType(const char type);
    void ConnectToHost(const char *szHost);
    bool WaitForResponse(int waitTime = CFTP_POLL_WAIT_TIME);
    bool WriteToServer(const char *msg, bool bWait = true);
    bool IsInDebugMode();
    bool WriteToDst(int sfd, int dfd, TransType type );
    void ServerTerminates();
    bool IsSuccess(const char *msg);
    void Reset();
    void Prompt();
    void Run();
    bool IsToExit();
    void ExecCmd(const char *msg);
//    bool IsIpv6Avail();

    bool PasvSaveAddrAndPort(const char *szAddrPort);
    bool PasvSaveAddrAndPort4NE(const char *szAddrPort);
    bool PasvSaveAddrAndPort4E(const char *szAddrPort);
    bool PasvSaveAddrAndPort6(const char *szAddrPort);

    bool PollWait(const int waitFd, const int waitType, const int waitTime);
    bool IsInPasv();
    bool PasvWait();
    bool PasvSockInit();
    bool PasvConnect();
    bool EPasv();
    bool NEPasv();

    bool PortInit();
    bool PortConnect();
    bool EPort(int port);
    bool NEPort(int port);
    bool IsExtended();

    bool DoJob(TransType type, const char *fname);

    int GetFamily();    
    bool SetFamily(const int family);
    int GetProto();
    int GetSockType();
    int GetListenPort();
    const char *GetLocalAddress();
    bool SendUserInfo();

    //basic ftp commands that are used to communicate with servers
    bool Abor();
    bool Cd(const char *dir);
    bool List(const char *fname = NULL);
    bool Pass(const char *pass = "XXXX");
    bool Pasv();
    bool Port();            //use socket
    bool Pwd();
    bool Quit();
    bool Retr(const char *fname = NULL );
    bool Stor(const char *fname);
    bool Syst();
    bool Type(const char type = 'A');
    bool User(const char *user = "anonymous");

    //commands for user to use
    void UAbort(const char *arg = NULL);
    void UClose(const char *arg = NULL);
    void UCd(const char *dir);
    void UDebug(const char *arg);
    void UExtend(const char *arg);
    void UGet(const char *fname = NULL);
    void UHelp(const char *arg);
    void UIpv4(const char *arg);
    void UIpv6(const char *arg);
    void UList(const char *fname = NULL);
    void UOpen(const char *szHost);
    void UPasv(const char *arg);
    void UPut(const char *fname = NULL);
    void UPwd(const char *fname);
    void UQuit(const char *arg = NULL);

private:
    int m_iDataSock;
    int m_iCtlSock;
    int m_iListenSock;
    bool m_bExit;
    int m_iFamily;
    int m_iProto;
    int m_iSocktype;
    int m_iListenPort;
    int m_iReDataPort;
    int m_iCtlPort;
    bool m_bDebug;
    bool m_bPasv;
    struct cmd *m_cmdArr;
    struct sockaddr_storage m_ss;
    struct sockaddr_storage m_ssData;
    char m_szLocalAddr[BUFSIZ];
    char m_szReDataAddr[BUFSIZ];
    char m_cTransType;
    bool m_bExtend;

};

#endif // CFTP_H
