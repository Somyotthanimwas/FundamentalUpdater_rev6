#ifndef FUNDAMENTALANALYTICS_H
#define FUNDAMENTALANALYTICS_H

#include <string>
#include <filesystem>

// ============================================================
// FundamentalAnalytics
//
// New in Rev6. Consumes the existing fundamental_v4.csv
// (unchanged, still produced by Fundamental V4 / CsvConverter)
// and adds three features on top of it, without touching
// Price V4 / Fundamental V4 / Excel V4 logic:
//
//   1. Historical snapshot  -> Data/Fundamental/fundamental_history.csv
//   2. Screener / filter    -> Data/Fundamental/screener_result.csv
//   3. LINE alert           -> reuses LineNotifier, sends a summary
//
// Screener thresholds are read from config.ini, section [SCREENER]:
//
//   [SCREENER]
//   Enabled=true
//   MaxPE=15
//   MinROE=15
//   MaxPBV=2
//   MinDividendYield=3
//   MaxDE=1.5
//   TopN=10
//
// Any threshold left blank/absent is not applied (no filtering
// on that field).
// ============================================================

class FundamentalAnalytics
{
public:

    // Runs all three features against an existing, already
    // verified fundamental_v4.csv. Never throws; returns false
    // on a hard failure (e.g. cannot read the source CSV), but
    // a screener/LINE failure alone should not fail the whole
    // Fundamental workflow, so callers may choose to ignore a
    // false return here and continue.
    static bool Run(
        const std::filesystem::path& projectDir,
        const std::filesystem::path& fundamentalCsvPath
    );
};

#endif
