# image -> .icon : bake a PNG/TGA/DDS into the encrypted .icon container the
# editor loads. Reverse of deicon.ps1; the .icon side mirrors what
# BinaryOutputArchive::Field("data", ...) + BinaryArchive::Write() produce.
#
#   ..\enicon.bat            # bake every *.png / *.tga / *.dds in the current folder
#   enicon.bat .\Asset       # bake every image in .\Asset
#
# PNG/TGA are compressed to BC7_UNORM DDS via texconv.exe first (same settings
# as texconv.bat). A sibling *.dds is used as-is. Needs texconv.exe next to this
# script; no other dependencies.

param(
    [string]$Path = ".",
    [string]$Format = "BC7_UNORM"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$texconv = Join-Path $scriptDir "texconv.exe"

# Key seed: SC_ENCRYPTION_KEY_SEED in FoundationEngine/Prelude.h
$seed = "8e5d9cac-4c56-4513-813c-09a0f1168a83"
$key = [System.Security.Cryptography.SHA256]::Create().ComputeHash([System.Text.Encoding]::UTF8.GetBytes($seed))

# FNV-1a 32-bit hash of the field name "data" (see BinaryField in BinaryArchive.cpp).
$dataId = [uint32]0xD872E2A5L

$dir = (Resolve-Path $Path).Path
$sources = Get-ChildItem -Path $dir -File | Where-Object { $_.Extension -match '^\.(png|tga|dds)$' }

# Prefer an explicit .dds over a same-named .png/.tga so we bake it only once.
$byStem = @{}
foreach ($s in $sources)
{
    $stem = $s.BaseName
    if (-not $byStem.ContainsKey($stem) -or $s.Extension -eq ".dds")
    {
        $byStem[$stem] = $s
    }
}
if ($byStem.Count -eq 0) { Write-Host "no *.png / *.tga / *.dds in $dir"; exit 0 }

foreach ($stem in $byStem.Keys)
{
    $src = $byStem[$stem]
    $ddsPath = Join-Path $dir "$stem.dds"

    if ($src.Extension -ne ".dds")
    {
        & $texconv -f $Format -m 0 -y -ft dds -o $dir $src.FullName | Out-Null
        if (-not (Test-Path $ddsPath)) { Write-Warning "$($src.Name): texconv produced no dds"; continue }
    }

    $dds = [System.IO.File]::ReadAllBytes($ddsPath)
    if ($dds.Length -lt 4 -or $dds[0] -ne 0x44 -or $dds[1] -ne 0x44 -or $dds[2] -ne 0x53 -or $dds[3] -ne 0x20)
    {
        Write-Warning "$stem.dds: not a DDS file"; continue
    }

    # plaintext = one record: [u32 dataId][u32 size][ [u32 count][dds bytes] ]
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    $bw.Write([uint32]$dataId)
    $bw.Write([uint32]($dds.Length + 4))
    $bw.Write([uint32]$dds.Length)
    $bw.Write($dds)
    $bw.Flush()
    $plain = $ms.ToArray()

    $iv = New-Object byte[] 16
    [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($iv)

    $aes = [System.Security.Cryptography.Aes]::Create()
    $aes.Mode = [System.Security.Cryptography.CipherMode]::CBC
    $aes.Padding = [System.Security.Cryptography.PaddingMode]::PKCS7
    $aes.Key = $key
    $aes.IV = $iv
    $cipher = $aes.CreateEncryptor().TransformFinalBlock($plain, 0, $plain.Length)

    $out = New-Object byte[] ($iv.Length + $cipher.Length)
    [System.Array]::Copy($iv, 0, $out, 0, $iv.Length)
    [System.Array]::Copy($cipher, 0, $out, $iv.Length, $cipher.Length)
    [System.IO.File]::WriteAllBytes((Join-Path $dir "$stem.icon"), $out)

    Write-Host "$($src.Name) -> $stem.icon"
}
