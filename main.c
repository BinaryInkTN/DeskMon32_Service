//
// Created by USER on 28/01/2026.
//

#include "library.h"

int _tmain(int argc, TCHAR *argv[])
{
    if (argc > 1)
    {
        if (_tcscmp(argv[1], TEXT("install")) == 0)
        {
            SvcInstall();
            return 0;
        }
    }

    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { SVC_NAME, SvcMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcher(DispatchTable))
    {
        SvcReportEvent(TEXT("StartServiceCtrlDispatcher"));
    }

    return 0;
}