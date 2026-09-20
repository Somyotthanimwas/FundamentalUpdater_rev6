// FundamentalUpdater_rev6 technical entry/exit screener
// Runs outside AmiBroker through OLE automation.
// Input:  Data/Fundamental/screener_result.csv
// Output: Data/Fundamental/technical_entry_exit.csv
//         Data/Fundamental/technical_alert.txt
//
// The screener deliberately uses the existing Fundamental screener as its
// candidate universe, then reads historical OHLCV from AmiBroker.
//
// Strategy:
// - 1-2 week swing horizon
// - Trend: SMA20 > SMA50 and close > SMA20
// - Momentum: 10-day ROC > 2%
// - Breakout: close within 1% of the prior 20-day high
// - Volume confirmation: volume >= 1.2 x 20-day average
// - RSI14: 50..75
// - ATR14 volatility cap: ATR14 / close <= 8%
// - Score candidates and return the top 5.
// - Reference entry is the latest close. Target1 = +5%, Target2 = +10%.
// - Stop is max(4%, 2*ATR) below reference entry.
// - This is a signal/ranking system, not a guarantee of profit.

var fso = new ActiveXObject("Scripting.FileSystemObject");
var shell = new ActiveXObject("WScript.Shell");

function die(msg) {
    WScript.Echo("TECHNICAL SCREENER ERROR: " + msg);
    WScript.Quit(1);
}

function csvFields(line) {
    // Fundamental CSV fields are simple numeric/text fields. This parser
    // handles quoted commas as well, so it is safe for future changes.
    var a = [], cur = "", quoted = false;
    for (var i = 0; i < line.length; i++) {
        var c = line.charAt(i);
        if (c === '"') {
            if (quoted && i + 1 < line.length && line.charAt(i + 1) === '"') {
                cur += '"'; i++;
            } else {
                quoted = !quoted;
            }
        } else if (c === ',' && !quoted) {
            a.push(cur); cur = "";
        } else {
            cur += c;
        }
    }
    a.push(cur);
    return a;
}

function num(v) {
    var x = parseFloat(String(v).replace(/,/g, ""));
    return isFinite(x) ? x : NaN;
}

function sma(values, n) {
    if (values.length < n) return NaN;
    var s = 0;
    for (var i = values.length - n; i < values.length; i++) s += values[i];
    return s / n;
}

function highest(values, n) {
    if (values.length < n) return NaN;
    var start = values.length - n;
    var m = -Infinity;
    for (var i = start; i < values.length; i++) if (values[i] > m) m = values[i];
    return m;
}

function rsi(closes, n) {
    if (closes.length < n + 1) return NaN;
    var gain = 0, loss = 0;
    for (var i = closes.length - n; i < closes.length; i++) {
        var d = closes[i] - closes[i - 1];
        if (d > 0) gain += d;
        else loss -= d;
    }
    if (loss === 0) return 100;
    var rs = (gain / n) / (loss / n);
    return 100 - (100 / (1 + rs));
}

function atr(highs, lows, closes, n) {
    if (closes.length < n + 1) return NaN;
    var sum = 0;
    for (var i = closes.length - n; i < closes.length; i++) {
        var tr1 = highs[i] - lows[i];
        var tr2 = Math.abs(highs[i] - closes[i - 1]);
        var tr3 = Math.abs(lows[i] - closes[i - 1]);
        sum += Math.max(tr1, tr2, tr3);
    }
    return sum / n;
}

function isoDate(d) {
    var y = d.getFullYear();
    var m = d.getMonth() + 1;
    var day = d.getDate();
    return y + "-" + (m < 10 ? "0" : "") + m + "-" + (day < 10 ? "0" : "") + day;
}

function fmt(x) {
    return isFinite(x) ? x.toFixed(2) : "";
}

if (WScript.Arguments.length < 2) die("usage: technical_screener.js <screener.csv> <output-dir>");

var inputPath = WScript.Arguments(0);
var outputDir = WScript.Arguments(1);

if (!fso.FileExists(inputPath)) die("candidate CSV not found: " + inputPath);
if (!fso.FolderExists(outputDir)) fso.CreateFolder(outputDir);

var input = fso.OpenTextFile(inputPath, 1, false, -1);
var header = input.ReadLine();
if (!header) die("candidate CSV is empty");

var headerFields = csvFields(header);
var symbolIndex = -1;
for (var h = 0; h < headerFields.length; h++) {
    if (headerFields[h].toLowerCase() === "symbol") { symbolIndex = h; break; }
}
if (symbolIndex < 0) die("Symbol column not found");

var candidates = [];
while (!input.AtEndOfStream) {
    var line = input.ReadLine();
    if (!line) continue;
    var fields = csvFields(line);
    if (symbolIndex < fields.length && fields[symbolIndex]) candidates.push(fields[symbolIndex].trim());
}
input.Close();

var AB = new ActiveXObject("Broker.Application");
var results = [];
var skipped = 0;

for (var s = 0; s < candidates.length; s++) {
    var symbol = candidates[s];
    try {
        var stock = AB.Stocks.Item(symbol);
        if (!stock) { skipped++; continue; }

        var q = stock.Quotations;
        var count = q.Count;
        if (count < 55) { skipped++; continue; }

        // Read enough bars for SMA50 + RSI/ATR + breakout.
        var take = Math.min(count, 80);
        var opens = [], highs = [], lows = [], closes = [], volumes = [], dates = [];

        for (var j = count - take; j < count; j++) {
            var bar = q.Item(j);
            if (!bar) continue;
            opens.push(num(bar.Open));
            highs.push(num(bar.High));
            lows.push(num(bar.Low));
            closes.push(num(bar.Close));
            volumes.push(num(bar.Volume));
            dates.push(new Date(bar.Date));
        }

        if (closes.length < 55) { skipped++; continue; }

        var close = closes[closes.length - 1];
        var open = opens[opens.length - 1];
        var sma20 = sma(closes, 20);
        var sma50 = sma(closes, 50);
        var prior20 = closes.slice(0, closes.length - 1);
        var high20 = highest(prior20, 20);
        var avgVol20 = sma(volumes.slice(0, volumes.length - 1), 20);
        var roc10 = (close / closes[closes.length - 11] - 1) * 100;
        var r = rsi(closes, 14);
        var a = atr(highs, lows, closes, 14);

        if (!isFinite(close) || !isFinite(sma20) || !isFinite(sma50) ||
            !isFinite(high20) || !isFinite(avgVol20) || !isFinite(roc10) ||
            !isFinite(r) || !isFinite(a)) { skipped++; continue; }

        var trend = (sma20 > sma50 && close > sma20);
        var momentum = (roc10 > 2);
        var breakout = (close >= high20 * 0.99);
        var volumeOk = (volumes[volumes.length - 1] >= avgVol20 * 1.20);
        var rsiOk = (r >= 50 && r <= 75);
        var volPct = (a / close) * 100;
        var volatilityOk = (volPct <= 8);

        // Score is intentionally transparent: 6 binary components.
        var score = 0;
        if (trend) score += 25;
        if (momentum) score += 20;
        if (breakout) score += 20;
        if (volumeOk) score += 15;
        if (rsiOk) score += 10;
        if (volatilityOk) score += 10;

        // Require trend plus at least two confirmations.
        var confirmations = 0;
        if (momentum) confirmations++;
        if (breakout) confirmations++;
        if (volumeOk) confirmations++;
        if (rsiOk) confirmations++;
        if (volatilityOk) confirmations++;

        if (!trend || confirmations < 2 || score < 55) continue;

        var stopPct = Math.max(4, (a / close) * 200);
        if (stopPct > 10) stopPct = 10;

        results.push({
            symbol: symbol,
            date: isoDate(dates[dates.length - 1]),
            open: open,
            close: close,
            sma20: sma20,
            sma50: sma50,
            roc10: roc10,
            rsi14: r,
            atr14: a,
            volumeRatio: volumes[volumes.length - 1] / avgVol20,
            score: score,
            target5: close * 1.05,
            target10: close * 1.10,
            stop: close * (1 - stopPct / 100),
            trend: trend,
            breakout: breakout,
            volumeOk: volumeOk
        });
    } catch (e) {
        skipped++;
    }
}

results.sort(function(a, b) {
    if (b.score !== a.score) return b.score - a.score;
    if (b.roc10 !== a.roc10) return b.roc10 - a.roc10;
    return b.volumeRatio - a.volumeRatio;
});

var outPath = fso.BuildPath(outputDir, "technical_entry_exit.csv");
var out = fso.CreateTextFile(outPath, true, true);
out.WriteLine("Rank,Symbol,SignalDate,ReferenceEntry,Target5,Target10,StopLoss,SMA20,SMA50,ROC10,RSI14,ATR14,VolumeRatio,Score");

var alert = "";
var shown = Math.min(5, results.length);

for (var k = 0; k < shown; k++) {
    var x = results[k];
    out.WriteLine(
        (k + 1) + "," + x.symbol + "," + x.date + "," +
        fmt(x.close) + "," + fmt(x.target5) + "," + fmt(x.target10) + "," +
        fmt(x.stop) + "," + fmt(x.sma20) + "," + fmt(x.sma50) + "," +
        fmt(x.roc10) + "," + fmt(x.rsi14) + "," + fmt(x.atr14) + "," +
        fmt(x.volumeRatio) + "," + x.score
    );

    alert += (k + 1) + ". " + x.symbol +
        " Entry " + fmt(x.close) +
        " T5 " + fmt(x.target5) +
        " T10 " + fmt(x.target10) +
        " SL " + fmt(x.stop) +
        " Score " + x.score + "\n";
}
out.Close();

if (shown === 0) {
    alert = "วันนี้ยังไม่มีหุ้นที่ผ่าน Technical Entry criteria";
}

var alertPath = fso.BuildPath(outputDir, "technical_alert.txt");
var af = fso.CreateTextFile(alertPath, true, true);
af.WriteLine("Technical Swing Screener");
af.WriteLine("Candidates: " + candidates.length);
af.WriteLine("Qualified: " + results.length);
af.WriteLine("Skipped/no history: " + skipped);
af.WriteLine("");
af.Write(alert);
af.Close();

WScript.Echo("TECHNICAL SCREENER OK");
WScript.Echo("Candidates : " + candidates.length);
WScript.Echo("Qualified  : " + results.length);
WScript.Echo("Top shown  : " + shown);
WScript.Echo("Output     : " + outPath);
