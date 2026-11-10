# Aspose.Slides for C++ in Sift (high-fidelity PowerPoint preview)

Sift's native Qt presentation tabs can use **Aspose.Slides for C++** as an
offline rendering backend. When enabled, this handles **.ppt** and **.pptx**
slide images using an established presentation engine rather than Sift's
incomplete OOXML drawing code. It preserves the original file and displays
images through the existing thumbnail/zoom controls.

## Important licensing distinction

Aspose.Slides.Cpp **is commercial software**. The evaluation package adds
watermarks to rendered slides. A purchased or temporary license is required
to evaluate accurate, watermark-free output. Review the Aspose license and
redistribution rights before publishing a Sift installer. The repository
contains **no SDK binaries, proprietary license file, or license key**.

Official sources:
- https://docs.aspose.com/slides/cpp/installation/
- https://docs.aspose.com/slides/cpp/licensing/
- https://docs.aspose.com/slides/cpp/convert-slide/
- https://www.nuget.org/packages/Aspose.Slides.Cpp

## Build (Windows x64 / MSVC)

The pinned SDK version is **26.9.0**. The SDK consists of two official NuGet
packages, `Aspose.Slides.Cpp` and
`CodePorting.Translator.Cs2Cpp.Framework`. It does *not* require an
end-user Microsoft Office or LibreOffice installation.

Run from PowerShell after reading the vendor license:

```powershell
./scripts/setup-aspose.ps1 -AcceptAsposeLicense -QtPrefix "C:/Qt/6.7.3/msvc2019_64"
```

This downloads the build-time SDK into the gitignored `build/aspose-sdk`
folder, configures the release build with `SIFT_USE_ASPOSE=ON`, and builds
Sift. On a machine where Qt's CMake prefix is already configured, the
`-QtPrefix` option can be omitted.

Alternatively, set these CMake options manually:
- `SIFT_USE_ASPOSE=ON`
- `ASPOSE_SLIDES_INCLUDE_DIR`: directory containing `DOM/Presentation.h`
- `ASPOSE_SLIDES_LIBRARY`: absolute path to the x64 Aspose import `.lib`
- `ASPOSE_SLIDES_EXTRA_INCLUDE_DIRS`: CodePorting `build/native/include`
- `ASPOSE_SLIDES_EXTRA_LIBS`: CodePorting x64 import `.lib`
- `ASPOSE_RUNTIME_DLLS`: paths to the two release runtime DLLs

Aspose C++ can render both legacy `.ppt` and modern `.pptx`. The existing
`PptxDocument` is retained as the basic fallback when compiling without SDK,
so normal development builds and existing Qt PDF functionality still work.
A build without the SDK **does not** get Aspose's improved rendering.

## License at runtime

For local evaluation with a valid license, set the environment variable
`SIFT_ASPOSE_LICENSE` to the full path of your signed license file **before
launching Sift**. License validation happens offline once per process.
Do not commit this file or expose it in CI logs. Without it, the Aspose
evaluation watermark is expected on rendered slides.

## Deployment: one app, no separately installed reader

A production Sift installer must package:
1. `Sift.exe` and its Qt runtime/plugins (e.g. via `windeployqt`).
2. `Aspose.Slides_vc14x64.dll`.
3. `codeporting.translator.cs2cpp.framework_vc14x64.dll`.
4. Other runtime dependencies required by the chosen MSVC/Qt distribution.
5. Vendor license activation in a manner permitted by the purchased terms.

The CMake Aspose-enabled build copies the two supplied runtime DLLs to the
executable directory; final installer creation and redistribution rights must
be verified separately. **Users should never be prompted to install a
converter, PowerPoint, LibreOffice, or Aspose SDK.**

## Tests and limitations

GitHub Actions includes an SDK-enabled Windows build and a smoke test for
PPTX rendering and a generated legacy PPT. Test runs use evaluation mode:
watermarks are expected. A smoke test is *not* pixel-perfect compatibility
certification.

Some slide output can still differ from Microsoft PowerPoint when fonts are
missing or when the file uses exotic effects. The current synchronous
thumbnail renderer can also be slow for complex or large decks; isolating
rendering in a dedicated worker/process remains a production-hardening task.

Sift has not yet been tested against the original `Chapter 11.pptx` from
the user comparison; screenshots alone cannot validate its compatibility.
