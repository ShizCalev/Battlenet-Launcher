#include "../Source/launcher.h"
#include "../Source/client_log.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <shellapi.h>
#include <stdexcept>

using namespace launcher;
using namespace std::chrono_literals;

void Check(bool bCondition, const char* pszMessage)
{
	if (!bCondition)
	{
		throw std::runtime_error(pszMessage);
	}
}

std::wstring Self()
{
	return ImagePath(GetCurrentProcess());
}

std::wstring Sibling(const wchar_t* pszName)
{
	const auto strPath = Self();
	return strPath.substr(0, strPath.find_last_of(L'\\') + 1) + pszName;
}

void StopMocks()
{
	for (const auto& stProcess : Processes())
	{
		Handle hProcess(OpenProcess(
			PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE, stProcess.th32ProcessID));
		if (hProcess &&
			(Equal(ImagePath(hProcess.get()), Sibling(L"Battle.net.exe")) ||
				Equal(ImagePath(hProcess.get()), Sibling(L"mock-game.exe"))))
		{
			TerminateProcess(hProcess.get(), 0);
			WaitForSingleObject(hProcess.get(), 5000);
		}
	}
}

Options TestOptions()
{
	Options stOptions;
	stOptions.strClientPath = Sibling(L"Battle.net.exe");
	stOptions.strProduct = L"test";
	stOptions.strExecutable = Sibling(L"mock-game.exe");
	stOptions.secTimeout = 30s;
	return stOptions;
}

void CheckLogReadiness()
{
	const auto pathRoot = std::filesystem::path{Sibling(L"LogTests")} / std::to_wstring(Now());
	std::filesystem::create_directories(pathRoot);
	const std::string strLogin = "I 2026-11-05 00:00:00 [BNLogin] {Main} Logged into Battle.net successfully.\n";
	const std::string strReady = "I 2026-11-05 00:00:01 [GameController] {Main} GameController initialization complete\n";
	auto fnWrite = [](const std::filesystem::path& pathFile, std::string_view svText, bool bAppend = false)
	{
		std::ofstream fileLog(pathFile, std::ios::binary | (bAppend ? std::ios::app : std::ios::trunc));
		fileLog << svText;
		Check(fileLog.good(), "Write log fixture");
	};

	const auto pathOld = pathRoot / L"battle.net-old.log";
	fnWrite(pathOld, strLogin + strReady);
	ClientLogMonitor stMonitor(pathRoot);
	Check(stMonitor.Poll() == ClientLogState::WaitingForLog, "Ignore previous session logs");
	fnWrite(pathOld, strLogin + strReady, true);
	Check(stMonitor.Poll() == ClientLogState::WaitingForLog, "Ignore appended old-session readiness");
	fnWrite(pathRoot / L"libcef-unrelated.log", strLogin + strReady);
	Check(stMonitor.Poll() == ClientLogState::WaitingForLog, "Ignore other log types");

	const auto pathCurrent = pathRoot / L"battle.net-current.log";
	fnWrite(pathCurrent, strLogin);
	Check(stMonitor.Poll() == ClientLogState::WaitingForGames, "Login alone is not ready");
	fnWrite(pathRoot / L"battle.net-helper.log", strReady);
	Check(stMonitor.Poll() != ClientLogState::Ready, "Do not combine readiness across client logs");
	fnWrite(pathCurrent, strReady.substr(0, strReady.size() - 1), true);
	Check(stMonitor.Poll() == ClientLogState::WaitingForGames, "Wait for complete log lines");
	fnWrite(pathCurrent, "\n", true);
	Check(stMonitor.Poll() == ClientLogState::Ready, "Detect current-session readiness");
	fnWrite(pathCurrent, "I [GameProvider] {Main} Game provider is no longer initialized and is now reset\n", true);
	Check(stMonitor.Poll() == ClientLogState::WaitingForLogin, "Reset readiness after logout");
	fnWrite(pathCurrent, strLogin + strReady);
	Check(stMonitor.Poll() == ClientLogState::Ready, "Handle a truncated log");

	const auto pathReordered = pathRoot / L"battle.net-reordered.log";
	ClientLogMonitor stReordered(pathRoot);
	fnWrite(pathReordered, strReady);
	Check(stReordered.Poll() == ClientLogState::WaitingForLogin, "Game initialization alone is not ready");
	fnWrite(pathReordered, strLogin, true);
	Check(stReordered.Poll() == ClientLogState::Ready, "Detect readiness when initialization precedes login");

	ClientLogMonitor stMissing(pathRoot / L"NewLogs");
	Check(stMissing.Poll() == ClientLogState::WaitingForLog, "Wait for a missing log directory");
	std::filesystem::create_directories(pathRoot / L"NewLogs");
	fnWrite(pathRoot / L"NewLogs" / L"battle.net-new.log", strLogin + strReady);
	Check(stMissing.Poll() == ClientLogState::Ready, "Detect newly created log directory");

	const auto pathUpdates = pathRoot / L"Updates";
	std::filesystem::create_directories(pathUpdates);
	ClientLogMonitor stUpdates(pathUpdates, L"WoWF");
	const auto pathUpdateLog = pathUpdates / L"battle.net-updates.log";
	auto fnUpdate = [&](std::string_view svLine, GameUpdateState eExpected)
	{
		fnWrite(pathUpdateLog, std::string(svLine) + "\n", true);
		stUpdates.Poll();
		Check(stUpdates.UpdateState() == eExpected, "Selected-game update state");
	};
	fnUpdate("D [ProductState] {Main} InstallState (wowf): currentOperation=UpToDate currentOperationStatus=Paused", GameUpdateState::Ready);
	fnUpdate("D [ProductState] {Main} InstallState (wowf): currentOperation=Update currentOperationStatus=New playable=0", GameUpdateState::Pending);
	fnUpdate("D [ProductState] {Main} InstallState (wowf): currentOperationStatus=Waiting", GameUpdateState::Updating);
	fnUpdate("D [InstallManager] {Main} Update started: wowf", GameUpdateState::Updating);
	fnUpdate("D [ProductState] {Main} InstallState (wowf): currentOperationStatus=Finalizing progress=1", GameUpdateState::Updating);
	fnUpdate("D [ProductState] {Main} InstallState (wowf): playable=1 currentStage=Complete", GameUpdateState::Updating);
	fnUpdate("D [InstallManager] {Main} Update completed: wowf_beta", GameUpdateState::Updating);
	fnUpdate("D [ProductState] {Main} InstallState (s2): currentOperation=UpToDate", GameUpdateState::Updating);
	fnUpdate("D [ProductState] {Main} InstallState (wowf): currentOperationStatus=Paused", GameUpdateState::Paused);
	fnUpdate("D [ProductState] {Main} InstallState (wowf): currentOperationStatus=Running", GameUpdateState::Updating);
	fnUpdate("D [ProductState] {Main} InstallState (wowf): currentOperationStatus=Failed", GameUpdateState::Failed);
	fnUpdate("D [InstallManager] {Main} Update completed: wowf", GameUpdateState::Ready);
	fnUpdate("D [ProductState] {Main} InstallState (wowf): currentOperation=UpToDate currentOperationStatus=Paused", GameUpdateState::Ready);

	ClientLogMonitor stAlias(pathUpdates, L"D3");
	fnWrite(pathUpdates / L"battle.net-alias.log", "D [ProductState] {Main} InstallState (diablo3): currentOperation=Update currentOperationStatus=Running\n");
	stAlias.Poll();
	Check(stAlias.UpdateState() == GameUpdateState::Updating, "Launch-code to update-UID mapping");
}

int wmain(int nArgCount, wchar_t** ppszArguments)
{
	if (nArgCount > 1 && std::wstring(ppszArguments[1]).find(L"--exec=") == 0)
	{
		if (std::filesystem::exists(Sibling(L"update-pending.flag")))
		{
			std::ofstream(Sibling(L"early-launch.flag")) << "Launch requested during update";
			return EXIT_FAILURE;
		}
		if (std::wstring(ppszArguments[1]).find(L"timeout") == std::wstring::npos)
		{
			auto hGame = Start(Sibling(L"mock-game.exe"), L"--mock-game");
		}

		Sleep(60000);
		return EXIT_SUCCESS;
	}

	if (nArgCount > 1 && Equal(ppszArguments[1], L"--mock-game"))
	{
		Sleep(1000);
		auto hReplacement = Start(Self(), L"--replacement");
		return EXIT_SUCCESS;
	}

	if (nArgCount > 1 && Equal(ppszArguments[1], L"--replacement"))
	{
		Sleep(2000);
		return EXIT_SUCCESS;
	}

	if (nArgCount > 1 && Equal(ppszArguments[1], L"--autostarted"))
	{
		const auto pathLogs = std::filesystem::path{Sibling(L"Logs")};
		std::filesystem::create_directories(pathLogs);
		const auto pathLog = pathLogs / (L"battle.net-" + std::to_wstring(GetCurrentProcessId()) + L".log");
		const bool bSimulateUpdate = GetEnvironmentVariableW(L"BNET_LAUNCHER_TEST_UPDATE", nullptr, 0) != 0;
		if (bSimulateUpdate)
		{
			std::ofstream(Sibling(L"update-pending.flag")) << "Updating";
		}
		Sleep(1200);
		std::ofstream fileLog(pathLog, std::ios::binary);
		fileLog << "I 2026-11-05 00:00:00 [BNLogin] {Main} Logged into Battle.net successfully.\n"
				<< std::flush;
		Sleep(300);
		if (bSimulateUpdate)
		{
			fileLog << "D [ProductState] {Main} InstallState (test): currentOperation=Update currentOperationStatus=Running\n";
		}
		fileLog << "I 2026-11-05 00:00:01 [GameController] {Main} GameController initialization complete\n"
				<< std::flush;
		if (bSimulateUpdate)
		{
			Sleep(3500);
			fileLog << "D [ProductState] {Main} InstallState (test): currentOperationStatus=Finalizing playable=1 progress=1\n"
					<< std::flush;
			Sleep(500);
			std::filesystem::remove(Sibling(L"update-pending.flag"));
			fileLog << "D [InstallManager] {Main} Update completed: test\n"
					<< std::flush;
		}
		Sleep(60000);
		return EXIT_SUCCESS;
	}

	if (nArgCount > 1 && Equal(ppszArguments[1], L"--idle"))
	{
		Sleep(60000);
		return EXIT_SUCCESS;
	}

	try
	{
		CheckLogReadiness();
		Check(CopyFileW(Self().c_str(), Sibling(L"Battle.net.exe").c_str(), FALSE) != 0, "Copy client fixture");
		Check(CopyFileW(Self().c_str(), Sibling(L"mock-game.exe").c_str(), FALSE) != 0, "Copy game fixture");
		for (const std::wstring strValue :
			{L"", L"simple", L"path with spaces\\", L"a\\\"b", L"C:\\Unicode \u00e9\\game.exe"})
		{
			const auto strCommandLine = L"test.exe " + Quote(strValue);
			int nParsedCount = 0;
			auto ppszParsedArguments = CommandLineToArgvW(strCommandLine.c_str(), &nParsedCount);
			Check(ppszParsedArguments && nParsedCount == 2 && strValue == ppszParsedArguments[1],
				"Command-line quote round trip");
			LocalFree(ppszParsedArguments);
		}

		Check(Equal(L"", L""), "Empty string views");
		Check(Equal(std::wstring_view{L"Wow.exe suffix"}.substr(0, 7), L"WOW.EXE"), "Non-terminated string view");
		Check(Equal(L"\u00c9.exe", L"\u00e9.EXE"), "Ordinal Unicode comparison");
		Check(Matches(L"SC2.exe|SC2_x64.exe", L"sc2_X64.exe"), "SC2 executable match");
		Check(!Matches(L"SC2.exe|SC2_x64.exe", L"SC2Switcher_x64.exe"), "Reject bootstrap executable");
		Options stWowOptions;
		stWowOptions.strExecutable = L"Wow.exe";
		stWowOptions.strFolder = L"_forever_";
		Check(IsGame(stWowOptions, L"Wow.exe", L"F:\\WoW\\_forever_\\Wow.exe"), "Forever match");
		Check(!IsGame(stWowOptions, L"Wow.exe", L"F:\\WoW\\_retail_\\Wow.exe"), "Retail isolation");
		auto stOptions = TestOptions();
		std::vector<std::wstring> vecArgumentStorage{
			L"launcher", L"WOWF", stOptions.strClientPath, L"--timeout", L"15"};
		std::vector<wchar_t*> vecArguments;
		for (auto& strText : vecArgumentStorage)
		{
			vecArguments.push_back(&strText[0]);
		}

		const auto stParsedOptions = Parse(vecArguments);
		Check(stParsedOptions.strProduct == L"WoWF" && stParsedOptions.strFolder == L"_forever_" &&
				stParsedOptions.secTimeout == 15s,
			"Parse preset");
		bool bRejected = false;
		for (const auto* pszInvalid : {L"-1", L"0", L"3601", L"99999999999999999999", L"1.5", L""})
		{
			vecArgumentStorage.back() = pszInvalid;
			vecArguments.back() = vecArgumentStorage.back().data();
			bRejected = false;
			try
			{
				Parse(vecArguments);
			}
			catch (...)
			{
				bRejected = true;
			}

			Check(bRejected, "Reject invalid timeout");
		}

		DWORD dwHandlesBefore = 0, dwHandlesAfter = 0;
		GetProcessHandleCount(GetCurrentProcess(), &dwHandlesBefore);
		for (int nIndex = 0; nIndex < 100; ++nIndex)
		{
			std::vector<Process> vecDiscovered;
			CollectGames(stOptions, Now(), vecDiscovered);
		}

		GetProcessHandleCount(GetCurrentProcess(), &dwHandlesAfter);
		Check(dwHandlesAfter <= dwHandlesBefore + 1, "Process discovery leaks handles");

		const auto ullStartedAt = GetTickCount64();
		Check(Run(stOptions, Sibling(L"Logs")) == EXIT_SUCCESS, "Launch and monitor mock game");
		Check(GetTickCount64() - ullStartedAt >= 7500, "Monitor game replacement and exit grace");
		Sleep(200);
		for (const auto& stProcess : Processes())
		{
			Handle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, stProcess.th32ProcessID));
			Check(!hProcess || !Equal(ImagePath(hProcess.get()), stOptions.strClientPath),
				"Owned client was not terminated");
		}

		auto hExistingClient = Start(stOptions.strClientPath, L"--idle");
		Check(Run(stOptions, Sibling(L"Logs")) == EXIT_SUCCESS, "Run with existing client");
		Check(WaitForSingleObject(hExistingClient.get(), 0) == WAIT_OBJECT_0, "Preexisting client was not restarted");
		bool bReplacementAlive = false;
		for (const auto& stProcess : Processes())
		{
			Handle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, stProcess.th32ProcessID));
			if (hProcess && Equal(ImagePath(hProcess.get()), stOptions.strClientPath))
			{
				bReplacementAlive = true;
			}
		}
		Check(bReplacementAlive, "Restarted client was not left open");
		StopMocks();
		stOptions.bKeepClient = true;
		Check(Run(stOptions, Sibling(L"Logs")) == EXIT_SUCCESS, "Run with keep-client");
		bool bKeptClient = false;
		for (const auto& stProcess : Processes())
		{
			Handle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, stProcess.th32ProcessID));
			if (hProcess && Equal(ImagePath(hProcess.get()), stOptions.strClientPath))
			{
				bKeptClient = true;
			}
		}

		Check(bKeptClient, "keep-client did not preserve client");
		StopMocks();
		stOptions.bKeepClient = false;
		auto hRunningGame = Start(stOptions.strExecutable, L"--idle");
		bRejected = false;
		try
		{
			Run(stOptions, Sibling(L"Logs"));
		}
		catch (...)
		{
			bRejected = true;
		}

		Check(bRejected, "Existing game must be rejected");
		Check(WaitForSingleObject(hRunningGame.get(), 0) == WAIT_TIMEOUT, "Existing game was modified");
		StopMocks();
		std::filesystem::remove(Sibling(L"early-launch.flag"));
		std::filesystem::remove(Sibling(L"update-pending.flag"));
		SetEnvironmentVariableW(L"BNET_LAUNCHER_TEST_UPDATE", L"1");
		stOptions.secTimeout = 3s;
		Check(Run(stOptions, Sibling(L"Logs")) == EXIT_SUCCESS, "Update exceeding launch timeout must finish before launch");
		Check(!std::filesystem::exists(Sibling(L"early-launch.flag")), "Launch request was sent during update");
		SetEnvironmentVariableW(L"BNET_LAUNCHER_TEST_UPDATE", nullptr);
		for (const auto& stProcess : Processes())
		{
			Handle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, stProcess.th32ProcessID));
			Check(!hProcess || !Equal(ImagePath(hProcess.get()), stOptions.strClientPath), "Owned client not cleaned up after update and game");
		}
		stOptions.strProduct = L"timeout";
		stOptions.secTimeout = 1s;
		bRejected = false;
		try
		{
			Run(stOptions, Sibling(L"Logs"));
		}
		catch (...)
		{
			bRejected = true;
		}

		Check(bRejected, "Startup timeout must fail");
		Sleep(200);
		for (const auto& stProcess : Processes())
		{
			Handle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, stProcess.th32ProcessID));
			Check(!hProcess || !Equal(ImagePath(hProcess.get()), stOptions.strClientPath),
				"Timeout left owned client running");
		}

		std::cout << "All launcher tests passed.\n";
		return EXIT_SUCCESS;
	}
	catch (const std::exception& exError)
	{
		StopMocks();
		std::cerr << exError.what() << '\n';
		return EXIT_FAILURE;
	}
}
