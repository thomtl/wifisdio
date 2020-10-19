#pragma once

#include <nds.h>

#define NDMAGCNT (*(vu32*)(0x4004100))
#define NDMAxSAD(ch) (*(vu32*)(0x4004104 + (ch * 0x1C)))
#define NDMAxDAD(ch) (*(vu32*)(0x4004108 + (ch * 0x1C)))
#define NDMAxTCNT(ch) (*(vu32*)(0x400410C + (ch * 0x1C)))
#define NDMAxWCNT(ch) (*(vu32*)(0x4004110 + (ch * 0x1C)))
#define NDMAxBCNT(ch) (*(vu32*)(0x4004114 + (ch * 0x1C)))
#define NDMAxCNT(ch) (*(vu32*)(0x400411C + (ch * 0x1C)))