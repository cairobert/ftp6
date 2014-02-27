#include "cftp.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>


int main()
{
    CFTP ftp;
    CFTP::Start(&ftp);
    return 0;
}
