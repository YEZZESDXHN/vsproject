/*----------------------------------------------------------------------------
|
| File Name: capldll.cpp
|
|            Example of a capl DLL implementation module and using CAPLLbacks.
|-----------------------------------------------------------------------------
|               A U T H O R   I D E N T I T Y
|-----------------------------------------------------------------------------
|   Author             Initials
|   ------             --------
|   Thomas  Riegraf    Ri              Vector Informatik GmbH
|   Hans    Quecke     Qu              Vector Informatik GmbH
|   Stefan  Albus      As              Vector Informatik GmbH
|-----------------------------------------------------------------------------
|               R E V I S I O N   H I S T O R Y
|-----------------------------------------------------------------------------
| Date         Ver  Author  Description
| ----------   ---  ------  --------------------------------------------------
| 2003-10-07   1.0  As      Created
| 2007-03-26   1.1  Ej      Export of the DLL function table as variable
|                           Use of CAPL_DLL_INFO3
|                           Support of long name CAPL function calls
| 2020-01-23   1.2  As      Support for GCC and Clang compiler on Linux
|                           Support for MINGW-64 compiler on Windows
|-----------------------------------------------------------------------------
|               C O P Y R I G H T
|-----------------------------------------------------------------------------
| Copyright (c) 1994 - 2003 by Vector Informatik GmbH.  All rights reserved.
 ----------------------------------------------------------------------------*/


#define USECDLL_FEATURE
#define _BUILDNODELAYERDLL

#include "../Includes/cdll.h"
#include "../Includes/VIA.h"
#include "../Includes/VIA_CDLL.h"
#include "./VS 2017 Project/aes.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>

#if defined(_WIN64) || defined(__linux__)
  #define X64
#endif


class CaplInstanceData;
typedef std::map<uint32_t, CaplInstanceData*> VCaplMap;
typedef std::map<uint32_t, VIACapl*> VServiceMap;


// ============================================================================
// global variables
// ============================================================================

static uint32_t data = 0;
static char dlldata[100];

VCaplMap    gCaplMap;
VServiceMap gServiceMap;


// ============================================================================
// CaplInstanceData
//
// Data local for a single CAPL Block.
//
// A CAPL-DLL can be used by more than one CAPL-Block, so every piece of
// information thats like a global variable in CAPL, must now be wrapped into
// an instance of an object.
// ============================================================================
class CaplInstanceData
{
public:
  CaplInstanceData(VIACapl* capl);

  void GetCallbackFunctions();
  void ReleaseCallbackFunctions();

  // Definition of the class function.
  // This class function will call the CAPL callback functions
  uint32_t ShowValue(uint32_t x);
  uint32_t ShowDates(int16_t x, uint32_t y, int16_t z);
  void     DllInfo(const char* x);
  void     ArrayValues(uint32_t flags, uint32_t numberOfDatabytes, uint8_t databytes[], uint8_t controlcode);
  void     DllVersion(const char* y);

private:

  // Pointer of the CAPL callback functions
  VIACaplFunction*  mShowValue;
  VIACaplFunction*  mShowDates;
  VIACaplFunction*  mDllInfo;
  VIACaplFunction*  mArrayValues;
  VIACaplFunction*  mDllVersion;

  VIACapl*          mCapl;
};


CaplInstanceData::CaplInstanceData(VIACapl* capl)
  // This function will initialize the CAPL callback function
  // with the NLL Pointer
 : mCapl(capl),
   mShowValue(nullptr),
   mShowDates(nullptr),
   mDllInfo(nullptr),
   mArrayValues(nullptr),
   mDllVersion(nullptr)
{}

static bool sCheckParams(VIACaplFunction* f, char rtype, const char* ptype)
{
  char      type;
  int32_t   pcount;
  VIAResult rc;

  // check return type
  rc = f->ResultType(&type);
  if (rc!=kVIA_OK || type!=rtype)
  {
    return false;
  }

  // check number of parameters
  rc = f->ParamCount(&pcount);
  if (rc!=kVIA_OK || strlen(ptype)!=pcount )
  {
    return false;
  }

  // check type of parameters
  for (int32_t i=0; i<pcount; ++i)
  {
    rc = f->ParamType(&type, i);
    if (rc!=kVIA_OK || type!=ptype[i])
    {
      return false;
    }
  }

  return true;
}

static VIACaplFunction* sGetCaplFunc(VIACapl* capl, const char* fname, char rtype, const char* ptype)
{
  VIACaplFunction* f;

  // get capl function object
  VIAResult rc =  capl->GetCaplFunction(&f, fname);
  if (rc!=kVIA_OK || f==nullptr)
  {
    return nullptr;
  }

  // check signature of function
  if ( sCheckParams(f, rtype, ptype) )
  {
     return f;
  }
  else
  {
    capl->ReleaseCaplFunction(f);
    return nullptr;
  }
}

void CaplInstanceData::GetCallbackFunctions()
{
  // Get a CAPL function handle. The handle stays valid until end of
  // measurement or a call of ReleaseCaplFunction.
  mShowValue   = sGetCaplFunc(mCapl, "CALLBACK_ShowValue", 'D', "D");
  mShowDates   = sGetCaplFunc(mCapl, "CALLBACK_ShowDates", 'D', "IDI");
  mDllInfo     = sGetCaplFunc(mCapl, "CALLBACK_DllInfo", 'V', "C");
  mArrayValues = sGetCaplFunc(mCapl, "CALLBACK_ArrayValues", 'V', "DBB");
  mDllVersion  = sGetCaplFunc(mCapl, "CALLBACK_DllVersion", 'V', "C");
}

void CaplInstanceData::ReleaseCallbackFunctions()
{
  // Release all the requested Callback functions
  mCapl->ReleaseCaplFunction(mShowValue);
  mShowValue = nullptr;
  mCapl->ReleaseCaplFunction(mShowDates);
  mShowDates = nullptr;
  mCapl->ReleaseCaplFunction(mDllInfo);
  mDllInfo = nullptr;
  mCapl->ReleaseCaplFunction(mArrayValues);
  mArrayValues = nullptr;
  mCapl->ReleaseCaplFunction(mDllVersion);
  mDllVersion = nullptr;
}

void CaplInstanceData::DllVersion(const char* y)
{
  // Prepare the parameters for the call stack of CAPL.
  // Arrays uses a 8 byte (64 bit DLL: 12 byte) on the stack, 4 Bytes for the number of element,
  // and 4 bytes (64 bit DLL: 8 byte) for the pointer to the array
  int32_t sizeX = (int32_t)strlen(y)+1;

#if defined(X64)
  uint8_t params[16];              // parameters for call stack, 16 Bytes total (8 bytes per parameter, reverse order of parameters)
  memcpy(params+8, &sizeX, 4);   // array size    of first parameter, 4 Bytes
  memcpy(params+0, &y,     8);   // array pointer of first parameter, 8 Bytes
#else
  uint8_t params[8];               // parameters for call stack, 8 Bytes total
  memcpy(params+0, &sizeX, 4);   // array size    of first parameter, 4 Bytes
  memcpy(params+4, &y,     4);   // array pointer of first parameter, 4 Bytes
#endif

  if(mDllVersion!=nullptr)
  {
    uint32_t result; // dummy variable
    VIAResult rc =  mDllVersion->Call(&result, params);
  }
}


uint32_t CaplInstanceData::ShowValue(uint32_t x)
{
#if defined(X64)
  uint8_t params[8];               // parameters for call stack, 8 Bytes total
  memcpy(params + 0, &x, 8);     // first parameter, 8 Bytes
#else
  void* params = &x;   // parameters for call stack
#endif

  uint32_t result;

  if(mShowValue!=nullptr)
  {
    VIAResult rc =  mShowValue->Call(&result, params);
    if (rc==kVIA_OK)
    {
       return result;
    }
  }
  return -1;
}

uint32_t CaplInstanceData::ShowDates(int16_t x, uint32_t y, int16_t z)
{
  // Prepare the parameters for the call stack of CAPL. The stack grows
  // from top to down, so the first parameter in the parameter list is the last
  // one in memory. CAPL uses also a 32 bit alignment for the parameters.

#if defined(X64)
  uint8_t params[24];          // parameters for call stack, 24 Bytes total (8 bytes per parameter, reverse order of parameters)
  memcpy(params+16, &z, 2);  // third  parameter, offset 16, 2 Bytes
  memcpy(params+ 8, &y, 4);  // second parameter, offset 8,  4 Bytes
  memcpy(params+ 0, &x, 2);  // first  parameter, offset 0,  2 Bytes
#else
  uint8_t params[12];         // parameters for call stack, 12 Bytes total
  memcpy(params+0, &z, 2);  // third  parameter, offset 0, 2 Bytes
  memcpy(params+4, &y, 4);  // second parameter, offset 4, 4 Bytes
  memcpy(params+8, &x, 2);  // first  parameter, offset 8, 2 Bytes
#endif

  uint32_t result;

  if(mShowDates!=nullptr)
  {
    VIAResult rc =  mShowDates->Call(&result, params);
    if (rc==kVIA_OK)
    {
       return rc;   // call successful
    }
  }

  return -1; // call failed
}

void CaplInstanceData::DllInfo(const char* x)
{
  // Prepare the parameters for the call stack of CAPL.
  // Arrays uses a 8 byte (64 bit DLL: 12 byte) on the stack, 4 Bytes for the number of element,
  // and 4 bytes (64 bit DLL: 8 byte) for the pointer to the array
  int32_t sizeX = (int32)strlen(x)+1;

#if defined(X64)
  uint8_t params[16];              // parameters for call stack, 16 Bytes total (8 bytes per parameter, reverse order of parameters)
  memcpy(params+8, &sizeX, 4);   // array size    of first parameter, 4 Bytes
  memcpy(params+0, &x,     8);   // array pointer of first parameter, 8 Bytes
#else
  uint8_t params[8];               // parameters for call stack, 8 Bytes total
  memcpy(params+0, &sizeX, 4);   // array size    of first parameter, 4 Bytes
  memcpy(params+4, &x,     4);   // array pointer of first parameter, 4 Bytes
#endif

  if(mDllInfo!=nullptr)
  {
    uint32_t result; // dummy variable
    VIAResult rc =  mDllInfo->Call(&result, params);
  }
}

void CaplInstanceData::ArrayValues(uint32_t flags, uint32_t numberOfDatabytes, uint8_t databytes[], uint8_t controlcode)
{
  // Prepare the parameters for the call stack of CAPL. The stack grows
  // from top to down, so the first parameter in the parameter list is the last
  // one in memory. CAPL uses also a 32 bit alignment for the parameters.
  // Arrays uses a 8 byte (64 bit DLL: 12 byte) on the stack, 4 Bytes for the number of element,
  // and 4 bytes (64 bit DLL: 8 byte) for the pointer to the array

#if defined(X64)
  uint8_t params[32];                           // parameters for call stack, 32 Bytes total (8 bytes per parameter, reverse order of parameters)
  memcpy(params+24, &controlcode,       1);   // third parameter,                  offset 24, 1 Bytes
  memcpy(params+16, &numberOfDatabytes, 4);   // second parameter (array size),    offset 16, 4 Bytes
  memcpy(params+ 8, &databytes,         8);   // second parameter (array pointer), offset  8, 8 Bytes
  memcpy(params+ 0, &flags,             4);   // first  parameter,                 offset  0, 4 Bytes
#else
  uint8_t params[16];                           // parameters for call stack, 16 Bytes total
  memcpy(params+ 0, &controlcode,       1);   // third parameter,                  offset  0, 1 Bytes
  memcpy(params+ 4, &numberOfDatabytes, 4);   // second parameter (array size),    offset  4, 4 Bytes
  memcpy(params+ 8, &databytes,         4);   // second parameter (array pointer), offset  8, 4 Bytes
  memcpy(params+12, &flags,             4);   // first  parameter,                 offset 12, 4 Bytes
#endif

  if(mArrayValues!=nullptr)
  {
    uint32_t result; // dummy variable
    VIAResult rc =  mArrayValues ->Call(&result, params);
  }

}

CaplInstanceData* GetCaplInstanceData(uint32_t handle)
{
  VCaplMap::iterator lSearchResult(gCaplMap.find(handle));
  if ( gCaplMap.end()==lSearchResult )
  {
    return nullptr;
  } 
  else 
  {
    return lSearchResult->second;
  }
}

// ============================================================================
// CaplInstanceData
//
// Data local for a single CAPL Block.
//
// A CAPL-DLL can be used by more than one CAPL-Block, so every piece of
// information thats like a global variable in CAPL, must now be wrapped into
// an instance of an object.
// ============================================================================

void CAPLEXPORT CAPLPASCAL appInit (uint32_t handle)
{
  CaplInstanceData* instance = GetCaplInstanceData(handle);
  if ( nullptr==instance )
  {
    VServiceMap::iterator lSearchService(gServiceMap.find(handle));
    if ( gServiceMap.end()!=lSearchService )
    {
      VIACapl* service = lSearchService->second;
      try
      {
        instance = new CaplInstanceData(service);
      }
      catch ( std::bad_alloc& )
      {
        return; // proceed without change
      }
      instance->GetCallbackFunctions();
      gCaplMap[handle] = instance;
    }
  }
}

void CAPLEXPORT CAPLPASCAL appEnd (uint32_t handle)
{
  CaplInstanceData* inst = GetCaplInstanceData(handle);
  if (inst==nullptr)
  {
    return;
  }
  inst->ReleaseCallbackFunctions();

  delete inst;
  inst = nullptr;
  gCaplMap.erase(handle);
}

int32_t CAPLEXPORT CAPLPASCAL appSetValue (uint32_t handle, int32_t x)
{
  CaplInstanceData* inst = GetCaplInstanceData(handle);
  if (inst==nullptr)
  {
    return -1;
  }

  return inst->ShowValue(x);
}

int32_t CAPLEXPORT CAPLPASCAL appReadData (uint32_t handle, int32_t a)
{
  CaplInstanceData* inst = GetCaplInstanceData(handle);
  if (inst==nullptr)
  {
    return -1;
  }

  int16_t  x = (a>=0) ? +1 : -1;
  uint32_t y = abs(a);
  int16_t  z = (int16)(a & 0x0f000000) >> 24;

  inst->DllVersion("Version 1.1");

  inst->DllInfo("DLL: processing");

  uint8_t databytes[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

  inst->ArrayValues( 0xaabbccdd, sizeof(databytes), databytes, 0x01);

  return inst->ShowDates( x, y, z);
}


// ============================================================================
// VIARegisterCDLL
// ============================================================================

VIACLIENT(void) VIARegisterCDLL (VIACapl* service)
{
  uint32_t  handle;
  VIAResult result;

  if (service==nullptr)
  {
    return;
  }

  result = service->GetCaplHandle(&handle);
  if(result!=kVIA_OK)
  {
    return;
  }

  // appInit (internal) resp. "DllInit" (CAPL code) has to follow
  gServiceMap[handle] = service;
}

void ClearAll()
{
  // destroy objects created by this DLL
  // may result from forgotten DllEnd calls
  VCaplMap::iterator lIter=gCaplMap.begin();
  const int32_t cNumberOfEntries = (int32_t)gCaplMap.size();
  int32_t i = 0;
  while ( lIter!=gCaplMap.end() && i<cNumberOfEntries )
  {
    appEnd( (*lIter).first );
    lIter = gCaplMap.begin(); // first element should have vanished
    i++; // assure that no more erase trials take place than the original size of the map
  }

  // just for clarity (would be done automatically)
  gCaplMap.clear();
  gServiceMap.clear();
}

void CAPLEXPORT CAPLPASCAL voidFct( void )
{
   // do something
   data = 55;
}

uint32_t CAPLEXPORT CAPLPASCAL appLongFuncName( void )
{
  return 1;
}

void CAPLEXPORT CAPLPASCAL appPut(uint32_t x)
{
  data = x;
}

uint32_t CAPLEXPORT CAPLPASCAL appGet(void)
{
  return data;
}

int32_t CAPLEXPORT CAPLPASCAL appAdd(int32_t x, int32_t y)
{
  int32_t z = x + y;

  return z;
}

int32_t CAPLEXPORT CAPLPASCAL appSubtract(int32_t x, int32_t y)
{
  int32_t z = x - y;

  return z;
}

int32_t CAPLEXPORT CAPLPASCAL appAddValues63(int32_t val01, int32_t val02, int32_t val03, int32_t val04, int32_t val05, int32_t val06, int32_t val07, int32_t val08,
                                             int32_t val09, int32_t val10, int32_t val11, int32_t val12, int32_t val13, int32_t val14, int32_t val15, int32_t val16,
                                             int32_t val17, int32_t val18, int32_t val19, int32_t val20, int32_t val21, int32_t val22, int32_t val23, int32_t val24,
                                             int32_t val25, int32_t val26, int32_t val27, int32_t val28, int32_t val29, int32_t val30, int32_t val31, int32_t val32,
                                             int32_t val33, int32_t val34, int32_t val35, int32_t val36, int32_t val37, int32_t val38, int32_t val39, int32_t val40,
                                             int32_t val41, int32_t val42, int32_t val43, int32_t val44, int32_t val45, int32_t val46, int32_t val47, int32_t val48,
                                             int32_t val49, int32_t val50, int32_t val51, int32_t val52, int32_t val53, int32_t val54, int32_t val55, int32_t val56,
                                             int32_t val57, int32_t val58, int32_t val59, int32_t val60, int32_t val61, int32_t val62, int32_t val63)
{
  int32_t z = val01+val02+val03+val04+val05+val06+val07+val08
            + val09+val10+val11+val12+val13+val14+val15+val16
            + val17+val18+val19+val20+val21+val22+val23+val24
            + val25+val26+val27+val28+val29+val30+val31+val32
            + val33+val34+val35+val36+val37+val38+val39+val40
            + val41+val42+val43+val44+val45+val46+val47+val48
            + val49+val50+val51+val52+val53+val54+val55+val56
            + val57+val58+val59+val60+val61+val62+val63;

  return z;
}

int32_t CAPLEXPORT CAPLPASCAL appAddValues64(int32_t val01, int32_t val02, int32_t val03, int32_t val04, int32_t val05, int32_t val06, int32_t val07, int32_t val08,
                                             int32_t val09, int32_t val10, int32_t val11, int32_t val12, int32_t val13, int32_t val14, int32_t val15, int32_t val16,
                                             int32_t val17, int32_t val18, int32_t val19, int32_t val20, int32_t val21, int32_t val22, int32_t val23, int32_t val24,
                                             int32_t val25, int32_t val26, int32_t val27, int32_t val28, int32_t val29, int32_t val30, int32_t val31, int32_t val32,
                                             int32_t val33, int32_t val34, int32_t val35, int32_t val36, int32_t val37, int32_t val38, int32_t val39, int32_t val40,
                                             int32_t val41, int32_t val42, int32_t val43, int32_t val44, int32_t val45, int32_t val46, int32_t val47, int32_t val48,
                                             int32_t val49, int32_t val50, int32_t val51, int32_t val52, int32_t val53, int32_t val54, int32_t val55, int32_t val56,
                                             int32_t val57, int32_t val58, int32_t val59, int32_t val60, int32_t val61, int32_t val62, int32_t val63, int32_t val64)
{
  int32_t z = val01+val02+val03+val04+val05+val06+val07+val08
            + val09+val10+val11+val12+val13+val14+val15+val16
            + val17+val18+val19+val20+val21+val22+val23+val24
            + val25+val26+val27+val28+val29+val30+val31+val32
            + val33+val34+val35+val36+val37+val38+val39+val40
            + val41+val42+val43+val44+val45+val46+val47+val48
            + val49+val50+val51+val52+val53+val54+val55+val56
            + val57+val58+val59+val60+val61+val62+val63+val64;

  return z;
}
#define EightLongPars 'L','L','L','L','L','L','L','L'
#define SixtyFourLongPars EightLongPars,EightLongPars,EightLongPars,EightLongPars,EightLongPars,EightLongPars,EightLongPars,EightLongPars

void CAPLEXPORT CAPLPASCAL appGetDataTwoPars(uint32_t numberBytes, uint8_t dataBlock[] )
{
  unsigned int i;
  for (i = 0; i < numberBytes; i++) 
  {
    dataBlock[i] = dlldata[i];
  }
}

void CAPLEXPORT CAPLPASCAL appPutDataTwoPars(uint32_t numberBytes, const uint8_t dataBlock[] )
{
  unsigned int i;
  for (i = 0; i < numberBytes; i++) 
  {
    dlldata[i] = dataBlock[i];
  }
}

// get data from DLL into CAPL memory
void CAPLEXPORT CAPLPASCAL appGetDataOnePar(uint8_t dataBlock[] )
{
  //  get first element
  dataBlock[0] = (uint8_t)data;
}

// put data from CAPL array to DLL
void CAPLEXPORT CAPLPASCAL appPutDataOnePar(const uint8_t dataBlock[] )
{
  // put first element
  data = dataBlock[0];

}
// ============================================================================
// CAPL_DLL_INFO_LIST : list of exported functions
//   The first field is predefined and mustn't be changed!
//   The list has to end with a {0,0} entry!
// New struct supporting function names with up to 50 characters
// ============================================================================





/******************************************************************************
 * Name:    CRC-4/ITU           x4+x+1
 * Poly:    0x03
 * Init:    0x00
 * Refin:   True
 * Refout:  True
 * Xorout:  0x00
 * Note:
 *****************************************************************************/
uint8_t crc4_itu(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint8_t crc = 0;                // Initial value
	while (length--)
	{
		crc ^= *data++;                 // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0x0C;// 0x0C = (reverse 0x03)>>(8-4)
			else
				crc = (crc >> 1);
		}
	}
	return crc;
}

/******************************************************************************
 * Name:    CRC-5/EPC           x5+x3+1
 * Poly:    0x09
 * Init:    0x09
 * Refin:   False
 * Refout:  False
 * Xorout:  0x00
 * Note:
 *****************************************************************************/
uint8_t crc5_epc(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint8_t crc = 0x48;        // Initial value: 0x48 = 0x09<<(8-5)
	while (length--)
	{
		crc ^= *data++;        // crc ^= *data; data++;
		for (i = 0; i < 8; i++)
		{
			if (crc & 0x80)
				crc = (crc << 1) ^ 0x48;        // 0x48 = 0x09<<(8-5)
			else
				crc <<= 1;
		}
	}
	return crc >> 3;
}

/******************************************************************************
 * Name:    CRC-5/ITU           x5+x4+x2+1
 * Poly:    0x15
 * Init:    0x00
 * Refin:   True
 * Refout:  True
 * Xorout:  0x00
 * Note:
 *****************************************************************************/
uint8_t crc5_itu(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint8_t crc = 0;                // Initial value
	while (length--)
	{
		crc ^= *data++;                 // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0x15;// 0x15 = (reverse 0x15)>>(8-5)
			else
				crc = (crc >> 1);
		}
	}
	return crc;
}

/******************************************************************************
 * Name:    CRC-5/USB           x5+x2+1
 * Poly:    0x05
 * Init:    0x1F
 * Refin:   True
 * Refout:  True
 * Xorout:  0x1F
 * Note:
 *****************************************************************************/
uint8_t crc5_usb(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint8_t crc = 0x1F;                // Initial value
	while (length--)
	{
		crc ^= *data++;                 // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0x14;// 0x14 = (reverse 0x05)>>(8-5)
			else
				crc = (crc >> 1);
		}
	}
	return crc ^ 0x1F;
}

/******************************************************************************
 * Name:    CRC-6/ITU           x6+x+1
 * Poly:    0x03
 * Init:    0x00
 * Refin:   True
 * Refout:  True
 * Xorout:  0x00
 * Note:
 *****************************************************************************/
uint8_t crc6_itu(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint8_t crc = 0;         // Initial value
	while (length--)
	{
		crc ^= *data++;        // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0x30;// 0x30 = (reverse 0x03)>>(8-6)
			else
				crc = (crc >> 1);
		}
	}
	return crc;
}

/******************************************************************************
 * Name:    CRC-7/MMC           x7+x3+1
 * Poly:    0x09
 * Init:    0x00
 * Refin:   False
 * Refout:  False
 * Xorout:  0x00
 * Use:     MultiMediaCard,SD,ect.
 *****************************************************************************/
uint8_t crc7_mmc(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint8_t crc = 0;        // Initial value
	while (length--)
	{
		crc ^= *data++;        // crc ^= *data; data++;
		for (i = 0; i < 8; i++)
		{
			if (crc & 0x80)
				crc = (crc << 1) ^ 0x12;        // 0x12 = 0x09<<(8-7)
			else
				crc <<= 1;
		}
	}
	return crc >> 1;
}

/******************************************************************************
 * Name:    CRC-8               x8+x2+x+1
 * Poly:    0x07
 * Init:    0x00
 * Refin:   False
 * Refout:  False
 * Xorout:  0x00
 * Note:
 *****************************************************************************/
uint8_t crc8(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint8_t crc = 0;        // Initial value
	while (length--)
	{
		crc ^= *data++;        // crc ^= *data; data++;
		for (i = 0; i < 8; i++)
		{
			if (crc & 0x80)
				crc = (crc << 1) ^ 0x07;
			else
				crc <<= 1;
		}
	}
	return crc;
}

/******************************************************************************
 * Name:    CRC-8/ITU           x8+x2+x+1
 * Poly:    0x07
 * Init:    0x00
 * Refin:   False
 * Refout:  False
 * Xorout:  0x55
 * Alias:   CRC-8/ATM
 *****************************************************************************/
uint8_t crc8_itu(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint8_t crc = 0;        // Initial value
	while (length--)
	{
		crc ^= *data++;        // crc ^= *data; data++;
		for (i = 0; i < 8; i++)
		{
			if (crc & 0x80)
				crc = (crc << 1) ^ 0x07;
			else
				crc <<= 1;
		}
	}
	return crc ^ 0x55;
}

/******************************************************************************
 * Name:    CRC-8/ROHC          x8+x2+x+1
 * Poly:    0x07
 * Init:    0xFF
 * Refin:   True
 * Refout:  True
 * Xorout:  0x00
 * Note:
 *****************************************************************************/
uint8_t crc8_rohc(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint8_t crc = 0xFF;         // Initial value
	while (length--)
	{
		crc ^= *data++;            // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0xE0;        // 0xE0 = reverse 0x07
			else
				crc = (crc >> 1);
		}
	}
	return crc;
}

/******************************************************************************
 * Name:    CRC-8/MAXIM         x8+x5+x4+1
 * Poly:    0x31
 * Init:    0x00
 * Refin:   True
 * Refout:  True
 * Xorout:  0x00
 * Alias:   DOW-CRC,CRC-8/IBUTTON
 * Use:     Maxim(Dallas)'s some devices,e.g. DS18B20
 *****************************************************************************/
uint8_t crc8_maxim(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint8_t crc = 0;         // Initial value
	while (length--)
	{
		crc ^= *data++;        // crc ^= *data; data++;
		for (i = 0; i < 8; i++)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0x8C;        // 0x8C = reverse 0x31
			else
				crc >>= 1;
		}
	}
	return crc;
}

/******************************************************************************
 * Name:    CRC-16/IBM          x16+x15+x2+1
 * Poly:    0x8005
 * Init:    0x0000
 * Refin:   True
 * Refout:  True
 * Xorout:  0x0000
 * Alias:   CRC-16,CRC-16/ARC,CRC-16/LHA
 *****************************************************************************/
uint16_t crc16_ibm(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint16_t crc = 0;        // Initial value
	while (length--)
	{
		crc ^= *data++;        // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0xA001;        // 0xA001 = reverse 0x8005
			else
				crc = (crc >> 1);
		}
	}
	return crc;
}

/******************************************************************************
 * Name:    CRC-16/MAXIM        x16+x15+x2+1
 * Poly:    0x8005
 * Init:    0x0000
 * Refin:   True
 * Refout:  True
 * Xorout:  0xFFFF
 * Note:
 *****************************************************************************/
uint16_t crc16_maxim(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint16_t crc = 0;        // Initial value
	while (length--)
	{
		crc ^= *data++;        // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0xA001;        // 0xA001 = reverse 0x8005
			else
				crc = (crc >> 1);
		}
	}
	return ~crc;    // crc^0xffff
}

/******************************************************************************
 * Name:    CRC-16/USB          x16+x15+x2+1
 * Poly:    0x8005
 * Init:    0xFFFF
 * Refin:   True
 * Refout:  True
 * Xorout:  0xFFFF
 * Note:
 *****************************************************************************/
uint16_t crc16_usb(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint16_t crc = 0xffff;        // Initial value
	while (length--)
	{
		crc ^= *data++;            // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0xA001;        // 0xA001 = reverse 0x8005
			else
				crc = (crc >> 1);
		}
	}
	return ~crc;    // crc^0xffff
}

/******************************************************************************
 * Name:    CRC-16/MODBUS       x16+x15+x2+1
 * Poly:    0x8005
 * Init:    0xFFFF
 * Refin:   True
 * Refout:  True
 * Xorout:  0x0000
 * Note:
 *****************************************************************************/
uint16_t crc16_modbus(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint16_t crc = 0xffff;        // Initial value
	while (length--)
	{
		crc ^= *data++;            // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0xA001;        // 0xA001 = reverse 0x8005
			else
				crc = (crc >> 1);
		}
	}
	return crc;
}

/******************************************************************************
 * Name:    CRC-16/CCITT        x16+x12+x5+1
 * Poly:    0x1021
 * Init:    0x0000
 * Refin:   True
 * Refout:  True
 * Xorout:  0x0000
 * Alias:   CRC-CCITT,CRC-16/CCITT-TRUE,CRC-16/KERMIT
 *****************************************************************************/
uint16_t crc16_ccitt(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint16_t crc = 0;        // Initial value
	while (length--)
	{
		crc ^= *data++;        // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0x8408;        // 0x8408 = reverse 0x1021
			else
				crc = (crc >> 1);
		}
	}
	return crc;
}

/******************************************************************************
 * Name:    CRC-16/CCITT-FALSE   x16+x12+x5+1
 * Poly:    0x1021
 * Init:    0xFFFF
 * Refin:   False
 * Refout:  False
 * Xorout:  0x0000
 * Note:
 *****************************************************************************/
uint16_t crc16_ccitt_false(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint16_t crc = 0xffff;        //Initial value
	while (length--)
	{
		crc ^= (uint16_t)(*data++) << 8; // crc ^= (uint6_t)(*data)<<8; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc <<= 1;
		}
	}
	return crc;
}

/******************************************************************************
 * Name:    CRC-16/X25          x16+x12+x5+1
 * Poly:    0x1021
 * Init:    0xFFFF
 * Refin:   True
 * Refout:  True
 * Xorout:  0XFFFF
 * Note:
 *****************************************************************************/
uint16_t crc16_x25(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint16_t crc = 0xffff;        // Initial value
	while (length--)
	{
		crc ^= *data++;            // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0x8408;        // 0x8408 = reverse 0x1021
			else
				crc = (crc >> 1);
		}
	}
	return ~crc;                // crc^Xorout
}

/******************************************************************************
 * Name:    CRC-16/XMODEM       x16+x12+x5+1
 * Poly:    0x1021
 * Init:    0x0000
 * Refin:   False
 * Refout:  False
 * Xorout:  0x0000
 * Alias:   CRC-16/ZMODEM,CRC-16/ACORN
 *****************************************************************************/
uint16_t crc16_xmodem(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint16_t crc = 0;            // Initial value
	while (length--)
	{
		crc ^= (uint16_t)(*data++) << 8; // crc ^= (uint16_t)(*data)<<8; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc <<= 1;
		}
	}
	return crc;
}

/******************************************************************************
 * Name:    CRC-16/DNP          x16+x13+x12+x11+x10+x8+x6+x5+x2+1
 * Poly:    0x3D65
 * Init:    0x0000
 * Refin:   True
 * Refout:  True
 * Xorout:  0xFFFF
 * Use:     M-Bus,ect.
 *****************************************************************************/
uint16_t crc16_dnp(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint16_t crc = 0;            // Initial value
	while (length--)
	{
		crc ^= *data++;            // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0xA6BC;        // 0xA6BC = reverse 0x3D65
			else
				crc = (crc >> 1);
		}
	}
	return ~crc;                // crc^Xorout
}

/******************************************************************************
 * Name:    CRC-32  x32+x26+x23+x22+x16+x12+x11+x10+x8+x7+x5+x4+x2+x+1
 * Poly:    0x4C11DB7
 * Init:    0xFFFFFFF
 * Refin:   True
 * Refout:  True
 * Xorout:  0xFFFFFFF
 * Alias:   CRC_32/ADCCP
 * Use:     WinRAR,ect.
 *****************************************************************************/
uint32_t crc32(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint32_t crc = 0xffffffff;        // Initial value
	while (length--)
	{
		crc ^= *data++;                // crc ^= *data; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0xEDB88320;// 0xEDB88320= reverse 0x04C11DB7
			else
				crc = (crc >> 1);
		}
	}
	return ~crc;
}

/******************************************************************************
 * Name:    CRC-32/MPEG-2  x32+x26+x23+x22+x16+x12+x11+x10+x8+x7+x5+x4+x2+x+1
 * Poly:    0x4C11DB7
 * Init:    0xFFFFFFF
 * Refin:   False
 * Refout:  False
 * Xorout:  0x0000000
 * Note:
 *****************************************************************************/
uint32_t crc32_mpeg_2(uint8_t *data, uint16_t length)
{
	uint8_t i;
	uint32_t crc = 0xffffffff;  // Initial value
	while (length--)
	{
		crc ^= (uint32_t)(*data++) << 24;// crc ^=(uint32_t)(*data)<<24; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 0x80000000)
				crc = (crc << 1) ^ 0x04C11DB7;
			else
				crc <<= 1;
		}
	}
	return crc;
}






uint32_t crc16_ccitt_falsetest(uint8_t *data, uint32_t length)
{
	uint8_t i;
	uint16_t crc = 0xffff;        //Initial value
	while (length--)
	{
		crc ^= (uint16_t)(*data++) << 8; // crc ^= (uint6_t)(*data)<<8; data++;
		for (i = 0; i < 8; ++i)
		{
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc <<= 1;
		}
	}
	return crc;
}




































CAPL_DLL_INFO4 table[] = {
{CDLL_VERSION_NAME, (CAPL_FARCALL)CDLL_VERSION, "", "", CAPL_DLL_CDECL, 0xabcd, CDLL_EXPORT },

  {"dllInit",           (CAPL_FARCALL)appInit,          "CAPL_DLL","This function will initialize all callback functions in the CAPLDLL",'V', 1, "D", "", {"handle"}},
  {"dllEnd",            (CAPL_FARCALL)appEnd,           "CAPL_DLL","This function will release the CAPL function handle in the CAPLDLL",'V', 1, "D", "", {"handle"}},
  {"dllSetValue",       (CAPL_FARCALL)appSetValue,      "CAPL_DLL","This function will call a callback functions",'L', 2, "DL", "", {"handle","x"}},
  {"dllReadData",       (CAPL_FARCALL)appReadData,      "CAPL_DLL","This function will call a callback functions",'L', 2, "DL", "", {"handle","x"}},
  {"dllPut",            (CAPL_FARCALL)appPut,           "CAPL_DLL","This function will save data from CAPL to DLL memory",'V', 1, "D", "", {"x"}},
  {"dllGet",            (CAPL_FARCALL)appGet,           "CAPL_DLL","This function will read data from DLL memory to CAPL",'D', 0, "", "", {""}},
  {"dllVoid",           (CAPL_FARCALL)voidFct,          "CAPL_DLL","This function will overwrite DLL memory from CAPL without parameter",'V', 0, "", "", {""}},
  {"dllPutDataOnePar",  (CAPL_FARCALL)appPutDataOnePar, "CAPL_DLL","This function will put data from CAPL array to DLL",'V', 1, "B", "\001", {"datablock"}},
  {"dllGetDataOnePar",  (CAPL_FARCALL)appGetDataOnePar, "CAPL_DLL","This function will get data from DLL into CAPL memory",'V', 1, "B", "\001", {"datablock"}},
  {"dllPutDataTwoPars", (CAPL_FARCALL)appPutDataTwoPars,"CAPL_DLL","This function will put two datas from CAPL array to DLL",'V', 2, "DB", "\000\001", {"noOfBytes","datablock"}},// number of pars in octal format
  {"dllGetDataTwoPars", (CAPL_FARCALL)appGetDataTwoPars,"CAPL_DLL","This function will get two datas from DLL into CAPL memory",'V', 2, "DB", "\000\001", {"noOfBytes","datablock"}},
  {"dllAdd",            (CAPL_FARCALL)appAdd,           "CAPL_DLL","This function will add two values. The return value is the result",'L', 2, "LL", "", {"x","y"}},
  //{"crc16_ccitt_false", (CAPL_FARCALL)crc16_ccitt_false,"CAPL_DLL_TEST","crc16_ccitt_false",'W', 2, "BW", "\001\000", {"data","length"}},
  {"demo1",				(CAPL_FARCALL)crc16_ccitt_falsetest,"CAPL_DLL_TEST","crc16_ccitt_false",'D', 2, {'B' - 128,'D'}, "\001\000", {"data","length"}},
  {"dllSubtract",       (CAPL_FARCALL)appSubtract,      "CAPL_DLL","This function will substract two values. The return value is the result",'L', 2, "LL", "", {"x","y"}},
  {"dllSupportLongFunctionNamesWithUpTo50Characters",   (CAPL_FARCALL)appLongFuncName,      "CAPL_DLL","This function shows the support of long function names",'D', 0, "", "", {""}},
  {"dllAdd63Parameters", (CAPL_FARCALL)appAddValues63,  "CAPL_DLL", "This function will add 63 values. The return value is the result",'L', 63, "LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL",  "", {"val01","val02","val03","val04","val05","val06","val07","val08","val09","val10","val11","val12","val13","val14","val15","val16","val17","val18","val19","val20","val21","val22","val23","val24","val25","val26","val27","val28","val29","val30","val31","val32","val33","val34","val35","val36","val37","val38","val39","val40","val41","val42","val43","val44","val45","val46","val47","val48","val49","val50","val51","val52","val53","val54","val55","val56","val57","val58","val59","val60","val61","val62","val63"}},
  {"dllAdd64Parameters", (CAPL_FARCALL)appAddValues64,  "CAPL_DLL", "This function will add 64 values. The return value is the result",'L', 64, {SixtyFourLongPars},                                                "", {"val01","val02","val03","val04","val05","val06","val07","val08","val09","val10","val11","val12","val13","val14","val15","val16","val17","val18","val19","val20","val21","val22","val23","val24","val25","val26","val27","val28","val29","val30","val31","val32","val33","val34","val35","val36","val37","val38","val39","val40","val41","val42","val43","val44","val45","val46","val47","val48","val49","val50","val51","val52","val53","val54","val55","val56","val57","val58","val59","val60","val61","val62","val63","val64"}},

{0, 0}
};
CAPLEXPORT CAPL_DLL_INFO4* caplDllTable4 = table;




