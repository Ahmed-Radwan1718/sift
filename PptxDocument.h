#pragma once

#include <QImage>
#include <QSize>
#include <QSizeF>
#include <QString>

#include <memory>

// Offline, read-only PowerPoint Open XML (.pptx) loader and slide renderer.
// The archive is never extracted to disk and no executable content is run.
class PptxDocument final
{
public:
    PptxDocument();
    ~PptxDocument();

    PptxDocument(const PptxDocument &) = delete;
    PptxDocument &operator=(const PptxDocument &) = delete;

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
