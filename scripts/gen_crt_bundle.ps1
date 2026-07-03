param(
    [Parameter(Mandatory = $true)]
    [string] $InputPem,

    [Parameter(Mandatory = $true)]
    [string] $OutputBin
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-U16BE {
    param(
        [System.IO.BinaryWriter] $Writer,
        [int] $Value
    )

    $Writer.Write([byte](($Value -shr 8) -band 0xff))
    $Writer.Write([byte]($Value -band 0xff))
}

function Join-Bytes {
    param([object[]] $Parts)

    $length = 0
    foreach ($part in $Parts) {
        $part = [byte[]] $part
        if ($null -ne $part) { $length += $part.Length }
    }

    $result = New-Object byte[] $length
    $offset = 0
    foreach ($part in $Parts) {
        $part = [byte[]] $part
        if ($null -eq $part) { continue }
        [Array]::Copy($part, 0, $result, $offset, $part.Length)
        $offset += $part.Length
    }

    return $result
}

function Encode-DerLength {
    param([int] $Length)

    if ($Length -lt 128) {
        return [byte[]] @($Length)
    }

    $bytes = New-Object System.Collections.Generic.List[byte]
    $value = $Length
    while ($value -gt 0) {
        $bytes.Insert(0, [byte]($value -band 0xff))
        $value = $value -shr 8
    }

    $result = New-Object byte[] ($bytes.Count + 1)
    $result[0] = [byte](0x80 -bor $bytes.Count)
    [Array]::Copy($bytes.ToArray(), 0, $result, 1, $bytes.Count)
    return $result
}

function Encode-Der {
    param(
        [byte] $Tag,
        [byte[]] $Content
    )

    return Join-Bytes @([byte[]] @($Tag), (Encode-DerLength $Content.Length), $Content)
}

function Encode-DerOid {
    param([string] $Oid)

    $parts = @($Oid.Split(".") | ForEach-Object { [int64] $_ })
    $content = New-Object System.Collections.Generic.List[byte]
    $content.Add([byte](($parts[0] * 40) + $parts[1]))

    for ($i = 2; $i -lt $parts.Count; $i++) {
        $value = $parts[$i]
        $encoded = New-Object System.Collections.Generic.List[byte]
        $encoded.Insert(0, [byte]($value -band 0x7f))
        $value = $value -shr 7

        while ($value -gt 0) {
            $encoded.Insert(0, [byte](($value -band 0x7f) -bor 0x80))
            $value = $value -shr 7
        }

        $content.AddRange($encoded)
    }

    return Encode-Der 0x06 $content.ToArray()
}

function Get-SubjectPublicKeyInfo {
    param([System.Security.Cryptography.X509Certificates.X509Certificate2] $Cert)

    $publicKey = $Cert.PublicKey
    $algorithm = Encode-Der 0x30 (Join-Bytes @(
        (Encode-DerOid $publicKey.Oid.Value),
        $publicKey.EncodedParameters.RawData
    ))
    $publicKeyBits = Encode-Der 0x03 (Join-Bytes @(
        [byte[]] @(0x00),
        $publicKey.EncodedKeyValue.RawData
    ))

    return Encode-Der 0x30 (Join-Bytes @(
        $algorithm,
        $publicKeyBits
    ))
}

$pemText = Get-Content -Raw -LiteralPath $InputPem
$matches = [regex]::Matches(
    $pemText,
    "-----BEGIN CERTIFICATE-----\s+([A-Za-z0-9+/=\r\n]+?)\s+-----END CERTIFICATE-----"
)

if ($matches.Count -eq 0) {
    throw "No PEM certificates found in $InputPem"
}

$entries = foreach ($match in $matches) {
    $certBytes = [Convert]::FromBase64String($match.Groups[1].Value)
    $cert = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($certBytes)

    try {
        $spki = Get-SubjectPublicKeyInfo $cert

        [pscustomobject]@{
            Subject = $cert.SubjectName.RawData
            PublicKey = $spki
            SortKey = [System.BitConverter]::ToString($cert.SubjectName.RawData)
        }
    } finally {
        $cert.Dispose()
    }
}

$entries = @($entries | Sort-Object SortKey)
$outputDir = Split-Path -Parent $OutputBin
if ($outputDir -and -not (Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$stream = [System.IO.File]::Open($OutputBin, [System.IO.FileMode]::Create)
try {
    $writer = [System.IO.BinaryWriter]::new($stream)
    Write-U16BE $writer $entries.Count

    foreach ($entry in $entries) {
        Write-U16BE $writer $entry.Subject.Length
        Write-U16BE $writer $entry.PublicKey.Length
        $writer.Write([byte[]] $entry.Subject)
        $writer.Write([byte[]] $entry.PublicKey)
    }
} finally {
    if ($null -ne $writer) { $writer.Dispose() }
    $stream.Dispose()
}

Write-Host "Wrote $($entries.Count) certs to $OutputBin"
