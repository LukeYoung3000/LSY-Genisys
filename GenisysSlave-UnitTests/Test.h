#pragma once

#include <iostream>

#include "Logging.h"
#include "DataFrame.h"


// Convert byte to binary string
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0') 



class DataFrame_Tests
{
public:

	bool RunTests();
	bool FunctionalTest01();
	bool FunctionalTest02();


public: // Helpers

	void PrintFrame(LSY::DataFrame & df);

};







