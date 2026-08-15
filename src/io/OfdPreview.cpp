// OfdPreview.cpp — 保存内容の組み立て (OfdPreview.h 参照)
#include "OfdPreview.h"

#include "OfdIO.h"

namespace ofd {

OfdPreviewText buildOfdPreview(const Project &p)
{
    OfdPreviewText t;
    t.ofd   = OfdIO::serialize(p);
    t.ofdx  = OfdxIO::serialize(p);
    t.ofdBytes = t.ofd.toUtf8().size();

    // QString::split で末尾の "\n" が空要素を生むので、行数はそれを除いて数える
    const QStringList rows = t.ofd.split('\n');
    t.ofdRows = rows.size();
    if (!rows.isEmpty() && rows.back().isEmpty()) t.ofdRows -= 1;

    // extraLines は `end` の直前に、読み込んだ順のまま並んでいるはず。
    // **そう並んでいることを確かめてから**行番号を返す — 並びが想定と違う
    // ときに当てずっぽうで強調すると、保持されていない行を保持されたように
    // 見せてしまう (絶対規則 5 の趣旨)。
    const QStringList &extra = p.extraLines();
    if (extra.isEmpty()) return t;

    // 末尾の "end" 行を探す (その後ろは空行のみ)
    int endRow = -1;
    for (int i = t.ofdRows - 1; i >= 0; --i) {
        if (rows[i].trimmed().isEmpty()) continue;
        if (rows[i] == QLatin1String("end")) endRow = i;
        break;
    }
    if (endRow < 0) return t;

    const int first = endRow - extra.size();
    if (first < 0) return t;
    for (int k = 0; k < extra.size(); ++k)
        if (rows[first + k] != extra[k]) return t;   // 並びが違う → 強調しない

    for (int k = 0; k < extra.size(); ++k) t.extraRows.push_back(first + k);
    return t;
}

} // namespace ofd
