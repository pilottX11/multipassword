#pragma once
// Read-only display of one item field in the detail pane:
//   small grey label, value below, action buttons (reveal / copy / open) on
//   the right - as in the reference design's "Password  ••••••••  👁 ⧉".

#include <QWidget>

#include "core/Item.h"

class QLabel;
class QToolButton;
class QTimer;
class QGridLayout;

namespace mp {

class ClipboardManager;

class FieldRow : public QWidget {
    Q_OBJECT
public:
    FieldRow(const FieldDef& def, const QString& value, ClipboardManager* clipboard,
             QWidget* parent = nullptr);
    ~FieldRow() override;

    void setRevealed(bool revealed);
    bool isRevealed() const { return m_revealed; }

signals:
    void openUrlRequested(const QString& url);

private:
    void rebuildValue();
    void copyValue();
    void updateTotp();
    QString maskedText() const;

    FieldDef m_def;
    QString m_value;
    ClipboardManager* m_clipboard;
    bool m_revealed = false;
    QLabel* m_valueLabel = nullptr;
    QWidget* m_seedGrid = nullptr;
    QGridLayout* m_seedLayout = nullptr;
    QToolButton* m_revealBtn = nullptr;
    QToolButton* m_copyBtn = nullptr;
    QToolButton* m_openBtn = nullptr;
    QLabel* m_totpTimer = nullptr;
    QTimer* m_timer = nullptr;
    QString m_totpCode;
};

}  // namespace mp
