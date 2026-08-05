// RirAutoAssign.h — 受音点別 RIR WAV の自動割当 (可聴化タブ ⑤ の補助)。
//
// ソルバ実行や実測が生成したフォルダ直下の *.wav を、受音点名との対応で
// 各受音点 (ReceiverRow::rirFile) へ割り当てる。対応規則は単純かつ説明可能に
// 3 段のみ (上位規則で一致したらそこで確定):
//   (1) 完全一致        <name>.wav
//   (2) 接頭/接尾       rir_<name>.wav / <name>_rir.wav
//   (3) 唯一の rir.wav  フォルダに rir.wav が 1 個だけ、かつ照合対象の
//                       受音点が 1 行だけならその行へ
// 比較は正規化キー (拡張子・大文字小文字・英数字以外の記号を無視) で行う。
// 同一規則内で候補が複数ある行は割り当てず、候補一覧を理由として返す
// (黙って恣意的に選ばない)。
//
// Qt Core のみ (Widget 非依存) — selftest からテーブル駆動で検証する。
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace ofd {
namespace rirauto {

// 照合キーの正規化: 小文字化し、英数字以外 (記号・空白・'_' 等) を除去する。
// 例: "RIR_R-1" → "rirr1"。ファイル名側は拡張子を除いてから渡すこと。
QString normalizeKey(const QString &s);

// 割当の根拠 (行の状態欄にそのまま説明できる粒度)
enum class Rule {
    None = 0,     // 一致なし (未割当)
    Exact,        // (1) <name>.wav
    Affix,        // (2) rir_<name>.wav / <name>_rir.wav
    SingleRir,    // (3) 唯一の rir.wav → 唯一の照合対象行
    Ambiguous     // 同一規則内で候補が複数 → 割り当てない (candidates が理由)
};

struct Assignment {
    int  fileIndex = -1;      // wavNames のインデックス (-1 = 未割当)
    Rule rule = Rule::None;
    QStringList candidates;   // Ambiguous のときの候補ファイル名 (2 個以上)
};

// wavNames:      フォルダ直下の WAV ファイル名 (拡張子込み・パスなし)
// receiverNames: 受音点名。空白のみの名前は一括レンダリングの既定名と同じ
//                P<行番号> (1 始まり) として照合する
// eligible:      行ごとの照合対象フラグ (無効行や「既存設定を守る」行は
//                false)。receiverNames と同数であること
// 戻り値は receiverNames と同じ長さ。eligible = false の行は None のまま。
// 決定的 (同一入力 → 同一結果)。
QVector<Assignment> assign(const QStringList &wavNames,
                           const QStringList &receiverNames,
                           const QVector<bool> &eligible);

// フォルダ直下の WAV ファイル名 (拡張子 .wav の大小を問わず、名前順で決定的)。
// フォルダが無い・空なら空リスト。再帰はしない (直下のみ)。
QStringList listWavFiles(const QString &dirPath);

} // namespace rirauto
} // namespace ofd
