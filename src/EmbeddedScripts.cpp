#include "EmbeddedScripts.h"

#include <fstream>
#include <string>

namespace EmbeddedScripts
{

bool WriteImportBat(const std::string& path)
{
    std::ofstream out(path);

    if (!out)
        return false;

    out << R"BAT(@echo off
setlocal

set "BASE=%~dp0"
set "LOG=%BASE%import_v4.log"

echo ============================================================>>"%LOG%"
echo [%date% %time%] START IMPORT>>"%LOG%"

cd /d "%BASE%"

tasklist /FI "IMAGENAME eq Broker.exe" | find /I "Broker.exe" >nul

if %errorlevel%==0 (
    echo [%date% %time%] Broker already running - hiding window>>"%LOG%"
) else (
    echo [%date% %time%] Starting Broker HIDDEN>>"%LOG%"

    powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Process -FilePath 'C:\Program Files (x86)\AmiBroker\Broker.exe' -WindowStyle Hidden"

    timeout /t 30 /nobreak >nul
)

echo [%date% %time%] Hiding Broker window>>"%LOG%"

powershell.exe -NoProfile -WindowStyle Hidden -Command "$sig='[DllImport(\"user32.dll\")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);'; Add-Type -MemberDefinition $sig -Name Win32ShowWindow -Namespace Win32; Get-Process Broker -ErrorAction SilentlyContinue | ForEach-Object { if ($_.MainWindowHandle -ne 0) { [Win32.Win32ShowWindow]::ShowWindow($_.MainWindowHandle,0) } }"

echo [%date% %time%] Running VBS>>"%LOG%"

C:\Windows\SysWOW64\cscript.exe //nologo "%BASE%import_set_v4.vbs" "%~1" >>"%LOG%" 2>&1

if %errorlevel%==0 (
    echo [%date% %time%] IMPORT SUCCESS>>"%LOG%"
) else (
    echo [%date% %time%] IMPORT FAILED ExitCode=%errorlevel%>>"%LOG%"
)

echo [%date% %time%] END IMPORT>>"%LOG%"
echo ============================================================>>"%LOG%"

endlocal
exit /b
)BAT";

    return true;
}


bool WriteImportVbs(const std::string& path)
{
    std::ofstream out(path);

    if (!out)
        return false;

    out << R"VBS(Option Explicit

Dim AB
Dim fso
Dim csvFile
Dim dbPath
Dim result
Dim errNumber
Dim errDescription
Dim i

If WScript.Arguments.Count < 1 Then
    WScript.Echo "ERROR: CSV path argument missing"
    WScript.Quit 1
End If

csvFile = WScript.Arguments(0)
dbPath = "C:\Program Files (x86)\AmiBroker\Data"

WScript.Echo "CSV     : " & csvFile
WScript.Echo "Database: " & dbPath

Set fso = CreateObject("Scripting.FileSystemObject")

If Not fso.FileExists(csvFile) Then
    WScript.Echo "ERROR: CSV not found"
    WScript.Quit 1
End If

' ------------------------------------------------------------
' Create 32-bit AmiBroker COM object with retry.
' This script is intentionally executed by SysWOW64\cscript.exe.
' ------------------------------------------------------------

For i = 1 To 30

    On Error Resume Next
    Err.Clear

    Set AB = CreateObject("Broker.Application")

    errNumber = Err.Number
    errDescription = Err.Description

    On Error GoTo 0

    If Not AB Is Nothing Then
        Exit For
    End If

    WScript.Echo "Waiting for Broker.Application... attempt " & i & "/30"

    If errNumber <> 0 Then
        WScript.Echo "COM error " & errNumber & ": " & errDescription
    End If

    WScript.Sleep 1000

Next

If AB Is Nothing Then
    WScript.Echo "ERROR: Cannot create Broker.Application"
    WScript.Echo "Last COM error " & errNumber & ": " & errDescription
    WScript.Quit 2
End If

WScript.Echo "Broker.Application created"

On Error Resume Next
Err.Clear
AB.Visible = False
errNumber = Err.Number
errDescription = Err.Description
On Error GoTo 0

If errNumber <> 0 Then
    WScript.Echo "WARNING: Cannot hide AmiBroker: " & errNumber & " - " & errDescription
End If

' ------------------------------------------------------------
' Load database
' ------------------------------------------------------------

On Error Resume Next
Err.Clear
result = AB.LoadDatabase(dbPath)
errNumber = Err.Number
errDescription = Err.Description
On Error GoTo 0

If errNumber <> 0 Then
    WScript.Echo "ERROR: LoadDatabase COM error " & errNumber & ": " & errDescription
    AB.Quit
    Set AB = Nothing
    WScript.Quit 3
End If

If result <> True Then
    WScript.Echo "ERROR: LoadDatabase failed. Result=" & result
    AB.Quit
    Set AB = Nothing
    WScript.Quit 4
End If

WScript.Echo "Database loaded"

WScript.Sleep 3000

' ------------------------------------------------------------
' Import CSV
' ------------------------------------------------------------

On Error Resume Next
Err.Clear
result = AB.Import(0, csvFile, "custom1.format")
errNumber = Err.Number
errDescription = Err.Description
On Error GoTo 0

If errNumber <> 0 Then
    WScript.Echo "ERROR: Import COM error " & errNumber & ": " & errDescription
    AB.Quit
    Set AB = Nothing
    WScript.Quit 5
End If

WScript.Echo "Import result: " & result

' AmiBroker Import() returns an import result code.
' Do not compare it with VBScript True (-1).
' A result of 0 is a valid successful import result.

WScript.Sleep 5000

' ------------------------------------------------------------
' Refresh
' ------------------------------------------------------------

On Error Resume Next
Err.Clear
AB.RefreshAll
errNumber = Err.Number
errDescription = Err.Description
On Error GoTo 0

If errNumber <> 0 Then
    WScript.Echo "ERROR: RefreshAll COM error " & errNumber & ": " & errDescription
    AB.Quit
    Set AB = Nothing
    WScript.Quit 7
End If

WScript.Sleep 2000

' ------------------------------------------------------------
' Save database
' ------------------------------------------------------------

On Error Resume Next
Err.Clear
AB.SaveDatabase
errNumber = Err.Number
errDescription = Err.Description
On Error GoTo 0

If errNumber <> 0 Then
    WScript.Echo "ERROR: SaveDatabase COM error " & errNumber & ": " & errDescription
    AB.Quit
    Set AB = Nothing
    WScript.Quit 8
End If

WScript.Echo "Database saved"

WScript.Sleep 3000

' ------------------------------------------------------------
' Quit AmiBroker
' ------------------------------------------------------------

On Error Resume Next
Err.Clear
AB.Quit
errNumber = Err.Number
errDescription = Err.Description
Set AB = Nothing
On Error GoTo 0

If errNumber <> 0 Then
    WScript.Echo "WARNING: AmiBroker Quit returned " & errNumber & ": " & errDescription
End If

WScript.Sleep 3000

WScript.Echo "AmiBroker import finished"
WScript.Quit 0
)VBS";

    return true;
}


bool WriteExportOhlcBat(const std::string& path)
{
    std::ofstream out(path);

    if (!out)
        return false;

    out << R"BAT(@echo off
setlocal

set "BASE=%~dp0"
set "LOG=%BASE%export_ohlc_v6.log"

echo ============================================================>>"%LOG%"
echo [%date% %time%] START EXPORT OHLC>>"%LOG%"

cd /d "%BASE%"

tasklist /FI "IMAGENAME eq Broker.exe" | find /I "Broker.exe" >nul

if %errorlevel%==0 (
    echo [%date% %time%] Broker already running - hiding window>>"%LOG%"
) else (
    echo [%date% %time%] Starting Broker HIDDEN>>"%LOG%"

    powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Process -FilePath 'C:\Program Files (x86)\AmiBroker\Broker.exe' -WindowStyle Hidden"

    timeout /t 30 /nobreak >nul
)

echo [%date% %time%] Hiding Broker window>>"%LOG%"

powershell.exe -NoProfile -WindowStyle Hidden -Command "$sig='[DllImport(\"user32.dll\")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);'; Add-Type -MemberDefinition $sig -Name Win32ShowWindow -Namespace Win32; Get-Process Broker -ErrorAction SilentlyContinue | ForEach-Object { if ($_.MainWindowHandle -ne 0) { [Win32.Win32ShowWindow]::ShowWindow($_.MainWindowHandle,0) } }"

echo [%date% %time%] Running VBS>>"%LOG%"

C:\Windows\SysWOW64\cscript.exe //nologo "%BASE%export_ohlc_v6.vbs" "%~1" "%~2" "%~3" >>"%LOG%" 2>&1

if %errorlevel%==0 (
    echo [%date% %time%] EXPORT SUCCESS>>"%LOG%"
) else (
    echo [%date% %time%] EXPORT FAILED ExitCode=%errorlevel%>>"%LOG%"
)

echo [%date% %time%] END EXPORT>>"%LOG%"
echo ============================================================>>"%LOG%"

endlocal
exit /b
)BAT";

    return true;
}


bool WriteExportOhlcVbs(const std::string& path)
{
    std::ofstream out(path);

    if (!out)
        return false;

    out << R"VBS(Option Explicit

' Arguments:
'   1) symbolsFile - text file, one ticker per line (e.g. symbols.txt)
'   2) outputCsv   - where to write symbol,date,open,high,low,close,volume
'   3) barsWanted  - how many most-recent daily bars per symbol (optional, default 90)

Dim AB
Dim fso
Dim symbolsFile
Dim outputCsv
Dim barsWanted
Dim dbPath
Dim result
Dim tsIn
Dim tsOut
Dim ticker
Dim stock
Dim quotes
Dim total
Dim startIdx
Dim i
Dim q
Dim d
Dim dateText
Dim symbolCount
Dim rowCount

On Error Resume Next

Set fso = CreateObject("Scripting.FileSystemObject")

If WScript.Arguments.Count < 2 Then
    WScript.Echo "ERROR: usage: export_ohlc_v6.vbs <symbolsFile> <outputCsv> [barsWanted]"
    WScript.Quit 1
End If

symbolsFile = WScript.Arguments(0)
outputCsv   = WScript.Arguments(1)

If WScript.Arguments.Count >= 3 Then
    barsWanted = CInt(WScript.Arguments(2))
Else
    barsWanted = 90
End If

dbPath = "C:\Program Files (x86)\AmiBroker\Data"

WScript.Echo "Symbols : " & symbolsFile
WScript.Echo "Output  : " & outputCsv
WScript.Echo "Bars    : " & barsWanted
WScript.Echo "Database: " & dbPath

If Not fso.FileExists(symbolsFile) Then
    WScript.Echo "ERROR: symbols file not found"
    WScript.Quit 1
End If

Set AB = CreateObject("Broker.Application")

If AB Is Nothing Then
    WScript.Echo "ERROR: Cannot create Broker.Application"
    WScript.Quit 1
End If

WScript.Echo "Broker.Application created"

AB.Visible = False

result = AB.LoadDatabase(dbPath)

If result <> True Then
    WScript.Echo "ERROR: LoadDatabase failed"
    AB.Quit
    WScript.Quit 1
End If

WScript.Echo "Database loaded"

WScript.Sleep 3000

Set tsIn = fso.OpenTextFile(symbolsFile, 1)
Set tsOut = fso.CreateTextFile(outputCsv, True)

tsOut.WriteLine "symbol,date,open,high,low,close,volume"

symbolCount = 0
rowCount = 0

Do While Not tsIn.AtEndOfStream

    ticker = Trim(tsIn.ReadLine)

    If Len(ticker) > 0 Then

        Set stock = AB.Stocks(ticker)

        If Not (stock Is Nothing) Then

            Set quotes = stock.Quotations

            total = quotes.Count

            If total > 0 Then

                startIdx = total - barsWanted

                If startIdx < 0 Then
                    startIdx = 0
                End If

                For i = startIdx To total - 1

                    Set q = quotes(i)

                    d = q.Date

                    dateText = Year(d) & "-" & Right("0" & Month(d), 2) & "-" & Right("0" & Day(d), 2)

                    tsOut.WriteLine ticker & "," & dateText & "," & q.Open & "," & q.High & "," & q.Low & "," & q.Close & "," & q.Volume

                    rowCount = rowCount + 1

                Next

            End If

        End If

        symbolCount = symbolCount + 1

        If symbolCount Mod 100 = 0 Then
            WScript.Echo "... " & symbolCount & " symbols processed, " & rowCount & " rows so far"
        End If

    End If

Loop

tsIn.Close
tsOut.Close

WScript.Echo "Done. Symbols processed: " & symbolCount & ", rows written: " & rowCount

AB.Quit

Set AB = Nothing

WScript.Sleep 2000

WScript.Quit 0
)VBS";

    return true;
}

}