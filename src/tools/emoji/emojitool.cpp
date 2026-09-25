// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Damien Degois & Contributors

#include "emojitool.h"

#include <QComboBox>
#include <QFile>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QRawFont>
#include <QTextStream>
#include <QVBoxLayout>

namespace {
// Radius offset shared with the circle counter so both stamps match in size.
const int THICKNESS_OFFSET = 15;

const QString DEFAULT_EMOJI = QStringLiteral(u"\u2705"); // check mark
const int GLYPH_PX = 22;
const int CELL_PX = 36;

int pixelSize(int toolSize)
{
    return (toolSize + THICKNESS_OFFSET) * 2;
}

QFont emojiFont(int px)
{
    QFont font;
    // One color emoji font per platform; Qt walks the list until one exists.
    font.setFamilies({ QStringLiteral("Noto Color Emoji"),
                       QStringLiteral("Apple Color Emoji"),
                       QStringLiteral("Segoe UI Emoji") });
    font.setPixelSize(px);
    return font;
}

QRect glyphRect(const QString& emoji, int toolSize, const QPoint& center)
{
    QRect rect =
      QFontMetrics(emojiFont(pixelSize(toolSize))).boundingRect(emoji);
    rect.moveCenter(center);
    return rect;
}
}

EmojiConfig::EmojiConfig(QWidget* parent)
  : QWidget(parent)
  , m_search(new QLineEdit())
  , m_groupBox(new QComboBox())
  , m_grid(new QListWidget())
{
    loadEntries();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_search->setPlaceholderText(m_entries.isEmpty()
                                   ? tr("No emoji font found")
                                   : tr("Search or paste an emoji"));
    m_search->setClearButtonEnabled(true);
    m_search->installEventFilter(this);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& t) {
        // Pasted glyph (IME picker, clipboard) rather than a name: use as is
        if (!t.isEmpty() && t.at(0).unicode() > 0x7F) {
            pick(t.trimmed());
        }
        refresh();
    });
    layout->addWidget(m_search);

    m_groupBox->addItem(tr("All"));
    m_groupBox->addItems(m_groups);
    connect(
      m_groupBox, &QComboBox::currentIndexChanged, this, &EmojiConfig::refresh);
    layout->addWidget(m_groupBox);

    m_grid->setFlow(QListView::LeftToRight);
    m_grid->setWrapping(true);
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setUniformItemSizes(true);
    m_grid->setGridSize(QSize(CELL_PX, CELL_PX));
    m_grid->setFont(emojiFont(GLYPH_PX));
    m_grid->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_grid->setEnabled(!m_entries.isEmpty());
    connect(m_grid,
            &QListWidget::itemClicked,
            this,
            [this](QListWidgetItem* i) { pick(i->text()); });
    layout->addWidget(m_grid);

    refresh();
}

void EmojiConfig::loadEntries()
{
    QFile file(QStringLiteral(":/emoji/emoji-test.txt"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QRawFont raw = QRawFont::fromFont(emojiFont(GLYPH_PX));
    QTextStream in(&file);
    int group = -1;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.startsWith(QLatin1String("# group: "))) {
            const QString name = line.mid(9);
            // "Component" holds skin tones and hair, not stampable emojis
            group = name == QLatin1String("Component")
                      ? -1
                      : (m_groups.append(name), m_groups.size() - 1);
            continue;
        }
        if (group < 0 || line.isEmpty() || line.startsWith('#') ||
            !line.contains(QLatin1String("; fully-qualified"))) {
            continue;
        }
        // "1F4A9 ; fully-qualified # <glyph> E0.6 pile of poo"
        QString glyph;
        bool supported = true;
        const QStringList hexes =
          line.section(';', 0, 0).simplified().split(' ');
        for (const QString& hex : hexes) {
            char32_t cp = hex.toUInt(nullptr, 16);
            glyph += QString::fromUcs4(&cp, 1);
            // Skin tone variants: keep the grid to base emojis
            if (cp >= 0x1F3FB && cp <= 0x1F3FF) {
                supported = false;
            }
            const bool invisible = cp == 0x200D || cp == 0xFE0F ||
                                   cp == 0x20E3 ||
                                   (cp >= 0xE0020 && cp <= 0xE007F);
            if (!invisible && !raw.supportsCharacter(cp)) {
                supported = false;
            }
        }
        if (supported) {
            m_entries.append(
              { glyph,
                line.section('#', 1).simplified().section(' ', 2),
                group });
        }
    }
}

void EmojiConfig::refresh()
{
    const QString needle = m_search->text().trimmed();
    const int group = m_groupBox->currentIndex() - 1;
    m_grid->clear();
    for (const Entry& e : m_entries) {
        const bool match = needle.isEmpty()
                             ? (group < 0 || e.group == group)
                             : e.name.contains(needle, Qt::CaseInsensitive);
        if (match) {
            auto* item = new QListWidgetItem(e.glyph);
            item->setToolTip(e.name);
            item->setTextAlignment(Qt::AlignCenter);
            m_grid->addItem(item);
        }
    }
}

void EmojiConfig::pick(const QString& emoji)
{
    emit emojiChanged(emoji);
    // A focused line edit swallows the editor shortcuts (Ctrl+C, Ctrl+Z, ...)
    m_search->clearFocus();
}

void EmojiConfig::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_search->setFocus();
}

void EmojiConfig::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    m_search->clearFocus();
}

bool EmojiConfig::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_search && (event->type() == QEvent::ShortcutOverride ||
                            event->type() == QEvent::KeyPress)) {
        const int key = static_cast<QKeyEvent*>(event)->key();
        const bool press = event->type() == QEvent::KeyPress;
        // Both keys must not reach the editor: Return accepts the capture,
        // Escape closes it
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            if (press && m_grid->count() > 0) {
                pick(m_grid->item(0)->text());
            }
            event->accept();
            return true;
        }
        if (key == Qt::Key_Escape) {
            if (press) {
                m_search->clearFocus();
            }
            event->accept();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

EmojiTool::EmojiTool(QObject* parent)
  : AbstractTwoPointTool(parent)
  , m_emoji(DEFAULT_EMOJI)
  , m_valid(false)
{}

QIcon EmojiTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "emoji.svg");
}

QString EmojiTool::name() const
{
    return tr("Emoji");
}

QString EmojiTool::description() const
{
    return tr("Stamp an emoji");
}

QString EmojiTool::info()
{
    return QString("%1 - %2").arg(name(), m_emoji);
}

bool EmojiTool::isValid() const
{
    return m_valid;
}

CaptureTool::Type EmojiTool::type() const
{
    return CaptureTool::TYPE_EMOJI;
}

QRect EmojiTool::mousePreviewRect(const CaptureContext& context) const
{
    return glyphRect(m_emoji, context.toolSize, context.mousePos);
}

QRect EmojiTool::boundingRect() const
{
    if (!isValid()) {
        return {};
    }
    return glyphRect(m_emoji, size(), points().first);
}

QWidget* EmojiTool::configurationWidget()
{
    auto* config = new EmojiConfig();
    connect(config, &EmojiConfig::emojiChanged, this, &EmojiTool::setEmoji);
    return config;
}

CaptureTool* EmojiTool::copy(QObject* parent)
{
    auto* tool = new EmojiTool(parent);
    AbstractTwoPointTool::copyParams(this, tool);
    tool->m_emoji = m_emoji;
    tool->m_valid = m_valid;
    return tool;
}

void EmojiTool::process(QPainter& painter, const QPixmap& pixmap)
{
    Q_UNUSED(pixmap)
    painter.save();
    painter.setFont(emojiFont(pixelSize(size())));
    painter.drawText(boundingRect(), Qt::AlignCenter, m_emoji);
    painter.restore();
}

void EmojiTool::paintMousePreview(QPainter& painter,
                                  const CaptureContext& context)
{
    painter.save();
    painter.setOpacity(0.5);
    painter.setFont(emojiFont(pixelSize(context.toolSize)));
    painter.drawText(mousePreviewRect(context), Qt::AlignCenter, m_emoji);
    painter.restore();
}

void EmojiTool::drawStart(const CaptureContext& context)
{
    AbstractTwoPointTool::drawStart(context);
    m_valid = true;
}

void EmojiTool::pressed(CaptureContext& context)
{
    Q_UNUSED(context)
}

void EmojiTool::setEditMode(bool editMode)
{
    if (editMode) {
        m_emojiOld = m_emoji;
    }
    CaptureTool::setEditMode(editMode);
}

bool EmojiTool::isChanged()
{
    return m_emoji != m_emojiOld;
}

void EmojiTool::setEmoji(const QString& emoji)
{
    m_emoji = emoji;
    // Editing a placed stamp: one pick is the whole edit, commit right away.
    if (editMode()) {
        emit requestAction(REQ_COMMIT_CURRENT_TOOL);
    }
}
