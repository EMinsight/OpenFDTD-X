// OfdPreview.h — 「保存したら何が書かれるか」を組み立てる (読み取り専用)
//
// 保存経路と**同じ関数**を通す:
//   .ofd  … OfdIO::serialize()   (OfdIO::save がそのまま書く文字列)
//   .ofdx … OfdxIO::serialize()  (OfdxIO::save がそのまま書くバイト列)
// なので、ここに出るものと実際に保存されるものは一致する (selftest で判定)。
//
// GUI が知らないキー (Project::extraLines) は `end` の直前にまとめて
// 書き戻される。プレビューではその行を強調できるよう行番号を返す
// (ラウンドトリップで手編集が保たれていることを目で確認するため)。
//
// Qt Widgets に依存しない (selftest は QCoreApplication で動く)。
#ifndef OFD_IO_OFDPREVIEW_H
#define OFD_IO_OFDPREVIEW_H

#include <QByteArray>
#include <QString>
#include <QVector>

#include "../core/Project.h"

namespace ofd {

struct OfdPreviewText {
    QString    ofd;            // .ofd の中身 (改行は LF)
    QByteArray ofdx;           // .ofdx の中身 (JSON, indented)
    QVector<int> extraRows;    // .ofd のうち extraLines 由来の行 (0 始まり)
    int ofdBytes = 0;          // UTF-8 バイト数 (LF のまま数えた値)
    int ofdRows  = 0;          // 行数
};

// extraRows は「末尾の end の直前に extraLines と同じ並びで載っている」ことを
// 確かめてから返す。確かめられない場合は**空**にする (誤った強調を出さない)。
OfdPreviewText buildOfdPreview(const Project &p);

} // namespace ofd

#endif // OFD_IO_OFDPREVIEW_H
