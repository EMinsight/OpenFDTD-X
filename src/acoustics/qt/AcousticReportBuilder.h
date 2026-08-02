// AcousticReportBuilder.h — RIR 分析 / 歌声分析の一括レポート生成
// (HTML / CSV)。負債 #10 の「レポート出力」。
//
// 既にタブ上で実行済みの分析結果を受け取ってまとめるだけで、分析の再実行は
// しない (GUI 規則「重い処理を同期実行しない」)。未実行の分析はレポート上に
// **「未実行」と明示**する (絶対規則 5: 未完成・未実行を動作済みに見せない)。
//
// Widget に依存しないため selftest から直接検証できる (QtAcousticAdapter と
// 同じ扱い)。出力は現在時刻を含まない — 同じ入力からは同じバイト列が出る
// (再現性と selftest での検証のため)。
#pragma once
#include <QString>

#include "../core/RirAnalyzer.h"
#include "../core/VocalAnalyzer.h"

namespace ofd {

// レポートに載せる入力一式。分析結果は has* が true のときだけ意味を持つ。
struct AcousticReportInput {
    QString projectTitle;         // .ofd のタイトル (空可)
    QString rirFile;              // 実測 RIR の WAV (表示用の名前)
    QString voiceFile;            // 歌唱 WAV (表示用の名前)
    int     calibrationState = 2; // 0=Absolute 1=Relative 2=Uncalibrated
    double  calibrationOffsetDb = 0.0;

    bool hasRir = false;
    acoustics::RirAnalysisResult rir;

    bool hasVocal = false;
    acoustics::VocalAnalysisResult vocal;

    // 可聴化は設定のみ (レポートは畳み込み結果を持たない)
    QString auralizationDryFile;
    QString auralizationOutputFile;
};

class AcousticReportBuilder {
public:
    // 単一 HTML (外部 CSS/JS/画像を参照しない自己完結文書)
    static QString buildHtml(const AcousticReportInput &in);

    // 両分析を 1 ファイルにまとめた CSV (先頭列 source で系統を区別する)
    static QString buildCsv(const AcousticReportInput &in);

    // どちらか一方でも実行済みか (未実行のみならレポートを出さない判断に使う)
    static bool hasAnyResult(const AcousticReportInput &in);

    // 校正状態のラベル (I18n 済み)
    static QString calibrationLabel(int calibrationState);
};

} // namespace ofd
