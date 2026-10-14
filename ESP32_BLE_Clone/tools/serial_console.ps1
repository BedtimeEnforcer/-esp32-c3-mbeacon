$ErrorActionPreference = 'Stop'
$ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
Write-Host ('Available ports: ' + ($ports -join ', '))
$name = (Read-Host 'Enter port name (example COM5)').Trim().ToUpperInvariant()
if ($name -notmatch '^COM[0-9]+$') { throw 'Invalid COM port name.' }

$port = New-Object System.IO.Ports.SerialPort($name, 115200)
$port.ReadTimeout = 100
$port.WriteTimeout = 1000
$port.DtrEnable = $false
$port.RtsEnable = $false
$logPath = Join-Path $PSScriptRoot ('serial_capture_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '.txt')
$writer = $null
try {
    $port.Open()
    $writer = New-Object System.IO.StreamWriter($logPath, $false, [System.Text.Encoding]::UTF8)
    $writer.AutoFlush = $true
    Write-Host ('Connected to ' + $name + ' at 115200. Type INFO/SCAN/START/LOG and press Enter. Esc exits.')
    Write-Host ('PC session log: ' + $logPath)
    $line = ''
    while ($true) {
        $incoming = $port.ReadExisting()
        if ($incoming.Length -gt 0) {
            [Console]::Write($incoming)
            $writer.Write($incoming)
        }
        if ([Console]::KeyAvailable) {
            $key = [Console]::ReadKey($true)
            if ($key.Key -eq [ConsoleKey]::Escape) { break }
            if ($key.Key -eq [ConsoleKey]::Enter) {
                [Console]::WriteLine()
                if ($line.Length -gt 0) {
                    $port.Write($line + "`n")
                    $writer.WriteLine('> ' + $line)
                    $line = ''
                }
            } elseif ($key.Key -eq [ConsoleKey]::Backspace) {
                if ($line.Length -gt 0) {
                    $line = $line.Substring(0, $line.Length - 1)
                    [Console]::Write("`b `b")
                }
            } elseif (-not [char]::IsControl($key.KeyChar) -and $line.Length -lt 80) {
                $line += $key.KeyChar
                [Console]::Write($key.KeyChar)
            }
        }
        Start-Sleep -Milliseconds 25
    }
} catch {
    Write-Host ('Serial console error: ' + $_.Exception.Message) -ForegroundColor Red
    Write-Host 'Check the COM number and close any other serial monitor or flashing tool using it.'
} finally {
    if ($writer) { $writer.Dispose() }
    if ($port.IsOpen) { $port.Close() }
    $port.Dispose()
    Write-Host 'Serial console closed.'
    Read-Host 'Press Enter to close window' | Out-Null
}
