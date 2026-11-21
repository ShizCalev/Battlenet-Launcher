#include "client_log.h"
#include "launcher.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <stdexcept>

namespace launcher
{
namespace
{
std::vector<std::filesystem::path> ListClientLogs(const std::filesystem::path& pathDirectory)
{
	std::error_code ecError;
	std::filesystem::directory_iterator itDirectory(pathDirectory, ecError);
	if (ecError && ecError != std::errc::no_such_file_or_directory)
	{
		throw std::runtime_error("Cannot read the Battle.net log directory: " + ecError.message());
	}

	std::vector<std::filesystem::path> vecLogs;
	for (const auto& stEntry : itDirectory)
	{
		const auto& pathFile = stEntry.path();
		if (pathFile.filename().native().starts_with(L"battle.net-") && pathFile.extension() == L".log" && stEntry.is_regular_file(ecError))
		{
			vecLogs.push_back(pathFile);
		}
	}
	return vecLogs;
}
}

ClientLogMonitor::ClientLogMonitor(std::filesystem::path pathDirectory, std::wstring_view svProduct) : m_pathDirectory(std::move(pathDirectory))
{
	for (const wchar_t wchCharacter : svProduct)
	{
		m_strProduct += static_cast<char>(std::towlower(wchCharacter));
	}
	const std::pair<std::string_view, std::string_view> arrAliases[] =
		{
			{"vipr", "viper"}, {"lazr", "lazarus"}, {"d3", "diablo3"}, {"d3t", "diablo3_ptr"},
			{"fen", "fenris"}, {"wtcg", "hs_beta"}, {"hero", "heroes"}, {"herot", "heroes_ptr"},
			{"pro", "prometheus"}, {"wowc", "wow_classic"}, {"wowt", "wow_ptr"}};
	for (const auto& [svCode, svUid] : arrAliases)
	{
		if (m_strProduct == svCode)
		{
			m_strProduct = svUid;
			break;
		}
	}
	for (const auto& pathFile : ListClientLogs(m_pathDirectory))
	{
		m_setOldLogs.insert(pathFile);
	}
}

GameUpdateState ClientLogMonitor::UpdateState() const
{
	return m_eUpdateState;
}

void ClientLogMonitor::Cursor::ConsumeUpdate(std::string_view svLine, std::string_view svProduct)
{
	if (svProduct.empty())
	{
		return;
	}
	if (svLine.ends_with('\r'))
	{
		svLine.remove_suffix(1);
	}
	const std::string strUid(svProduct);
	if (svLine.ends_with("[InstallManager] {Main} Update started: " + strUid))
	{
		strOperation = "Update";
		strOperationStatus = "Running";
	}
	else if (svLine.ends_with("[InstallManager] {Main} Update completed: " + strUid))
	{
		strOperation = "UpToDate";
		strOperationStatus.clear();
	}
	else
	{
		const auto strPrefix = "[ProductState] {Main} InstallState (" + strUid + "):";
		const auto nPosition = svLine.find(strPrefix);
		if (nPosition == std::string_view::npos)
		{
			return;
		}
		svLine.remove_prefix(nPosition + strPrefix.size());
		auto fnReadField = [svLine](std::string_view svKey, std::string& strValue)
		{
			const auto strKey = " " + std::string(svKey) + "=";
			const auto nStart = svLine.find(strKey);
			if (nStart != std::string_view::npos)
			{
				const auto nValue = nStart + strKey.size();
				strValue = svLine.substr(nValue, svLine.find(' ', nValue) - nValue);
			}
		};
		fnReadField("currentOperation", strOperation);
		fnReadField("currentOperationStatus", strOperationStatus);
	}

	if (strOperation == "UpToDate")
	{
		eUpdateState = GameUpdateState::Ready;
	}
	else if (strOperation == "Update")
	{
		if (strOperationStatus == "Paused")
		{
			eUpdateState = GameUpdateState::Paused;
		}
		else if (strOperationStatus == "Failed" || strOperationStatus == "Error")
		{
			eUpdateState = GameUpdateState::Failed;
		}
		else if (strOperationStatus == "Waiting" || strOperationStatus == "Initializing" || strOperationStatus == "Running" || strOperationStatus == "Finalizing")
		{
			eUpdateState = GameUpdateState::Updating;
		}
		else
		{
			eUpdateState = GameUpdateState::Pending;
		}
	}
}

void ClientLogMonitor::Cursor::Consume(std::string_view svBytes, std::string_view svProduct)
{
	for (const char chByte : svBytes)
	{
		if (chByte != '\n')
		{
			if (!bDiscardLine && strPartialLine.size() < 65536)
			{
				strPartialLine += chByte;
			}
			else
			{
				strPartialLine.clear();
				bDiscardLine = true;
			}
			continue;
		}

		if (!bDiscardLine)
		{
			ConsumeUpdate(strPartialLine, svProduct);
			// Newer clients (2.53+) can finish GameController initialization before logging the
			// successful login, so track both independently and require both for readiness.
			if (strPartialLine.contains("[BNLogin] {Main} Logged into Battle.net successfully."))
			{
				bLoggedIn = true;
			}
			else if (strPartialLine.contains("[GameController] {Main} Beginning GameController initialization"))
			{
				bGamesReady = false;
			}
			else if (strPartialLine.contains("[GameController] {Main} GameController initialization complete"))
			{
				bGamesReady = true;
			}
			else if (strPartialLine.contains("[GameProvider] {Main} Game provider is no longer initialized and is now reset"))
			{
				bLoggedIn = false;
				bGamesReady = false;
			}
		}
		strPartialLine.clear();
		bDiscardLine = false;
	}
}

ClientLogState ClientLogMonitor::Poll()
{
	ClientLogState eState = ClientLogState::WaitingForLog;
	for (const auto& pathFile : ListClientLogs(m_pathDirectory))
	{
		if (m_setOldLogs.contains(pathFile))
		{
			continue;
		}

		Handle hFile(CreateFileW(pathFile.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
		if (hFile.get() == INVALID_HANDLE_VALUE)
		{
			continue;
		}

		LARGE_INTEGER liSize{};
		if (!GetFileSizeEx(hFile.get(), &liSize))
		{
			continue;
		}
		eState = std::max(eState, ClientLogState::WaitingForLogin);

		auto& stCursor = m_mapCursors[pathFile];
		const auto ullSize = static_cast<std::uint64_t>(liSize.QuadPart);
		if (ullSize < stCursor.ullOffset)
		{
			stCursor = {};
		}
		LARGE_INTEGER liOffset{};
		liOffset.QuadPart = static_cast<LONGLONG>(stCursor.ullOffset);
		if (!SetFilePointerEx(hFile.get(), liOffset, nullptr, FILE_BEGIN))
		{
			continue;
		}

		std::array<char, 16384> arrBuffer{};
		const auto ullEnd = std::min(ullSize, stCursor.ullOffset + 1024 * 1024);
		while (stCursor.ullOffset < ullEnd)
		{
			DWORD dwRead = 0;
			const auto dwRequested = static_cast<DWORD>(std::min<std::uint64_t>(arrBuffer.size(), ullEnd - stCursor.ullOffset));
			if (!ReadFile(hFile.get(), arrBuffer.data(), dwRequested, &dwRead, nullptr) || !dwRead)
			{
				break;
			}
			stCursor.Consume({arrBuffer.data(), dwRead}, m_strProduct);
			stCursor.ullOffset += dwRead;
		}

		if (stCursor.ullOffset == ullSize && stCursor.eUpdateState != GameUpdateState::Unknown)
		{
			m_eUpdateState = stCursor.eUpdateState;
		}

		if (stCursor.bLoggedIn)
		{
			eState = std::max(eState, ClientLogState::WaitingForGames);
		}
		if (stCursor.bLoggedIn && stCursor.bGamesReady && stCursor.ullOffset == ullSize)
		{
			eState = ClientLogState::Ready;
		}
	}
	return eState;
}
}
