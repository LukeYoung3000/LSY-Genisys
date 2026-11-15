#pragma once

#include <string>
#include <functional>

namespace LSY
{
	class Logging
	{
	private:

		static std::function<bool(std::string)> log_debug;
		static std::function<bool(std::string)> log_info;
		static std::function<bool(std::string)> log_warning;
		static std::function<bool(std::string)> log_error;
		static std::function<bool(std::string)> log_proto;

	public:

		// Add Logging Interface Methods
		static bool AddLogDebugFunc(std::function<bool(std::string)> func);
		static bool AddLogInfoFunc(std::function<bool(std::string)> func);
		static bool AddLogWarningFunc(std::function<bool(std::string)> func);
		static bool AddLogErrorFunc(std::function<bool(std::string)> func);
		static bool AddLogProtoFunc(std::function<bool(std::string)> func);

		// Write Log Methods
		static bool LogDebug(const std::string &log_msg);
		static bool LogInfo(const std::string &log_msg);
		static bool LogWarning(const std::string &log_msg);
		static bool LogError(const std::string& log_msg);
		static bool LogProto(const std::string &log_msg);

		static bool LogDebugF(const char* format, ...);
		static bool LogInfoF(const char* format, ...);
		static bool LogWarningF(const char* format, ...);
		static bool LogErrorF(const char* format, ...);
		static bool LogProtoF(const char* format, ...);

	};
}


