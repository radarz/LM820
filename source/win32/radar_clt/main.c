#include <stdio.h>
#include <windows.h>

extern int  radar_clt_main(int argc, char *argv[]);
extern void radar_clt_exit(void);

static BOOL CtrlHandler(DWORD fdwctrltype)
{
    switch (fdwctrltype)
    {
        // handle the ctrl-c signal. 
    case CTRL_C_EVENT:
        printf( "Ctrl-C event\n" );
        break;

        // ctrl-close: confirm that the user wants to exit. 
    case CTRL_CLOSE_EVENT:
        printf( "Ctrl-Close event\n" );
        break;

        // pass other signals to the next handler. 
    case CTRL_BREAK_EVENT:
        printf( "Ctrl-Break event\n" );
        break;

    case CTRL_LOGOFF_EVENT:
        printf( "Ctrl-Logoff event\n" );
        break;

    case CTRL_SHUTDOWN_EVENT:
        printf( "Ctrl-Shutdown event\n" );
        break;

    default:
        break;
    }

    radar_clt_exit();

    return TRUE;
}

int main(int argc, char *argv[])
{
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
    
    radar_clt_main(argc, argv);

    return 0;
}