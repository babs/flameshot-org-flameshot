// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Damien Degois & Contributors

#include "emojiconfig.h"

#include <QComboBox>
#include <QFile>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QRawFont>
#include <QTextStream>
#include <QVBoxLayout>

namespace {
const int GLYPH_PX = 22;
const int CELL_PX = 36;
}

QFont EmojiConfig::font(int px)
{
    QFont result;
    // One color emoji font per platform; Qt walks the list until one exists.
    result.setFamilies({ QStringLiteral("Noto Color Emoji"),
                         QStringLiteral("Apple Color Emoji"),
                         QStringLiteral("Segoe UI Emoji") });
    result.setPixelSize(px);
    return result;
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
    m_grid->setFont(font(GLYPH_PX));
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
    QRawFont raw = QRawFont::fromFont(font(GLYPH_PX));
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
