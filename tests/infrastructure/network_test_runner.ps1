param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,
    [Parameter(Mandatory = $true)]
    [ValidateSet('TlsUntrusted', 'FixtureFailure')]
    [string]$Scenario
)

# PowerShell 7 / .NET CertificateRequest; no certificate is installed in a trust store.
# Default-keyset PFX import may create a temporary native private-key file on Windows;
# disposing the imported certificate releases its non-persisted key container.
$ErrorActionPreference = 'Stop'
$process = $null
$listener = $null
$client = $null
$stream = $null
$certificate = $null
$rsa = $null
$result = 1

function Start-Native([string[]]$NativeArguments) {
    $info = [System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($argument in $NativeArguments) {
        $info.ArgumentList.Add($argument)
    }
    $native = [System.Diagnostics.Process]::new()
    $native.StartInfo = $info
    try {
        if (-not $native.Start()) { throw 'Native test could not start' }
        return $native
    } catch {
        $native.Dispose()
        throw
    }
}

function Wait-TaskReady($Pending, [int]$TimeoutMs) {
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    while (-not $Pending.IsCompleted) {
        if ($timer.ElapsedMilliseconds -ge $TimeoutMs) {
            throw "Fixture operation exceeded ${TimeoutMs}ms"
        }
        [System.Threading.Thread]::Sleep(10)
    }
}

try {
    if ($PSVersionTable.PSVersion.Major -lt 7) {
        throw 'This test requires PowerShell 7 (pwsh), not Windows PowerShell 5'
    }
    if ($Scenario -eq 'FixtureFailure') {
        $process = Start-Native -NativeArguments @('--fixture-fail')
    } else {
        $rsa = [System.Security.Cryptography.RSA]::Create(2048)
        $request = [System.Security.Cryptography.X509Certificates.CertificateRequest]::new(
            'CN=127.0.0.1', $rsa,
            [System.Security.Cryptography.HashAlgorithmName]::SHA256,
            [System.Security.Cryptography.RSASignaturePadding]::Pkcs1)
        $san = [System.Security.Cryptography.X509Certificates.SubjectAlternativeNameBuilder]::new()
        $san.AddIpAddress([System.Net.IPAddress]::Loopback)
        $request.CertificateExtensions.Add($san.Build())
        $usage = [System.Security.Cryptography.OidCollection]::new()
        [void]$usage.Add([System.Security.Cryptography.Oid]::new('1.3.6.1.5.5.7.3.1'))
        $request.CertificateExtensions.Add(
            [System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]::new(
                $usage, $false))
        $selfSignedCertificate = $request.CreateSelfSigned(
            [System.DateTimeOffset]::UtcNow.AddMinutes(-5),
            [System.DateTimeOffset]::UtcNow.AddMinutes(10))
        try {
            $pfx = $selfSignedCertificate.Export(
                [System.Security.Cryptography.X509Certificates.X509ContentType]::Pfx,
                [string]::Empty)
        } finally {
            $selfSignedCertificate.Dispose()
        }
        try {
            $certificate = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new(
                $pfx, [string]::Empty,
                [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::DefaultKeySet)
        } finally {
            [Array]::Clear($pfx, 0, $pfx.Length)
        }
        $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
        $listener.Start(1)
        $port = $listener.LocalEndpoint.Port
        $accept = $listener.AcceptTcpClientAsync()
        $process = Start-Native -NativeArguments @('--tls-untrusted', "https://127.0.0.1:$port/")
        Wait-TaskReady $accept 5000
        $client = $accept.GetAwaiter().GetResult()
        $stream = [System.Net.Security.SslStream]::new($client.GetStream(), $false)
        $options = [System.Net.Security.SslServerAuthenticationOptions]::new()
        $options.ServerCertificate = $certificate
        $options.ClientCertificateRequired = $false
        $options.EnabledSslProtocols = [System.Security.Authentication.SslProtocols]::Tls12
        $authentication = $stream.AuthenticateAsServerAsync(
            $options, [System.Threading.CancellationToken]::None)
        Wait-TaskReady $authentication 5000
        $certificateRejected = $false
        try {
            $authentication.GetAwaiter().GetResult()
        } catch {
            # A client that rejects our untrusted chain terminates this real TLS handshake.
            if ($_.Exception.GetBaseException() -isnot
                [System.Security.Authentication.AuthenticationException] -and
                $_.Exception.GetBaseException() -isnot [System.IO.IOException]) { throw }
            $certificateRejected = $true
        }
        if (-not $certificateRejected) {
            # Schannel can finish the handshake before WinHTTP rejects the chain.
            # Check for HTTP application data instead of treating handshake success as trust.
            $incoming = [byte[]]::new(4096)
            $read = $stream.ReadAsync($incoming, 0, $incoming.Length)
            Wait-TaskReady $read 4500
            $received = 0
            try {
                $received = $read.GetAwaiter().GetResult()
            } catch {
                if ($_.Exception.GetBaseException() -isnot [System.IO.IOException]) { throw }
            }
            if ($received -gt 0) {
                # Send a complete response: never synthesize failure by closing trusted TLS.
                $body = [System.Text.Encoding]::ASCII.GetBytes(
                    "HTTP/1.1 200 OK`r`nContent-Length: 2`r`nConnection: close`r`n`r`nOK")
                $write = $stream.WriteAsync($body, 0, $body.Length)
                Wait-TaskReady $write 2000
                $write.GetAwaiter().GetResult()
                throw 'Self-signed TLS certificate was unexpectedly accepted for HTTP'
            }
        }
    }
    if (-not $process.WaitForExit(6000)) { throw 'Native test exceeded 6000ms' }
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    if ($stdout) { [Console]::Out.Write($stdout) }
    if ($stderr) { [Console]::Error.Write($stderr) }
    $expectedExit = if ($Scenario -eq 'FixtureFailure') { 1 } else { 0 }
    if ($process.ExitCode -ne $expectedExit) {
        throw "Expected native exit $expectedExit, actual $($process.ExitCode)"
    }
    if ($Scenario -eq 'FixtureFailure' -and
        -not $stderr.Contains('intentional fixture initialization failure')) {
        throw 'Native diagnostic did not report the intentional fixture failure'
    }
    $result = 0
} catch {
    [Console]::Error.WriteLine("network runner: $($_.Exception.Message)")
} finally {
    if ($stream) { $stream.Dispose() }
    if ($client) { $client.Dispose() }
    if ($listener) { $listener.Stop() }
    if ($certificate) { $certificate.Dispose() }
    if ($rsa) { $rsa.Dispose() }
    if ($process) {
        if (-not $process.HasExited) {
            $process.Kill($true)
            $process.WaitForExit()
        }
        $process.Dispose()
    }
}
exit $result
