
#ifndef __TRAPFWDCONFIG_H__
#define __TRAPFWDCONFIG_H__

#include "Config.h"

class TrapFwdConfig : public Config
{
  public:

    enum Protocol
    {
      UDP = 0,
      TCP
    };

  private:
    
    // temporary storage
    char         mHost[64];
    char         mFormatString[64];
    char         mLogFile[64];
    char         mListenInterface[64];
    char         mTrapFilterFile[256];

  protected:



  public:

    TrapFwdConfig(const char* filename);
    ~TrapFwdConfig();

    int          NumberOfUdpDestinations();
    int          NumberOfTcpDestinations();

    // index is sequential
    const char*  TrapHost(int index);
    int          TrapPort(int index);
    Protocol     TrapProtocol(int index);
    const char*  TrapFilterFile(int index);


    unsigned int TrapFilterFileRefresh();

    int          ListenPort();
    const char*  ListenInterface();

    const char*  LogFile();
    const char*  FormatString();


};
#endif

