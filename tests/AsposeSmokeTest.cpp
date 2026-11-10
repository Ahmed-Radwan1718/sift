#include "AsposePresentationDocument.h"

#include <DOM/Presentation.h>
#include <Export/SaveFormat.h>
#include <system/smart_ptr.h>
#include <system/string.h>

#include <QApplication>
#include <QColor>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>

#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc < 2)
        return 2;

    const QString source = QString::fromLocal8Bit(argv[1]);
    AsposePresentationDocument deck;
    QString error;
    if (!deck.load(source, &error)) {
        std::fprintf(stderr, "Cannot open PPTX: %s\n", qPrintable(error));
        return 3;
    }
    if (deck.slideCount() != 3)
        return 4;

    QString warning;
    QImage first = deck.renderSlide(0, QSize(1000, 563), &warning);
    if (first.isNull()) {
        std::fprintf(stderr, "Cannot render PPTX slide: %s\n", qPrintable(warning));
        return 5;
    }

    // Exercise the legacy binary .ppt reader using the same source content,
    // converted by the SDK itself. The application never converts on open.
    QTemporaryDir temporary;
    if (!temporary.isValid())
        return 6;
    const QString pptPath = temporary.filePath(QStringLiteral("sample.ppt"));
    try {
        auto pres = System::MakeObject<Aspose::Slides::Presentation>(
            System::String::FromUtf16(source.toStdU16String()));
        pres->Save(System::String::FromUtf16(pptPath.toStdU16String()),
                   Aspose::Slides::Export::SaveFormat::Ppt);
        pres->Dispose();
    } catch (...) {
        std::fprintf(stderr, "Aspose could not generate a legacy PPT fixture\n");
        return 7;
    }
    if (!QFileInfo::exists(pptPath))
        return 8;

    AsposePresentationDocument legacy;
    if (!legacy.load(pptPath, &error)) {
        std::fprintf(stderr, "Cannot open legacy PPT: %s\n", qPrintable(error));
        return 9;
    }
    const QImage legacySlide = legacy.renderSlide(0, QSize(960, 540), &warning);
    if (legacySlide.isNull()) {
        std::fprintf(stderr, "Cannot render legacy PPT: %s\n", qPrintable(warning));
        return 10;
    }

    if (legacySlide.width() != 960 || first.width() != 1000)
        return 11;

    std::printf("Aspose evaluation backend rendered PPTX and PPT successfully.\n");
    return 0;
}
