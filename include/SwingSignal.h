#ifndef SWINGSIGNAL_H
#define SWINGSIGNAL_H

#include <filesystem>

// ============================================================
// SwingSignal
//
// New in Rev6. Picks a short list (default 5) of swing-trade
// candidates aimed at a 5-10% move over roughly 1-2 weeks, by
// combining:
//
//   1. FUNDAMENTAL FILTER  - the symbol must already be in
//      today's screener_result.csv (produced by
//      FundamentalAnalytics from the existing [SCREENER]
//      thresholds in config.ini).
//
//   2. TECHNICAL SIGNAL    - computed from daily OHLC history
//      (Data/Price/ohlc_history.csv, exported from AmiBroker):
//        - Breakout : close > highest high of the prior 20 bars,
//                     with volume >= 1.5x its 20-bar average.
//        - Pullback : close above SMA20 (uptrend intact) with
//                     RSI14 in a 40-55 "healthy pullback" zone
//                     (not overbought, not falling knife).
//
// This is a rule-based idea generator, not a prediction or
// financial advice: it does not guarantee any stock will move
// 5-10%, and every candidate should still be checked manually
// before placing a trade.
//
// Output: Data/Price/swing_candidates.csv, and a LINE summary
// via the same LineNotifier/config credentials used elsewhere.
// ============================================================

class SwingSignal
{
public:

    static bool Run(
        const std::filesystem::path& projectDir,
        const std::filesystem::path& ohlcHistoryCsvPath,
        const std::filesystem::path& screenerCsvPath,
        int topN = 5
    );
};

#endif
