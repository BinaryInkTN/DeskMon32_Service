#ifndef SERVICE_LIBRARY_H
#define SERVICE_LIBRARY_H
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <stdio.h>
#pragma comment(lib, "advapi32.lib")

#define SVC_NAME TEXT("DeskMon32_Service")

typedef struct LibContext {
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    SERVICE_STATUS          gSvcStatus;
    SERVICE_STATUS_HANDLE   gSvcStatusHandle;
    HANDLE                  ghSvcStopEvent;
} LibContext;

VOID __stdcall SvcStart(VOID);
VOID SvcInstall(void);
VOID WINAPI SvcCtrlHandler( DWORD );
VOID WINAPI SvcMain( DWORD, LPTSTR * );
VOID ReportSvcStatus( DWORD, DWORD, DWORD );
VOID SvcInit( DWORD, LPTSTR * );
VOID SvcReportEvent( LPTSTR );


#endif // SERVICE_LIBRARY_H