#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace launcher
{
enum class ClientLogState
{
	WaitingForLog,
	WaitingForLogin,
	WaitingForGames,
	Ready
};

enum class GameUpdateState
{
	Unknown,
	Ready,
	Pending,
	Updating,
	Paused,
	Failed
};

class ClientLogMonitor
{
  public:
	explicit ClientLogMonitor(std::filesystem::path pathDirectory, std::wstring_view svProduct = {});
	ClientLogState Poll();
	GameUpdateState UpdateState() const;

  private:
	struct Cursor
	{
		std::uint64_t ullOffset = 0;
		std::string strPartialLine;
		bool bLoggedIn = false;
		bool bGamesReady = false;
		bool bDiscardLine = false;
		std::string strOperation;
		std::string strOperationStatus;
		GameUpdateState eUpdateState = GameUpdateState::Unknown;

		void Consume(std::string_view svBytes, std::string_view svProduct);
		void ConsumeUpdate(std::string_view svLine, std::string_view svProduct);
	};

	std::filesystem::path m_pathDirectory;
	std::string m_strProduct;
	GameUpdateState m_eUpdateState = GameUpdateState::Unknown;
	std::set<std::filesystem::path> m_setOldLogs;
	std::map<std::filesystem::path, Cursor> m_mapCursors;
};
}
