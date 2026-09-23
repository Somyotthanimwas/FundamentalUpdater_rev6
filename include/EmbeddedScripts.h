#pragma once

#include <string>

namespace EmbeddedScripts
{
    bool WriteImportBat(const std::string& path);
    bool WriteImportVbs(const std::string& path);

    // NEW in Rev6: export historical daily OHLC out of AmiBroker
    // (for the swing-trade candidate scan), mirroring the existing
    // hidden-Broker.exe + cscript pattern used for import.
    bool WriteExportOhlcBat(const std::string& path);
    bool WriteExportOhlcVbs(const std::string& path);
}
