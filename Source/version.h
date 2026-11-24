#pragma once
// ReSharper disable CppClangTidyModernizeMacroToEnum

#define APP_NAME "Battlenet-Launcher"

#define VERSION_MAJOR 2
#define VERSION_MINOR 0
#define VERSION_PATCH 0
#define VERSION_CI_BUILD 0

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
#define VERSION_STRING STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH) "." STRINGIFY(VERSION_CI_BUILD)

#define COMPANY_NAME "Afevis / ShizCalev @ Github"
#define PRODUCT_NAME APP_NAME
#define PRODUCT_VERSION VERSION_STRING
#define FILE_VERSION VERSION_STRING
#define FILE_DESCRIPTION "Battlenet Steam Overlay Launcher"
#define INTERNAL_NAME APP_NAME
#define ORIGINAL_FILENAME APP_NAME
#define LEGAL_COPYRIGHT "© 2026 Afevis. Licensed under the MIT License."
#define LEGAL_TRADEMARKS ""
