#include "SwingSignal.h"
#include "Config.h"
#include "LineNotifier.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

namespace
{
    // ปรับปรุงใหม่: รองรับเครื่องหมายคำพูด (Quotes) และคอมมาภายในข้อมูล CSV
    std::vector<std::string> SplitCsvLine(const std::string& line)
    {
        std::vector<std::string> fields;
        std::string field;
        bool inQuotes = false;

        for (size_t i = 0; i < line.size(); ++i)
        {
            char c = line[i];

            if (c == '"')
            {
                if (inQuotes && i + 1 < line.size() && line[i + 1] == '"')
                {
                    field += '"';
                    i++; // ข้าม Escaped Quote ถัดไป
                }
                else
                {
                    inQuotes = !inQuotes;
                }
            }
            else if (c == ',' && !inQuotes)
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

    struct Bar
    {
        std::string date;
        double open = 0.0;
        double high = 0.0;
        double low = 0.0;
        double close = 0.0;
        double volume = 0.0;
    };

    double SMA(const std::vector<Bar>& bars, int endIndexInclusive, int n)
    {
        int count = 0;
        double sum = 0.0;

        for (int i = endIndexInclusive; i >= 0 && count < n; --i, ++count)
        {
            sum += bars[i].close;
        }

        return count > 0 ? sum / count : 0.0;
    }

    double AverageVolume(const std::vector<Bar>& bars, int endIndexInclusive, int n)
    {
        int count = 0;
        double sum = 0.0;

        for (int i = endIndexInclusive; i >= 0 && count < n; --i, ++count)
        {
            sum += bars[i].volume;
        }

        return count > 0 ? sum / count : 0.0;
    }

    double HighestHigh(const std::vector<Bar>& bars, int endIndexInclusive, int n)
    {
        int count = 0;
        double best = 0.0;

        for (int i = endIndexInclusive; i >= 0 && count < n; --i, ++count)
        {
            if (bars[i].high > best)
                best = bars[i].high;
        }

        return best;
    }

    double RSI(const std::vector<Bar>& bars, int endIndexInclusive, int n)
    {
        int startIndex = endIndexInclusive - n;

        if (startIndex < 0)
            return -1.0;

        double gainSum = 0.0;
        double lossSum = 0.0;

        for (int i = startIndex + 1; i <= endIndexInclusive; ++i)
        {
            double change = bars[i].close - bars[i - 1].close;

            if (change > 0)
                gainSum += change;
            else
                lossSum += -change;
        }

        double avgGain = gainSum / n;
        double avgLoss = lossSum / n;

        if (avgLoss == 0.0)
            return 100.0;

        double rs = avgGain / avgLoss;

        return 100.0 - (100.0 / (1.0 + rs));
    }

    struct FundamentalInfo
    {
        double pe = 0.0;
        double roe = 0.0;
    };

    struct Candidate
    {
        std::string symbol;
        double lastClose = 0.0;
        double sma20 = 0.0;
        double rsi14 = 0.0;
        double volRatio = 0.0;
        bool breakout = false;
        bool pullback = false;
        int score = 0;
        FundamentalInfo fundamentals;
    };
}

bool SwingSignal::Run(
    const fs::path& projectDir,
    const fs::path& ohlcHistoryCsvPath,
    const fs::path& screenerCsvPath,
    int topN
)
{
    // ========================================================
    // 0) LOAD CONFIG & TRADING MULTIPLIERS
    // ========================================================
    Config config;
    if (!config.Load((projectDir / "config.ini").string()))
    {
        std::cerr << "SWING: Cannot load config.ini, using default trading multipliers.\n";
    }

    // ค่า Default ตามระบบเดิม
    double target1 = 1.05;  // +5%
    double target2 = 1.10;  // +10%
    double stopLoss = 0.97; // -3%

    try 
    {
        std::string t1 = config.Get("Target1Multiplier");
        std::string t2 = config.Get("Target2Multiplier");
        std::string sl = config.Get("StopLossMultiplier");

        if (!t1.empty()) target1 = std::stod(t1);
        if (!t2.empty()) target2 = std::stod(t2);
        if (!sl.empty()) stopLoss = std::stod(sl);
    } 
    catch (...) 
    {
        // ใช้ค่า Default หากแปลงข้อมูลไม่สำเร็จ
    }


    // ========================================================
    // 1) LOAD screener_result.csv
    // ========================================================

    std::map<std::string, FundamentalInfo> qualified;

    {
        std::ifstream in(screenerCsvPath.string());

        if (!in)
        {
            std::cerr
                << "SWING: Cannot open "
                << screenerCsvPath
                << " (run the fundamental screener first)\n";

            return false;
        }

        std::string header;
        std::getline(in, header);

        std::string line;

        while (std::getline(in, line))
        {
            if (line.empty())
                continue;

            std::vector<std::string> f = SplitCsvLine(line);

            if (f.size() < 13)
                continue;

            FundamentalInfo info;

            double v = 0.0;
            info.pe = TryParseDouble(f[6], v) ? v : 0.0;
            info.roe = TryParseDouble(f[12], v) ? v : 0.0;

            qualified[f[0]] = info;
        }
    }

    if (qualified.empty())
    {
        std::cout
            << "SWING: screener_result.csv has no qualified "
               "symbols today, nothing to scan.\n";

        return true;
    }

    std::cout
        << "SWING: "
        << qualified.size()
        << " fundamentally-qualified symbols to check technicals for.\n";


    // ========================================================
    // 2) LOAD ohlc_history.csv
    // ========================================================

    std::map<std::string, std::vector<Bar>> bySymbol;

    {
        std::ifstream in(ohlcHistoryCsvPath.string());

        if (!in)
        {
            std::cerr
                << "SWING: Cannot open "
                << ohlcHistoryCsvPath
                << " (run the OHLC export from AmiBroker first)\n";

            return false;
        }

        std::string header;
        std::getline(in, header);

        std::string line;

        while (std::getline(in, line))
        {
            if (line.empty())
                continue;

            std::vector<std::string> f = SplitCsvLine(line);

            if (f.size() < 7)
                continue;

            const std::string& symbol = f[0];

            if (qualified.find(symbol) == qualified.end())
                continue;

            Bar bar;
            bar.date = f[1];

            double v = 0.0;
            bar.open   = TryParseDouble(f[2], v) ? v : 0.0;
            bar.high   = TryParseDouble(f[3], v) ? v : 0.0;
            bar.low    = TryParseDouble(f[4], v) ? v : 0.0;
            bar.close  = TryParseDouble(f[5], v) ? v : 0.0;
            bar.volume = TryParseDouble(f[6], v) ? v : 0.0;

            bySymbol[symbol].push_back(bar);
        }
    }

    std::cout
        << "SWING: OHLC history loaded for "
        << bySymbol.size()
        << " / "
        << qualified.size()
        << " qualified symbols.\n";


    // ========================================================
    // 3) COMPUTE TECHNICAL SIGNALS PER SYMBOL
    // ========================================================

    std::vector<Candidate> candidates;
    candidates.reserve(qualified.size());

    const int MIN_BARS_NEEDED = 25;

    for (auto& kv : bySymbol)
    {
        const std::string& symbol = kv.first;
        std::vector<Bar>& bars = kv.second;

        std::sort(
            bars.begin(),
            bars.end(),
            [](const Bar& a, const Bar& b) { return a.date < b.date; });

        if (static_cast<int>(bars.size()) < MIN_BARS_NEEDED)
            continue;

        int last = static_cast<int>(bars.size()) - 1;

        double sma20 = SMA(bars, last, 20);
        double rsi14 = RSI(bars, last, 14);
        double avgVol20 = AverageVolume(bars, last - 1, 20);
        double hh20 = HighestHigh(bars, last - 1, 20);

        if (rsi14 < 0.0)
            continue;

        double volRatio =
            avgVol20 > 0.0 ? bars[last].volume / avgVol20 : 0.0;

        bool breakout =
            bars[last].close > hh20 &&
            volRatio >= 1.5;

        bool pullback =
            bars[last].close > sma20 &&
            rsi14 >= 40.0 &&
            rsi14 <= 55.0;

        if (!breakout && !pullback)
            continue;

        Candidate c;
        c.symbol = symbol;
        c.lastClose = bars[last].close;
        c.sma20 = sma20;
        c.rsi14 = rsi14;
        c.volRatio = volRatio;
        c.breakout = breakout;
        c.pullback = pullback;
        c.score = (breakout ? 2 : 0) + (pullback ? 1 : 0);
        c.fundamentals = qualified[symbol];

        candidates.push_back(c);
    }

    std::cout
        << "SWING: "
        << candidates.size()
        << " symbols triggered a technical signal.\n";


    // ========================================================
    // 4) RANK: score desc, then ROE desc as tie-break
    // ========================================================

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const Candidate& a, const Candidate& b)
        {
            if (a.score != b.score)
                return a.score > b.score;

            return a.fundamentals.roe > b.fundamentals.roe;
        });

    if (static_cast<int>(candidates.size()) > topN)
    {
        candidates.resize(topN);
    }


    // ========================================================
    // 5) WRITE swing_candidates.csv
    // ========================================================

    const fs::path outPath =
        projectDir / "Data" / "Price" / "swing_candidates.csv";

    std::error_code mkdirError;
    fs::create_directories(outPath.parent_path(), mkdirError);

    std::ofstream out(outPath.string(), std::ios::out | std::ios::trunc);

    if (out)
    {
        out
            << "symbol,last_close,signal,rsi14,vol_ratio,"
               "pe,roe,target_5pct,target_10pct,stop_loss_3pct\n";

        for (const auto& c : candidates)
        {
            std::string signal =
                c.breakout ? "breakout" : "pullback";

            out
                << c.symbol << ","
                << c.lastClose << ","
                << signal << ","
                << c.rsi14 << ","
                << c.volRatio << ","
                << c.fundamentals.pe << ","
                << c.fundamentals.roe << ","
                << (c.lastClose * target1) << ","
                << (c.lastClose * target2) << ","
                << (c.lastClose * stopLoss) << "\n";
        }

        std::cout
            << "SWING: candidates written -> "
            << outPath
            << "\n";
    }
    else
    {
        std::cerr
            << "SWING: Cannot write "
            << outPath
            << "\n";
    }


    // ========================================================
    // 6) LINE SUMMARY
    // ========================================================

    if (config.Get("AccessToken").empty())
    {
        std::cerr
            << "SWING: AccessToken is empty in config.ini, skipping LINE alert.\n";

        return true;
    }

    std::ostringstream message;

    message
        << "Swing scan: พบ "
        << candidates.size()
        << " ตัวเลือก (พื้นฐานผ่าน screener + สัญญาณเทคนิค)\n";

    for (const auto& c : candidates)
    {
        message
            << c.symbol
            << " @ "
            << c.lastClose
            << " ("
            << (c.breakout ? "breakout" : "pullback")
            << ", RSI "
            << static_cast<int>(c.rsi14)
            << ") เป้า +5~10% = "
            << (c.lastClose * target1)
            << "-"
            << (c.lastClose * target2)
            << " / stop -3% = "
            << (c.lastClose * stopLoss)
            << "\n";
    }

    if (candidates.empty())
    {
        message << "ไม่มีตัวเลือกเข้าเงื่อนไขวันนี้";
    }
    else
    {
        message
            << "* แนวคิดจากกฎที่ตั้งไว้เท่านั้น "
               "ไม่ใช่การการันตีกำไร ตรวจสอบเองก่อนเทรดทุกครั้ง";
    }

    LineNotifier notifier;

    bool sent = notifier.Send(
        config.Get("AccessToken"),
        config.Get("UserId"),
        message.str());

    if (sent)
    {
        std::cout << "SWING: LINE summary sent.\n";
    }
    else
    {
        std::cerr << "SWING: LINE summary FAILED.\n";
    }

    return true;
}