#include "FundamentalAnalytics.h"
#include "Config.h"
#include "LineNotifier.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iostream>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <set>

namespace fs = std::filesystem;

namespace
{
    // Column order written by FundamentalCsvConverter, kept in
    // one place so a future column change only needs updating here.
    enum Column
    {
        COL_SYMBOL = 0,
        COL_LAST,
        COL_PERCENT_CHANGE,
        COL_VOLUME,
        COL_VALUE,
        COL_MARKET_CAP,
        COL_PE,
        COL_PBV,
        COL_DE_RATIO,
        COL_DPS,
        COL_EPS,
        COL_ROA,
        COL_ROE,
        COL_NET_PROFIT_MARGIN,
        COL_DIVIDEND_YIELD,
        COL_BOOK_VALUE_PER_SHARE,
        COL_LISTED_SHARE,
        COL_COUNT
    };

    std::vector<std::string> SplitCsvLine(const std::string& line)
    {
        std::vector<std::string> fields;
        std::string field;

        for (char c : line)
        {
            if (c == ',')
            {
                fields.push_back(field);
                field.clear();
            }
            else if (c != '\r')
            {
                field += c;
            }
        }

        fields.push_back(field);

        return fields;
    }

    // Empty / non-numeric fields are treated as "unknown" and
    // never match a threshold (so a stock with a blank ROE is
    // simply skipped for a MinROE filter, not falsely included
    // or excluded).
    bool TryParseDouble(const std::string& text, double& out)
    {
        if (text.empty())
            return false;

        try
        {
            size_t consumed = 0;
            out = std::stod(text, &consumed);
            return consumed > 0;
        }
        catch (...)
        {
            return false;
        }
    }

    std::string TodayDateString()
    {
        std::time_t now = std::time(nullptr);
        std::tm local{};

#if defined(_WIN32)
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif

        char buf[16] = {};

        std::snprintf(
            buf,
            sizeof(buf),
            "%04d-%02d-%02d",
            local.tm_year + 1900,
            local.tm_mon + 1,
            local.tm_mday
        );

        return std::string(buf);
    }

    struct ScreenerCriteria
    {
        bool enabled = false;

        bool hasMaxPE = false;
        double maxPE = 0.0;

        bool hasMinROE = false;
        double minROE = 0.0;

        bool hasMaxPBV = false;
        double maxPBV = 0.0;

        bool hasMinDividendYield = false;
        double minDividendYield = 0.0;

        bool hasMaxDE = false;
        double maxDE = 0.0;

        int topN = 10;
    };

    ScreenerCriteria LoadScreenerCriteria(Config& config)
    {
        ScreenerCriteria criteria;

        const std::string enabledText = config.Get("Enabled");

        criteria.enabled =
            !enabledText.empty() &&
            enabledText != "0" &&
            enabledText != "false" &&
            enabledText != "False";

        double parsed = 0.0;

        if (TryParseDouble(config.Get("MaxPE"), parsed))
        {
            criteria.hasMaxPE = true;
            criteria.maxPE = parsed;
        }

        if (TryParseDouble(config.Get("MinROE"), parsed))
        {
            criteria.hasMinROE = true;
            criteria.minROE = parsed;
        }

        if (TryParseDouble(config.Get("MaxPBV"), parsed))
        {
            criteria.hasMaxPBV = true;
            criteria.maxPBV = parsed;
        }

        if (TryParseDouble(config.Get("MinDividendYield"), parsed))
        {
            criteria.hasMinDividendYield = true;
            criteria.minDividendYield = parsed;
        }

        if (TryParseDouble(config.Get("MaxDE"), parsed))
        {
            criteria.hasMaxDE = true;
            criteria.maxDE = parsed;
        }

        if (TryParseDouble(config.Get("TopN"), parsed) && parsed >= 1.0)
        {
            criteria.topN = static_cast<int>(parsed);
        }

        return criteria;
    }

    bool RowMatchesCriteria(
        const std::vector<std::string>& fields,
        const ScreenerCriteria& criteria
    )
    {
        double value = 0.0;

        if (criteria.hasMaxPE)
        {
            if (!TryParseDouble(fields[COL_PE], value))
                return false;

            if (value <= 0.0 || value > criteria.maxPE)
                return false;
        }

        if (criteria.hasMinROE)
        {
            if (!TryParseDouble(fields[COL_ROE], value))
                return false;

            if (value < criteria.minROE)
                return false;
        }

        if (criteria.hasMaxPBV)
        {
            if (!TryParseDouble(fields[COL_PBV], value))
                return false;

            if (value <= 0.0 || value > criteria.maxPBV)
                return false;
        }

        if (criteria.hasMinDividendYield)
        {
            if (!TryParseDouble(fields[COL_DIVIDEND_YIELD], value))
                return false;

            if (value < criteria.minDividendYield)
                return false;
        }

        if (criteria.hasMaxDE)
        {
            if (!TryParseDouble(fields[COL_DE_RATIO], value))
                return false;

            if (value < 0.0 || value > criteria.maxDE)
                return false;
        }

        return true;
    }
}


bool FundamentalAnalytics::Run(
    const fs::path& projectDir,
    const fs::path& fundamentalCsvPath
)
{
    // ========================================================
    // READ SOURCE fundamental_v4.csv
    // (already verified by RunFundamental before this is called)
    // ========================================================

    std::ifstream source(fundamentalCsvPath.string());

    if (!source)
    {
        std::cerr
            << "ANALYTICS: Cannot open "
            << fundamentalCsvPath
            << "\n";

        return false;
    }

    std::string header;

    if (!std::getline(source, header))
    {
        std::cerr
            << "ANALYTICS: fundamental_v4.csv has no header.\n";

        return false;
    }

    std::vector<std::vector<std::string>> rows;

    std::string line;

    while (std::getline(source, line))
    {
        if (line.empty())
            continue;

        std::vector<std::string> fields = SplitCsvLine(line);

        if (fields.size() < static_cast<size_t>(COL_COUNT))
            continue;

        rows.push_back(std::move(fields));
    }

    std::cout
        << "ANALYTICS: loaded "
        << rows.size()
        << " symbols from fundamental_v4.csv\n";


    // ========================================================
    // 1) HISTORICAL SNAPSHOT
    //
    // Appends today's rows (with a Date column) to
    // Data/Fundamental/fundamental_history.csv. Safe to run
    // more than once per day: any existing rows for today's
    // date are replaced rather than duplicated.
    // ========================================================

    const fs::path historyPath =
        projectDir / "Data" / "Fundamental" / "fundamental_history.csv";

    const std::string today = TodayDateString();

    std::vector<std::string> historyLines;
    std::string historyHeader = "date," + header;

    if (fs::exists(historyPath))
    {
        std::ifstream existing(historyPath.string());
        std::string existingLine;

        if (std::getline(existing, existingLine))
        {
            historyHeader = existingLine;
        }

        while (std::getline(existing, existingLine))
        {
            if (existingLine.empty())
                continue;

            // Drop any prior rows for today's date so re-runs
            // on the same day overwrite instead of duplicating.
            if (existingLine.rfind(today + ",", 0) == 0)
                continue;

            historyLines.push_back(existingLine);
        }
    }

    for (const auto& fields : rows)
    {
        std::string rowText = today;

        for (const auto& f : fields)
        {
            rowText += ",";
            rowText += f;
        }

        historyLines.push_back(rowText);
    }

    std::ofstream historyOut(
        historyPath.string(),
        std::ios::out | std::ios::trunc
    );

    if (!historyOut)
    {
        std::cerr
            << "ANALYTICS: Cannot write "
            << historyPath
            << "\n";
    }
    else
    {
        historyOut << historyHeader << "\n";

        for (const auto& l : historyLines)
        {
            historyOut << l << "\n";
        }

        std::cout
            << "ANALYTICS: history snapshot written -> "
            << historyPath
            << " ("
            << historyLines.size()
            << " total rows)\n";
    }


    // ========================================================
    // LOAD config.ini (screener thresholds + LINE credentials)
    // ========================================================

    Config config;

    if (!config.Load((projectDir / "config.ini").string()))
    {
        std::cerr
            << "ANALYTICS: Cannot load config.ini, "
               "skipping screener/alert.\n";

        return true;
    }

    ScreenerCriteria criteria = LoadScreenerCriteria(config);

    if (!criteria.enabled)
    {
        std::cout
            << "ANALYTICS: Screener disabled "
               "([SCREENER] Enabled=false), skipping.\n";

        return true;
    }


    // ========================================================
    // 2) SCREENER / FILTER
    // ========================================================

    struct Match
    {
        std::string symbol;
        double pe = 0.0;
        double roe = 0.0;
    };

    std::vector<Match> matches;

    const fs::path screenerPath =
        projectDir / "Data" / "Fundamental" / "screener_result.csv";

    std::ofstream screenerOut(
        screenerPath.string(),
        std::ios::out | std::ios::trunc
    );

    if (screenerOut)
    {
        screenerOut << header << "\n";
    }

    for (const auto& fields : rows)
    {
        if (!RowMatchesCriteria(fields, criteria))
            continue;

        if (screenerOut)
        {
            for (size_t i = 0; i < fields.size(); ++i)
            {
                screenerOut << fields[i];

                if (i + 1 < fields.size())
                    screenerOut << ",";
            }

            screenerOut << "\n";
        }

        Match m;
        m.symbol = fields[COL_SYMBOL];

        double v = 0.0;
        m.pe = TryParseDouble(fields[COL_PE], v) ? v : 0.0;
        m.roe = TryParseDouble(fields[COL_ROE], v) ? v : 0.0;

        matches.push_back(m);
    }

    std::cout
        << "ANALYTICS: screener matched "
        << matches.size()
        << " / "
        << rows.size()
        << " symbols -> "
        << screenerPath
        << "\n";


    // ========================================================
    // 3) LINE ALERT
    //
    // Sends a short summary (count + top N by lowest P/E) using
    // the same LineNotifier / config credentials as the existing
    // Price and Fundamental notifications. A failure here does
    // not fail the overall workflow.
    // ========================================================

    std::sort(
        matches.begin(),
        matches.end(),
        [](const Match& a, const Match& b)
        {
            // Symbols with no P/E (<=0) sort last.
            if (a.pe <= 0.0) return false;
            if (b.pe <= 0.0) return true;
            return a.pe < b.pe;
        }
    );

    std::ostringstream message;

    message
        << "Screener: พบ "
        << matches.size()
        << " หุ้นเข้าเงื่อนไข\n";

    const int shown = std::min(
        static_cast<int>(matches.size()),
        criteria.topN
    );

    for (int i = 0; i < shown; ++i)
    {
        message
            << matches[i].symbol
            << " (PE "
            << matches[i].pe
            << ", ROE "
            << matches[i].roe
            << "%)\n";
    }

    if (matches.empty())
    {
        message << "ไม่มีหุ้นเข้าเงื่อนไขวันนี้";
    }

    LineNotifier notifier;

    bool sent = notifier.Send(
        config.Get("AccessToken"),
        config.Get("UserId"),
        message.str()
    );

    if (sent)
    {
        std::cout << "ANALYTICS: LINE screener alert sent.\n";
    }
    else
    {
        std::cerr << "ANALYTICS: LINE screener alert FAILED.\n";
    }

    return true;
}
