// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Damien Degois & Contributors

#pragma once

#include "tools/abstracttwopointtool.h"

class EmojiTool : public AbstractTwoPointTool
{
    Q_OBJECT
public:
    explicit EmojiTool(QObject* parent = nullptr);

    QIcon icon(const QColor& background, bool inEditor) const override;
    QString name() const override;
    QString description() const override;
    QString info() override;
    bool isValid() const override;

    QRect mousePreviewRect(const CaptureContext& context) const override;
    QRect boundingRect() const override;

    QWidget* configurationWidget() override;
    CaptureTool* copy(QObject* parent = nullptr) override;
    void process(QPainter& painter, const QPixmap& pixmap) override;
    void paintMousePreview(QPainter& painter,
                           const CaptureContext& context) override;
    void setEmoji(const QString& emoji);

protected:
    CaptureTool::Type type() const override;

public slots:
    void drawStart(const CaptureContext& context) override;
    void pressed(CaptureContext& context) override;

private:
    QString m_emoji;
    bool m_valid;
};
