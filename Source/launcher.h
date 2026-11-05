#pragma once

#include <windows.h>
#include <tlhelp32.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace launcher
{
struct HandleCloser
{
	void operator()(void* hHandle) const noexcept
	{
		if (hHandle && hHandle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hHandle);
		}
	}
};

using Handle = std::unique_ptr<void, HandleCloser>;

struct Options
{
	std::wstring strCode;
	std::wstring strClientPath;
	std::wstring strProduct;
	std::wstring strExecutable;
	std::wstring strFolder;
	std::chrono::seconds secTimeout{180};
	bool bDryRun = false;
	bool bKeepClient = false;
	bool bHelp = false;
};

struct Process
{
	DWORD dwProcessId;
	Handle hProcess;
};

bool Equal(std::wstring_view svLeft, std::wstring_view svRight);
bool Matches(std::wstring_view svNames, std::wstring_view svName);
std::wstring Quote(std::wstring_view svValue);
Options Parse(std::span<wchar_t* const> spArguments);
void Help();
int Run(const Options& stOptions, const std::filesystem::path& pathLogDirectory = {});

std::wstring ImagePath(HANDLE hProcess);
ULONGLONG Now();
std::vector<PROCESSENTRY32W> Processes();
bool IsGame(const Options& stOptions, const std::wstring& strName, const std::wstring& strPath);
void CollectGames(const Options& stOptions, ULONGLONG ullCreatedSince, std::vector<Process>& vecTracked);
Handle Start(const std::wstring& strExecutable, const std::wstring& strArguments);
}
