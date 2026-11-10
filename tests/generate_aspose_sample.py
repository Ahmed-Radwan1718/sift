#!/usr/bin/env python3
"""Generate a representative standards-compliant PowerPoint deck for Aspose CI.

This tool runs only on CI/developer machines; no Python dependency ships in Sift.
"""
import sys
from pptx import Presentation
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE
from pptx.util import Inches, Pt

pres = Presentation()
pres.slide_width = Inches(13.333)
pres.slide_height = Inches(7.5)

def add_slide(title, color):
    slide = pres.slides.add_slide(pres.slide_layouts[6])
    title_box = slide.shapes.add_textbox(Inches(0.8), Inches(0.3),
                                         Inches(11.5), Inches(0.75))
    paragraph = title_box.text_frame.paragraphs[0]
    run = paragraph.add_run()
    run.text = title
    run.font.size = Pt(36)
    run.font.bold = True
    run.font.color.rgb = RGBColor(170, 75, 0)

    shape = slide.shapes.add_shape(
        MSO_SHAPE.RECTANGLE, Inches(1.0), Inches(2.0),
        Inches(4.5), Inches(3.0))
    shape.fill.solid()
    shape.fill.fore_color.rgb = RGBColor(*color)
    shape.line.fill.background()

    text_box = slide.shapes.add_textbox(Inches(6.0), Inches(2.0),
                                        Inches(6.5), Inches(3.0))
    paragraph = text_box.text_frame.paragraphs[0]
    run = paragraph.add_run()
    run.text = "Force equals mass times acceleration."
    run.font.size = Pt(26)
    run.font.color.rgb = RGBColor(30, 50, 180)
    return slide

add_slide("Newton's Second Law", (230, 45, 45))
add_slide("Vector Forces", (50, 80, 235))
add_slide("Acceleration", (70, 180, 110))

pres.save(sys.argv[1])
print(sys.argv[1])
