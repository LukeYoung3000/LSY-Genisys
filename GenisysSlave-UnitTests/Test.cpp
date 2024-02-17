#include "Test.h"

using namespace LSY;




bool DataFrame_Tests::RunTests()
{
	bool result = true;

	result = result && FunctionalTest01();
	result = result && FunctionalTest02();

	return result;
}





/* ----- FuctionalTest01 -----

This method tests "User API" of the LSY::DataFrame class.
The following methods are tested against a known good byte vector.

	- WriteBit
	- WriteByte
	- WriteBytes
	- ReadBit
	- ReadByte
	- ReadBytes
	- []

*/
bool DataFrame_Tests::FunctionalTest01()
{

	std::string	frame_name = "Test Frame";
	uint64_t	frame_num_bytes = 20;
	DataFrame data_frame(frame_name, frame_num_bytes);

	Logging::LogDebugF("----- DataFrame_Tests::FunctionalTest01 ------");



	// ------ WriteBit Tests -------
	bool write_bit_success = true;

	write_bit_success = write_bit_success && data_frame.WriteBit(0, 1, true);	// byte[0] = 130
	write_bit_success = write_bit_success && data_frame.WriteBit(0, 7, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(1, 0, true);	// byte[1] = 255
	write_bit_success = write_bit_success && data_frame.WriteBit(1, 1, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(1, 2, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(1, 3, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(1, 4, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(1, 5, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(1, 6, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(1, 7, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(2, 0, true);	// byte[2] = 252
	write_bit_success = write_bit_success && data_frame.WriteBit(2, 1, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(2, 2, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(2, 3, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(2, 4, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(2, 5, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(2, 6, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(2, 7, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(2, 0, false);
	write_bit_success = write_bit_success && data_frame.WriteBit(2, 1, false);
	write_bit_success = write_bit_success && data_frame.WriteBit(3, 0, true);	// byte[3] = 15
	write_bit_success = write_bit_success && data_frame.WriteBit(3, 1, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(3, 2, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(3, 3, true);
	write_bit_success = write_bit_success && data_frame.WriteBit(4, 0, true);	// byte[4] = 1

	if (!write_bit_success)
	{
		Logging::LogErrorF("DataFrame_Tests::FunctionalTest01: Failed WriteBit Test");
		return false;
	}
		



	// ------ WriteByte Tests -------
	bool write_byte_success = true;

	write_byte_success = write_byte_success && data_frame.WriteByte(5, 100);	// byte[5] = 100
	write_byte_success = write_byte_success && data_frame.WriteByte(6, 12);		// byte[6] = 12
	write_byte_success = write_byte_success && data_frame.WriteByte(7, 55);		// byte[7] = 55
	write_byte_success = write_byte_success && data_frame.WriteByte(8, 32);		// byte[8] = 32
	write_byte_success = write_byte_success && data_frame.WriteByte(9, 1);		// byte[9] = 1

	if (!write_byte_success)
	{
		Logging::LogErrorF("DataFrame_Tests::FunctionalTest01: Failed WriteByte Test");
		return false;
	}
		



	// ------ WriteBytes Tests -------
	std::vector<uint8_t> vals = { 255, 43, 11, 2, 240 };

	if (!data_frame.WriteBytes(10, vals))
	{
		Logging::LogErrorF("DataFrame_Tests::FunctionalTest01: Failed WriteBytes Test");
		return false;
	}
		




	// ------ Print Test Data Frame ------
	Logging::LogDebugF("----- Test Data Frame ------");
	PrintFrame(data_frame);




	// ------ Create Result Vector -------
	std::vector<uint8_t> result = std::vector<uint8_t>(frame_num_bytes, 0);
	result[0] = 130;
	result[1] = 255;
	result[2] = 252;
	result[3] = 15;
	result[4] = 1;
	result[5] = 100;
	result[6] = 12;
	result[7] = 55;
	result[8] = 32;
	result[9] = 1;
	result[10] = 255;
	result[11] = 43;
	result[12] = 11;
	result[13] = 2;
	result[14] = 240;






	// ------ Compare Result Using "ReadBit" ------
	for (size_t i = 0; i < result.size(); i++)
	{
		for (size_t j = 0; j < 8; j++)
		{
			bool bit = false;
			bool read_bit_success = data_frame.ReadBit(i, (uint8_t)j, bit);

			if (!read_bit_success)
			{
				Logging::LogErrorF("DataFrame_Tests::FunctionalTest01: Failed ReadBit Test 1");
				return false;
			}

			uint8_t result_byte = result[i];
			result_byte = result_byte >> j;
			result_byte = result_byte & (uint8_t)1;
			bool result_bit = result_byte;

			if (result_bit != bit)
			{
				Logging::LogErrorF("DataFrame_Tests::FunctionalTest01: Failed ReadBit Test 2");
				return false;
			}

		}
	}





	// ------ Compare Result Using "ReadByte" ------
	for (size_t i = 0; i < result.size(); i++)
	{
		uint8_t byte = 0;
		bool read_byte_success = data_frame.ReadByte(i, byte);

		if (!read_byte_success)
		{
			Logging::LogErrorF("DataFrame_Tests::FunctionalTest01: Failed ReadByte Test 1");
			return false;
		}
			
		if (result[i] != byte)
		{
			Logging::LogErrorF("DataFrame_Tests::FunctionalTest01: Failed ReadByte Test 2");
			return false;
		}

		// Check [] Operator Overload
		if (result[i] != data_frame[i])
		{
			Logging::LogErrorF("DataFrame_Tests::FunctionalTest01: Failed ReadByte Test 3");
			return false;
		}
	}

	


	// ------ Compare Result Using "ReadBytes" ------
	std::vector<uint8_t> bytes;
	
	if (!data_frame.ReadBytes(0, frame_num_bytes, bytes))
	{
		Logging::LogErrorF("DataFrame_Tests::FunctionalTest01: Failed ReadBytes Test 1");
		return false;
	}

	if (bytes.size() != result.size())
	{
		Logging::LogErrorF("DataFrame_Tests::FunctionalTest01: Failed ReadBytes Test 2");
		return false;
	}

	for (size_t i = 0; i < result.size(); i++)
	{
		if (result[i] != bytes[i])
		{
			Logging::LogErrorF("DataFrame_Tests::FunctionalTest01: Failed ReadBytes Test 3");
			return false;
		}
			
	}
		



	Logging::LogInfoF("DataFrame_Tests::FunctionalTest01: Passed Test");
	return true;

}





/* ----- FuctionalTest02 -----

This method tests "Interface To Genisys Protocol" of the LSY::DataFrame class.
The following methods are tested against know good data.

	- GetUpdates
	- DataAck

*/
bool DataFrame_Tests::FunctionalTest02()
{

	std::string frame_name = "FunctionalTest02";
	uint64_t frame_num_bytes = 20;
	DataFrame data_frame(frame_name, frame_num_bytes);

	std::vector<uint64_t> byte_offsets;
	std::vector<uint8_t> values;
	bool test = false;



	Logging::LogDebugF("----- DataFrame_Tests::FunctionalTest02 ------");





	// Write Bytes To Data Frame
	data_frame.WriteByte(0, 100);
	data_frame.WriteByte(2, 50);



	// Test Genisys Interface 3 Times
	// "GetUpdate" Should Return The Same Results 3 Times
	for (size_t i = 0; i < 3; i++)
	{
		if (!data_frame.GetUpdates(byte_offsets, values))
		{
			Logging::LogErrorF("DataFrame_Tests::FunctionalTest02: Failed GetUpdates Test 1");
			return false;
		}

		if (byte_offsets.size() != 2)
		{
			Logging::LogErrorF("DataFrame_Tests::FunctionalTest02: Failed GetUpdates Test 2");
			return false;
		}

		test = \
			byte_offsets[0] == 0 && values[0] == 100 && \
			byte_offsets[1] == 2 && values[1] == 50;

		if (!test)
		{
			Logging::LogErrorF("DataFrame_Tests::FunctionalTest02: Failed GetUpdates Test 3");
			return false;
		}
	}

	// Check The Get Updates Doesn't Return Any Updates
	if (!data_frame.GetUpdates(byte_offsets, values))
	{
		Logging::LogErrorF("DataFrame_Tests::FunctionalTest02: Failed GetUpdates Test 4");
		return false;
	}

	if (!byte_offsets.empty() || !values.empty())
	{
		Logging::LogErrorF("DataFrame_Tests::FunctionalTest02: Failed GetUpdates Test 5");
		return false;
	}









	// Write Bytes To Data Frame
	data_frame.WriteByte(4, 12);
	data_frame.WriteByte(10, 1);



	// Test Genisys Interface 1 Time
	// Call "DataAck" After First "GetUpdates" Call
	if (!data_frame.GetUpdates(byte_offsets, values))
	{
		Logging::LogErrorF("DataFrame_Tests::FunctionalTest02: Failed GetUpdates Test 6");
		return false;
	}

	if (byte_offsets.size() != 2)
	{
		Logging::LogErrorF("DataFrame_Tests::FunctionalTest02: Failed GetUpdates Test 7");
		return false;
	}

	test = \
		byte_offsets[0] == 4  && values[0] == 12 && \
		byte_offsets[1] == 10 && values[1] == 1;

	if (!test)
	{
		Logging::LogErrorF("DataFrame_Tests::FunctionalTest02: Failed GetUpdates Test 8");
		return false;
	}

	if (!data_frame.DataAck())
	{
		Logging::LogErrorF("DataFrame_Tests::FunctionalTest02: Failed GetUpdates Test 9");
		return false;
	}

	if (!data_frame.GetUpdates(byte_offsets, values))
	{
		Logging::LogErrorF("DataFrame_Tests::FunctionalTest02: Failed GetUpdates Test 10");
		return false;
	}

	if (!byte_offsets.empty() || !values.empty())
	{
		Logging::LogErrorF("DataFrame_Tests::FunctionalTest02: Failed GetUpdates Test 11");
		return false;
	}









	Logging::LogInfoF("DataFrame_Tests::FunctionalTest02: Passed Test");
	return true;

}




void DataFrame_Tests::PrintFrame(DataFrame & df)
{

	std::vector<uint8_t> values;
	df.ReadBytes(0, df.GetNumBytes(), values);

	Logging::LogDebugF("|----------------------------------|");
	Logging::LogDebugF("|--Index--|DecVal|HexVal|--BinVal--|");
	for (size_t i = 0; i < values.size(); i++)
		Logging::LogDebugF("|%9d|%6d|%6x| " BYTE_TO_BINARY_PATTERN " |", i, values[i], values[i], BYTE_TO_BINARY(values[i]));

}