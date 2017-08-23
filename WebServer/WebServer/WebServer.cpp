// WebServer.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <WinSock2.h>

#define BUF_SIZE 2048
#define BUF_SMALL 100

unsigned WINAPI RequestHandler(void* arg);
char* ContentType(char* file);
void SendData(SOCKET sock, char* ct, char* fileName);
void SendErrorMsg(SOCKET sock);
void ErrorHandling(char* message);
void getHostName(char* hostName);
int execmd(char* cmd, char* result);
void DisplayEntries(char* computerName);

int _tmain(int argc, _TCHAR* argv[])
{
	if (2 != argc)
	{
		printf("Usage:%s <port>", argv[0]);
		exit(1);
	}

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		ErrorHandling("WSAStartup error");
	}

	SOCKET hServSock = socket(PF_INET, SOCK_STREAM, 0);

	SOCKADDR_IN servAddr;
	memset(&servAddr, 0, sizeof(hServSock));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port= htons(_tstoi(argv[1]));
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	const int on=1;
	setsockopt(hServSock, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)); 

	if (bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
	{
		ErrorHandling("bind error");
	}

	if (listen(hServSock, 10) == SOCKET_ERROR)
	{
		ErrorHandling("listen error");
	}

	while(1)
	{
		SOCKADDR_IN clntAddr;
		int nClntAddr = sizeof(clntAddr);
		SOCKET hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &nClntAddr);
		printf("Connection request:%s:%d\n",
			inet_ntoa(clntAddr.sin_addr), ntohs(clntAddr.sin_port));

		DWORD dwThreadID;
		HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, RequestHandler, (void*)hClntSock, 0, (unsigned*)&dwThreadID);
	}

	closesocket(hServSock);
	WSACleanup();
	return 0;
}

unsigned WINAPI RequestHandler(void* arg)
{
	SOCKET hClntSock = (SOCKET)arg;
	char buf[BUF_SIZE];
	char method[BUF_SMALL];
	char ct[BUF_SMALL];
	char fileName[BUF_SMALL];

	recv(hClntSock, buf,BUF_SIZE, 0);
	if (strstr(buf, "HTTP/") == NULL) // 查看是否是HTTP提出的请求
	{
		SendErrorMsg(hClntSock);
		closesocket(hClntSock);
		return 1;
	}

	strcpy(method, strtok(buf, " /"));
	if (strcmp(method, "GET")) // 查看是否为GET方式的请求
	{
		SendErrorMsg(hClntSock);
	}

	strcpy(fileName, strtok(NULL, " /"));
	strcpy(ct, ContentType(fileName));
	SendData(hClntSock, ct, fileName);

	return 0;
}

void SendData(SOCKET sock, char* ct, char* fileName)
{
	char protocol[] = "HTTP/1.0 200 OK\r\n";
	char servName[] = "Server:simple web server\r\n";
	char cntLen[] = "Content-length:2048\r\n";
	char cntType[BUF_SMALL];
	char buf[BUF_SIZE];
	FILE* sendFile;

	sprintf(cntType, "Content-type:%s\r\n\r\n", ct);
	sendFile = fopen(fileName, "r");
	if (sendFile == NULL)
	{
		SendErrorMsg(sock);
		return;
	}

	/*传输头信息*/
	send(sock, protocol, strlen(protocol), 0);
	send(sock, servName, strlen(servName), 0);
	send(sock, cntLen, strlen(cntLen), 0);
	send(sock, cntType, strlen(cntType), 0);

	char hostName[BUF_SMALL];
	getHostName(hostName);

	while(fgets(buf, BUF_SIZE, sendFile) != NULL)
	{
		char* sReplaceStr = strstr(buf, "%s");
		if (NULL != sReplaceStr)
		{
			char bufTmp[BUF_SIZE];
			sprintf_s(bufTmp, buf, hostName);
			send(sock, bufTmp, strlen(bufTmp), 0);
		}
		else
		{
			send(sock, buf, strlen(buf), 0);
		}
		
	}

	Sleep(100); // 网页访问时老是刷不出，怀疑是sock关闭太快了
	closesocket(sock);
}

void SendErrorMsg(SOCKET sock)
{
	char protocol[] = "HTTP/1.0 400 Bad Request\r\n";
	char servName[] = "Server:simple web server\r\n";
	char cntLen[] = "Content-length:2048\r\n";
	char cntType[] = "Content-type:text/html\r\n\r\n";
	char content[] = "<html><head><title>NETWORK</title></head>"
		"<body><font size=+5><br> 发生错误！查看请求文件名和请求方式！</font></body></html>";

	send(sock, protocol, strlen(protocol), 0);
	send(sock, servName, strlen(servName), 0);
	send(sock, cntLen, strlen(cntLen), 0);
	send(sock, cntType, strlen(cntType), 0);
	send(sock, content, strlen(content), 0);

	closesocket(sock);
}

char* ContentType(char* file)
{
	char extension[BUF_SMALL];
	char fileName[BUF_SMALL];
	strcpy(fileName, file);
	strtok(fileName, ".");
	strcpy(extension, strtok(NULL, "."));
	if (!strcmp(extension, "html") || !strcmp(extension, "htm"))
	{
		return "text/html";
	}
	else
	{
		return "text/plain";
	}
}

void ErrorHandling(char* message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

void getHostName(char* hostName)
{
	char cmdResult[BUF_SIZE];
	memset(cmdResult, 0, sizeof(cmdResult));
	char* cmd = "netstat -ano | findstr 192\.168\..*3389";
	execmd(cmd, cmdResult);
	if (strlen(cmdResult) == 0)
	{
		strcpy(hostName, "nobody");
		return;
	}

// 	int nStartIdx = 0;
// 	int nEndIdx = 0;
// 	for (int i = strlen(cmdResult); i >= 0; --i)
// 	{
// 		if (cmdResult[i] == ':')
// 		{
// 			if (nEndIdx == 0)
// 			{
// 				nEndIdx = i - 1;
// 			}
// 		}
// 
// 		if ((i < nEndIdx) && (cmdResult[i] == ' '))
// 		{
// 			if (nStartIdx == 0)
// 			{
// 				nStartIdx = i + 1;
// 				break;
// 			}
// 		}
// 	}
// 
// 	char ipAddr[BUF_SMALL];
// 	int j = 0;
// 	for (int i = nStartIdx; i <= nEndIdx; ++i)
// 	{
// 		ipAddr[j++] = cmdResult[i];
// 	}
// 	ipAddr[j] = '\0';
// 
// 	SOCKADDR_IN addr;
// 	memset(&addr, 0, sizeof(addr));
// 	addr.sin_addr.s_addr = inet_addr(ipAddr);
// 
// 	struct hostent *host = gethostbyaddr((char*)&addr.sin_addr, 4, AF_INET);
// 	if (NULL == host)
// 	{
// 		ErrorHandling("gethostbyaddr error");
// 	}
// 
// 	strcpy(hostName, ipAddr);

	// gethostbyaddr返回的计算机名是bogon，改成从事件日志中读
	DisplayEntries(hostName);
}

int execmd(char* cmd, char* result) 
{
	char buffer[128];                                            
	FILE* pipe = _popen(cmd, "r");            //打开管道，并执行命令 
	if (NULL == pipe)
		return 0;                      //返回0表示运行失败 

	while(!feof(pipe)) 
	{
		if(fgets(buffer, 128, pipe))
		{             
			strcat(result,buffer);
		}
	}
	_pclose(pipe);                            //关闭管道 
	return 1;                                 //返回1表示运行成功 
}

void DisplayEntries(char* computerName)  
{  
    char *tempBuf=new char[100];  
    memset(tempBuf,0,100);  
    HANDLE h;  
    EVENTLOGRECORD *pevlr;   
    TCHAR bBuffer[4096] = {0};   
   
    DWORD dwRead, dwNeeded, cRecords, dwThisRecord = 0;   
   
	// Open the Application event log.   
	/*Windows 日志：  
	应用程序          对应于OpenEventLog（NULL,"Application"）  
	安全              对应于OpenEventLog（NULL,"Security"）  
	setup  
	系统              对应于OpenEventLog（NULL,"System"）*/  
  
	h = OpenEventLog( NULL,   /*use local computer*/  _T("Security"));   // source name : System.  
	if (h == NULL)   
	{    
		printf("Could not open the Application event log."); 
	}  

	pevlr = (EVENTLOGRECORD *) &bBuffer;   
  
	//GetOldestEventLogRecord(h, &dwThisRecord);  
	// Opening the event log positions the file pointer for this   
	// handle at the beginning of the log. Read the records   
	// sequentially until there are no more.   
  
	while (ReadEventLog(h,                // event log handle   
						EVENTLOG_BACKWARDS_READ |  // reads forward   
						EVENTLOG_SEQUENTIAL_READ, // sequential read   
						0,            // ignored for sequential reads   
						pevlr,        // pointer to buffer   
						4096,  // size of buffer   
						&dwRead,      // number of bytes read   
						&dwNeeded))   // bytes in next record   
	{
		bool bBreak = false;
		while (dwRead > 0)   
		{   
			if (pevlr->EventID == 4778)
			{
				UINT nOffset = pevlr->StringOffset;
				LPCTSTR pStr = (LPCTSTR)((LPBYTE) pevlr + nOffset);
				nOffset += _tcslen(pStr) * sizeof(TCHAR) + sizeof(TCHAR);
				pStr = (LPCTSTR)((LPBYTE) pevlr + nOffset);
				nOffset += _tcslen(pStr) * sizeof(TCHAR) + sizeof(TCHAR);
				pStr = (LPCTSTR)((LPBYTE) pevlr + nOffset);
				nOffset += _tcslen(pStr) * sizeof(TCHAR) + sizeof(TCHAR);
				pStr = (LPCTSTR)((LPBYTE) pevlr + nOffset);
				nOffset += _tcslen(pStr) * sizeof(TCHAR) + sizeof(TCHAR);
				pStr = (LPCTSTR)((LPBYTE) pevlr + nOffset);

				WideCharToMultiByte (CP_OEMCP, NULL, pStr, -1, computerName, 100, NULL, FALSE);
				if (strcmp(computerName, "Unknown") != 0)
				{
					bBreak = true;
					break;						
				}
				
			}
  
			dwRead -= pevlr->Length;   
			pevlr = (EVENTLOGRECORD *) ((LPBYTE) pevlr + pevlr->Length);   
		}   
  
		if (bBreak)
		{
			break;
		}

		pevlr = (EVENTLOGRECORD *) &bBuffer;   
	}   
  
	CloseEventLog(h);   
}