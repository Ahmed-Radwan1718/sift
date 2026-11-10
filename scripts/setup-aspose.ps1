# Developer-only setup for the official commercial Aspose.Slides C++ engine.
# End users never run this script: release installers must bundle the required DLLs.
# No SDK files or license keys are committed to the Sift repository.
param(
    [switch]$AcceptAsposeLicense,
    [string]$Version = "26.9.0",
    [string]$QtPrefix = "",
    [string]$Generator = "Ninja",
    [string]$BuildDirectory = "build-aspose"
)

$ErrorActionPreference = "Stop"
if (-not $AcceptAsposeLicense) {
    throw "Review Aspose's license terms first, then rerun with -AcceptAsposeLicense."
}

$repository = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$sdkRoot = Join-Path $repository "build/aspose-sdk"
New-Item -Path $sdkRoot -ItemType Directory -Force | Out-Null

foreach ($name in @("aspose.slides.cpp", "codeporting.translator.cs2cpp.framework")) {
    $destination = Join-Path $sdkRoot $name
    if (-not (Test-Path $destination)) {
        $packageUrl = "https://api.nuget.org/v3-flatcontainer/$name/$Version/$name.$Version.nupkg"
        $archive = Join-Path $sdkRoot "$name.zip"
        Write-Host "Downloading official build-time SDK: $name $Version"
        Invoke-WebRequest -Uri $packageUrl -OutFile $archive
        Expand-Archive -Path $archive -DestinationPath $destination
        Remove-Item $archive
    }
}

$slides = Join-Path $sdkRoot "aspose.slides.cpp/build/native"
$framework = Join-Path $sdkRoot "codeporting.translator.cs2cpp.framework/build/native"
$slidesInclude = Join-Path $slides "include/aspose.slides.cpp"
$slidesLibrary = Join-Path $slides "lib/Aspose.Slides_vc14x64.lib"
$slidesDll = Join-Path $slides "lib/Aspose.Slides_vc14x64.dll"
$frameworkInclude = Join-Path $framework "include"
$frameworkLibrary = Join-Path $framework "lib/codeporting.translator.cs2cpp.framework_vc14x64.lib"
$frameworkDll = Join-Path $framework "lib/codeporting.translator.cs2cpp.framework_vc14x64.dll"

foreach ($required in @(
    (Join-Path $slidesInclude "DOM/Presentation.h"),
    $slidesLibrary, $slidesDll,
    (Join-Path $frameworkInclude "system/string.h"),
    $frameworkLibrary, $frameworkDll
)) {
    if (-not (Test-Path $required)) {
        throw "Aspose SDK is incomplete or has a different layout: $required"
    }
}

$arguments = @(
    "-S", $repository, "-B", (Join-Path $repository $BuildDirectory),
    "-G", $Generator,
    "-DCMAKE_BUILD_TYPE=Release",
    "-DBUILD_TESTING=ON",
    "-DSIFT_USE_ASPOSE=ON",
    "-DASPOSE_SLIDES_INCLUDE_DIR=$slidesInclude",
    "-DASPOSE_SLIDES_LIBRARY=$slidesLibrary",
    "-DASPOSE_SLIDES_EXTRA_INCLUDE_DIRS=$frameworkInclude",
    "-DASPOSE_SLIDES_EXTRA_LIBS=$frameworkLibrary",
    "-DASPOSE_RUNTIME_DLLS=$slidesDll;$frameworkDll"
)
if ($QtPrefix) {
    $arguments += "-DCMAKE_PREFIX_PATH=$QtPrefix"
}

& cmake @arguments
if ($LASTEXITCODE -ne 0) { throw "Aspose-enabled CMake configure failed." }

& cmake --build (Join-Path $repository $BuildDirectory) --config Release
if ($LASTEXITCODE -ne 0) { throw "Aspose-enabled Sift build failed." }

Write-Host "Aspose-enabled Sift built successfully."
Write-Host "Run ctest --test-dir $BuildDirectory --output-on-failure for smoke tests."
Write-Host "Without a valid Aspose license the viewer shows evaluation watermarks."
