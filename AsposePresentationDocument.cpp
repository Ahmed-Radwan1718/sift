#include "AsposePresentationDocument.h"

#include <DOM/Presentation.h>
#include <DOM/ISlide.h>
#include <DOM/ISlideCollection.h>
#include <DOM/ISlideSize.h>
#include <IImage.h>
#include <ImageFormat.h>
#include <Util/License.h>
#include <drawing/size.h>
#include <system/smart_ptr.h>
#include <system/string.h>

#include <QCache>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QTemporaryDir>
#include <QtGlobal>

#include <algorithm>
#include <memory>
#include <mutex>

namespace {
constexpr qint64 kMaximumFileBytes = 256LL * 1024 * 1024;
constexpr int kMaximumSlides = 2000;
constexpr int kMaximumRenderDimension = 4096;

System::String asposeString(const QString &text)
{
    return System::String::FromUtf16(text.toStdU16String());
}

struct LicenseState
{
    bool licensed = false;
    QString error;
};

const LicenseState &licenseState()
{
    static std::once_flag once;
    static LicenseState state;
    std::call_once(once, []() {
        const QString path = qEnvironmentVariable("SIFT_ASPOSE_LICENSE");
        if (path.isEmpty())
            return; // Free evaluation mode: watermarked slide images.
        const QFileInfo info(path);
        if (!info.exists() || !info.isFile()) {
            state.error = QStringLiteral(
                "Aspose license path is invalid. Check SIFT_ASPOSE_LICENSE.");
            return;
        }
        try {
            auto license = System::MakeObject<Aspose::Slides::License>();
            license->SetLicense(asposeString(info.absoluteFilePath()));
            state.licensed = license->IsLicensed();
            if (!state.licensed)
                state.error = QStringLiteral("Aspose license was not accepted.");
        } catch (...) {
            state.error = QStringLiteral("Unable to apply the Aspose license.");
        }
    });
    return state;
}
} // namespace

struct AsposePresentationDocument::Impl
{
    System::SharedPtr<Aspose::Slides::Presentation> presentation;
    QSizeF size;
    int count = 0;
    mutable QCache<QString, QImage> rendered{128 * 1024}; // KB
    bool evaluation = false;

    ~Impl()
    {
        if (presentation != nullptr) {
            try { presentation->Dispose(); } catch (...) { }
        }
    }
};

AsposePresentationDocument::AsposePresentationDocument() = default;
AsposePresentationDocument::~AsposePresentationDocument() = default;

bool AsposePresentationDocument::load(const QString &filePath, QString *error)
{
    if (error)
        error->clear();

    auto fail = [&](const QString &message) {
        m_impl.reset();
        if (error) *error = message;
        return false;
    };
    m_impl.reset();

    const QFileInfo info(filePath);
    const QString suffix = info.suffix().toLower();
    if (suffix != QLatin1String("ppt") && suffix != QLatin1String("pptx"))
        return fail(QStringLiteral("Only .ppt and .pptx presentations are supported."));

    if (!info.exists() || !info.isFile() || !info.isReadable())
        return fail(QStringLiteral("The PowerPoint file cannot be opened."));
    if (info.size() <= 0 || info.size() > kMaximumFileBytes)
        return fail(QStringLiteral("The presentation is empty or exceeds the 256 MB safety limit."));

    const LicenseState &license = licenseState();
    if (!license.error.isEmpty())
        return fail(license.error);

    try {
        auto impl = std::make_unique<Impl>();
        impl->evaluation = !license.licensed;
        impl->presentation = System::MakeObject<Aspose::Slides::Presentation>(
            asposeString(info.absoluteFilePath()));

        impl->count = impl->presentation->get_Slides()->get_Count();
        if (impl->count < 1 || impl->count > kMaximumSlides)
            return fail(QStringLiteral("The presentation has no slides or exceeds the 2,000-slide limit."));

        const auto dimensions = impl->presentation->get_SlideSize()->get_Size();
        impl->size = QSizeF(dimensions.get_Width(), dimensions.get_Height());
        if (impl->size.width() <= 0 || impl->size.height() <= 0)
            return fail(QStringLiteral("The presentation has invalid slide dimensions."));

        m_impl = std::move(impl);
        return true;
    } catch (...) {
        return fail(QStringLiteral("Aspose could not decode the presentation. "
                                   "It may be damaged or password protected."));
    }
}

int AsposePresentationDocument::slideCount() const
{
    return m_impl ? m_impl->count : 0;
}

QSizeF AsposePresentationDocument::slideSize() const
{
    return m_impl ? m_impl->size : QSizeF(16, 9);
}

QString AsposePresentationDocument::slideTitle(int index) const
{
    return index >= 0 && index < slideCount()
        ? QStringLiteral("Slide %1").arg(index + 1) : QString();
}

QImage AsposePresentationDocument::renderSlide(int index, const QSize &pixelSize,
                                               QString *warning) const
{
    if (warning) warning->clear();
    if (!m_impl || index < 0 || index >= m_impl->count ||
        pixelSize.width() < 1 || pixelSize.height() < 1 ||
        pixelSize.width() > kMaximumRenderDimension ||
        pixelSize.height() > kMaximumRenderDimension)
        return {};

    const QString cacheKey = QStringLiteral("%1:%2:%3")
        .arg(index).arg(pixelSize.width()).arg(pixelSize.height());
    if (const auto *cached = m_impl->rendered.object(cacheKey)) {
        if (warning && m_impl->evaluation)
            *warning = QStringLiteral("Aspose evaluation mode: slide output contains a watermark.");
        return *cached;
    }

    try {
        QTemporaryDir scratch;
        if (!scratch.isValid()) {
            if (warning)
                *warning = QStringLiteral("Unable to create temporary rendering workspace.");
            return {};
        }
        // Only a temporary raster is written. The source .ppt/.pptx is never
        // copied, altered, or executed. The temporary image is deleted on return.
        const QString pngPath = scratch.filePath(QStringLiteral("slide.png"));
        auto image = m_impl->presentation->get_Slide(index)->GetImage(
            System::Drawing::Size(pixelSize.width(), pixelSize.height()));
        image->Save(asposeString(pngPath), Aspose::Slides::ImageFormat::Png);
        image->Dispose();

        QImageReader reader(pngPath, "png");
        const QSize dimensions = reader.size();
        if (dimensions.isEmpty() ||
            dimensions.width() > kMaximumRenderDimension ||
            dimensions.height() > kMaximumRenderDimension)
            return {};
        const QImage rendered = reader.read();
        if (rendered.isNull()) {
            if (warning)
                *warning = QStringLiteral("Slide image could not be decoded.");
            return {};
        }
        m_impl->rendered.insert(cacheKey, new QImage(rendered),
            std::max(1, static_cast<int>(rendered.sizeInBytes() / 1024)));
        if (warning && m_impl->evaluation)
            *warning = QStringLiteral("Aspose evaluation mode: slide output contains a watermark.");
        return rendered;
    } catch (...) {
        if (warning)
            *warning = QStringLiteral("This slide could not be rendered by Aspose.");
        return {};
    }
}
