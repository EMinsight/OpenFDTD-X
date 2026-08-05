// RirAutoAssign.cpp — 受音点別 RIR WAV の自動割当 (規則はヘッダ参照)
#include "RirAutoAssign.h"

#include <QDir>
#include <QFileInfo>

namespace ofd {
namespace rirauto {

QString normalizeKey(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (const QChar &c : s) {
        // 英数字のみ残す (記号・空白・'_'・全角記号などは区切り扱いで無視)。
        // 数字と英字以外の「文字」(日本語など) は isLetter で残す —
        // 「受音点A」のような名前も照合できるようにする。
        if (c.isLetterOrNumber()) out += c.toLower();
    }
    return out;
}

QVector<Assignment> assign(const QStringList &wavNames,
                           const QStringList &receiverNames,
                           const QVector<bool> &eligible)
{
    QVector<Assignment> out(receiverNames.size());
    if (eligible.size() != receiverNames.size()) return out;   // 契約違反

    // ファイル名 → 正規化キー (拡張子は除く)。空キーのファイルは照合不能
    QStringList fileKeys;
    fileKeys.reserve(wavNames.size());
    for (const QString &f : wavNames)
        fileKeys << normalizeKey(QFileInfo(f).completeBaseName());

    int nEligible = 0;
    for (bool e : eligible) if (e) ++nEligible;

    // 規則 (3) 用: キーがちょうど "rir" のファイル (rir.wav 等) の唯一性
    int rirIdx = -1, rirCount = 0;
    for (int f = 0; f < fileKeys.size(); ++f) {
        if (fileKeys.at(f) == QLatin1String("rir")) { rirIdx = f; ++rirCount; }
    }

    for (int i = 0; i < receiverNames.size(); ++i) {
        if (!eligible.at(i)) continue;
        // 空名 (と記号だけの名前) は一括レンダリングの既定名 P<行番号> で照合
        QString key = normalizeKey(receiverNames.at(i));
        if (key.isEmpty()) key = QStringLiteral("p%1").arg(i + 1);

        // (1) 完全一致 → (2) rir 接頭/接尾 の順で候補を集め、上位規則で
        // 一致したらそこで確定する。同一規則内の複数候補は割り当てない。
        const QString affixA = QStringLiteral("rir") + key;   // rir_<name>
        const QString affixB = key + QStringLiteral("rir");   // <name>_rir
        for (int pass = 0; pass < 2; ++pass) {
            QVector<int> hits;
            for (int f = 0; f < fileKeys.size(); ++f) {
                const QString &fk = fileKeys.at(f);
                if (fk.isEmpty()) continue;
                const bool hit = (pass == 0)
                    ? (fk == key)
                    : (fk == affixA || fk == affixB);
                if (hit) hits << f;
            }
            if (hits.isEmpty()) continue;
            if (hits.size() == 1) {
                out[i].fileIndex = hits.first();
                out[i].rule = (pass == 0) ? Rule::Exact : Rule::Affix;
            } else {
                out[i].rule = Rule::Ambiguous;
                for (int f : hits) out[i].candidates << wavNames.at(f);
            }
            break;
        }

        // (3) rir.wav が 1 個だけ、かつ照合対象が 1 行だけならその行へ
        if (out[i].rule == Rule::None && rirCount == 1 && nEligible == 1) {
            out[i].fileIndex = rirIdx;
            out[i].rule = Rule::SingleRir;
        }
    }
    return out;
}

QStringList listWavFiles(const QString &dirPath)
{
    // QDir の名前フィルタは大文字小文字の扱いが OS 依存なので、
    // 全ファイルを列挙してから拡張子を自前で判定する (決定的に名前順)
    const QStringList all =
        QDir(dirPath).entryList(QDir::Files, QDir::Name);
    QStringList out;
    for (const QString &f : all) {
        if (f.endsWith(QLatin1String(".wav"), Qt::CaseInsensitive)) out << f;
    }
    return out;
}

} // namespace rirauto
} // namespace ofd
