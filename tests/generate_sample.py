#!/usr/bin/env python3
"""Generate a small, valid two-slide OOXML presentation for the smoke test."""
import pathlib
import sys
import zipfile

destination = pathlib.Path(sys.argv[1])
destination.parent.mkdir(parents=True, exist_ok=True)

content_types = """<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/>
  <Override PartName="/ppt/slides/slide1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>
  <Override PartName="/ppt/slides/slide2.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>
  <Override PartName="/ppt/slides/slide3.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>
  <Override PartName="/ppt/slideLayouts/slideLayout1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml"/>
  <Override PartName="/ppt/slideMasters/slideMaster1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml"/>
</Types>"""

presentation = """<?xml version="1.0" encoding="UTF-8"?>
<p:presentation xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"
                xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
  <p:sldIdLst><p:sldId id="256" r:id="rIdA"/><p:sldId id="257" r:id="rIdB"/><p:sldId id="258" r:id="rIdC"/></p:sldIdLst>
  <p:sldSz cx="12192000" cy="6858000"/>
</p:presentation>"""

relationships = """<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rIdA" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide1.xml"/>
  <Relationship Id="rIdB" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide2.xml"/>
  <Relationship Id="rIdC" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide3.xml"/>
</Relationships>"""

def slide(color, caption):
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"
       xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main">
 <p:cSld><p:spTree>
  <p:nvGrpSpPr/><p:grpSpPr/>
  <p:sp>
    <p:nvSpPr><p:cNvPr id="2" name="Test shape"/><p:cNvSpPr/><p:nvPr/></p:nvSpPr>
    <p:spPr>
      <a:xfrm><a:off x="1000000" y="1500000"/><a:ext cx="7000000" cy="3500000"/></a:xfrm>
      <a:prstGeom prst="rect"><a:avLst/></a:prstGeom>
      <a:solidFill><a:srgbClr val="{color}"/></a:solidFill>
    </p:spPr>
  </p:sp>
  <p:sp>
    <p:nvSpPr><p:cNvPr id="3" name="Title"/><p:cNvSpPr/><p:nvPr/></p:nvSpPr>
    <p:spPr><a:xfrm><a:off x="900000" y="100000"/><a:ext cx="9000000" cy="700000"/></a:xfrm></p:spPr>
    <p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:rPr sz="3000"/><a:t>{caption}</a:t></a:r></a:p></p:txBody>
  </p:sp>
 </p:spTree></p:cSld>
</p:sld>"""


# Real slide XML often holds only placeholders without a:xfrm. The actual
# coordinates are inherited from a layout or master. Exercise that path.
master = """<?xml version="1.0" encoding="UTF-8"?>
<p:sldMaster xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"
             xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main">
 <p:cSld><p:spTree>
 <p:nvGrpSpPr/><p:grpSpPr/>
 <p:sp><p:nvSpPr><p:cNvPr id="2" name="Master Title"/><p:cNvSpPr/>
   <p:nvPr><p:ph type="title"/></p:nvPr></p:nvSpPr>
  <p:spPr><a:xfrm><a:off x="700000" y="250000"/>
    <a:ext cx="9600000" cy="950000"/></a:xfrm></p:spPr>
 </p:sp>
 <p:sp><p:nvSpPr><p:cNvPr id="3" name="Master Body"/><p:cNvSpPr/>
   <p:nvPr><p:ph type="body" idx="1"/></p:nvPr></p:nvSpPr>
  <p:spPr><a:xfrm><a:off x="700000" y="2000000"/>
    <a:ext cx="10400000" cy="3200000"/></a:xfrm></p:spPr>
 </p:sp>
 </p:spTree></p:cSld>
</p:sldMaster>"""

layout = """<?xml version="1.0" encoding="UTF-8"?>
<p:sldLayout xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"
             xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main">
 <p:cSld><p:spTree>
 <p:nvGrpSpPr/><p:grpSpPr/>
 <p:sp><p:nvSpPr><p:cNvPr id="2" name="Layout Title"/><p:cNvSpPr/>
  <p:nvPr><p:ph type="title"/></p:nvPr></p:nvSpPr><p:spPr/></p:sp>
 <p:sp><p:nvSpPr><p:cNvPr id="3" name="Layout Body"/><p:cNvSpPr/>
  <p:nvPr><p:ph idx="1"/></p:nvPr></p:nvSpPr><p:spPr/></p:sp>
 </p:spTree></p:cSld>
</p:sldLayout>"""

slide3 = """<?xml version="1.0" encoding="UTF-8"?>
<p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"
       xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main">
 <p:cSld><p:spTree>
 <p:nvGrpSpPr/><p:grpSpPr/>
 <p:sp>
  <p:nvSpPr><p:cNvPr id="2" name="Title"/><p:cNvSpPr/>
   <p:nvPr><p:ph type="title"/></p:nvPr></p:nvSpPr><p:spPr/>
  <p:txBody><a:bodyPr/><a:lstStyle/><a:p>
   <a:r><a:rPr sz="3400"><a:solidFill><a:srgbClr val="B54E00"/></a:solidFill></a:rPr>
    <a:t>Newton's Second Law</a:t></a:r></a:p></p:txBody>
 </p:sp>
 <p:sp>
  <p:nvSpPr><p:cNvPr id="3" name="Body"/><p:cNvSpPr/>
   <p:nvPr><p:ph idx="1"/></p:nvPr></p:nvSpPr><p:spPr/>
  <p:txBody><a:bodyPr/><a:lstStyle/><a:p>
   <a:r><a:rPr sz="3300"><a:solidFill><a:srgbClr val="1B39D8"/></a:solidFill></a:rPr>
    <a:t>Force equals mass times acceleration</a:t></a:r></a:p></p:txBody>
 </p:sp>
 </p:spTree></p:cSld>
</p:sld>"""

layout_relationships = """<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
 <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster"
 Target="../slideMasters/slideMaster1.xml"/>
</Relationships>"""
slide3_relationships = """<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
 <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout"
 Target="../slideLayouts/slideLayout1.xml"/>
</Relationships>"""

with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("[Content_Types].xml", content_types)
    archive.writestr("ppt/presentation.xml", presentation)
    archive.writestr("ppt/_rels/presentation.xml.rels", relationships)
    archive.writestr("ppt/slides/slide1.xml", slide("FF0000", "First slide"))
    archive.writestr("ppt/slides/slide2.xml", slide("0000FF", "Second slide"))
    archive.writestr("ppt/slides/slide3.xml", slide3)
    archive.writestr("ppt/slides/_rels/slide3.xml.rels", slide3_relationships)
    archive.writestr("ppt/slideLayouts/slideLayout1.xml", layout)
    archive.writestr("ppt/slideLayouts/_rels/slideLayout1.xml.rels", layout_relationships)
    archive.writestr("ppt/slideMasters/slideMaster1.xml", master)
print(destination)
