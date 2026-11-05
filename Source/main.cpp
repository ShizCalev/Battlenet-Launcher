#include "launcher.h"
#include "version.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

int wmain(int nArgCount, wchar_t** ppszArguments)
{
	int nExitCode = EXIT_SUCCESS;
	const std::span spArguments{ppszArguments, static_cast<size_t>(nArgCount)};

#ifdef _DEBUG
	DWORD dwConsoleProcessId = 0;
	const bool bOwnConsole = GetConsoleProcessList(&dwConsoleProcessId, 1) == 1;
	std::wcout << APP_NAME << L" " << VERSION_STRING << std::endl;
#else
	const bool bConsole = std::ranges::any_of(spArguments.subspan(1), [](std::wstring_view svArgument)
	{
		return svArgument == L"--dry-run" || svArgument == L"--help" || svArgument == L"--list" || svArgument == L"-h";
	});
	if (!bConsole)
	{
		FreeConsole();
	}
#endif

	try
	{
		const auto stOptions = launcher::Parse(spArguments);
		if (stOptions.bHelp)
		{
			launcher::Help();
		}
		else
		{
			nExitCode = launcher::Run(stOptions);
		}
	}
	catch (const std::exception& exError)
	{
#ifdef _DEBUG
		std::cerr << "Error: " << exError.what() << std::endl;
#else
		if (bConsole)
		{
			std::cerr << "Error: " << exError.what() << std::endl;
		}
		else
		{
			MessageBoxA(nullptr, exError.what(), "Battle.net Steam Overlay Launcher", MB_OK | MB_ICONERROR);
		}
#endif
		nExitCode = EXIT_FAILURE;
	}

#ifdef _DEBUG
	if (bOwnConsole)
	{
		std::wcout << L"Press Enter to close the launcher." << std::endl;
		std::wstring strInput;
		std::getline(std::wcin, strInput);
	}

#endif

	return nExitCode;
}
