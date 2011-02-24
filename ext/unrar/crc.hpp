#ifndef _RAR_CRC_
#define _RAR_CRC_

extern uint CRCTab[256];

void InitCRC();
uint CRC(uint StartCRC,const void *Addr,size_t Size);
ushort OldCRC(ushort StartCRC,const void *Addr,size_t Size);

#endif
