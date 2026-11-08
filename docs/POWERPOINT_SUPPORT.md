# PowerPoint support in Sift (preview)

Sift reads modern **.pptx** files locally without Microsoft Office, LibreOffice,
an online service, or any runtime downloads. The original presentation is never
edited or copied out to an external application.

The viewer offers:
- One tab per presentation; slide thumbnails, previous/next navigation and direct slide selection.
- Keyboard navigation, page number entry, zoom, fit-width, fit-page, and Ctrl+mousewheel zoom.
- Static rendering of ordinary text boxes, basic shapes, raster images, backgrounds,
  inherited slide placeholder positions, slide themes, and many tables.
- Error messages for malformed, empty, encrypted, or oversized archives.
- Limits on archive size, individual ZIP entries, slide count, and output raster size.
- No macros, code, links or embedded objects are executed.

## Known limitations

This is **not** a complete implementation of the PowerPoint rendering engine.
It does not render animations or transitions, live media, SmartArt, embedded charts,
all custom geometry, all gradients, or advanced font/layout effects faithfully.
Some unsupported media and advanced objects are explicitly reported in the viewer;
other unsupported effects may simply render differently. Fonts must be present
on the machine for exact text layout.

The legacy binary **.ppt** format is not implemented; this preview targets .pptx.
A slide may not match Microsoft's rendering pixel-for-pixel.

For a production-grade fidelity guarantee, Sift would need a thoroughly tested,
fully bundled presentation rendering engine (and its redistributable license),
or much broader OOXML feature coverage. That decision should be based on a
corpus of actual presentations, not a promise about a small native parser.

## Dependencies and licensing

Uses the existing Qt 6 Widgets environment, Qt XML, and an embedded copy of
[miniz](https://github.com/richgel999/miniz) with its license in
`third_party/miniz/LICENSE`. End users install no additional app or converter.
CMake builds the miniz source as part of Sift, with writer APIs disabled.

## Validation

`tests/generate_sample.py` creates a small two-slide OOXML test deck.
`tests/PptxSmokeTest.cpp` validates slide extraction, slide order, RGB fills,
and a malformed-archive error. The Windows CI workflow compiles Sift and runs
those tests. This is **smoke-test coverage**, not a full compatibility suite.

Real-world release testing should include lecture slide decks of different
aspect ratios; diagrams, math, tables and charts; embedded/linked images;
templates, custom fonts, non-English text, and damaged documents.
