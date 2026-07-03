# TLS Certificate Bundle

This firmware validates WSS certificates with the ESP x509 certificate bundle embedded from:

```text
data/cert/x509_crt_bundle.bin
```

The source PEM bundle is stored at:

```text
data/cert/cacrt_all.pem
```

## Refresh Certificates

Run this from the repo root to download the current Mozilla CA bundle from curl and regenerate the ESP bundle:

```powershell
.\scripts\update_certs.ps1
```

If your PowerShell policy blocks local scripts, use a process-scoped bypass:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\update_certs.ps1
```

If you already have an updated PEM file and only want to regenerate the `.bin`:

```powershell
.\scripts\update_certs.ps1 -SkipDownload
```

The firmware embeds the generated `.bin` through `board_build.embed_txtfiles` in `platformio.ini`, so devices receive certificate updates when the firmware is rebuilt and reflashed.
