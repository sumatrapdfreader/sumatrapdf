$ErrorActionPreference = 'Stop'

$webui = Join-Path $PSScriptRoot '..\prettysumatra\webui'
$vendor = Join-Path $webui 'vendor'
$faDir = Join-Path $vendor 'fontawesome'
$faFonts = Join-Path $faDir 'webfonts'
$interDir = Join-Path $vendor 'inter'
$interFonts = Join-Path $interDir 'fonts'

New-Item -ItemType Directory -Force -Path $faFonts | Out-Null
New-Item -ItemType Directory -Force -Path $interFonts | Out-Null

$faBase = 'https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0'
$faCssPath = Join-Path $faDir 'all.min.css'
Invoke-WebRequest -Uri "$faBase/css/all.min.css" -OutFile $faCssPath
$faCss = Get-Content -Raw $faCssPath
$faNames = @('fa-solid-900.woff2','fa-regular-400.woff2','fa-brands-400.woff2')
foreach ($name in $faNames) {
    $src = "$faBase/webfonts/$name"
    $dst = Join-Path $faFonts $name
    Invoke-WebRequest -Uri $src -OutFile $dst
}
$faCss = $faCss.Replace('../webfonts/fa-solid-900.woff2', 'webfonts/fa-solid-900.woff2')
$faCss = $faCss.Replace('../webfonts/fa-regular-400.woff2', 'webfonts/fa-regular-400.woff2')
$faCss = $faCss.Replace('../webfonts/fa-brands-400.woff2', 'webfonts/fa-brands-400.woff2')
Set-Content -Path $faCssPath -Value $faCss -Encoding utf8

$interCssPath = Join-Path $interDir 'inter.css'
Invoke-WebRequest -Uri 'https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap' -OutFile $interCssPath
$interCss = Get-Content -Raw $interCssPath
$matches = [regex]::Matches($interCss, 'https://fonts\.gstatic\.com/[^\)\s]+\.ttf')
$seen = @{}
foreach ($m in $matches) {
    $url = $m.Value
    if ($seen.ContainsKey($url)) { continue }
    $seen[$url] = $true
    $name = Split-Path $url -Leaf
    Invoke-WebRequest -Uri $url -OutFile (Join-Path $interFonts $name)
    $interCss = $interCss.Replace($url, "fonts/$name")
}
Set-Content -Path $interCssPath -Value $interCss -Encoding utf8

Write-Host 'Downloaded offline web UI assets.'
