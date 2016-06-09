#include "UdpServer.h"
#include "UdpClient.h"
#include "ClientSocket.h"
#include "TrapFwdConfig.h"
#include "SnmpParser.h"
#include "vbs.h"

#ifndef _WIN32
#include <signal.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#else
#include <direct.h>
#endif

#include <iostream>
using namespace std;

#include "unordered_map.hpp"
#include "filesystem/fstream.hpp"

typedef boost::unordered_map<std::string, int> mapT;

typedef struct _udpDestinations {
  UdpClient*                 udpclient;
  char                       filterFile[256];
  mapT                       filterMap;
} UdpDestinations;

typedef struct _tdpDestinations {
  ClientSocket*              tcpclient;
  char                       filterFile[256];
  mapT                       filterMap;
} TcpDestinations;


int  main(int argc, char** argv);
void OutputTrap(Packet* p);
void ParseFormatString();
void PeriodicOutput();
void LoadFilterMap(mapT&, char*);

char*      gLogFile = NULL;
char*      gFormatString = NULL;
char*      gConfigFile = "trapfwd.cfg";
char*      gVersion = "Version 2.7";
int        gUdpDest = 0;
int        gTcpDest = 0;
ofstream   gOutFile;
bool       gDaemon;
bool       gPeriodicOutput = 0;
unsigned int gFilterRefreshRate = 720;

unsigned long gNumberOfTrapsForwarded = 0;
unsigned long gNumberOfNonTrapMessages = 0;
unsigned long gNumberOfTrapsFilteredOut = 0;

time_t     gTime;
time_t     gNextTime;
time_t     gNextFileRefreshTime;
int        gfString[10];
int        gOids[3];

#define GVERSION        0
#define GCOMMUNITY      1
#define GGENERIC        2
#define GSPECIFIC       3
#define GTIMETICKS      4
#define GSENDERIP       5
#define GSENDEROID      6
#define GVARBINDS       7
#define GVBOID          8
#define GVBTYPE         9
#define GVBDATA        10
#define GTEXTTIMESTAMP 11
#define GSYSTEMTIME    12


#ifndef _WIN32
void sigHandler(int sig);
#endif

int
main(int argc, char** argv)
{
#ifdef _WIN32
WSAData w;
int err = WSAStartup(0x101, &w);
if (err != 0)
  int werr = WSAGetLastError();
#endif

  try
  {
    if (argc > 1)
    {
      for (int x = 1; x < argc; x++)
      {
	if (argv[x][0] == '-')
	{
	  switch (argv[x][1])
	  {
  	    case 'f':
	      gConfigFile = argv[x + 1];
	      break;
  	    case 'd':
	      gDaemon = TRUE;
	      break;
  	    case 'o':
	      gPeriodicOutput = TRUE;
	      break;
	  }
	}
	else
	  if (!strcmp(argv[x], "dumpVer"))
	  {
	    cout << "TrapFwd from your friends at trapreceiver.com." << endl << gVersion << endl;
	    return 1;
	  }
      }
    }
  }
  catch(...)
  {

  }

#ifndef _WIN32
  if (gDaemon)
  {
    ofstream pidFile("/var/run/trapfwd.pid");
    pidFile << getpid() << endl;
    pidFile.close();
    
    // daemon stuff first
    int cPid;
    if ((cPid = fork()) > 0)
      exit(0);
    
    setpgrp();
    signal(SIGHUP, SIG_IGN);
    
    if ((cPid = fork()) > 0)
      exit(0);
    
    for (int fd = 0; fd < NOFILE; fd++)
      close(fd);
    
    chdir("/");
    
    umask(0);
    
    signal(SIGCLD, SIG_IGN);
  } 
#endif

  // now on with the show
  try
  {
    gTime = time(0);
#ifndef _WIN32
    signal(SIGUSR1, sigHandler);
    signal(SIGQUIT, sigHandler);
#endif

    TrapFwdConfig config(gConfigFile);
    
    if (!config.ConfigFileFound())      
    {
#ifndef _WIN32
      ofstream errFile("/tmp/trapfwd.err", ios::app);
#else
      ofstream errFile("trapfwd.err", ios::app);
#endif
      errFile << "Could not find config file " << gConfigFile << endl;
      char buf[512];
#ifndef _WIN32
      getcwd(buf, 511);
#else
      _getcwd(buf, 511);
#endif
      errFile << "in: " << buf << endl;
      errFile.close();
      return 1;
    }
    gUdpDest = config.NumberOfUdpDestinations();
    gTcpDest = config.NumberOfTcpDestinations();


    UdpDestinations** udpArray = new UdpDestinations* [gUdpDest];
    TcpDestinations** tcpArray = new TcpDestinations* [gTcpDest];

    //    UdpClient** udpArray = new UdpClient* [gUdpDest];
    //    ClientSocket** tcpArray = new ClientSocket* [gTcpDest];

    int udpIndex = 0;
    int tcpIndex = 0;
    for (int x = 0; x < (gUdpDest + gTcpDest); x++)
    {
      int port = config.TrapPort(x);
      char* host = (char*)config.TrapHost(x);
      if (host == NULL || strlen(host) == 0)
	continue;

      //////////////////////////////////////////
      // get the new config parameters
      char* filterFile = (char*)config.TrapFilterFile(x);
      //////////////////////////////////////////
	    
      if (config.TrapProtocol(x) == TrapFwdConfig::UDP)
      {
	// udpArray[udpIndex++] = new UdpClient(port, host);
	udpArray[udpIndex] = new UdpDestinations();
	udpArray[udpIndex]->udpclient = new UdpClient(port, host);
	strcpy(udpArray[udpIndex]->filterFile, filterFile);
	if (filterFile != NULL)
	  LoadFilterMap(udpArray[udpIndex]->filterMap, filterFile);
	udpIndex++;
      }
      else
        if (config.TrapProtocol(x) == TrapFwdConfig::TCP)
	{
	  // tcpArray[tcpIndex++] = new ClientSocket(host, port);
	  tcpArray[tcpIndex] = new TcpDestinations();
	  tcpArray[tcpIndex]->tcpclient = new ClientSocket(host, port);
	  strcpy(tcpArray[tcpIndex]->filterFile, filterFile);
	  if (filterFile != NULL)
	    LoadFilterMap(tcpArray[tcpIndex]->filterMap, filterFile);
	  tcpIndex++;
	}
    }

    int port = config.ListenPort();
    const char* listenIf = config.ListenInterface();
    UdpServer listenPort(port, listenIf);
    gFilterRefreshRate = config.TrapFilterFileRefresh();
    //    UdpServer listenPort(port);
    listenPort.Timeout(60);

    gLogFile = (char*)config.LogFile();
    if (gLogFile != NULL)
    {
      gOutFile.open(gLogFile, ios::app);
      gFormatString = (char*)config.FormatString();
      if (gFormatString != NULL)
	ParseFormatString();
    }
    gNextTime = gTime + 300;  // initialize to the starting point plus 5 minutes
    gNextFileRefreshTime = gTime + (gFilterRefreshRate * 60); // minutes * 60 = seconds
    for (;;)
    {
      if (listenPort.IsReady())
      {
	Packet* p = listenPort.Receive(0);

	//////////////////////////////////////////////////////////////
	time_t tempTime = time(0);
	if (tempTime >= gNextTime) 
	{
	  PeriodicOutput();
	  gNextTime = tempTime + 300;  // set to the next 5 minute interval
	}
	//////////////////////////////////////////////////////////////
	if (tempTime >= gNextFileRefreshTime)
	{
	  for (int xx = 0; xx < gUdpDest; xx++) {
	    LoadFilterMap(udpArray[xx]->filterMap, udpArray[xx]->filterFile);
	  }
	  for (int xxx = 0; xxx < gTcpDest; xxx++) {
	    LoadFilterMap(tcpArray[xxx]->filterMap, tcpArray[xxx]->filterFile);
	  }
	  gNextFileRefreshTime += (gFilterRefreshRate * 60);
	}
	//////////////////////////////////////////////////////////////

	if (p != NULL)
	{
	  if (p->Type() == V2TRAP)
	  {
	    V2Notification* pdu = (V2Notification*)p->pdu();
	    if (pdu != NULL)
	      pdu->SenderIP(listenPort.Peer());
	  }

	  if (p->Type() == V1TRAP || p->Type() == V2TRAP)
	  {
	    gNumberOfTrapsForwarded++;
	    for (int xx = 0; xx < gUdpDest; xx++)
	    {
	      if (!(udpArray[xx]->filterMap.empty()))             // we have a filterlist
	      {
		mapT thisMap = udpArray[xx]->filterMap;
		if (thisMap.find((char*)p->SenderIP()) != thisMap.end()) // it is in the list
		  udpArray[xx]->udpclient->Send(p);               // so forward it
		else 
		  gNumberOfTrapsFilteredOut++;                    // count those filtered out
	      }
	      else // not filtering, just forward
		  udpArray[xx]->udpclient->Send(p);           
	    }
	    for (int xxx = 0; xxx < gTcpDest; xxx++)
	    {
	      if (!(tcpArray[xxx]->filterMap.empty()))            // we have a filter list
	      {
		mapT thisMap = tcpArray[xxx]->filterMap;
		if (thisMap.find((char*)p->SenderIP()) != thisMap.end()) // it is in the list
		{
		  if (!tcpArray[xxx]->tcpclient->Connected())     // so forward it
		    tcpArray[xxx]->tcpclient->Connect();
		  if (tcpArray[xxx]->tcpclient->Connected())
		    tcpArray[xxx]->tcpclient->Send(p);
		}
		else
		  gNumberOfTrapsFilteredOut++;                    // count those filtered out
	      }
	      else // not filtering, just forward
	      {
		  if (!tcpArray[xxx]->tcpclient->Connected())     // so forward it
		    tcpArray[xxx]->tcpclient->Connect();
		  if (tcpArray[xxx]->tcpclient->Connected())
		    tcpArray[xxx]->tcpclient->Send(p);
	      }
	    }
	  
	    // log it
	    if (gLogFile != NULL)
	      OutputTrap(p);

	  } // if (p->Type() == V1TRAP || p->Type() == V2TRAP)
	  else 
	    gNumberOfNonTrapMessages++;
	  delete p;
	} // if (p != NULL)
	else {} // cout << "p is null" << endl;
      } // if (listenPort.IsReady())
      else
      {
#ifndef _WIN32
	ofstream errFile("/tmp/trapfwd.err", ios::app);
#else
	ofstream errFile("trapfwd.err", ios::app);
#endif
	errFile << "listenPort not ready: <" << listenPort.ErrorCode() << ">" << endl;
	errFile.close();
#ifndef _IRIX
       	Sleep(10000);
#else
        sleep(10);
#endif
      }
    } // for (;;)
  }
  catch(...)
  {

  }
#ifdef _WIN32
WSACleanup();
#endif
  return 0;
}

#ifndef _WIN32
void sigHandler(int sig)
{
  if (sig == SIGUSR1)
  {
    PeriodicOutput();
    signal(SIGUSR1, sigHandler);
  }
  else
    if (sig ==  SIGQUIT)
      signal(SIGQUIT, sigHandler);
}
#endif


void
ParseFormatString()
{
  int pos = 0;
  int len = strlen(gFormatString);
  char* t = gFormatString;
  for (int x = 0; x < 10; x++)
    gfString[x] = -1;

#if defined  (_IRIX) || defined (_HPUX) || defined (_LINUX)
  for (int x = 0; x < 3; x++)
#else    
  for (int x = 0; x < 3; x++)
#endif
    gOids[x] = -1;
    

  while (len > 0)
  {
    if (*t == '%')
    {
      t++; len--;
      switch (*t)
      {
        case 'T':
	  gfString[pos++] = GTEXTTIMESTAMP;
	  break;
        case 'S':
	  gfString[pos++] = GSYSTEMTIME;
	  break;
        case 'v':
	  gfString[pos++] = GVERSION;
          break;
        case 'c':
	  gfString[pos++] = GCOMMUNITY;
          break;
        case 'g':
	  gfString[pos++] = GGENERIC;
          break;
        case 's':
	  gfString[pos++] = GSPECIFIC;
          break;
        case 't':
	  gfString[pos++] = GTIMETICKS;
          break;
        case 'i':
	  gfString[pos++] = GSENDERIP;
          break;
        case 'o':
	  gfString[pos++] = GSENDEROID;
          break;
        case 'b':
          {
	    gfString[pos++] = GVARBINDS;
	    int index = 0;
            while (len > 0 && index < 3)
	    {
              t++; len--;
	      if (*t == '%') // oops, didn't expect that!
	      {
		t--;len--; // back things up and try to finish it up
		break;
	      }
 	      if (*t == 'O')
		gOids[index++] = GVBOID;
	      else
	        if (*t == 'T')
		  gOids[index++] = GVBTYPE;
	        else
	          if (*t == 'D')
		    gOids[index++] = GVBDATA;
	          else
	            break;
            } // while (len > 0 && index < 3)
	    break;
          } // case 'b':
      } // switch(*t++)
      t++; len--;
    }
    else
    {
      t++; len--;
    }
  } // while (len)
}

void
OutputTrap(Packet* p)
{
  if (!p) return;

  if (p->Type() != V1TRAP && p->Type() != V2TRAP) 
    return;

  Pdu* thisPdu = p->pdu();
  for (int x = 0; x < 10; x++)
  {
    switch(gfString[x])
    {
      case GTEXTTIMESTAMP:
      {
	unsigned int t = 0;
	if (p->Type() == V1TRAP)
	  t = p->TimeTicks();
	else
	{
	  if (p->Type() == V2TRAP)
	  {
	    if (p->VbListLength() >= 2)
	    {
	      if (!strcmp(p->VbType(1), "TimeTick"))
	      {
		char* timeticks = (char*)(p->VbData(1));
		if (timeticks != NULL)
		  t = atoi(timeticks);
	      } // if (!strcmp(p->VbType(1), "TimeTick"))
	    } // if (p->VbListLength() >= 2)
	  } // if (p->Type == V2TRAP)
	}
	//	char timeVal[64];
	//	memset(timeVal, 0, 64);
	int days = t/(100*60*60*24);
	t -= days*(100*60*60*24);
	int hrs = t/(100*60*60);
	t -= hrs*(100*60*60);
	int mins = t/(100*60);
	t -= mins*(100*60);
	int secs = t/100;
	t -= secs*100;
	gOutFile << days << " days " << hrs << "h:" << mins << "m:" << secs << '.' << t << 's' << ' ';
      }
      break;
      case GSYSTEMTIME:
      {
	time_t now = time(0);
	struct tm* t = localtime(&now);
	if (t != NULL)
	{
	  char* tstring = asctime(t);
	  if (tstring != NULL)
	  {
	    tstring[24] = 0;
	    gOutFile << tstring << ' ';
	    tstring[24] = '\n';
	  }
	}
      }
      break;
     case GVERSION:   
       gOutFile << p->Version() << ' ';
       break;
     case GCOMMUNITY: 
       gOutFile << p-> Community() << ' ';
       break;
     case GGENERIC:   
       if (thisPdu != NULL && (p->Type() == V1TRAP))
	 gOutFile << p->GenericTrapType() << ' ';
       break;
     case GSPECIFIC:  
       if (thisPdu != NULL && (p->Type() == V1TRAP))
          gOutFile << p->SpecificTrapType() << ' ';
       break;
     case GTIMETICKS: 
       if (thisPdu != NULL && (p->Type() == V1TRAP))
          gOutFile << p->TimeTicks() << ' ';
       break;
     case GSENDERIP:  
       gOutFile << p->SenderIP() << ' ';
       break;
     case GSENDEROID: 
       //       if (thisPdu != NULL && (p->Type() == V1TRAP))
          gOutFile << p->SenderOID() << ' ';
       break;
     case GVARBINDS:     
     {
       int x = p->VbListLength();
       for (int y = 1; y <= x; y++)
       {
	 if (p->Type() == V2TRAP)
	   if (y == 1 && !strcmp(p->VbType(y), "TimeTick"))
	     continue;

	 if (p->Type() == V2TRAP)
	   if (y == 2 && !strcmp(p->VbType(y), "OID"))
	     continue;

	 for (int yy = 0; yy < 3; yy++)
	 {
	   if (gOids[yy] == GVBOID)
	     gOutFile << p->VbOID(y) << ' ';
	   else
	     if (gOids[yy] == GVBTYPE)
	       gOutFile << p->VbType(y) << ' ';
	     else
	       if (gOids[yy] == GVBDATA)
	       {
		 if (!strcmp(p->VbType(y), "TimeTick"))
		 {
		   unsigned int t = 0;
		   char* timeticks = (char*)(p->VbData(y));
		   if (timeticks != NULL)
		     t = atoi(timeticks);

		   int days = t/(100*60*60*24);
		   t -= days*(100*60*60*24);
		   int hrs = t/(100*60*60);
		   t -= hrs*(100*60*60);
		   int mins = t/(100*60);
		   t -= mins*(100*60);
		   int secs = t/100;
		   t -= secs*100;
		   gOutFile << days << " days " << hrs << "h:" << mins << "m:" << secs << '.' << t << 's' << ' ';
		 }
		 else
		   gOutFile << p->VbData(y) << ' ';
	       }
	       else
		 break;
	 }
       }
       break;
     }
    default:
      break;
    } // switch (gfString[x])
  } // for (int x = 0; x < 10; x++)
  gOutFile << endl;
}


void PeriodicOutput()
{
#ifdef _WIN32
  ofstream ofile("trapfwd.stat");
#else
  ofstream ofile("/tmp/trapfwd.stat");
#endif
  ofile << "Currently:" << endl;
  char newcTime[64];
  memset(newcTime, 0, 64);
  time_t t = time(0);
  int diffT = (int)difftime(t, gTime);
  int days = diffT/(60*60*24);
  diffT -= days*(60*60*24);
  int hrs = diffT/(60*60);
  diffT -= hrs*(60*60);
  int mins = diffT/(60);
  diffT -= mins*(60);
  sprintf(newcTime, "%d days %02dh:%02dm:%02ds", days, hrs, mins, diffT);
  ofile << "\tprocess run time is: " << newcTime << endl;
    
  ofile << '\t' << gVersion << endl;
  ofile << "\tConfig File:  " << gConfigFile << endl;
    
  if (gLogFile != NULL)
    {
      ofile << "\tLogging to: " << gLogFile << endl;
      ofile << "\tFormat String: " << gFormatString << endl;
    }
  ofile << "\tTotal Traps Forwarded: " << gNumberOfTrapsForwarded << endl;
  ofile << "\tTotal Traps Filtered Out: " << gNumberOfTrapsFilteredOut << endl;
  ofile << "\tTotal Non-Trap Messages Received: " << gNumberOfNonTrapMessages << endl;
  ofile << "\tTotal UDP Destinations: " << gUdpDest << endl;
  ofile << "\tTotal TCP Destinations: " << gTcpDest << endl;
}

void
LoadFilterMap(mapT& filterMap, char* filterFile)
{
  boost::filesystem::ifstream inFile(filterFile);
  filterMap.empty();
  while (inFile)
  {
    std::string s;
    std::getline( inFile, s );
    if (inFile)
      filterMap[s] = 1;
  }
}
