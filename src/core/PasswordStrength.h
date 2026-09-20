#pragma once
#include <QString>

namespace mp {

struct StrengthResult {
    double entropyBits = 0;   // estimated
    int score = 0;            // 0..4 (very weak .. very strong)
    QString label;            // "Weak", "Strong"...
    QString warning;          // optional advice
};

// Lightweight zxcvbn-inspired estimator: character-class entropy with
// penalties for repeats, sequences, keyboard walks and common words.
StrengthResult estimateStrength(const QString& password);

}  // namespace mp
