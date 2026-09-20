#pragma once
#include <QDialog>

class QLineEdit;
class QSlider;
class QSpinBox;
class QCheckBox;
class QLabel;
class QTabWidget;
class QComboBox;

namespace mp {

class PasswordGeneratorDialog : public QDialog {
    Q_OBJECT
public:
    explicit PasswordGeneratorDialog(QWidget* parent = nullptr);
    ~PasswordGeneratorDialog() override;
    QString result() const;

private:
    void regenerate();

    QTabWidget* m_tabs = nullptr;
    QLineEdit* m_output = nullptr;
    QLabel* m_strength = nullptr;
    // password tab
    QSlider* m_length = nullptr;
    QSpinBox* m_lengthSpin = nullptr;
    QCheckBox* m_lower = nullptr;
    QCheckBox* m_upper = nullptr;
    QCheckBox* m_digits = nullptr;
    QCheckBox* m_symbols = nullptr;
    QCheckBox* m_ambiguous = nullptr;
    // passphrase tab
    QSpinBox* m_words = nullptr;
    QComboBox* m_separator = nullptr;
    QCheckBox* m_capitalize = nullptr;
    QCheckBox* m_digitSuffix = nullptr;
};

}  // namespace mp
