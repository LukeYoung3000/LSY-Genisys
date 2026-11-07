#pragma once

#include "ConsoleControls.h"
#include <Windows.h>
#include <array>
#include <chrono>
#include <deque>
#include <map>

struct Slave
{
	uint8_t id;
	LSY::NetworkGenisysMaster::Endpoint endpoint;
	std::shared_ptr<LSY::MasterDataFrame> indications;
	ConsoleControls controls;
};

// Owns a separate console screen buffer; never writes scrolling output to it.
class ConsoleDashboard
{
public:
	enum class Action { QUIT, RECALL_SELECTED, RECALL_ALL, TOGGLE_BIT, SET_BYTE, SET_MODE, SEND_CONTROL, SEND_FULL };
	struct Command
	{
		Action action;
		uint8_t slave_id;
		size_t offset = 0;
		uint8_t bit = 0;
		uint8_t value = 0;
		bool include_mode = false;
	};
	ConsoleDashboard() = default;
	~ConsoleDashboard();
	ConsoleDashboard(const ConsoleDashboard&) = delete;
	ConsoleDashboard& operator=(const ConsoleDashboard&) = delete;
	bool Start(); // False for redirected input/output or unavailable console APIs.
	bool Active() const;
	void AddEvent(const std::string& message, bool error = false);
	void ObserveIndications(const Slave& slave);
	std::vector<Command> ReadActions(const std::vector<Slave>& slaves);
	uint8_t SelectedId(const std::vector<Slave>& slaves) const;
	bool Render(const std::vector<Slave>& slaves, const LSY::NetworkGenisysMaster& network,
		const LSY::NetworkGenisysMaster::Config& config);
private:
	using Clock = std::chrono::steady_clock;
	struct Event { std::string text; WORD color; };
	struct Changes
	{
		std::vector<uint8_t> values;
		std::vector<std::array<Clock::time_point, 8>> bits;
	};
	HANDLE original_ = INVALID_HANDLE_VALUE;
	HANDLE screen_ = INVALID_HANDLE_VALUE;
	HANDLE input_ = INVALID_HANDLE_VALUE;
	DWORD input_mode_ = 0;
	bool handler_installed_ = false;
	size_t selected_ = 0;
	size_t byte_offset_ = 0;
	size_t slave_top_ = 0;
	size_t log_offset_ = 0;
	size_t data_rows_ = 5;
	bool help_ = false;
	bool controls_view_ = false;
	size_t control_byte_ = 0;
	uint8_t control_bit_ = 7;
	enum class Edit { NONE, BYTE, MODE };
	Edit edit_ = Edit::NONE;
	std::string edit_text_;
	std::deque<Event> events_;
	std::map<uint8_t, Changes> changes_;
	std::vector<CHAR_INFO> cells_;
	int width_ = 0, height_ = 0;
	void Put(int x, int y, const std::string& text, WORD color = 7);
	void Stop();
	static volatile LONG stop_requested_;
	static BOOL WINAPI HandleControl(DWORD event);
};
