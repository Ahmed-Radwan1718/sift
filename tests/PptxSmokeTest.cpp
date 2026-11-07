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
    if (document.slideCount() != 2)
        return 4;
    if (document.slideTitle(0) != QStringLiteral("First slide") ||
        document.slideTitle(1) != QStringLiteral("Second slide"))
        return 5;

    QString warning;
    const QImage first = document.renderSlide(0, QSize(1000, 562), &warning);
    if (first.isNull() || !warning.isEmpty() ||
        first.pixelColor(300, 200) != QColor(QStringLiteral("#ff0000")))
        return 6;

    const QImage second = document.renderSlide(1, QSize(1000, 562), &warning);
    if (second.isNull() || !warning.isEmpty() ||
        second.pixelColor(300, 200) != QColor(QStringLiteral("#0000ff")))
        return 7;

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
