#include "util.h"

#define MAX_TIMER_DELAY_TICK    TS32_MAX

TBool TIMER_SetDelay_ms(Timer_t *timer, Counter_t delay)
{
    if (delay > MAX_TIMER_DELAY_TICK) return TFalse;
    
    timer->start = TIMER_GetNow();
    timer->delay = delay;
    timer->flag = 0;
    
    return TTrue;
}

void  TIMER_Start(Timer_t *timer)
{
    timer->start = TIMER_GetNow();
    
    timer->flag = 1;
}

void  TIMER_Stop(Timer_t *timer)
{
    timer->flag = 0;
}

TBool TIMER_Elapsed(Timer_t *timer)
{
    Counter_t   nTar;
    
    if (timer->flag == 0) return TFalse;
    
    nTar = (Counter_t)(timer->start + timer->delay);
    
    return (TBool)((Counter_t)(TIMER_GetNow() - nTar) < (((TU32)1)<<30));
}

TBool TIMER_isStarted(Timer_t *timer)
{
    return (TBool)(timer->flag != 0);
}

void  TIMER_Delay(Counter_t nMs)
{
    Counter_t nStart = TIMER_GetNow();
    while (((Counter_t)(TIMER_GetNow() - nStart)) < nMs);
}