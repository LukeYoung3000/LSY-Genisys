#include <stdarg.h>
#include <chrono>
#include <iostream>

#include "Logging.h"


namespace LSY
{
	std::function<bool(std::string)> log_debug;
	std::function<bool(std::string)> log_info;
	std::function<bool(std::string)> log_warning;
	std::function<bool(std::string)> log_error;

	bool Logging::AddLogDebugFunc(std::function<bool(std::string)> func)
	{
		if (func)
		{
			log_debug = func;
			return true;
		}
		return false;
	}

	bool Logging::AddLogInfoFunc(std::function<bool(std::string)> func)
	{
		if (func)
		{
			log_info = func;
			return true;
		}
		return false;
	}

	bool Logging::AddLogWarningFunc(std::function<bool(std::string)> func)
	{
		if (func)
		{
			log_warning = func;
			return true;
		}
		return false;
	}

	bool Logging::AddLogErrorFunc(std::function<bool(std::string)> func)
	{
		if (func)
		{
			log_error = func;
			return true;
		}
		return false;
	}

	bool Logging::LogDebug(const std::string &log_msg)
	{
		if (log_debug)
			return log_debug(log_msg);
		return false;
	}

	bool Logging::LogInfo(const std::string &log_msg)
	{
		if (log_info)
			return log_info(log_msg);
		return false;
	}

	bool Logging::LogWarning(const std::string &log_msg)
	{
		if (log_warning)
			return log_warning(log_msg);
		return false;
	}

	bool Logging::LogError(const std::string &log_msg)
	{
		if (log_error)
			return log_error(log_msg);
		return false;
	}


	bool Logging::LogDebugF(const char* format, ...)
	{
		char buffer[300];

		va_list args;
		va_start(args, format);

		int err = vsnprintf_s(buffer, 280, _TRUNCATE, format, args);

		va_end(args);

		if (err == -1)
		{
			// Error - Message Was Larger Than 300 Chars And Was Truncated
			std::string error_msg = "Logging::LogDebug: String Was Truncated";
			LogError(error_msg);
		}

		std::string log_msg = buffer;
		return LogDebug(log_msg);
	}

	bool Logging::LogInfoF(const char* format, ...)
	{
		char buffer[300];

		va_list args;
		va_start(args, format);

		int err = vsnprintf_s(buffer, 280, _TRUNCATE, format, args);

		va_end(args);

		if (err == -1)
		{
			// Error - Message Was Larger Than 300 Chars And Was Truncated
			std::string error_msg = "Logging::LogInfo: String Was Truncated";
			LogError(error_msg);
		}

		std::string log_msg = buffer;
		return LogInfo(log_msg);
	}

	bool Logging::LogWarningF(const char* format, ...)
	{
		char buffer[300];

		va_list args;
		va_start(args, format);

		int err = vsnprintf_s(buffer, 280, _TRUNCATE, format, args);

		va_end(args);

		if (err == -1)
		{
			// Error - Message Was Larger Than 300 Chars And Was Truncated
			std::string error_msg = "Logging::LogWarning: String Was Truncated";
			LogError(error_msg);
		}

		std::string log_msg = buffer;
		return LogWarning(log_msg);
	}

	bool Logging::LogErrorF(const char* format, ...)
	{
		char buffer[300];

		va_list args;
		va_start(args, format);

		int err = vsnprintf_s(buffer, 280, _TRUNCATE, format, args);

		va_end(args);

		if (err == -1)
		{
			// Error - Message Was Larger Than 300 Chars And Was Truncated
			std::string error_msg = "Logging::LogError: String Was Truncated";
			LogError(error_msg);
		}

		std::string log_msg = buffer;
		return LogError(log_msg);
	}
}

