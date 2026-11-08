#include "PptxDocument.h"

#include "third_party/miniz/miniz.h"

#include <QBuffer>
#include <QCache>
#include <QColor>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QTextDocument>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace {
constexpr qint64 kMaxArchiveBytes = 128LL * 1024 * 1024;
constexpr mz_uint64 kMaxEntryBytes = 32ULL * 1024 * 1024;
constexpr int kMaxSlides = 500;
constexpr qreal kEmuPerInch = 914400.0;

QString localName(const QDomElement &e)
{
    return e.tagName().section(QLatin1Char(':'), -1);
}

QDomElement child(const QDomElement &e, const QString &name)
{
    for (QDomElement c = e.firstChildElement(); !c.isNull(); c = c.nextSiblingElement())
        if (localName(c) == name)
            return c;
    return {};
}

QDomElement descendant(const QDomElement &e, const QString &name)
{
    for (QDomElement c = e.firstChildElement(); !c.isNull(); c = c.nextSiblingElement()) {
        if (localName(c) == name)
            return c;
        const QDomElement found = descendant(c, name);
        if (!found.isNull())
            return found;
    }
    return {};
}

QString namespacedAttribute(const QDomElement &e, const QString &name)
{
    // For slide IDs both "id" and "r:id" can exist. The relationship is
    // namespaced; preferring the plain "id" loses the target slide.
    const QDomNamedNodeMap attrs = e.attributes();
    QString plain;
    for (int i = 0; i < attrs.length(); ++i) {
        const QDomNode a = attrs.item(i);
        if (a.nodeName().section(QLatin1Char(':'), -1) != name)
            continue;
        if (a.nodeName().contains(QLatin1Char(':')))
            return a.nodeValue();
        plain = a.nodeValue();
    }
    return plain;
}

QString partRelativeTo(const QString &owner, QString target)
{
    target.replace(QLatin1Char('\\'), QLatin1Char('/'));
    target = target.section(QLatin1Char('#'), 0, 0).section(QLatin1Char('?'), 0, 0);
    const QString combined = target.startsWith(QLatin1Char('/'))
        ? target.mid(1) : QFileInfo(owner).path() + QLatin1Char('/') + target;
    const QString normalized = QDir::cleanPath(combined);
    return normalized.startsWith(QLatin1String("ppt/")) ? normalized : QString();
}

QString relsPartFor(const QString &part)
{
    return QFileInfo(part).path() + QStringLiteral("/_rels/")
        + QFileInfo(part).fileName() + QStringLiteral(".rels");
}

QColor colorValue(const QDomElement &container,
                  const QHash<QString, QColor> &theme,
                  const QColor &fallback = QColor())
{
    QDomElement c;
    for (QDomElement it = container.firstChildElement(); !it.isNull(); it = it.nextSiblingElement()) {
        const QString kind = localName(it);
        if (kind == QLatin1String("srgbClr") || kind == QLatin1String("schemeClr") ||
            kind == QLatin1String("sysClr") || kind == QLatin1String("prstClr")) {
            c = it;
            break;
        }
    }
    if (c.isNull())
        return fallback;

    QColor color;
    const QString kind = localName(c);
    if (kind == QLatin1String("schemeClr")) {
        QString key = c.attribute(QStringLiteral("val"));
        if (key == QLatin1String("bg1")) key = QStringLiteral("lt1");
        if (key == QLatin1String("tx1")) key = QStringLiteral("dk1");
        if (key == QLatin1String("bg2")) key = QStringLiteral("lt2");
        if (key == QLatin1String("tx2")) key = QStringLiteral("dk2");
        color = theme.value(key, fallback);
    } else if (kind == QLatin1String("sysClr")) {
        color = QColor(QLatin1Char('#') + c.attribute(QStringLiteral("lastClr")));
    } else if (kind == QLatin1String("srgbClr")) {
        color = QColor(QLatin1Char('#') + c.attribute(QStringLiteral("val")));
    } else {
        const QString val = c.attribute(QStringLiteral("val"));
        color = val == QLatin1String("white") ? Qt::white
              : val == QLatin1String("black") ? Qt::black : fallback;
    }
    if (!color.isValid())
        color = fallback;
    if (!color.isValid())
        return color;

    for (QDomElement modifier = c.firstChildElement(); !modifier.isNull();
         modifier = modifier.nextSiblingElement()) {
        const QString type = localName(modifier);
        const qreal amount = modifier.attribute(QStringLiteral("val")).toDouble() / 100000.0;
        if (type == QLatin1String("alpha"))
            color.setAlphaF(std::clamp(amount, 0.0, 1.0));
        else if (type == QLatin1String("tint")) {
            color.setRedF(color.redF() + (1.0 - color.redF()) * amount);
            color.setGreenF(color.greenF() + (1.0 - color.greenF()) * amount);
            color.setBlueF(color.blueF() + (1.0 - color.blueF()) * amount);
        } else if (type == QLatin1String("shade")) {
            color.setRedF(std::clamp(color.redF() * amount, 0.0, 1.0));
            color.setGreenF(std::clamp(color.greenF() * amount, 0.0, 1.0));
            color.setBlueF(std::clamp(color.blueF() * amount, 0.0, 1.0));
        } else if (type == QLatin1String("lumMod") || type == QLatin1String("lumOff")) {
            float h = 0, sat = 0, l = 0, a = 0;
            color.getHslF(&h, &sat, &l, &a);
            l = static_cast<float>(type == QLatin1String("lumMod")
                  ? l * amount : l + amount);
            color.setHslF(h, sat, std::clamp(l, 0.0f, 1.0f), a);
        }
    }
    return color;
}

QColor solidFill(const QDomElement &element,
                 const QHash<QString, QColor> &theme,
                 const QColor &fallback = QColor())
{
    if (!child(element, QStringLiteral("noFill")).isNull())
        return {};
    const QDomElement fill = child(element, QStringLiteral("solidFill"));
    return fill.isNull() ? fallback : colorValue(fill, theme, fallback);
}

QRectF boxFromTransform(const QDomElement &xfrm, qreal sx, qreal sy)
{
    const QDomElement off = child(xfrm, QStringLiteral("off"));
    const QDomElement ext = child(xfrm, QStringLiteral("ext"));
    if (off.isNull() || ext.isNull())
        return {};
    return QRectF(off.attribute(QStringLiteral("x")).toDouble() * sx,
                  off.attribute(QStringLiteral("y")).toDouble() * sy,
                  ext.attribute(QStringLiteral("cx")).toDouble() * sx,
                  ext.attribute(QStringLiteral("cy")).toDouble() * sy);
}

QPainterPath shapePath(const QString &geometry, const QRectF &r)
{
    QPainterPath path;
    if (geometry == QLatin1String("ellipse")) {
        path.addEllipse(r);
    } else if (geometry == QLatin1String("roundRect")) {
        const qreal radius = std::min(r.width(), r.height()) * 0.1;
        path.addRoundedRect(r, radius, radius);
    } else if (geometry == QLatin1String("triangle") ||
               geometry == QLatin1String("rtTriangle")) {
        path.moveTo(r.center().x(), r.top());
        path.lineTo(r.right(), r.bottom());
        path.lineTo(r.left(), r.bottom());
        path.closeSubpath();
    } else if (geometry == QLatin1String("diamond")) {
        path.moveTo(r.center().x(), r.top());
        path.lineTo(r.right(), r.center().y());
        path.lineTo(r.center().x(), r.bottom());
        path.lineTo(r.left(), r.center().y());
        path.closeSubpath();
    } else if (geometry == QLatin1String("chevron")) {
        const qreal indent = r.width() * 0.23;
        path.moveTo(r.left(), r.top());
        path.lineTo(r.right() - indent, r.top());
        path.lineTo(r.right(), r.center().y());
        path.lineTo(r.right() - indent, r.bottom());
        path.lineTo(r.left(), r.bottom());
        path.lineTo(r.left() + indent, r.center().y());
        path.closeSubpath();
    } else if (geometry == QLatin1String("line")) {
        path.moveTo(r.topLeft());
        path.lineTo(r.bottomRight());
    } else {
        path.addRect(r);
    }
    return path;
}

QString textOf(const QDomElement &txBody)
{
    QString result;
    for (QDomElement paragraph = txBody.firstChildElement();
         !paragraph.isNull(); paragraph = paragraph.nextSiblingElement()) {
        if (localName(paragraph) != QLatin1String("p"))
            continue;
        if (!result.isEmpty())
            result += QLatin1Char(' ');
        for (QDomElement run = paragraph.firstChildElement(); !run.isNull();
             run = run.nextSiblingElement()) {
            if (localName(run) == QLatin1String("r") ||
                localName(run) == QLatin1String("fld")) {
                result += child(run, QStringLiteral("t")).text();
            }
        }
    }
    return result.trimmed();
}

QHash<QString, QColor> fallbackTheme()
{
    return {
        {QStringLiteral("dk1"), QColor(Qt::black)},
        {QStringLiteral("lt1"), QColor(Qt::white)},
        {QStringLiteral("dk2"), QColor(QStringLiteral("#44546a"))},
        {QStringLiteral("lt2"), QColor(QStringLiteral("#e7e6e6"))},
        {QStringLiteral("accent1"), QColor(QStringLiteral("#4472c4"))},
        {QStringLiteral("accent2"), QColor(QStringLiteral("#ed7d31"))},
        {QStringLiteral("accent3"), QColor(QStringLiteral("#a5a5a5"))},
        {QStringLiteral("accent4"), QColor(QStringLiteral("#ffc000"))},
        {QStringLiteral("accent5"), QColor(QStringLiteral("#5b9bd5"))},
        {QStringLiteral("accent6"), QColor(QStringLiteral("#70ad47"))}
    };
}
} // namespace

struct PptxDocument::Impl
{
    struct Slide {
        QString part;
        QDomDocument xml;
        QHash<QString, QString> rels;
        QDomDocument layout;
        QHash<QString, QString> layoutRels;
        QDomDocument master;
        QHash<QString, QString> masterRels;
        QString title;
    };

    QByteArray bytes;
    mz_zip_archive zip = {};
    bool initialized = false;
    QSizeF emuSize = QSizeF(12192000, 6858000);
    QVector<Slide> slides;
    QHash<QString, QColor> theme = fallbackTheme();
    mutable QCache<QString, QImage> imageCache{32 * 1024}; // cost in KB
    mutable qint64 xmlBytesRead = 0;

    ~Impl()
    {
        if (initialized)
            mz_zip_reader_end(&zip);
    }

    QByteArray entry(const QString &path, bool *ok = nullptr) const
    {
        if (ok) *ok = false;
        if (path.isEmpty())
            return {};
        const QByteArray utf8 = path.toUtf8();
        const int index = mz_zip_reader_locate_file(const_cast<mz_zip_archive *>(&zip),
                                                    utf8.constData(), nullptr, 0);
        if (index < 0)
            return {};

        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(const_cast<mz_zip_archive *>(&zip),
                                     static_cast<mz_uint>(index), &stat) ||
            stat.m_uncomp_size > kMaxEntryBytes)
            return {};

        size_t unpacked = 0;
        void *buffer = mz_zip_reader_extract_to_heap(
            const_cast<mz_zip_archive *>(&zip), static_cast<mz_uint>(index),
            &unpacked, 0);
        if (!buffer)
            return {};
        QByteArray data(static_cast<const char *>(buffer), static_cast<qsizetype>(unpacked));
        mz_free(buffer);
        if (ok) *ok = true;
        return data;
    }

    bool xmlPart(const QString &path, QDomDocument *doc) const
    {
        bool found = false;
        const QByteArray source = entry(path, &found);
        if (!found || source.isEmpty() ||
            xmlBytesRead + source.size() > 192LL * 1024 * 1024)
            return false;
        xmlBytesRead += source.size();
        QString msg;
        int line = 0, column = 0;
        return doc->setContent(source, false, &msg, &line, &column);
    }

    QHash<QString, QString> relationships(const QString &owner) const
    {
        QHash<QString, QString> result;
        QDomDocument xml;
        if (!xmlPart(relsPartFor(owner), &xml))
            return result;

        for (QDomElement rel = xml.documentElement().firstChildElement();
             !rel.isNull(); rel = rel.nextSiblingElement()) {
            if (localName(rel) != QLatin1String("Relationship") ||
                rel.attribute(QStringLiteral("TargetMode")) == QLatin1String("External"))
                continue;
            const QString target = partRelativeTo(owner, rel.attribute(QStringLiteral("Target")));
            if (!target.isEmpty())
                result.insert(rel.attribute(QStringLiteral("Id")), target);
        }
        return result;
    }

    QImage picture(const QString &path) const
    {
        if (auto *cached = imageCache.object(path))
            return *cached;
        const QByteArray content = entry(path);
        QBuffer buffer;
        buffer.setData(content);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        const QSize dimensions = reader.size();
        if (dimensions.isEmpty() || dimensions.width() > 12000 ||
            dimensions.height() > 12000 ||
            static_cast<qint64>(dimensions.width()) * dimensions.height() > 25000000)
            return {};
        const QImage image = reader.read();
        if (image.isNull())
            return {};
        imageCache.insert(path, new QImage(image),
                          std::max(1, static_cast<int>(image.sizeInBytes() / 1024)));
        return image;
    }

    void parseTheme(const QDomDocument &master, const QHash<QString, QString> &masterRels)
    {
        Q_UNUSED(master);
        for (auto it = masterRels.cbegin(); it != masterRels.cend(); ++it) {
            if (!it.value().startsWith(QLatin1String("ppt/theme/")))
                continue;
            QDomDocument themeXml;
            if (!xmlPart(it.value(), &themeXml))
                continue;
            const QDomElement scheme = descendant(themeXml.documentElement(),
                                                   QStringLiteral("clrScheme"));
            for (QDomElement e = scheme.firstChildElement(); !e.isNull();
                 e = e.nextSiblingElement()) {
                const QColor c = colorValue(e, theme);
                if (c.isValid())
                    theme.insert(localName(e), c);
            }
            return;
        }
    }

    void drawText(QPainter &p, const QDomElement &txBody, const QRectF &box,
                  qreal sx, qreal sy, QColor defaultColor,
                  int defaultSize = 1800) const
    {
        if (txBody.isNull() || box.width() < 2 || box.height() < 2)
            return;

        const QDomElement bodyPr = child(txBody, QStringLiteral("bodyPr"));
        const qreal left = bodyPr.attribute(QStringLiteral("lIns"),
                                            QStringLiteral("91440")).toDouble() * sx;
        const qreal right = bodyPr.attribute(QStringLiteral("rIns"),
                                             QStringLiteral("91440")).toDouble() * sx;
        const qreal top = bodyPr.attribute(QStringLiteral("tIns"),
                                           QStringLiteral("45720")).toDouble() * sy;
        const qreal bottom = bodyPr.attribute(QStringLiteral("bIns"),
                                              QStringLiteral("45720")).toDouble() * sy;
        const QRectF inner = box.adjusted(left, top, -right, -bottom);
        if (inner.width() <= 2 || inner.height() <= 2)
            return;

        const qreal fontPxPerPoint = sx * kEmuPerInch / 72.0;
        const QDomElement fit = child(bodyPr, QStringLiteral("normAutofit"));
        const qreal fitRatio = fit.isNull()
            ? 1.0 : std::clamp(fit.attribute(QStringLiteral("fontScale"),
                                             QStringLiteral("100000")).toDouble() / 100000.0,
                                0.4, 1.0);

        // Create real Qt text runs, instead of HTML with inline CSS. Imported
        // PowerPoint decks frequently mix inherited run properties, paragraph
        // defaults and explicitly colored text. The QTextCursor path preserves
        // glyphs, per-run formatting and line breaks without HTML parsing.
        QTextDocument document;
        document.setDocumentMargin(0);
        document.setDefaultFont(QFont(QStringLiteral("Arial")));

        auto populate = [&](qreal shrink) {
            document.clear();
            QTextCursor cursor(&document);
            const QDomElement listStyle = child(txBody, QStringLiteral("lstStyle"));
            bool firstParagraph = true;
            for (QDomElement paragraph = txBody.firstChildElement();
                 !paragraph.isNull(); paragraph = paragraph.nextSiblingElement()) {
                if (localName(paragraph) != QLatin1String("p"))
                    continue;
                const QDomElement pPr = child(paragraph, QStringLiteral("pPr"));
                const int level = std::clamp(
                    pPr.attribute(QStringLiteral("lvl"), QStringLiteral("0")).toInt(), 0, 8);
                const QDomElement listPPr = child(listStyle,
                    QStringLiteral("lvl%1pPr").arg(level + 1));
                const QDomElement defRPr = child(pPr, QStringLiteral("defRPr"));
                const QDomElement listRPr = child(listPPr, QStringLiteral("defRPr"));
                const QDomElement defaultRun = defRPr.isNull() ? listRPr : defRPr;

                const QString alignValue = pPr.attribute(
                    QStringLiteral("algn"), listPPr.attribute(QStringLiteral("algn")));
                QTextBlockFormat block;
                if (alignValue == QLatin1String("ctr"))
                    block.setAlignment(Qt::AlignHCenter);
                else if (alignValue == QLatin1String("r"))
                    block.setAlignment(Qt::AlignRight);
                else if (alignValue == QLatin1String("just"))
                    block.setAlignment(Qt::AlignJustify);
                else
                    block.setAlignment(Qt::AlignLeft);
                block.setTopMargin(0);
                block.setBottomMargin(0);
                block.setLeftMargin(0);
                block.setRightMargin(0);
                const QDomElement lnSpc = child(pPr, QStringLiteral("lnSpc"));
                const QDomElement pct = child(lnSpc, QStringLiteral("spcPct"));
                if (!pct.isNull()) {
                    const qreal percent = pct.attribute(QStringLiteral("val")).toDouble() / 1000.0;
                    if (percent > 20 && percent < 300)
                        block.setLineHeight(percent, QTextBlockFormat::ProportionalHeight);
                }
                if (firstParagraph) {
                    cursor.setBlockFormat(block);
                    firstParagraph = false;
                } else {
                    cursor.insertBlock(block);
                }

                auto formatFor = [&](const QDomElement &runPr) {
                    QTextCharFormat fmt;
                    const QString rawSize = runPr.attribute(
                        QStringLiteral("sz"), defaultRun.attribute(
                            QStringLiteral("sz"), QString::number(defaultSize)));
                    const int pointHundredths = rawSize.toInt();
                    const qreal px = std::clamp(pointHundredths / 100.0
                        * fontPxPerPoint * fitRatio * shrink, 6.0, 250.0);
                    fmt.setFontPixelSize(px);
                    const QDomElement latin = child(runPr, QStringLiteral("latin"));
                    const QDomElement fallbackLatin = child(defaultRun, QStringLiteral("latin"));
                    const QString face = latin.attribute(QStringLiteral("typeface"),
                        fallbackLatin.attribute(QStringLiteral("typeface"), QStringLiteral("Arial")));
                    fmt.setFontFamilies(QStringList{face});
                    const QString bold = runPr.attribute(
                        QStringLiteral("b"), defaultRun.attribute(QStringLiteral("b")));
                    const QString italic = runPr.attribute(
                        QStringLiteral("i"), defaultRun.attribute(QStringLiteral("i")));
                    fmt.setFontWeight(bold == QLatin1String("1") ? QFont::Bold : QFont::Normal);
                    fmt.setFontItalic(italic == QLatin1String("1"));
                    QColor ink = solidFill(runPr, theme);
                    if (!ink.isValid())
                        ink = solidFill(defaultRun, theme, defaultColor);
                    fmt.setForeground(ink.isValid() ? ink : QColor(Qt::black));
                    return fmt;
                };

                const QDomElement bullet = child(pPr, QStringLiteral("buChar"));
                const QDomElement fallbackBullet = child(listPPr, QStringLiteral("buChar"));
                const QDomElement chosenBullet = bullet.isNull() ? fallbackBullet : bullet;
                if (!chosenBullet.isNull() &&
                    child(pPr, QStringLiteral("buNone")).isNull()) {
                    cursor.insertText(chosenBullet.attribute(QStringLiteral("char"))
                          + QLatin1Char(' '), formatFor(defaultRun));
                }

                bool hasText = false;
                for (QDomElement run = paragraph.firstChildElement();
                     !run.isNull(); run = run.nextSiblingElement()) {
                    const QString kind = localName(run);
                    if (kind == QLatin1String("br")) {
                        cursor.insertText(QString(QChar::LineSeparator),
                                          formatFor(defaultRun));
                        hasText = true;
                        continue;
                    }
                    if (kind != QLatin1String("r") && kind != QLatin1String("fld"))
                        continue;
                    const QDomElement runPr = child(run, QStringLiteral("rPr"));
                    const QString text = child(run, QStringLiteral("t")).text();
                    if (text.isEmpty())
                        continue;
                    cursor.insertText(text, formatFor(runPr.isNull() ? defaultRun : runPr));
                    hasText = true;
                }
                if (!hasText)
                    cursor.insertText(QStringLiteral(" "), formatFor(defaultRun));
            }
            document.setTextWidth(inner.width());
        };

        qreal shrink = 1.0;
        for (int i = 0; i < 5; ++i) {
            populate(shrink);
            if (document.size().height() <= inner.height() || shrink <= 0.48)
                break;
            const qreal measuredHeight = std::max(document.size().height(), 1.0);
            shrink *= std::clamp(inner.height() / measuredHeight, 0.55, 0.94);
        }

        qreal y = inner.top();
        const QString anchor = bodyPr.attribute(QStringLiteral("anchor"));
        if (anchor == QLatin1String("ctr"))
            y += std::max(0.0, (inner.height() - document.size().height()) / 2.0);
        else if (anchor == QLatin1String("b"))
            y += std::max(0.0, inner.height() - document.size().height());

        p.save();
        p.setClipRect(inner);
        p.translate(inner.left(), y);
        document.drawContents(&p);
        p.restore();
    }

    QDomElement inheritedTransform(const QDomElement &shape,
                                   const QDomDocument *layout,
                                   const QDomDocument *master) const
    {
        const QDomElement placeholder = descendant(child(shape, QStringLiteral("nvSpPr")),
                                                    QStringLiteral("ph"));
        if (placeholder.isNull())
            return {};
        const QString index = placeholder.attribute(QStringLiteral("idx"),
                                                     QStringLiteral("0"));
        const QString type = placeholder.attribute(QStringLiteral("type"));
        for (const QDomDocument *source : {layout, master}) {
            if (!source || source->isNull())
                continue;
            const QDomElement tree = descendant(source->documentElement(),
                                                 QStringLiteral("spTree"));
            for (QDomElement candidate = tree.firstChildElement();
                 !candidate.isNull(); candidate = candidate.nextSiblingElement()) {
                if (localName(candidate) != QLatin1String("sp"))
                    continue;
                const QDomElement ph = descendant(child(candidate, QStringLiteral("nvSpPr")),
                                                  QStringLiteral("ph"));
                if (ph.isNull())
                    continue;
                if (ph.attribute(QStringLiteral("idx"), QStringLiteral("0")) != index &&
                    (type.isEmpty() || ph.attribute(QStringLiteral("type")) != type))
                    continue;
                const QDomElement xfrm = child(child(candidate, QStringLiteral("spPr")),
                                                QStringLiteral("xfrm"));
                if (!xfrm.isNull())
                    return xfrm;
            }
        }
        return {};
    }

    void drawShapes(QPainter &p, const QDomElement &shapeTree,
                    const QHash<QString, QString> &rels,
                    qreal sx, qreal sy, int *unsupported,
                    bool skipPlaceholders = false,
                    const QDomDocument *layout = nullptr,
                    const QDomDocument *master = nullptr) const
    {
        if (shapeTree.isNull())
            return;
        for (QDomElement e = shapeTree.firstChildElement(); !e.isNull();
             e = e.nextSiblingElement()) {
            const QString kind = localName(e);
            if (kind == QLatin1String("AlternateContent")) {
                // PowerPoint can wrap older-compatible pictures and shapes in
                // Markup Compatibility AlternateContent. The fallback usually
                // contains the static picture needed for an offline preview.
                QDomElement chosen = child(e, QStringLiteral("Fallback"));
                if (chosen.isNull())
                    chosen = child(e, QStringLiteral("Choice"));
                drawShapes(p, chosen, rels, sx, sy, unsupported, skipPlaceholders,
                           layout, master);
                continue;
            }
            if (kind == QLatin1String("grpSp")) {
                const QDomElement xfrm = child(child(e, QStringLiteral("grpSpPr")),
                                                QStringLiteral("xfrm"));
                const QDomElement off = child(xfrm, QStringLiteral("off"));
                const QDomElement ext = child(xfrm, QStringLiteral("ext"));
                const QDomElement chOff = child(xfrm, QStringLiteral("chOff"));
                const QDomElement chExt = child(xfrm, QStringLiteral("chExt"));
                const qreal childW = chExt.attribute(QStringLiteral("cx")).toDouble();
                const qreal childH = chExt.attribute(QStringLiteral("cy")).toDouble();
                p.save();
                if (childW > 0 && childH > 0) {
                    p.translate(off.attribute(QStringLiteral("x")).toDouble() * sx,
                                off.attribute(QStringLiteral("y")).toDouble() * sy);
                    p.scale(ext.attribute(QStringLiteral("cx")).toDouble() / childW,
                            ext.attribute(QStringLiteral("cy")).toDouble() / childH);
                    p.translate(-chOff.attribute(QStringLiteral("x")).toDouble() * sx,
                                -chOff.attribute(QStringLiteral("y")).toDouble() * sy);
                }
                drawShapes(p, e, rels, sx, sy, unsupported, skipPlaceholders,
                           layout, master);
                p.restore();
                continue;
            }

            if (kind == QLatin1String("sp") || kind == QLatin1String("cxnSp")) {
                if (skipPlaceholders && !descendant(child(e, QStringLiteral("nvSpPr")),
                                                    QStringLiteral("ph")).isNull())
                    continue;

                const QDomElement spPr = child(e, QStringLiteral("spPr"));
                QDomElement xfrm = child(spPr, QStringLiteral("xfrm"));
                if (xfrm.isNull())
                    xfrm = inheritedTransform(e, layout, master);
                const QRectF rect = boxFromTransform(xfrm, sx, sy);
                if (rect.width() <= 0 || rect.height() <= 0) {
                    if (!textOf(child(e, QStringLiteral("txBody"))).isEmpty())
                        ++*unsupported; // Do not silently hide unresolved text.
                    continue;
                }

                p.save();
                const qreal angle = xfrm.attribute(QStringLiteral("rot")).toDouble() / 60000.0;
                if (angle != 0.0) {
                    p.translate(rect.center());
                    p.rotate(angle);
                    p.translate(-rect.center());
                }
                if (xfrm.attribute(QStringLiteral("flipH")) == QLatin1String("1") ||
                    xfrm.attribute(QStringLiteral("flipV")) == QLatin1String("1")) {
                    p.translate(rect.center());
                    p.scale(xfrm.attribute(QStringLiteral("flipH")) == QLatin1String("1") ? -1 : 1,
                            xfrm.attribute(QStringLiteral("flipV")) == QLatin1String("1") ? -1 : 1);
                    p.translate(-rect.center());
                }

                const QDomElement geometry = child(spPr, QStringLiteral("prstGeom"));
                const QString preset = kind == QLatin1String("cxnSp")
                    ? QStringLiteral("line")
                    : geometry.attribute(QStringLiteral("prst"), QStringLiteral("rect"));
                const QColor fill = solidFill(spPr, theme);
                const QDomElement ln = child(spPr, QStringLiteral("ln"));
                const QColor outline = solidFill(ln, theme, QColor(QStringLiteral("#555555")));
                const qreal lineWidth = ln.attribute(QStringLiteral("w"),
                                                     QStringLiteral("12700")).toDouble()
                                        * ((sx + sy) / 2.0);
                p.setBrush(fill.isValid() ? QBrush(fill) : Qt::NoBrush);
                p.setPen(ln.isNull() || !outline.isValid()
                             ? QPen(Qt::NoPen)
                             : QPen(outline, std::max(0.5, lineWidth)));
                if (fill.isValid() || !ln.isNull())
                    p.drawPath(shapePath(preset, rect));

                const QDomElement text = child(e, QStringLiteral("txBody"));
                const QDomElement ph = descendant(child(e, QStringLiteral("nvSpPr")),
                                                   QStringLiteral("ph"));
                const QString type = ph.attribute(QStringLiteral("type"));
                const int defaultSize = type == QLatin1String("title") ||
                                        type == QLatin1String("ctrTitle") ? 3600 : 1800;
                drawText(p, text, rect, sx, sy,
                         theme.value(QStringLiteral("dk1"), Qt::black), defaultSize);
                p.restore();
                continue;
            }

            if (kind == QLatin1String("pic")) {
                const QDomElement xfrm = child(child(e, QStringLiteral("spPr")),
                                                QStringLiteral("xfrm"));
                const QRectF rect = boxFromTransform(xfrm, sx, sy);
                const QDomElement blipFill = child(e, QStringLiteral("blipFill"));
                const QDomElement blip = descendant(blipFill, QStringLiteral("blip"));
                const QString target = rels.value(namespacedAttribute(blip, QStringLiteral("embed")));
                const QImage image = picture(target);
                if (rect.isEmpty() || image.isNull()) {
                    ++*unsupported;
                    continue;
                }
                QRectF source(0, 0, image.width(), image.height());
                const QDomElement crop = child(blipFill, QStringLiteral("srcRect"));
                if (!crop.isNull()) {
                    const qreal left = crop.attribute(QStringLiteral("l")).toDouble() / 100000.0;
                    const qreal right = crop.attribute(QStringLiteral("r")).toDouble() / 100000.0;
                    const qreal top = crop.attribute(QStringLiteral("t")).toDouble() / 100000.0;
                    const qreal bottom = crop.attribute(QStringLiteral("b")).toDouble() / 100000.0;
                    source = source.adjusted(image.width() * left, image.height() * top,
                                             -image.width() * right, -image.height() * bottom);
                }
                if (source.width() <= 0 || source.height() <= 0)
                    continue;
                p.save();
                const qreal angle = xfrm.attribute(QStringLiteral("rot")).toDouble() / 60000.0;
                p.translate(rect.center());
                p.rotate(angle);
                p.scale(xfrm.attribute(QStringLiteral("flipH")) == QLatin1String("1") ? -1 : 1,
                        xfrm.attribute(QStringLiteral("flipV")) == QLatin1String("1") ? -1 : 1);
                p.translate(-rect.center());
                p.drawImage(rect, image, source);
                p.restore();
                continue;
            }

            if (kind == QLatin1String("graphicFrame")) {
                const QDomElement xfrm = child(e, QStringLiteral("xfrm"));
                const QRectF rect = boxFromTransform(xfrm, sx, sy);
                const QDomElement table = descendant(e, QStringLiteral("tbl"));
                if (table.isNull()) {
                    ++*unsupported; // Chart, SmartArt, embedded OLE object, etc.
                    continue;
                }
                const QDomElement grid = child(table, QStringLiteral("tblGrid"));
                QVector<qreal> columnWidths;
                for (QDomElement col = grid.firstChildElement(); !col.isNull();
                     col = col.nextSiblingElement()) {
                    if (localName(col) == QLatin1String("gridCol"))
                        columnWidths.append(col.attribute(QStringLiteral("w")).toDouble() * sx);
                }
                qreal y = rect.top();
                for (QDomElement row = table.firstChildElement(); !row.isNull();
                     row = row.nextSiblingElement()) {
                    if (localName(row) != QLatin1String("tr"))
                        continue;
                    const qreal rowHeight = row.attribute(QStringLiteral("h")).toDouble() * sy;
                    qreal x = rect.left();
                    int col = 0;
                    for (QDomElement cell = row.firstChildElement(); !cell.isNull();
                         cell = cell.nextSiblingElement()) {
                        if (localName(cell) != QLatin1String("tc"))
                            continue;
                        const qreal w = col < columnWidths.size()
                            ? columnWidths.at(col) : rect.width() / std::max(1, static_cast<int>(columnWidths.size()));
                        const QRectF cellRect(x, y, w, rowHeight);
                        const QColor fill = solidFill(child(cell, QStringLiteral("tcPr")), theme,
                                                       QColor(Qt::white));
                        p.fillRect(cellRect, fill);
                        p.setPen(QPen(QColor(QStringLiteral("#cccccc")), 0.75));
                        p.drawRect(cellRect);
                        drawText(p, child(cell, QStringLiteral("txBody")), cellRect,
                                 sx, sy, theme.value(QStringLiteral("dk1"), Qt::black));
                        x += w;
                        ++col;
                    }
                    y += rowHeight;
                }
            }
        }
    }

    QColor background(const QDomDocument &xml) const
    {
        const QDomElement bg = descendant(xml.documentElement(), QStringLiteral("bg"));
        if (bg.isNull())
            return {};
        const QDomElement bgPr = child(bg, QStringLiteral("bgPr"));
        if (!bgPr.isNull())
            return solidFill(bgPr, theme);
        const QDomElement bgRef = child(bg, QStringLiteral("bgRef"));
        return colorValue(bgRef, theme);
    }
};

PptxDocument::PptxDocument() = default;
PptxDocument::~PptxDocument() = default;

bool PptxDocument::load(const QString &filePath, QString *error)
{
    auto fail = [&](const QString &message) {
        if (error) *error = message;
        return false;
    };
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("This PowerPoint file could not be opened."));
    if (file.size() <= 0 || file.size() > kMaxArchiveBytes)
        return fail(QStringLiteral("This presentation is empty or exceeds the 128 MB safety limit."));

    auto doc = std::make_unique<Impl>();
    doc->bytes = file.readAll();
    if (doc->bytes.size() != file.size() ||
        !mz_zip_reader_init_mem(&doc->zip, doc->bytes.constData(),
                                static_cast<size_t>(doc->bytes.size()), 0))
        return fail(QStringLiteral("This is not a valid .pptx archive."));
    doc->initialized = true;

    QDomDocument presentation;
    if (!doc->xmlPart(QStringLiteral("ppt/presentation.xml"), &presentation))
        return fail(QStringLiteral("PowerPoint content is missing or invalid."));

    const QDomElement size = descendant(presentation.documentElement(), QStringLiteral("sldSz"));
    if (!size.isNull()) {
        const qreal cx = size.attribute(QStringLiteral("cx")).toDouble();
        const qreal cy = size.attribute(QStringLiteral("cy")).toDouble();
        if (cx > 0 && cy > 0)
            doc->emuSize = QSizeF(cx, cy);
    }

    const auto rels = doc->relationships(QStringLiteral("ppt/presentation.xml"));
    const QDomElement slideList = descendant(presentation.documentElement(),
                                             QStringLiteral("sldIdLst"));
    for (QDomElement item = slideList.firstChildElement(); !item.isNull();
         item = item.nextSiblingElement()) {
        if (localName(item) != QLatin1String("sldId"))
            continue;
        if (doc->slides.size() >= kMaxSlides)
            return fail(QStringLiteral("Presentations over 500 slides aren't supported."));
        const QString part = rels.value(namespacedAttribute(item, QStringLiteral("id")));
        if (part.isEmpty())
            return fail(QStringLiteral("A presentation slide is missing."));
        Impl::Slide slide;
        slide.part = part;
        if (!doc->xmlPart(part, &slide.xml))
            return fail(QStringLiteral("A slide is damaged or cannot be read."));
        slide.rels = doc->relationships(part);
        for (auto it = slide.rels.cbegin(); it != slide.rels.cend(); ++it) {
            if (it.value().startsWith(QLatin1String("ppt/slideLayouts/"))) {
                if (doc->xmlPart(it.value(), &slide.layout)) {
                    slide.layoutRels = doc->relationships(it.value());
                    for (auto m = slide.layoutRels.cbegin();
                         m != slide.layoutRels.cend(); ++m) {
                        if (m.value().startsWith(QLatin1String("ppt/slideMasters/"))) {
                            if (doc->xmlPart(m.value(), &slide.master)) {
                                slide.masterRels = doc->relationships(m.value());
                                doc->parseTheme(slide.master, slide.masterRels);
                            }
                            break;
                        }
                    }
                }
                break;
            }
        }
        const QDomElement tree = descendant(slide.xml.documentElement(),
                                             QStringLiteral("spTree"));
        for (QDomElement shape = tree.firstChildElement(); !shape.isNull();
             shape = shape.nextSiblingElement()) {
            if (localName(shape) != QLatin1String("sp"))
                continue;
            const QString title = textOf(child(shape, QStringLiteral("txBody")));
            if (!title.isEmpty()) {
                slide.title = title.left(100);
                break;
            }
        }
        doc->slides.append(std::move(slide));
    }
    if (doc->slides.isEmpty())
        return fail(QStringLiteral("This presentation contains no slides."));

    m_impl = std::move(doc);
    return true;
}

int PptxDocument::slideCount() const
{
    return m_impl ? m_impl->slides.size() : 0;
}

QSizeF PptxDocument::slideSize() const
{
    return m_impl ? m_impl->emuSize : QSizeF(16, 9);
}

QString PptxDocument::slideTitle(int index) const
{
    if (!m_impl || index < 0 || index >= slideCount())
        return {};
    const QString title = m_impl->slides.at(index).title;
    return title.isEmpty() ? QStringLiteral("Slide %1").arg(index + 1) : title;
}

QImage PptxDocument::renderSlide(int index, const QSize &pixelSize, QString *warning) const
{
    if (warning) warning->clear();
    if (!m_impl || index < 0 || index >= slideCount() ||
        pixelSize.isEmpty() || pixelSize.width() > 4096 || pixelSize.height() > 4096)
        return {};

    const auto &slide = m_impl->slides.at(index);
    const qreal sx = pixelSize.width() / m_impl->emuSize.width();
    const qreal sy = pixelSize.height() / m_impl->emuSize.height();

    QColor bg = m_impl->background(slide.xml);
    if (!bg.isValid()) bg = m_impl->background(slide.layout);
    if (!bg.isValid()) bg = m_impl->background(slide.master);

    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(bg.isValid() ? bg : QColor(Qt::white));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    int unsupported = 0;
    if (!slide.master.isNull())
        m_impl->drawShapes(painter, descendant(slide.master.documentElement(),
                                                QStringLiteral("spTree")),
                           slide.masterRels, sx, sy, &unsupported, true);
    if (!slide.layout.isNull())
        m_impl->drawShapes(painter, descendant(slide.layout.documentElement(),
                                                QStringLiteral("spTree")),
                           slide.layoutRels, sx, sy, &unsupported, true);
    m_impl->drawShapes(painter, descendant(slide.xml.documentElement(),
                                           QStringLiteral("spTree")),
                       slide.rels, sx, sy, &unsupported, false,
                       &slide.layout, &slide.master);
    painter.end();
    if (warning && unsupported)
        *warning = QStringLiteral("%1 text, image, or advanced PowerPoint element(s) could not be rendered accurately on this slide.")
                       .arg(unsupported);
    return image;
}
