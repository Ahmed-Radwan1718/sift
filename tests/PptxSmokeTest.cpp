#include "PptxDocument.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QTemporaryFile>

#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc < 2)
        return 2;

    PptxDocument document;
    QString error;
    if (!document.load(QString::fromLocal8Bit(argv[1]), &error)) {
        std::fprintf(stderr, "Failed to open example deck: %s\n", qPrintable(error));
        return 3;
    }
    if (document.slideCount() != 3)
        return 4;
    if (document.slideTitle(0) != QStringLiteral("First slide") ||
        document.slideTitle(1) != QStringLiteral("Second slide"))
        return 5;

    QString warning;
    const QImage first = document.renderSlide(0, QSize(1000, 562), &warning);
    if (first.isNull() || !warning.isEmpty() ||
        first.pixelColor(300, 200) != QColor(QStringLiteral("#ff0000")))
        return 6;

    // Pixel-verify real glyphs, not just the presence of text in the XML.
    // This would have caught the missing title and body text in lecture decks.
    int darkGlyphPixels = 0;
    for (int y = 2; y < 85; ++y) {
        for (int x = 65; x < 800; ++x) {
            const QColor c = first.pixelColor(x, y);
            if (c.red() < 120 && c.green() < 120 && c.blue() < 120)
                ++darkGlyphPixels;
        }
    }
    if (darkGlyphPixels < 40) {
        std::fprintf(stderr, "Text title is missing: found only %d dark pixels\\n",
                     darkGlyphPixels);
        return 10;
    }

    const QImage second = document.renderSlide(1, QSize(1000, 562), &warning);
    if (second.isNull() || !warning.isEmpty() ||
        second.pixelColor(300, 200) != QColor(QStringLiteral("#0000ff")))
        return 7;

    // Verify that text-only shapes which inherit all their geometry from
    // master/layout XML are still rendered where PowerPoint places them.
    const QImage inherited = document.renderSlide(2, QSize(1000, 562), &warning);
    if (inherited.isNull() || !warning.isEmpty())
        return 11;
    int orange = 0, blue = 0;
    for (int y = 0; y < inherited.height(); ++y) {
        for (int x = 0; x < inherited.width(); ++x) {
            const QColor c = inherited.pixelColor(x, y);
            if (y < 160 && c.red() > 120 && c.red() > c.green() * 1.7 &&
                c.green() > c.blue() * 1.2)
                ++orange;
            if (y > 125 && c.blue() > 135 && c.blue() > c.red() * 1.6)
                ++blue;
        }
    }
    if (orange < 30 || blue < 30) {
        std::fprintf(stderr, "Missing inherited text: orange=%d blue=%d\\n",
                     orange, blue);
        return 12;
    }

    QTemporaryFile invalid;
    if (!invalid.open())
        return 8;
    invalid.write("not a powerpoint archive");
    invalid.flush();
    PptxDocument corrupted;
    if (corrupted.load(invalid.fileName(), &error) || error.isEmpty())
        return 9;
    return 0;
}
