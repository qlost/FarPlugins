#include <farversion.hpp>

#define COMPANYNAME 0
#define PLUGIN_DESC L"FARDroid FAR Plugin"
#define PLUGIN_NAME L"FARDroid"
#define PLUGIN_FILENAME L"fardroid.dll"
#define PLUGIN_AUTHOR L"dimfish 2016, Vladimir Kubyshev 2010"
#define PLUGIN_MAJOR 2026
#define PLUGIN_MINOR 0
#define PLUGIN_REVISION 0
#define PLUGIN_BUILD 0
#include ".version.h"
#define PLUGIN_VERSION MAKEFARVERSION(PLUGIN_MAJOR,PLUGIN_MINOR,PLUGIN_REVISION,PLUGIN_BUILD,VS_RELEASE)
