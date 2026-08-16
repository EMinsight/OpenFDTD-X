// AcousticResultModel.h — RirAnalysisResult → 表表示用の行リスト変換と
// CSV / JSON 文字列化。GUI (RirAnalysisTab) とファイル出力の共通層。
//
// 行の文字列は言語非依存の生値 (指標名 / 数値 / 単位 / 品質トークン)。
// 画面向けの翻訳 (品質バッジ・「算出不可」表記など) はタブ側で行う。
#pragma once
#include <QString>
#include <QVector>

#include "../core/RirAnalyzer.h"
#include "../core/VocalAnalyzer.h"

namespace ofd {

// 表示規則のオプション (要求 §3.2 の 3 値表示規則)。
// コアは物理量を無条件に計算するが、規格上の意味を持つかは測定条件に
// 依存し、それはソフトウェアからは検証できない — 利用者の自己申告で
// 表示を切り替える。タブ・CSV・一括レポートの全経路が同じ規則に従う。
struct MetricDisplayOptions {
    // ST_early / ST_late の測定条件 (舞台上・音源から 1 m・空席、
    // ISO 3382-1 Annex C) を利用者が自己申告したか。
    //   申告あり → 値 + Warning「参考値 (測定条件は自己申告)」
    //   申告なし → 値を出さず「測定条件不適合」(既定 — 安全側)
    //   コアが invalid → そのまま「算出不可」
    bool stConditionDeclared = false;
};

// 指標表の1行 (指標 × 帯域)
struct AcousticResultRow {
    QString metric;   // "EDT" / "T20" / "T30" / "C50" / "C80" / "D50" / "Ts"
    QString band;     // 帯域ラベル ("125 Hz", "Full band" …)
    bool    valid = false;
    double  value = 0.0;      // valid 時のみ意味を持つ
    QString valueText;        // 表示用数値 (invalid 時は空)
    QString unit;             // "s" / "dB" / "-" 等
    QString quality;          // "valid" / "warning" / "invalid"
    QString warning;          // 品質低下・無効の理由 (空 = 問題なし)
};

class AcousticResultModel {
public:
    // 指標 × 帯域の行リスト (EDT/T20/T30/C50/C80/D50/Ts の順、帯域ごと)。
    // opts 省略時は「申告なし」= ST 系を表示しない安全側の既定。
    static QVector<AcousticResultRow>
    metricRows(const acoustics::RirAnalysisResult &result,
               const MetricDisplayOptions &opts = MetricDisplayOptions());

    // 品質トークン ("valid"/"warning"/"invalid")
    static QString qualityToken(acoustics::AnalysisQuality q);

    // 反射の時間区分ラベル: 0-20 / 20-80 / 80-200 / 200+ ms
    static QString reflectionBinLabel(double delaySeconds);

    // CSV 文字列化 (指標表 + 反射一覧 + 警告)。指標行は metricRows と
    // 同じ表示規則に従う (opts 省略時は「申告なし」)。
    static QString toCsv(const acoustics::RirAnalysisResult &result,
                         const MetricDisplayOptions &opts =
                             MetricDisplayOptions());

    // JSON 文字列化 (QJsonDocument 経由、前処理・直接音・帯域・反射を含む)。
    // JSON はコアの生値をそのまま持ち (機械可読の一次データ)、代わりに
    // st_condition_declared フラグを併記して読み手が規則を適用できるようにする。
    static QString toJson(const acoustics::RirAnalysisResult &result,
                          const MetricDisplayOptions &opts =
                              MetricDisplayOptions());

    // ── 歌声分析 (VocalAnalysisResult) ──────────────────────────────────────
    // 指標の行リスト (F0 統計 / ビブラート / HNR / スペクトル重心 /
    // 歌手フォルマント比 / フォルマント F1-F3 中央値 / 帯域エネルギー /
    // レベル)。SPL 行は Absolute 校正時のみ valid (コアの判定を反映)。
    static QVector<AcousticResultRow>
    vocalRows(const acoustics::VocalAnalysisResult &result);

    // CSV 文字列化 (指標表 + サマリー + F0 軌跡 + フォルマント軌跡 + 警告)
    static QString toCsv(const acoustics::VocalAnalysisResult &result);

    // JSON 文字列化 (指標 + F0/フォルマント軌跡 + LTAS + 倍音レベル + 警告)
    static QString toJson(const acoustics::VocalAnalysisResult &result);
};

} // namespace ofd
