// Common/CRC.cpp

#include "StdAfx.h"

extern "C" 
{ 
#include "7zCrc.h"
}

class CCRCTableInit
{
public:
  CCRCTableInit() { CrcGenerateTable(); }
} g_CRCTableInit;
