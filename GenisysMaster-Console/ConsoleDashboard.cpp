#include "ConsoleDashboard.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace
{
	constexpr WORD normal = 7, muted = 8, green = 10, red = 12, yellow = 14, cyan = 11;
	constexpr WORD selected = BACKGROUND_BLUE | BACKGROUND_INTENSITY | 15;
	using Comm = LSY::NetworkGenisysMaster::COMMUNICATIONSTATUS;
	const char* StatusName(Comm status)
	{
		return status == Comm::ONLINE ? "ONLINE" : status == Comm::OFFLINE ? "OFFLINE" : "UNKNOWN";
	}
}

volatile LONG ConsoleDashboard::stop_requested_ = 0;
BOOL WINAPI ConsoleDashboard::HandleControl(DWORD event)
{
	if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT) return FALSE;
	InterlockedExchange(&stop_requested_, 1);
	return TRUE;
}

ConsoleDashboard::~ConsoleDashboard() { Stop(); }
bool ConsoleDashboard::Active() const { return screen_ != INVALID_HANDLE_VALUE; }

bool ConsoleDashboard::Start()
{
	if (Active()) return true;
	original_ = GetStdHandle(STD_OUTPUT_HANDLE);
	input_ = GetStdHandle(STD_INPUT_HANDLE);
	DWORD output_mode;
	CONSOLE_SCREEN_BUFFER_INFO info{};
	if (!GetConsoleMode(original_, &output_mode) || !GetConsoleMode(input_, &input_mode_) ||
		!GetConsoleScreenBufferInfo(original_, &info)) return false;
	screen_ = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CONSOLE_TEXTMODE_BUFFER, nullptr);
	if (!Active()) return false;
	const SHORT width = info.srWindow.Right - info.srWindow.Left + 1;
	const SHORT height = info.srWindow.Bottom - info.srWindow.Top + 1;
	CONSOLE_SCREEN_BUFFER_INFO created{};
	if (!GetConsoleScreenBufferInfo(screen_, &created)) { Stop(); return false; }
	const COORD size{(std::max)(created.dwSize.X, width), (std::max)(created.dwSize.Y, height)};
	SMALL_RECT window{0, 0, static_cast<SHORT>(width - 1), static_cast<SHORT>(height - 1)};
	CONSOLE_CURSOR_INFO cursor{25, FALSE};
	const DWORD mode = (input_mode_ | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS) &
		~(ENABLE_QUICK_EDIT_MODE | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT);
	if (!SetConsoleScreenBufferSize(screen_, size) || !SetConsoleWindowInfo(screen_, TRUE, &window) ||
		!SetConsoleCursorInfo(screen_, &cursor) || !SetConsoleMode(input_, mode) ||
		!SetConsoleActiveScreenBuffer(screen_)) { Stop(); return false; }
	InterlockedExchange(&stop_requested_, 0);
	handler_installed_ = SetConsoleCtrlHandler(HandleControl, TRUE) != FALSE;
	if (!handler_installed_) { Stop(); return false; }
	return true;
}

void ConsoleDashboard::Stop()
{
	if (!Active()) return;
	if (handler_installed_) SetConsoleCtrlHandler(HandleControl, FALSE);
	handler_installed_ = false;
	SetConsoleMode(input_, input_mode_);
	SetConsoleActiveScreenBuffer(original_);
	CloseHandle(screen_);
	screen_ = INVALID_HANDLE_VALUE;
}

void ConsoleDashboard::AddEvent(const std::string& message, bool error)
{
	std::time_t now = std::time(nullptr);
	std::tm local{}; localtime_s(&local, &now);
	std::ostringstream text;
	text << std::put_time(&local, "%H:%M:%S") << "  ";
	for (unsigned char character : message)
		text << (character >= 32 && character < 127 ? static_cast<char>(character) : ' ');
	events_.push_back({text.str(), error ? red : normal});
	if (log_offset_) ++log_offset_;
	if (events_.size() > 200) events_.pop_front();
	log_offset_ = (std::min)(log_offset_, events_.empty() ? size_t(0) : events_.size() - 1);
}

void ConsoleDashboard::ObserveIndications(const Slave& slave)
{
	auto& changes = changes_[slave.id];
	const size_t count = static_cast<size_t>(slave.indications->GetNumBytes());
	changes.values.resize(count, 0); changes.bits.resize(count);
	const auto now = Clock::now();
	for (size_t offset = 0; offset < count; ++offset)
	{
		const uint8_t value = (*slave.indications)[offset];
		const uint8_t difference = changes.values[offset] ^ value;
		for (unsigned bit = 0; bit < 8; ++bit) if (difference & (1 << bit)) changes.bits[offset][bit] = now;
		changes.values[offset] = value;
	}
}

uint8_t ConsoleDashboard::SelectedId(const std::vector<Slave>& slaves) const
{
	return slaves.at((std::min)(selected_, slaves.size() - 1)).id;
}

std::vector<ConsoleDashboard::Command> ConsoleDashboard::ReadActions(const std::vector<Slave>& slaves)
{
	std::vector<Command> actions;
	auto command = [&](Action action)
	{
		Command item{action, SelectedId(slaves)};
		item.offset = control_byte_; item.bit = control_bit_;
		return item;
	};
	if (InterlockedCompareExchange(&stop_requested_, 0, 0)) { actions.push_back(command(Action::QUIT)); return actions; }
	DWORD available = 0;
	if (!GetNumberOfConsoleInputEvents(input_, &available)) { actions.push_back(command(Action::QUIT)); return actions; }
	for (DWORD i = 0; i < (std::min)(available, DWORD(64)); ++i)
	{
		INPUT_RECORD record{}; DWORD read = 0;
		if (!ReadConsoleInputW(input_, &record, 1, &read) || !read) break;
		if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) continue;
		const auto& key = record.Event.KeyEvent;
		for (WORD repeat = 0; repeat < (std::min)(key.wRepeatCount, WORD(32)); ++repeat)
		{
			selected_ = (std::min)(selected_, slaves.size() - 1);
			const size_t bytes = static_cast<size_t>(slaves[selected_].controls.frame->GetNumBytes());
			control_byte_ = (std::min)(control_byte_, bytes - 1);
			if (edit_ != Edit::NONE)
			{
				if (key.wVirtualKeyCode == VK_ESCAPE) { edit_ = Edit::NONE; edit_text_.clear(); }
				else if (key.wVirtualKeyCode == VK_BACK) { if (!edit_text_.empty()) edit_text_.pop_back(); }
				else if (key.wVirtualKeyCode == VK_RETURN)
				{
					try
					{
						auto item = command(edit_ == Edit::BYTE ? Action::SET_BYTE : Action::SET_MODE);
						item.include_mode = edit_text_ != "-";
						if (edit_ == Edit::BYTE || item.include_mode)
						{
							size_t used = 0;
							const unsigned long value = std::stoul(edit_text_, &used,
								edit_text_.size() > 2 && edit_text_[0] == '0' && (edit_text_[1] == 'x' || edit_text_[1] == 'X') ? 16 : 10);
							if (used != edit_text_.size() || value > 255 || edit_text_[0] == '-') throw std::runtime_error("Invalid byte");
							item.value = static_cast<uint8_t>(value);
						}
						actions.push_back(item);
						edit_ = Edit::NONE; edit_text_.clear();
					}
					catch (const std::exception&) { AddEvent("Enter a byte 0..255 or 0x00..0xFF; mode accepts - to omit", true); }
				}
				else
				{
					const wchar_t ch = key.uChar.UnicodeChar;
					if (edit_text_.size() < 8 && ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
						(ch >= 'A' && ch <= 'F') || ch == 'x' || ch == 'X' || ch == '-')) edit_text_ += static_cast<char>(ch);
				}
				continue;
			}
			if (help_)
			{
				if (key.wVirtualKeyCode == 'Q') actions.push_back(command(Action::QUIT));
				if (key.wVirtualKeyCode == 'H' || key.wVirtualKeyCode == VK_F1 || key.wVirtualKeyCode == VK_ESCAPE) help_ = false;
				continue;
			}
			switch (key.wVirtualKeyCode)
			{
			case 'Q': actions.push_back(command(Action::QUIT)); break;
			case 'R': actions.push_back(command(Action::RECALL_SELECTED)); break;
			case 'A': actions.push_back(command(Action::RECALL_ALL)); break;
			case 'H': case VK_F1: help_ = true; break;
			case 'C': controls_view_ = !controls_view_; byte_offset_ = 0; control_byte_ = 0; break;
			case 'J': if (controls_view_ && control_byte_ + 1 < bytes) ++control_byte_; break;
			case 'K': if (controls_view_ && control_byte_) --control_byte_; break;
			case VK_LEFT: if (control_bit_ < 7) ++control_bit_; break;
			case VK_RIGHT: if (control_bit_) --control_bit_; break;
			case VK_SPACE: if (controls_view_) actions.push_back(command(Action::TOGGLE_BIT)); break;
			case 'E': if (controls_view_) { edit_ = Edit::BYTE; edit_text_.clear(); } break;
			case 'M': if (controls_view_) { edit_ = Edit::MODE; edit_text_.clear(); } break;
			case 'S': if (controls_view_ && repeat == 0) actions.push_back(command(Action::SEND_CONTROL)); break;
			case 'F': if (controls_view_ && repeat == 0) actions.push_back(command(Action::SEND_FULL)); break;
			case VK_UP: if (selected_) { --selected_; byte_offset_ = 0; control_byte_ = 0; } break;
			case VK_DOWN: if (selected_ + 1 < slaves.size()) { ++selected_; byte_offset_ = 0; control_byte_ = 0; } break;
			case VK_PRIOR:
				byte_offset_ = byte_offset_ > data_rows_ ? byte_offset_ - data_rows_ : 0;
				if (controls_view_) control_byte_ = byte_offset_;
				break;
			case VK_NEXT:
				byte_offset_ += data_rows_;
				if (controls_view_) control_byte_ = (std::min)(byte_offset_, bytes - 1);
				break;
			case VK_HOME: byte_offset_ = 0; control_byte_ = 0; break;
			case VK_END: byte_offset_ = 224; control_byte_ = bytes - 1; break;
			default:
				if (key.uChar.UnicodeChar == '[') ++log_offset_;
				if (key.uChar.UnicodeChar == ']' && log_offset_) --log_offset_;
				break;
			}
		}
	}
	return actions;
}

void ConsoleDashboard::Put(int x, int y, const std::string& text, WORD color)
{
	if (y < 0 || y >= height_) return;
	for (size_t i = 0; i < text.size() && x + static_cast<int>(i) < width_; ++i)
	{
		if (x + static_cast<int>(i) < 0) continue;
		auto& cell = cells_[static_cast<size_t>(y) * width_ + x + i];
		cell.Char.AsciiChar = text[i]; cell.Attributes = color;
	}
}

bool ConsoleDashboard::Render(const std::vector<Slave>& slaves, const LSY::NetworkGenisysMaster& network,
	const LSY::NetworkGenisysMaster::Config& config)
{
	CONSOLE_SCREEN_BUFFER_INFO info{};
	if (!GetConsoleScreenBufferInfo(screen_, &info)) return false;
	width_ = info.srWindow.Right - info.srWindow.Left + 1;
	height_ = info.srWindow.Bottom - info.srWindow.Top + 1;
	CHAR_INFO blank{}; blank.Char.AsciiChar = ' '; blank.Attributes = normal;
	cells_.assign(static_cast<size_t>(width_) * height_, blank);
	Put(0, 0, "GENISYS MASTER | UDP " + config.local_ip_ + ":" + std::to_string(network.GetLocalPort()), cyan);
	if (width_ < 76 || height_ < 24)
	{
		Put(0, 2, "Resize terminal to at least 76 columns x 24 rows.", yellow);
		Put(0, 4, "Polling continues. Q: Quit | R: Recall selected | A: Recall all");
	}
	else if (help_)
	{
		Put(0, 2, "KEYBOARD HELP", cyan);
		const std::vector<std::string> help{
			"Up/Down: select slave | C: indications / controls",
			"PageUp/PageDn, Home/End: scroll data | J/K: control byte down/up",
			"Left/Right: select control bit | Space: toggle selected bit locally",
			"E: edit selected control byte | M: edit mode (- omits it)",
			"S: send changed controls | F: explicitly send FULL control frame",
			"R: recall selected | A: recall all | [ / ]: older/newer events",
			"H/F1/Esc: close help | Q/Ctrl+C: quit and restore console",
			"",
			"Control edits are LOCAL until S or F. No automatic control resend.",
			"Control values are desired bytes, NOT device output readback.",
			"Initial control values are local zeros, not a received database.",
			"ACKNOWLEDGED means a protocol response, not physical execution proof.",
			"Yellow indication bits changed in the last 1.2 seconds.",
			"Polling continues while editing and while help is open.",
			"Use --plain for text commands."

		};
		for (size_t i = 0; i < help.size(); ++i) Put(0, 4 + static_cast<int>(i), help[i]);
	}
	else
	{
		std::ostringstream settings;
		settings << (config.automatic_polling_ ? "AUTO" : "MANUAL") << " | Poll " << config.poll_interval_ms_ << " ms | Timeout " << config.response_timeout_ms_
			<< " ms | Retries " << config.retry_count_ << " | Recovery " << config.recovery_interval_ms_ << " ms";
		Put(0, 1, settings.str(), muted);
		const size_t table_rows = (std::min)(slaves.size(), static_cast<size_t>((std::min)(8, (std::max)(2, (height_ - 20) / 3))));
		selected_ = (std::min)(selected_, slaves.size() - 1);
		if (selected_ < slave_top_) slave_top_ = selected_;
		if (selected_ >= slave_top_ + table_rows) slave_top_ = selected_ - table_rows + 1;
		Put(0, 3, "  ID   Endpoint              Status   Data     Busy Complete  Timeouts  Retry", cyan);
		for (size_t row = 0; row < table_rows; ++row)
		{
			const size_t index = slave_top_ + row;
			if (index >= slaves.size()) break;
			const auto& slave = slaves[index];
			LSY::NetworkGenisysMaster::SlaveStatus status; network.GetSlaveStatus(slave.id, status);
			std::ostringstream text;
			text << (index == selected_ ? "> " : "  ") << std::setw(3) << unsigned(slave.id) << "  " << std::left << std::setw(22)
				<< (slave.endpoint.ip_address_ + ":" + std::to_string(slave.endpoint.port_)) << std::setw(9) << StatusName(status.communication)
				<< std::setw(9) << (status.indications_current ? "Current" : "Stale") << std::setw(5) << (status.busy ? "yes" : "no")
				<< std::setw(10) << status.completed_count << std::setw(10) << status.timeout_count << status.retries;
			const WORD color = index == selected_ ? selected : status.communication == Comm::ONLINE ? green : status.communication == Comm::OFFLINE ? red : yellow;
			Put(0, 4 + static_cast<int>(row), text.str(), color);
			// Keep the selected row's communication state legible with its own color.
			if (index == selected_) Put(29, 4 + static_cast<int>(row), StatusName(status.communication),
				BACKGROUND_BLUE | BACKGROUND_INTENSITY | (status.communication == Comm::ONLINE ? green : status.communication == Comm::OFFLINE ? red : yellow));
		}
		const auto& slave = slaves[selected_];
		const int data_start = 6 + static_cast<int>(table_rows);
		const int log_rows = (std::min)(6, (std::max)(3, height_ / 6));
		data_rows_ = static_cast<size_t>((std::max)(1, height_ - data_start - log_rows - 6 - (controls_view_ ? 2 : 0)));
		const auto frame = controls_view_ ? slave.controls.frame : slave.indications;
		const size_t bytes = static_cast<size_t>(frame->GetNumBytes());
		control_byte_ = (std::min)(control_byte_, bytes - 1);
		if (controls_view_)
		{
			if (control_byte_ < byte_offset_) byte_offset_ = control_byte_;
			if (control_byte_ >= byte_offset_ + data_rows_) byte_offset_ = control_byte_ - data_rows_ + 1;
		}
		byte_offset_ = (std::min)(byte_offset_, bytes > data_rows_ ? bytes - data_rows_ : 0);
		LSY::NetworkGenisysMaster::SlaveStatus status; network.GetSlaveStatus(slave.id, status);
		std::ostringstream title;
		title << "Slave " << unsigned(slave.id) << (controls_view_ ? " CONTROLS (local draft)" : " indications") << " | "
			<< (controls_view_ ? (slave.controls.Dirty() ? "MODIFIED" : "no new changes") : status.indications_current ? "CURRENT" : "STALE / unconfirmed")
			<< " | bytes " << byte_offset_ << '-' << (std::min)(bytes, byte_offset_ + data_rows_) - 1 << " of " << bytes;
		if (!controls_view_ && slave.indications->HasModeByte()) title << " | mode 0x" << std::hex << unsigned(slave.indications->GetModeByte());
		else if (!controls_view_) title << " | mode unknown";
		Put(0, data_start - 1, title.str(), cyan);
		if (controls_view_)
		{
			Put(0, data_start, "Last control: " + slave.controls.Result(), yellow);
			std::ostringstream mode;
			mode << "Mode: ";
			if (slave.controls.include_mode) mode << "0x" << std::hex << unsigned(slave.controls.mode);
			else mode << "omitted";
			mode << " | byte " << std::dec << control_byte_ << " bit " << unsigned(control_bit_) << " | E: byte M: mode S: send F: full";
			Put(0, data_start + 1, mode.str(), muted);
		}
		const int header_row = data_start + (controls_view_ ? 2 : 0);
		Put(0, header_row, "Byte   Hex     7 6 5 4 3 2 1 0", muted);
		const auto now = Clock::now();
		for (size_t row = 0; row < data_rows_ && row + byte_offset_ < bytes; ++row)
		{
			const size_t offset = row + byte_offset_;
			const uint8_t value = (*frame)[offset];
			std::ostringstream text;
			text << (controls_view_ && offset == control_byte_ ? ">" : " ") << std::setw(3) << offset << "   " << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << unsigned(value);
			const int y = header_row + 1 + static_cast<int>(row);
			Put(0, y, text.str(), controls_view_ ? (slave.controls.ByteChanged(offset) ? yellow : normal) : status.indications_current ? normal : muted);
			for (unsigned bit = 0; bit < 8; ++bit)
			{
				bool changed = false;
				const auto found = changes_.find(slave.id);
				if (!controls_view_ && found != changes_.end() && offset < found->second.bits.size())
					changed = now - found->second.bits[offset][bit] < std::chrono::milliseconds(1200);
				Put(15 + 2 * (7 - bit), y, (value & (1 << bit)) ? "1" : "0", controls_view_ && offset == control_byte_ && bit == control_bit_ ? selected : changed ? yellow :
					controls_view_ ? (slave.controls.ByteChanged(offset) ? yellow : normal) : status.indications_current ? ((value & (1 << bit)) ? green : muted) : muted);
			}
		}
		const int log_start = height_ - log_rows - 3;
		log_offset_ = (std::min)(log_offset_, events_.size() > static_cast<size_t>(log_rows) ? events_.size() - log_rows : 0);
		Put(0, log_start, "RECENT EVENTS | " + std::to_string(events_.size()) + "/200 | " + (log_offset_ ? "history ([ / ] to scroll)" : "live ([ for older)"), cyan);
		const size_t end = events_.size() - log_offset_;
		const size_t start = end > static_cast<size_t>(log_rows) ? end - log_rows : 0;
		for (size_t i = start; i < end; ++i) Put(0, log_start + 1 + static_cast<int>(i - start), events_[i].text, events_[i].color);
		if (edit_ != Edit::NONE) Put(0, height_ - 2,
			(edit_ == Edit::BYTE ? "Byte value (decimal/0x hex): " : "Mode value (decimal/0x hex, - omits): ") + edit_text_ + "_  Enter: stage  Esc: cancel", yellow);
		Put(0, height_ - 1, controls_view_ ? "C: Indications  J/K: Byte  Left/Right: Bit  Space: Toggle  S: Send  H: Help" :
			"Up/Down: Slave  PgUp/PgDn: Bytes  C: Controls  R: Recall  H: Help  Q: Quit", cyan);
	}
	const COORD size{static_cast<SHORT>(width_), static_cast<SHORT>(height_)};
	SMALL_RECT target = info.srWindow;
	return WriteConsoleOutputA(screen_, cells_.data(), size, COORD{0, 0}, &target) != FALSE;
}
