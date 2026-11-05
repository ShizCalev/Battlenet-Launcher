#include "launcher.h"
#include "client_log.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>
#include <format>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <thread>

namespace launcher
{
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

void Log(std::wstring_view svMessage)
{
#ifdef _DEBUG
	SYSTEMTIME stTime{};
	GetLocalTime(&stTime);
	std::wcout << std::format(L"[{:02}:{:02}:{:02}] {}", stTime.wHour, stTime.wMinute, stTime.wSecond, svMessage) << std::endl;
#else
	(void)svMessage;
#endif
}

struct Game
{
	std::wstring_view svCode;
	std::wstring_view svName;
	std::wstring_view svProduct;
	std::wstring_view svExecutable;
	std::wstring_view svFolder;
};

constexpr auto arrGames = std::to_array<Game>({
	{L"rtro", L"Blizzard Arcade Collection", L"rtro", L"client.exe", L""},
	{L"codmw2", L"Call of Duty: MWII | WZ2.0", L"AUKS", L"cod22-cod.exe", L""},
	{L"codhq", L"Call of Duty HQ", L"AUKS", L"cod.exe", L""},
	{L"codbo4", L"Call of Duty: Black Ops 4", L"VIPR", L"BlackOps4.exe", L""},
	{L"codbocw", L"Call of Duty: Black Ops Cold War", L"ZEUS", L"BlackOpsColdWar.exe", L""},
	{L"codmw2019", L"Call of Duty: Modern Warfare", L"ODIN", L"ModernWarfare.exe", L""},
	{L"codmw2cr", L"Call of Duty: Modern Warfare 2 Campaign Remastered", L"LAZR", L"MW2CR.exe", L""},
	{L"codvg", L"Call of Duty: Vanguard", L"FORE", L"Vanguard.exe", L""},
	{L"cb4", L"Crash Bandicoot 4: It's About Time", L"WLBY", L"CrashBandicoot4.exe", L""},
	{L"d2r", L"Diablo 2: Resurrected", L"OSI", L"D2R.exe", L""},
	{L"d3", L"Diablo 3", L"D3", L"Diablo III.exe|Diablo III64.exe", L""},
	{L"d3ptr", L"Diablo 3 Public Test Realm", L"d3t", L"Diablo III.exe|Diablo III64.exe", L""},
	{L"d4", L"Diablo 4", L"Fen", L"Diablo IV.exe", L""},
	{L"di", L"Diablo Immortal", L"ANBS", L"DiabloImmortal.exe", L""},
	{L"hs", L"Hearthstone", L"WTCG", L"Hearthstone.exe", L""},
	{L"hots", L"Heroes of the Storm", L"Hero", L"HeroesOfTheStorm.exe|HeroesOfTheStorm_x64.exe", L""},
	{L"hotsptr", L"Heroes of the Storm Public Test Realm", L"herot", L"HeroesOfTheStorm.exe|HeroesOfTheStorm_x64.exe", L""},
	{L"ow", L"Overwatch", L"Pro", L"Overwatch.exe", L""},
	{L"owptr", L"Overwatch Public Test Realm", L"prot", L"Overwatch.exe", L""},
	{L"scr", L"Starcraft Remastered", L"S1", L"StarCraft.exe", L""},
	{L"sc2", L"Starcraft 2", L"S2", L"SC2.exe|SC2_x64.exe", L""},
	{L"w1", L"Warcraft I: Orcs & Humans", L"W1", L"DOSBox.exe", L""},
	{L"w1r", L"Warcraft I Remastered", L"W1R", L"Warcraft.exe", L""},
	{L"w2", L"Warcraft II Battle.net Edition", L"W2", L"Warcraft II BNE.exe", L""},
	{L"w2r", L"Warcraft II Remastered", L"W2R", L"Warcraft II.exe", L""},
	{L"w3", L"Warcraft 3: Reforged", L"W3", L"Warcraft III.exe", L""},
	{L"wow", L"World of Warcraft", L"WoW", L"Wow.exe", L"_retail_"},
	{L"wowclassic", L"World of Warcraft Classic", L"WoWC", L"WowClassic.exe", L"_classic_"},
	{L"wowptr", L"World of Warcraft Public Test Realm", L"wowt", L"WowT.exe", L""},
	{L"wowf", L"World of Warcraft Forever", L"WoWF", L"Wow.exe", L"_forever_"},
	{L"d1", L"Diablo", L"D1", L"Diablo.exe", L""},
});

bool Equal(std::wstring_view svLeft, std::wstring_view svRight)
{
	return svLeft.size() == svRight.size() &&
		(svLeft.empty() ||
			CompareStringOrdinal(svLeft.data(), static_cast<int>(svLeft.size()), svRight.data(),
				static_cast<int>(svRight.size()), TRUE) == CSTR_EQUAL);
}

std::wstring Lower(std::wstring strText)
{
	std::ranges::transform(strText, strText.begin(), [](wchar_t wchCharacter)
	{
		return static_cast<wchar_t>(towlower(wchCharacter));
	});

	std::ranges::replace(strText, L'/', L'\\');
	return strText;
}

bool Matches(std::wstring_view svNames, std::wstring_view svName)
{
	return std::ranges::any_of(svNames | std::views::split(L'|'), [svName](auto rngName)
	{
		return Equal(std::wstring_view{rngName.begin(), rngName.end()}, svName);
	});
}

std::wstring Quote(std::wstring_view svValue)
{

	std::wstring strQuoted = L"\"";
	size_t nBackslashes = 0;
	for (wchar_t wchCharacter : svValue)
	{
		if (wchCharacter == L'\\')
		{
			++nBackslashes;
			continue;
		}

		strQuoted.append(nBackslashes * (wchCharacter == L'"' ? 2 : 1), L'\\');
		nBackslashes = 0;
		if (wchCharacter == L'"')
		{
			strQuoted += L'\\';
		}

		strQuoted += wchCharacter;
	}

	strQuoted.append(nBackslashes * 2, L'\\');
	return strQuoted + L'"';
}

std::wstring Environment(const wchar_t* pszName)
{
	const DWORD dwCharCount = GetEnvironmentVariableW(pszName, nullptr, 0);
	if (!dwCharCount)
	{
		return {};
	}

	std::wstring strValue(dwCharCount, L'\0');
	const DWORD dwWritten = GetEnvironmentVariableW(pszName, strValue.data(), dwCharCount);
	if (!dwWritten || dwWritten >= dwCharCount)
	{
		return {};
	}

	strValue.resize(dwWritten);
	return strValue;
}

bool FileExists(const fs::path& pathFile)
{
	std::error_code ecError;
	return fs::is_regular_file(pathFile, ecError);
}

std::wstring FullPath(const fs::path& pathFile)
{
	return fs::absolute(pathFile).lexically_normal().native();
}

std::wstring DiscoverClient()
{
	for (HKEY hRootKey : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER})
	{
		for (DWORD dwRegistryView : {RRF_SUBKEY_WOW6432KEY, RRF_SUBKEY_WOW6464KEY})
		{
			wchar_t wszInstallLocation[32768]{};
			DWORD dwBufferBytes = sizeof(wszInstallLocation);
			if (RegGetValueW(hRootKey, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Battle.net",
					L"InstallLocation", RRF_RT_REG_SZ | dwRegistryView, nullptr, wszInstallLocation,
					&dwBufferBytes) == ERROR_SUCCESS)
			{
				std::wstring strPath = std::wstring(wszInstallLocation) + L"\\Battle.net.exe";
				if (FileExists(strPath))
				{
					return FullPath(strPath);
				}
			}
		}
	}

	for (const auto* pszVariable : {L"ProgramFiles(x86)", L"ProgramFiles"})
	{
		const auto strBasePath = Environment(pszVariable);
		if (!strBasePath.empty() && FileExists(strBasePath + L"\\Battle.net\\Battle.net.exe"))
		{
			return strBasePath + L"\\Battle.net\\Battle.net.exe";
		}
	}

	throw std::runtime_error("Battle.net was not found. Supply its full executable path as the second argument.");
}

Options Parse(std::span<wchar_t* const> spArguments)
{
	Options stOptions;
	for (size_t nIndex = 1; nIndex < spArguments.size(); ++nIndex)
	{
		const std::wstring_view svArgument = spArguments[nIndex];
		if (svArgument == L"--help" || svArgument == L"--list" || svArgument == L"-h")
		{
			stOptions.bHelp = true;
		}
		else if (svArgument == L"--dry-run")
		{
			stOptions.bDryRun = true;
		}
		else if (svArgument == L"--keep-client")
		{
			stOptions.bKeepClient = true;
		}
		else if (svArgument == L"--timeout" || svArgument == L"--product" || svArgument == L"--exe")
		{
			if (++nIndex == spArguments.size())
			{
				throw std::runtime_error("Missing option value.");
			}

			const std::wstring strValue = spArguments[nIndex];
			if (svArgument == L"--timeout")
			{
				if (strValue.empty() || strValue.find_first_not_of(L"0123456789") != std::wstring::npos)
				{
					throw std::runtime_error("Timeout must be 1 to 3600 seconds.");
				}

				const auto ulSeconds = std::stoul(strValue);
				if (ulSeconds < 1 || ulSeconds > 3600)
				{
					throw std::runtime_error("Timeout must be 1 to 3600 seconds.");
				}

				stOptions.secTimeout = std::chrono::seconds{ulSeconds};
			}
			else if (svArgument == L"--product")
			{
				stOptions.strProduct = strValue;
			}
			else
			{
				stOptions.strExecutable = strValue;
			}
		}
		else if (svArgument.starts_with(L"--"))
		{
			throw std::runtime_error("Unknown option. Use --help.");
		}
		else if (stOptions.strCode.empty())
		{
			stOptions.strCode = svArgument;
		}
		else if (stOptions.strClientPath.empty())
		{
			stOptions.strClientPath = svArgument;
		}
		else
		{
			throw std::runtime_error("Too many arguments. Use --help.");
		}
	}

	if (stOptions.bHelp)
	{
		return stOptions;
	}

	if (stOptions.strCode.empty())
	{
		throw std::runtime_error("Specify a game code. Use --help to list games.");
	}

	const auto itGame = std::ranges::find_if(arrGames, [&](const Game& stCandidate)
	{
		return Equal(stOptions.strCode, stCandidate.svCode);
	});

	const Game* pGame = itGame == arrGames.end() ? nullptr : &*itGame;
	if (!pGame && stOptions.strExecutable.empty())
	{
		throw std::runtime_error("Unknown game. Use --list, or supply --exe for a raw product code.");
	}

	if (stOptions.strProduct.empty())
	{
		stOptions.strProduct = pGame ? pGame->svProduct : std::wstring_view{stOptions.strCode};
	}

	if (stOptions.strProduct.find_first_not_of(L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") !=
		std::wstring::npos)
	{
		throw std::runtime_error("Product codes may only contain letters, digits and underscores.");
	}

	if (stOptions.strExecutable.empty())
	{
		stOptions.strExecutable = pGame->svExecutable;
		stOptions.strFolder = pGame->svFolder;
	}
	else if (stOptions.strExecutable.find_first_of(L"/\\") != std::wstring::npos)
	{
		stOptions.strExecutable = FullPath(stOptions.strExecutable);
	}

	if (stOptions.strExecutable.find_first_of(L"*?\"") != std::wstring::npos)
	{
		throw std::runtime_error("Use an exact executable name or full path for --exe.");
	}

	stOptions.strClientPath = stOptions.strClientPath.empty() ? DiscoverClient() : FullPath(stOptions.strClientPath);
	if (!FileExists(stOptions.strClientPath))
	{
		throw std::runtime_error("The Battle.net executable does not exist.");
	}

	return stOptions;
}

void Help()
{
	std::wcout << L"Battlenet-Launcher <game> [Battle.net.exe path] [options]\n"
				  L"  --timeout SECONDS  Startup timeout (default 180; maximum 3600)\n"
				  L"  --exe NAME_OR_PATH Exact game executable to monitor (full path disambiguates installs)\n"
				  L"  --product CODE     Override Battle.net launch product\n"
				  L"  --keep-client      Leave a client started by this wrapper running\n"
				  L"  --dry-run          Print resolved settings without launching or closing anything\n"
				  L"  --help / --list    Show this list\n\n";
	for (const auto& stGame : arrGames)
	{
		std::wcout << stGame.svCode << L"\t" << stGame.svName << L"\n";
	}
}

std::wstring ImagePath(HANDLE hProcess)
{
	std::wstring strPath(32768, L'\0');
	DWORD dwCharCount = static_cast<DWORD>(strPath.size());
	if (!QueryFullProcessImageNameW(hProcess, 0, strPath.data(), &dwCharCount))
	{
		return {};
	}

	strPath.resize(dwCharCount);
	return strPath;
}

ULONGLONG Created(HANDLE hProcess)
{
	FILETIME ftCreated{}, ftExited{}, ftKernel{}, ftUser{};
	if (!GetProcessTimes(hProcess, &ftCreated, &ftExited, &ftKernel, &ftUser))
	{
		return 0;
	}

	return (static_cast<ULONGLONG>(ftCreated.dwHighDateTime) << 32) | ftCreated.dwLowDateTime;
}

ULONGLONG Now()
{
	FILETIME ftNow{};
	GetSystemTimeAsFileTime(&ftNow);
	return (static_cast<ULONGLONG>(ftNow.dwHighDateTime) << 32) | ftNow.dwLowDateTime;
}

std::vector<PROCESSENTRY32W> Processes()
{
	Handle hSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
	if (hSnapshot.get() == INVALID_HANDLE_VALUE)
	{
		throw std::runtime_error("Cannot enumerate processes.");
	}

	PROCESSENTRY32W stEntry{};
	stEntry.dwSize = sizeof(stEntry);
	std::vector<PROCESSENTRY32W> vecEntries;
	if (!Process32FirstW(hSnapshot.get(), &stEntry))
	{
		throw std::runtime_error("Cannot read process snapshot.");
	}

	do
	{
		vecEntries.push_back(stEntry);
	} while (Process32NextW(hSnapshot.get(), &stEntry));
	if (GetLastError() != ERROR_NO_MORE_FILES)
	{
		throw std::runtime_error("Incomplete process snapshot.");
	}

	return vecEntries;
}

bool IsGame(const Options& stOptions, const std::wstring& strName, const std::wstring& strPath)
{
	if (stOptions.strExecutable.find_first_of(L"\\/") != std::wstring::npos)
	{
		return Equal(Lower(stOptions.strExecutable), Lower(strPath));
	}

	if (!Matches(stOptions.strExecutable, strName))
	{
		return false;
	}

	return stOptions.strFolder.empty() ||
		Lower(strPath).find(L"\\" + Lower(stOptions.strFolder) + L"\\") != std::wstring::npos;
}

void CollectGames(const Options& stOptions, ULONGLONG ullCreatedSince, std::vector<Process>& vecTracked)
{
	const auto strBasename = fs::path{stOptions.strExecutable}.filename().native();
	for (const auto& stEntry : Processes())
	{
		if (std::ranges::any_of(vecTracked, [&](const Process& stProcess)
		{
			return stProcess.dwProcessId == stEntry.th32ProcessID;
		}))
		{
			continue;
		}

		if (!Matches(strBasename, stEntry.szExeFile))
		{
			continue;
		}

		Handle hProcess(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, stEntry.th32ProcessID));
		if (!hProcess || Created(hProcess.get()) < ullCreatedSince ||
			WaitForSingleObject(hProcess.get(), 0) != WAIT_TIMEOUT)
		{
			continue;
		}

		if (IsGame(stOptions, stEntry.szExeFile, ImagePath(hProcess.get())))
		{
			Log(std::format(L"Game detected: PID {}, parent PID {}, {}", stEntry.th32ProcessID, stEntry.th32ParentProcessID, ImagePath(hProcess.get())));
			vecTracked.push_back({stEntry.th32ProcessID, std::move(hProcess)});
		}
	}
}

Handle Start(const std::wstring& strExecutable, const std::wstring& strArguments)
{
	auto strCommandLine = Quote(strExecutable) + L" " + strArguments;
	Log(L"Starting: " + strCommandLine);
	STARTUPINFOW stStartup{};
	stStartup.cb = sizeof(stStartup);
	PROCESS_INFORMATION stProcessInfo{};
	const auto pathDirectory = fs::path{strExecutable}.parent_path();
	if (!CreateProcessW(strExecutable.c_str(), strCommandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
			pathDirectory.c_str(), &stStartup, &stProcessInfo))
	{
		throw std::runtime_error("CreateProcessW failed (Windows error " + std::to_string(GetLastError()) + ").");
	}

	Log(std::format(L"Started PID {}", stProcessInfo.dwProcessId));
	Handle hThread(stProcessInfo.hThread);
	return Handle(stProcessInfo.hProcess);
}

struct OwnedClient
{
	bool bEnabled;
	ULONGLONG ullCreatedSince;
	std::wstring strDirectory;
	std::vector<Process> vecProcesses;

	void Collect()
	{
		if (!bEnabled)
		{
			return;
		}

		for (const auto& stEntry : Processes())
		{
			if (!Equal(stEntry.szExeFile, L"Battle.net.exe"))
			{
				continue;
			}

			if (std::ranges::any_of(vecProcesses, [&](const Process& stProcess)
			{
				return stProcess.dwProcessId == stEntry.th32ProcessID;
			}))
			{
				continue;
			}

			Handle hProcess(OpenProcess(
				SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, stEntry.th32ProcessID));
			if (hProcess && Created(hProcess.get()) >= ullCreatedSince &&
				Lower(ImagePath(hProcess.get())).starts_with(strDirectory))
			{
				vecProcesses.push_back({stEntry.th32ProcessID, std::move(hProcess)});
			}
		}
	}

	~OwnedClient()
	{
		if (!bEnabled)
		{
			return;
		}

		try
		{
			Collect();
		}
		catch (...)
		{
		}
		for (auto& stProcess : vecProcesses)
		{
			if (WaitForSingleObject(stProcess.hProcess.get(), 0) == WAIT_TIMEOUT)
			{
				Log(std::format(L"Closing owned Battle.net PID {}", stProcess.dwProcessId));
				TerminateProcess(stProcess.hProcess.get(), EXIT_SUCCESS);
			}
		}
	}
};

std::vector<Process> FindClients(const std::wstring& strDirectory)
{
	std::vector<Process> vecClients;
	for (const auto& stEntry : Processes())
	{
		if (!Equal(stEntry.szExeFile, L"Battle.net.exe"))
		{
			continue;
		}

		Handle hProcess(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, stEntry.th32ProcessID));
		if (!hProcess)
		{
			if (GetLastError() == ERROR_INVALID_PARAMETER)
			{
				continue;
			}
			throw std::runtime_error("Cannot inspect an existing Battle.net process. Close Battle.net manually and retry.");
		}

		if (WaitForSingleObject(hProcess.get(), 0) == WAIT_TIMEOUT && Lower(ImagePath(hProcess.get())).starts_with(strDirectory))
		{
			vecClients.push_back({stEntry.th32ProcessID, std::move(hProcess)});
		}
	}
	return vecClients;
}

void StopClients(const std::vector<Process>& vecClients)
{
	std::vector<Handle> vecHandles;
	for (const auto& stClient : vecClients)
	{
		if (WaitForSingleObject(stClient.hProcess.get(), 0) == WAIT_OBJECT_0)
		{
			continue;
		}
		Handle hProcess(OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, stClient.dwProcessId));
		if (!hProcess || Created(hProcess.get()) != Created(stClient.hProcess.get()))
		{
			throw std::runtime_error("Cannot restart Battle.net. Close it manually and retry.");
		}
		vecHandles.push_back(std::move(hProcess));
	}

	for (const auto& hProcess : vecHandles)
	{
		Log(std::format(L"Stopping existing Battle.net PID {}", GetProcessId(hProcess.get())));
		if (!TerminateProcess(hProcess.get(), EXIT_SUCCESS) && WaitForSingleObject(hProcess.get(), 0) != WAIT_OBJECT_0)
		{
			throw std::runtime_error("Failed to stop Battle.net for restart.");
		}
	}
	for (const auto& hProcess : vecHandles)
	{
		if (WaitForSingleObject(hProcess.get(), 5000) != WAIT_OBJECT_0)
		{
			throw std::runtime_error("Battle.net did not exit in time for restart.");
		}
	}
}

int Run(const Options& stOptions, const fs::path& pathLogDirectory)
{
	const auto strArguments = L"--exec=" + Quote(L"launch " + stOptions.strProduct);
	if (stOptions.bDryRun)
	{
		std::wcout << L"Client: " << stOptions.strClientPath << L"\nArguments: " << strArguments << L"\nMonitor: "
				   << stOptions.strExecutable << L"\nFolder: " << stOptions.strFolder << L"\nStartup timeout: "
				   << stOptions.secTimeout.count() << L" seconds\n";
		return EXIT_SUCCESS;
	}

	Handle hMutex(CreateMutexW(nullptr, FALSE, L"Local\\BattlenetSteamOverlayLauncher"));
	const auto dwLockResult = hMutex ? WaitForSingleObject(hMutex.get(), 0) : WAIT_FAILED;
	if (dwLockResult != WAIT_OBJECT_0 && dwLockResult != WAIT_ABANDONED)
	{
		throw std::runtime_error("Another launcher session is active.");
	}

	struct MutexRelease
	{
		HANDLE hMutex;
		~MutexRelease()
		{
			ReleaseMutex(hMutex);
		}
	} stMutexLock{hMutex.get()};
	auto strClientDirectory = Lower(fs::path{stOptions.strClientPath}.parent_path().native());
	if (!strClientDirectory.ends_with(L'\\'))
	{
		strClientDirectory += L'\\';
	}

	const auto vecClients = FindClients(strClientDirectory);
	const bool bWasRunning = !vecClients.empty();

	std::vector<Process> vecExisting;
	CollectGames(stOptions, 0, vecExisting);
	if (!vecExisting.empty())
	{
		throw std::runtime_error("This game is already running. Close it before starting a Steam session.");
	}

	Log(std::format(L"Wrapper PID {}; SteamAppId: {}", GetCurrentProcessId(), Environment(L"SteamAppId")));
	Log(std::format(L"Product: {}; monitor: {}; folder: {}", stOptions.strProduct, stOptions.strExecutable, stOptions.strFolder));
	Log(bWasRunning ? L"Battle.net was open: restarting it and leaving it open afterward." : L"Battle.net was closed: starting a new client.");
	if (bWasRunning)
	{
		StopClients(vecClients);
		if (!FindClients(strClientDirectory).empty())
		{
			throw std::runtime_error("Battle.net is still running after restart cleanup. Close it manually and retry.");
		}
	}

	const auto strLocalAppData = Environment(L"LOCALAPPDATA");
	if (pathLogDirectory.empty() && strLocalAppData.empty())
	{
		throw std::runtime_error("LOCALAPPDATA is unavailable; cannot locate Battle.net logs.");
	}
	const auto pathLogs = pathLogDirectory.empty() ? fs::path{strLocalAppData} / L"Battle.net" / L"Logs" : pathLogDirectory;
	ClientLogMonitor stLogMonitor(pathLogs, stOptions.strProduct);
	const auto ullCreatedSince = Now();
	OwnedClient stOwnedClient{!bWasRunning && !stOptions.bKeepClient, ullCreatedSince, strClientDirectory, {}};
	auto tpDeadline = Clock::now() + stOptions.secTimeout;
	auto hBootstrap = Start(stOptions.strClientPath, L"--autostarted");
	Log(L"Watching new Battle.net session logs in: " + pathLogs.native());
	Log(L"Waiting for Battle.net login. Complete any login prompt in the client.");
	ClientLogState ePreviousState = ClientLogState::WaitingForLog;
	auto tpLoginProgress = Clock::now() + 5s;
	for (;;)
	{
		stOwnedClient.Collect();
		const auto eState = stLogMonitor.Poll();
		if (stLogMonitor.UpdateState() != GameUpdateState::Unknown && stLogMonitor.UpdateState() != GameUpdateState::Ready)
		{
			stOwnedClient.bEnabled = false;
		}
		if (Clock::now() >= tpDeadline)
		{
			if (eState == ClientLogState::WaitingForLog)
			{
				throw std::runtime_error("No new Battle.net session log became readable before the startup timeout.");
			}
			throw std::runtime_error("Timed out waiting for Battle.net login and game initialization. Complete login or increase --timeout.");
		}
		if (eState == ClientLogState::Ready)
		{
			Log(L"Battle.net reports successful login and completed GameController initialization.");
			break;
		}
		if (eState != ePreviousState)
		{
			Log(eState == ClientLogState::WaitingForGames ? L"Login succeeded; waiting for game initialization." : L"Waiting for successful login in the current session log.");
			ePreviousState = eState;
		}
		if (Clock::now() >= tpLoginProgress)
		{
			Log(std::format(L"Still waiting for Battle.net readiness ({} seconds remaining).", std::chrono::duration_cast<std::chrono::seconds>(tpDeadline - Clock::now()).count()));
			tpLoginProgress = Clock::now() + 5s;
		}
		std::this_thread::sleep_for(250ms);
	}

	if (FindClients(strClientDirectory).empty())
	{
		throw std::runtime_error("Battle.net exited before the game request could be sent.");
	}

	Log(GetModuleHandleW(L"gameoverlayrenderer64.dll") || GetModuleHandleW(L"gameoverlayrenderer.dll") ? L"Steam overlay module is loaded in the wrapper." : L"No Steam overlay module detected in the wrapper. Start this shortcut through Steam.");
	Handle hLaunchRequest;
	auto tpRetry = Clock::time_point::max();
	auto tpProgress = Clock::now() + 5s;
	bool bLaunchSent = false;
	bool bRetried = false;
	bool bWaitingForUpdate = false;
	bool bWasUpdating = false;
	GameUpdateState ePreviousUpdate = GameUpdateState::Unknown;
	std::vector<Process> vecGamesRunning;
	while (vecGamesRunning.empty())
	{
		stLogMonitor.Poll();
		const auto eUpdate = stLogMonitor.UpdateState();
		stOwnedClient.bEnabled = !bWasRunning && !stOptions.bKeepClient && (eUpdate == GameUpdateState::Unknown || eUpdate == GameUpdateState::Ready);
		stOwnedClient.Collect();
		CollectGames(stOptions, ullCreatedSince, vecGamesRunning);
		if (!vecGamesRunning.empty())
		{
			break;
		}

		const bool bUpdating = eUpdate == GameUpdateState::Updating;
		const bool bBlocked = eUpdate != GameUpdateState::Unknown && eUpdate != GameUpdateState::Ready;
		if (bBlocked)
		{
			bWaitingForUpdate = true;
			stOwnedClient.bEnabled = false;
		}
		if (bUpdating && FindClients(strClientDirectory).empty())
		{
			throw std::runtime_error("Battle.net closed while updating the game.");
		}
		if (bWasUpdating && !bUpdating)
		{
			tpDeadline = Clock::now() + stOptions.secTimeout;
		}
		bWasUpdating = bUpdating;

		if (eUpdate != ePreviousUpdate)
		{
			if (bUpdating)
			{
				Log(L"The selected game is updating; waiting without using the launch timeout.");
			}
			else if (eUpdate == GameUpdateState::Pending || eUpdate == GameUpdateState::Paused)
			{
				Log(L"The selected game needs an update. Start or resume it in Battle.net.");
			}
			ePreviousUpdate = eUpdate;
		}
		if (eUpdate == GameUpdateState::Failed)
		{
			throw std::runtime_error("The game update failed. Battle.net has been left open so you can retry it.");
		}
		if (bWaitingForUpdate && eUpdate == GameUpdateState::Ready)
		{
			Log(L"Game update completed. Resuming launch monitoring.");
			bLaunchSent = false;
			bWaitingForUpdate = false;
			stOwnedClient.bEnabled = !bWasRunning && !stOptions.bKeepClient;
			tpDeadline = Clock::now() + stOptions.secTimeout;
			tpRetry = Clock::now() + 10s;
			bRetried = false;
		}
		if (!bUpdating && Clock::now() >= tpDeadline)
		{
			if (bWaitingForUpdate)
			{
				throw std::runtime_error("The game update is waiting or paused. Battle.net has been left open; resume the update and try again.");
			}
			throw std::runtime_error("Timed out waiting for the game to start.");
		}
		if (!bWaitingForUpdate)
		{
			if (!bLaunchSent)
			{
				hLaunchRequest = Start(stOptions.strClientPath, strArguments);
				bLaunchSent = true;
				tpRetry = Clock::now() + 10s;
			}
			else if (!bRetried && Clock::now() >= tpRetry)
			{
				Log(L"Game not detected; resending the launch request once.");
				hLaunchRequest = Start(stOptions.strClientPath, strArguments);
				bRetried = true;
			}
		}
		if (Clock::now() >= tpProgress)
		{
			Log(bUpdating ? L"Waiting for the game update to finish." : L"Waiting for the game to start.");
			tpProgress = Clock::now() + 5s;
		}
		std::this_thread::sleep_for(250ms);
	}
	Log(L"Game is running; monitoring until it exits.");

	std::optional<Clock::time_point> optEmptySince;
	for (;;)
	{
		stOwnedClient.Collect();
		std::erase_if(vecGamesRunning, [](const Process& stProcess)
		{
			const auto dwWaitResult = WaitForSingleObject(stProcess.hProcess.get(), 0);
			if (dwWaitResult == WAIT_FAILED)
			{
				throw std::runtime_error("Failed to wait on the game process.");
			}

			if (dwWaitResult == WAIT_OBJECT_0)
			{
				Log(std::format(L"Game PID {} exited.", stProcess.dwProcessId));
			}
			return dwWaitResult == WAIT_OBJECT_0;
		});

		CollectGames(stOptions, ullCreatedSince, vecGamesRunning);
		if (!vecGamesRunning.empty())
		{
			optEmptySince.reset();
		}
		else if (!optEmptySince)
		{
			optEmptySince = Clock::now();
		}
		else if (Clock::now() - *optEmptySince >= 5s)
		{
			break;
		}

		if (vecGamesRunning.empty())
		{
			std::this_thread::sleep_for(250ms);
		}
		else
		{
			WaitForSingleObject(vecGamesRunning.front().hProcess.get(), 500);
		}
	}

	Log(L"Game session ended.");
	return EXIT_SUCCESS;
}

}
