param(
    [string]$HostRoot = 'C:\Users\dove\Documents\IntroOP',
    [string]$EvidenceDirectory = '',
    [switch]$VerifyOnly
)
$ErrorActionPreference = 'Stop'
$workspaceRoot = Split-Path -Parent $PSScriptRoot
if (-not $EvidenceDirectory) { $EvidenceDirectory = Join-Path $workspaceRoot 'build\rectification\introop-ime-device' }
$EvidenceDirectory = [IO.Path]::GetFullPath($EvidenceDirectory)
New-Item -ItemType Directory -Force -Path $EvidenceDirectory | Out-Null
. (Join-Path $HostRoot 'tools\Build-Environment.ps1')
$buildEnvironment = Get-IntroOPBuildEnvironment
$configPath = if ($env:Q10DEPLOY_CONFIG) { $env:Q10DEPLOY_CONFIG } else { Join-Path $env:LOCALAPPDATA 'Q10Deploy\config.json' }
$connection = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
if ($connection.DeviceHost -notmatch '^[A-Za-z0-9.-]+$' -or $connection.SshUser -ne 'root') { throw 'Unexpected diagnostic SSH target' }
if (-not (Test-Path -LiteralPath $connection.KnownHostsPath) -or -not (Test-Path -LiteralPath $connection.SshKeyPath)) { throw 'Pinned SSH host and private key are required' }
$ssh = Join-Path $env:SystemRoot 'System32\OpenSSH\ssh.exe'
$scp = Join-Path $env:SystemRoot 'System32\OpenSSH\scp.exe'
$sshOptions = @('-F','none','-o','BatchMode=yes','-o','IdentitiesOnly=yes','-o','StrictHostKeyChecking=yes',
    '-o','HostKeyAlgorithms=+ssh-rsa','-o','PubkeyAcceptedAlgorithms=+ssh-rsa','-o','MACs=+hmac-sha1',
    '-o',"UserKnownHostsFile=$($connection.KnownHostsPath)",'-o','ConnectTimeout=8','-i',$connection.SshKeyPath)
$sshTarget = $connection.SshUser + '@' + $connection.DeviceHost
function Invoke-DiagnosticRemote([string]$Script) {
    $arguments = @($sshOptions) + @('-p',[string]$connection.SshPort,'-T',$sshTarget,'sh')
    $quoted = foreach ($argument in $arguments) { '"' + ([string]$argument -replace '(\\*)"','$1$1\"' -replace '(\\+)$','$1$1') + '"' }
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $ssh
    $start.Arguments = $quoted -join ' '
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $start
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.StandardInput.Write($Script.Replace("`r",'') + "`n")
        $process.StandardInput.Close()
        if (-not $process.WaitForExit(45000)) { $process.Kill(); throw 'Diagnostic SSH timed out' }
        $errorOutput = $stderr.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) { throw "Diagnostic SSH failed: $errorOutput" }
        return $stdout.GetAwaiter().GetResult()
    } finally { $process.Dispose() }
}
$bar = Join-Path $HostRoot 'bb10-native\build\IntroOPImeDiagnostics.bar'
[xml]$descriptor = Get-Content -LiteralPath (Join-Path $HostRoot 'bb10-native\ime-test-descriptor.xml') -Raw
$version = [string]$descriptor.qnx.versionNumber + '.' + [string]$descriptor.qnx.buildId
$digest = (Get-FileHash -LiteralPath $bar -Algorithm SHA256).Hash
function Confirm-DiagnosticResult([string]$DiagnosticIdentity) {
    if ($DiagnosticIdentity -notmatch '^top\.blaccat\.IntroOPImeDiagnostics\.[A-Za-z0-9._-]+$') { throw 'Unexpected diagnostic identity' }
    $deviceLog = Invoke-DiagnosticRemote ("cat /accounts/1000/appdata/" + $DiagnosticIdentity + "/logs/log; cat /accounts/1000/appdata/" + $DiagnosticIdentity + "/logs/devmode_exitcode.txt 2>/dev/null || true")
    $deviceLog | Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'native-diagnostic.txt') -Encoding UTF8
    $passed = $deviceLog -match 'IntroOP native diagnostic: COMPLETE PASS' -and $deviceLog -notmatch 'IntroOP native diagnostic: FAIL'
    [ordered]@{ timestamp=[DateTimeOffset]::Now.ToString('o'); version=$version; barSha256=$digest; identity=$DiagnosticIdentity;
        status=$(if ($passed) {'PASS'} else {'FAIL'}); scope='REAL_CASCADES_SCENE_AND_DIALOG_WITH_SYNTHETIC_KEYEVENTS';
        physicalKeys='NOT_RUN'; touchAndFirstFrame='NOT_RUN'; clinicalStorageAttached=$false } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'result.json') -Encoding UTF8
    Write-Output $deviceLog
    if (-not $passed) { throw 'Real native diagnostic did not reach COMPLETE PASS' }
    Write-Output 'PASS: real Q10 Cascades focus and Sym Dialog, synthetic KeyEvents only'
}
if ($VerifyOnly) {
    $receipt = Get-Content -LiteralPath (Join-Path $EvidenceDirectory 'installation.txt') -Raw
    $receiptIdentity = [regex]::Match($receipt,'(?m)^actual_dname::(top\.blaccat\.IntroOPImeDiagnostics\.[A-Za-z0-9._-]+)\r?$')
    if (-not $receiptIdentity.Success -or $receipt -notmatch ('actual_app_version::' + [regex]::Escape($version))) { throw 'No matching diagnostic installation receipt' }
    Confirm-DiagnosticResult $receiptIdentity.Groups[1].Value
    return
}
# Never interrupt an existing clinical application or recording.
$clinicalProcess = Invoke-DiagnosticRemote "pidin ar | grep '[t]op\.blaccat\.GarminSyncBB10\.testDev_minSyncBB10f88546f1' || true"
if (-not [string]::IsNullOrWhiteSpace($clinicalProcess)) { throw 'IntroOP is running; isolated diagnostic postponed until normal exit' }
$fileName = 'IntroOPImeDiagnostics-' + $version + '-' + $digest.Substring(0,12).ToLower() + '.bar'
$remoteBar = '/tmp/q10deploy/' + $fileName
Invoke-DiagnosticRemote 'set -e; mkdir -p /tmp/q10deploy' | Out-Null
& $scp -O @sshOptions -P $connection.SshPort $bar ($sshTarget + ':' + $remoteBar)
if ($LASTEXITCODE -ne 0) { throw 'Diagnostic BAR upload failed' }
$readback = Join-Path $EvidenceDirectory $fileName
& $scp -O @sshOptions -P $connection.SshPort ($sshTarget + ':' + $remoteBar) $readback
if ($LASTEXITCODE -ne 0 -or (Get-FileHash -LiteralPath $readback -Algorithm SHA256).Hash -ne $digest) { throw 'Diagnostic BAR readback hash failed' }
$installScript = @'
set -e
. /base/scripts/sudtools.sh
sud_install_package_2 __BAR__
cat /pps/system/installer/upd/current/job.__FILENAME__
'@
$installation = Invoke-DiagnosticRemote $installScript.Replace('__BAR__',$remoteBar).Replace('__FILENAME__',$fileName)
$installation | Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'installation.txt') -Encoding UTF8
$identity = [regex]::Match($installation,'(?m)^actual_dname::(top\.blaccat\.IntroOPImeDiagnostics\.[A-Za-z0-9._-]+)\r?$')
if (-not $identity.Success -or $installation -notmatch 'result::success' -or $installation -notmatch 'progress::100' -or
    $installation -notmatch ('actual_app_version::' + [regex]::Escape($version))) { throw 'Diagnostic installation identity/version was not confirmed' }
$fullName = $identity.Groups[1].Value
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class ImeDiagnosticCredentialReader {
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
  private struct Credential {
    public int Flags,Type; public IntPtr TargetName,Comment; public long LastWritten;
    public int BlobSize; public IntPtr Blob; public int Persist,AttributeCount;
    public IntPtr Attributes,TargetAlias,UserName;
  }
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
  private static extern bool CredReadW(string target,int type,int flags,out IntPtr value);
  [DllImport("advapi32.dll")] private static extern void CredFree(IntPtr value);
  public static string Read(string host) {
    IntPtr value;
    if(!CredReadW("Q10Deploy/device-password/"+host,1,0,out value))return null;
    try {
      Credential c=(Credential)Marshal.PtrToStructure(value,typeof(Credential));
      byte[] bytes=new byte[c.BlobSize]; Marshal.Copy(c.Blob,bytes,0,bytes.Length);
      try {return Encoding.Unicode.GetString(bytes);} finally {Array.Clear(bytes,0,bytes.Length);}
    } finally {CredFree(value);}
  }
}
'@
$devicePassword = if ($env:Q10DEPLOY_DEVICE_PASSWORD) { $env:Q10DEPLOY_DEVICE_PASSWORD } else { [ImeDiagnosticCredentialReader]::Read($connection.DeviceHost) }
if ([string]::IsNullOrEmpty($devicePassword)) { throw 'Diagnostic installed; no stored developer password available to launch' }
try {
    $java = if ($connection.Java7Path) { $connection.Java7Path } else { $buildEnvironment.Java }
    $deployJar = Join-Path $buildEnvironment.HostRoot 'usr/lib/BarDeploy.jar'
    $savedPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $launch = & $java '-Djava.awt.headless=true' -jar $deployJar '-launchApp' -device $connection.DeviceHost -password $devicePassword -package-fullname $fullName 2>&1
        $launchExit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $savedPreference }
    $safeLaunch = ($launch | ForEach-Object { ([string]$_).Replace($devicePassword,'[redacted]') }) -join "`n"
    $safeLaunch | Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'launch.txt') -Encoding UTF8
    if ($launchExit -ne 0) { throw 'Diagnostic SDK launch failed; see redacted launch evidence' }
} finally { $devicePassword = $null }
Start-Sleep -Seconds 5
Confirm-DiagnosticResult $fullName
