// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Damien Degois & Contributors

#pragma once

#include <QWidget>

class QComboBox;
class QLineEdit;
class QListWidget;

// Side panel picker: name search, category filter and a grid of every emoji
// the platform emoji font can draw, from Unicode's emoji-test.txt.
class EmojiConfig : public QWidget
{
    Q_OBJECT
public:
    explicit EmojiConfig(QWidget* parent = nullptr);

    // Color emoji font shared by the picker grid and the stamp itself
    static QFont font(int pixelSize);

signals:
    void emojiChanged(const QString& emoji);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    struct Entry
    {
        QString glyph;
        QString name;
        int group;
    };
    void loadEntries();
    void refresh();
    void pick(const QString& emoji);

    QList<Entry> m_entries;
    QStringList m_groups;
    QLineEdit* m_search;
    QComboBox* m_groupBox;
    QListWidget* m_grid;
};
