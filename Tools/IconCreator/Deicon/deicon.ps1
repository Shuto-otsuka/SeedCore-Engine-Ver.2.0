# .icon (AES-256-CBC encrypted BinaryArchive wrapping a DDS) -> .dds + .png
# Reverse of the bake step; mirrors texconv.bat usage: run it inside a folder
# that holds *.icon files, or pass a folder path as the first argument.
#
#   ..\deicon.bat            # decode every *.icon in the current folder
#   deicon.bat .\Asset       # decode every *.icon in .\Asset
#
# Needs texconv.exe (sits next to this script). No other dependencies.

param([string]$Path = ".")

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$texconv = Join-Path $scriptDir "texconv.exe"

# Key seed: SC_ENCRYPTION_KEY_SEED in FoundationEngine/Prelude.h
$seed = "8e5d9cac-4c56-4513-813c-09a0f1168a83"
$key = [System.Security.Cryptography.SHA256]::Create().ComputeHash([System.Text.Encoding]::UTF8.GetBytes($seed))

# FNV-1a 32-bit hash of the field name "data" (see BinaryField in BinaryArchive.cpp).
# TextureLoader::CreateTexture always stores the image under this one field.
$dataId = [uint32]0xD872E2A5L

$files = Get-ChildItem -Path $Path -Filter *.icon -File
if (-not $files) { Write-Host "no *.icon in $Path"; exit 0 }

foreach ($f in $files)
{
    $raw = [System.IO.File]::ReadAllBytes($f.FullName)
    if ($raw.Length -lt 32) { Write-Warning "$($f.Name): too small"; continue }

    $iv = [byte[]]($raw[0..15])
    $ct = [byte[]]($raw[16..($raw.Length - 1)])

    $aes = [System.Security.Cryptography.Aes]::Create()
    $aes.Mode = [System.Security.Cryptography.CipherMode]::CBC
    $aes.Padding = [System.Security.Cryptography.PaddingMode]::PKCS7
    $aes.Key = $key
    $aes.IV = $iv
    $plain = $aes.CreateDecryptor().TransformFinalBlock($ct, 0, $ct.Length)

    # plaintext = records: [u32 id][u32 size][bytes] ...
    $payload = $null
    $off = 0
    while ($off + 8 -le $plain.Length)
    {
        $id = [System.BitConverter]::ToUInt32($plain, $off)
        $size = [System.BitConverter]::ToUInt32($plain, $off + 4)
        $off += 8
        if ($off + $size -gt $plain.Length) { break }
        if ($id -eq $dataId) { $payload = [byte[]]($plain[$off..($off + $size - 1)]) }
        $off += $size
    }
    if (-not $payload) { Write-Warning "$($f.Name): no 'data' field"; continue }

    # DynamicArray<Byte> serializes a u32 element count in front of the bytes
    if ($payload.Length -ge 4 -and [System.BitConverter]::ToUInt32($payload, 0) -eq $payload.Length - 4)
    {
        $payload = [byte[]]($payload[4..($payload.Length - 1)])
    }

    $stem = Join-Path $f.DirectoryName $f.BaseName
    $isDds = $payload.Length -ge 4 -and $payload[0] -eq 0x44 -and $payload[1] -eq 0x44 -and $payload[2] -eq 0x53 -and $payload[3] -eq 0x20

    if ($isDds)
    {
        [System.IO.File]::WriteAllBytes("$stem.dds", $payload)
        & $texconv -ft png -y -o $f.DirectoryName "$stem.dds" | Out-Null
        Write-Host "$($f.Name) -> $($f.BaseName).dds + $($f.BaseName).png"
    }
    elseif ($payload.Length -ge 8 -and $payload[0] -eq 0x89 -and $payload[1] -eq 0x50 -and $payload[2] -eq 0x4E -and $payload[3] -eq 0x47)
    {
        [System.IO.File]::WriteAllBytes("$stem.png", $payload)
        Write-Host "$($f.Name) -> $($f.BaseName).png"
    }
    else
    {
        [System.IO.File]::WriteAllBytes("$stem.bin", $payload)
        Write-Warning "$($f.Name): unknown payload, wrote $($f.BaseName).bin"
    }
}
