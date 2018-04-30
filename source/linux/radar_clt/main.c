#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

extern int  radar_clt_main(int argc, char *argv[]);
extern void radar_clt_exit(void);

static void signal_handler(int signo)	
{  
	printf("Signal %d received\n", signo);

    radar_clt_exit();
}  

static void signal_init(void)
{
    signal(SIGHUP,  &signal_handler);  
    signal(SIGSEGV, &signal_handler);  
    signal(SIGQUIT, &signal_handler);  
    signal(SIGINT,  &signal_handler);  
    signal(SIGTERM, &signal_handler);
}

int main(int argc, char *argv[])
{
    signal_init();

    radar_clt_main(argc, argv);

    return 0;
}