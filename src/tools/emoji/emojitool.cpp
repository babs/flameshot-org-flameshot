// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Damien Degois & Contributors

#include "emojitool.h"
#include "tools/emoji/emojiconfig.h"

#include <QFontMetrics>
#include <QPainter>

namespace {
// Radius offset shared with the circle counter so both stamps match in size.
const int THICKNESS_OFFSET = 15;

const QString DEFAULT_EMOJI = QStringLiteral(u"\u2705"); // check mark

int pixelSize(int toolSize)
{
    return (toolSize + THICKNESS_OFFSET) * 2;
}

QRect glyphRect(const QString& emoji, int toolSize, const QPoint& center)
{
    QRect rect =
      QFontMetrics(EmojiConfig::font(pixelSize(toolSize))).boundingRect(emoji);
    rect.moveCenter(center);
    return rect;
}
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
    return tr("Add an emoji to your capture");
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
    painter.setFont(EmojiConfig::font(pixelSize(size())));
    painter.drawText(boundingRect(), Qt::AlignCenter, m_emoji);
    painter.restore();
}

void EmojiTool::paintMousePreview(QPainter& painter,
                                  const CaptureContext& context)
{
    painter.save();
    painter.setOpacity(0.5);
    painter.setFont(EmojiConfig::font(pixelSize(context.toolSize)));
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
