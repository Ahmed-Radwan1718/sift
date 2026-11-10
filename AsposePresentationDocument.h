#pragma once

#include <QImage>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <memory>

// Licensed high-fidelity, offline PowerPoint renderer. Supports both .ppt and
// .pptx without running PowerPoint or changing the original presentation.
// Compiled only when SIFT_WITH_ASPOSE is enabled in CMake.
class AsposePresentationDocument final
{
public:
    AsposePresentationDocument();
    ~AsposePresentationDocument();

    AsposePresentationDocument(const AsposePresentationDocument &) = delete;
    AsposePresentationDocument &operator=(const AsposePresentationDocument &) = delete;

    bool load(const QString &filePath, QString *error);
    int slideCount() const;
    QSizeF slideSize() const;
    QString slideTitle(int index) const;
    QImage renderSlide(int index, const QSize &pixelSize,
                       QString *warning = nullptr) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
