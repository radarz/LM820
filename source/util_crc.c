#include "util.h"

/// CRC8_ATM
/// standard CRC8-ATM
/// POLY: 0x07 (x8+x2+x+1)
/// START CRC VAL: 0x00 
/// END XOR: 0x00 
/// DATA INVERTED: NO 
/// CRC INVERTED: NO
TU8 CRC_CalCrc8(TU8 *pBuf, TU16 nLen, TU8 nPrev)
{
    TU8 nCrc8 = nPrev; 
    TU8 i;
    
    while(nLen--)
    {
        nCrc8 ^= *pBuf++;
        for (i=0; i<8; i++)
        {
            if (nCrc8 & 0x80)
                nCrc8 = (nCrc8 << 1) ^ 0x07;
            else
                nCrc8 <<= 1;
        }
    }
    
    return nCrc8;
}