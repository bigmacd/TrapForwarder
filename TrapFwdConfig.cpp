
#ifndef __TRAPFWDCONFIG_H__
#include "TrapFwdConfig.h"
#endif

#include <iostream>

TrapFwdConfig::TrapFwdConfig(const char* filename)
              :Config(filename)
{
  memset(mHost, 0, 64);
  memset(mLogFile, 0, 64);
  memset(mFormatString, 0, 64);
  memset(mListenInterface, 0, 64);
}

TrapFwdConfig::~TrapFwdConfig()
{

}

int          
TrapFwdConfig::NumberOfUdpDestinations()
{
  int retVal = 0;
  char* dest = NULL;

  Lock();

  bool done = FALSE;
  int index = 0;
  while (!done)
  {
    dest = (char*)SearchOne("TrapProtocol:", index++);
    if (dest != NULL)
    {
      if (!stricmp(dest, "UDP"))
	retVal++;
    }
    else
      done = TRUE;
  }

  Unlock();

  return retVal;
}

int          
TrapFwdConfig::NumberOfTcpDestinations()
{
  int retVal = 0;
  char* dest = NULL;

  Lock();

  bool done = FALSE;
  int index = 0;
  while (!done)
  {
    dest = (char*)SearchOne("TrapProtocol:", index++);
    if (dest != NULL)
    {
      if (!stricmp(dest, "TCP"))
	retVal++;
    }
    else
      done = TRUE;
  }

  Unlock();

  return retVal;
}

// index is sequential
const char*  
TrapFwdConfig::TrapHost(int index)
{
  const char* retVal = NULL;
  Lock();
  retVal = SearchOne("TrapHost:", index);
  Unlock();
  
  memset(mHost, 0, 64);
  if (retVal != NULL)
    strcpy(mHost, retVal);
  return mHost;
}


int
TrapFwdConfig::TrapPort(int index)
{
  int retVal = 162;

  Lock();
  const char* value = SearchOne("TrapPort:", index);
  if (value != NULL)
    retVal = atoi(value);
  Unlock();
  return retVal;
}

TrapFwdConfig::Protocol     
TrapFwdConfig::TrapProtocol(int index)
{
  TrapFwdConfig::Protocol retVal = UDP;

  Lock();
  const char* value = SearchOne("TrapProtocol:", index);
  if (value != NULL)
  {
    if (!stricmp(value, "TCP"))
      retVal = TCP;
  }
  Unlock();
  return retVal;
}

const char*  
TrapFwdConfig::TrapFilterFile(int index)
{
  const char* retVal = NULL;
  Lock();
  retVal = SearchOne("TrapFilterFile:", index);
  Unlock();
  
  memset(mTrapFilterFile, 0, 256);
  if (retVal != NULL)
    strcpy(mTrapFilterFile, retVal);
  return mTrapFilterFile;
}

unsigned int 
TrapFwdConfig::TrapFilterFileRefresh()
{
  unsigned int retVal = 720;

  Lock();
  const char* value = Search("TrapFilterFileRefresh:");
  if (value != NULL)
    retVal = (unsigned int)atoi(value);
  Unlock();
  return retVal;
}

int          
TrapFwdConfig::ListenPort()
{
  int retVal = 162;

  Lock();
  const char* value = Search("ListenPort:");
  if (value != NULL)
    retVal = atoi(value);
  Unlock();

  return retVal;
}


const char*  
TrapFwdConfig::LogFile()
{
  const char* retVal = NULL;
  Lock();
  retVal = Search("LogFile:");
  Unlock();
  
  if (retVal == NULL)
    return NULL;
  else
  {
    memset(mLogFile, 0, 64);
    strcpy(mLogFile, retVal);
  }
  return mLogFile;
}

const char*  
TrapFwdConfig::FormatString()
{
  const char* retVal = NULL;
  Lock();
  retVal = Search("FormatString:");
  Unlock();

  memset(mFormatString, 0, 64);
  if (retVal != NULL)
    strcpy(mFormatString, retVal);
  return mFormatString;

}

const char*  
TrapFwdConfig::ListenInterface()
{
  const char* retVal = NULL;
  Lock();
  retVal = Search("InterfaceIp:");
  Unlock();
  
  if (retVal == NULL)
    return NULL;
  else
  {
    memset(mListenInterface, 0, 64);
    strcpy(mListenInterface, retVal);
  }
  return mListenInterface;
}
