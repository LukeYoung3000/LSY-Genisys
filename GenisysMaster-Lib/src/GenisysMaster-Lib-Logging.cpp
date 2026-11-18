#include <stdarg.h>
#include <chrono>
#include <iostream>

#include "GenisysMaster-Lib-Logging.h"


namespace LSY
{
	std::function<bool(std::string)> MasterLogging::log_debug;
	std::function<bool(std::string)> MasterLogging::log_info;
	std::function<bool(std::string)> MasterLogging::log_warning;
	std::function<bool(std::string)> MasterLogging::log_error;

	bool MasterLogging::AddLogDebugFunc(std::function<bool(std::string)> func)
	{
		if (func)
		{
			log_debug = func;
			return true;
		}
		return false;
	}

	bool MasterLogging::AddLogInfoFunc(std::function<bool(std::string)> func)
	{
		if (func)
		{
			log_info = func;
			return true;
		}
		return false;
	}

	bool MasterLogging::AddLogWarningFunc(std::function<bool(std::string)> func)
	{
		if (func)
		{
			log_warning = func;
			return true;
		}
		return false;
	}

	bool MasterLogging::AddLogErrorFunc(std::function<bool(std::string)> func)
	{
		if (func)
		{
			log_error = func;
			return true;
		}
		return false;
	}

	bool MasterLogging::LogDebug(const std::string &log_msg)
	{
		if (log_debug)
			return log_debug(log_msg);
		return false;
	}

	bool MasterLogging::LogInfo(const std::string &log_msg)
	{
		if (log_info)
			return log_info(log_msg);
		return false;
	}

	bool MasterLogging::LogWarning(const std::string &log_msg)
	{
		if (log_warning)
			return log_warning(log_msg);
		return false;
	}

	bool MasterLogging::LogError(const std::string &log_msg)
	{
		if (log_error)
			return log_error(log_msg);
		return false;
	}


	bool MasterLogging::LogDebugF(const char* format, ...)
	{
		char buffer[300];

		va_list args;
		va_start(args, format);

		int err = vsnprintf_s(buffer, 280, _TRUNCATE, format, args);

		va_end(args);

		if (err == -1)
		{
			// Error - Message Was Larger Than 300 Chars And Was Truncated
			std::string error_msg = "MasterLogging::LogDebug: String Was Truncated";
			LogError(error_msg);
		}

		std::string log_msg = buffer;
		return LogDebug(log_msg);
	}

	bool MasterLogging::LogInfoF(const char* format, ...)
	{
		char buffer[300];

		va_list args;
		va_start(args, format);

		int err = vsnprintf_s(buffer, 280, _TRUNCATE, format, args);

		va_end(args);

		if (err == -1)
		{
			// Error - Message Was Larger Than 300 Chars And Was Truncated
			std::string error_msg = "MasterLogging::LogInfo: String Was Truncated";
			LogError(error_msg);
		}

		std::string log_msg = buffer;
		return LogInfo(log_msg);
	}

	bool MasterLogging::LogWarningF(const char* format, ...)
	{
		char buffer[300];

		va_list args;
		va_start(args, format);

		int err = vsnprintf_s(buffer, 280, _TRUNCATE, format, args);

		va_end(args);

		if (err == -1)
		{
			// Error - Message Was Larger Than 300 Chars And Was Truncated
			std::string error_msg = "MasterLogging::LogWarning: String Was Truncated";
			LogError(error_msg);
		}

		std::string log_msg = buffer;
		return LogWarning(log_msg);
	}

	bool MasterLogging::LogErrorF(const char* format, ...)
	{
		char buffer[300];

		va_list args;
		va_start(args, format);

		int err = vsnprintf_s(buffer, 280, _TRUNCATE, format, args);

		va_end(args);

		if (err == -1)
		{
			// Error - Message Was Larger Than 300 Chars And Was Truncated
			std::string error_msg = "MasterLogging::LogError: String Was Truncated";
			LogError(error_msg);
		}

		std::string log_msg = buffer;
		return LogError(log_msg);
	}
}
