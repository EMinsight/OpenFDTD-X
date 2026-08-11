// AcousticSourceTab.h — 音源モデリングタブ (acoustic-source.jsx 相当)。
// AFMG EASE / Odeon / CATT-Acoustic 風の音源モジュール:
//   音源リスト          — スピーカー/ソナー配置表 + 共通設定
//   入力信号 (WAV)      — 無響録音ファイル入力 + ライブラリ + 波形プレビュー
//   指向性              — CLF/GLL 指向性 + 帯域別表 + ポーラプロット + 周波数特性
//   アレイ・ライン音源  — ラインアレイ / ビームステアリング / サブアレイ
//   可聴化 Auralization — IRF 畳み込み (HRTF/Ambisonics) + A/B 試聴
// 音響/水中ドメインで表示され、水中選択時はソナー音源リストに切替わる。
// 音源リストの「信号」列には音声ファイル (WAV 等) をボタンから割り当てられ
// (.ofdx acoustic.sources[].signal)、可聴化タブがドライ音源として取り込む。
#pragma once
#include <QScrollArea>
#include <QVector>
#include "../core/Project.h"   // AcousticSourceRow (音源リストの行)
#include "../audio/AudioEditEngine.h"   // SourcePrep (入力信号の前処理)

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;

namespace ofd {

class Project;
class MiniPlot;

// 水平面ポーラパターン描画 (一次指向性 r = a + b·cosθ — mock の SVG を
// 係数パラメトリック化したもの)
class PolarPatternView : public QWidget {
    Q_OBJECT
public:
    explicit PolarPatternView(QWidget *parent = nullptr);
    // 一次指向性の係数を設定 (a+b = 1 に正規化された値を想定)。
    // omni: (1,0) / cardioid: (0.5,0.5) / super: (0.37,0.63) /
    // hyper: (0.25,0.75) / fig-8: (0,1)
    void setPattern(double a, double b);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    double m_a = 0.5, m_b = 0.5;   // 既定はカーディオイド
};

class AcousticSourceTab : public QScrollArea {
    Q_OBJECT
public:
    explicit AcousticSourceTab(Project *project, QWidget *parent = nullptr);

    // 音源リスト (室内音響 .ofdx acoustic.sources) → ソルバ波源 (.ofd feed) の
    // 反映本体 (確認ダイアログ無し)。有効 (enabled) な行それぞれについて、
    // その位置に Z 向き・既定の振幅/位相/内部抵抗の feed を作り、既存 feed
    // リストを置き換える。反映件数 (= 新しい feed 数) を返す。有効行が 0 の
    // ときは feed を一切変更せず 0 を返す。
    // レベル [dB] は校正が無いため振幅へ換算しない (絶対規則 6) —
    // ソルバへ渡るのは位置のみ。Project::touch() は呼び出し側で行う。
    // ofdx_selftest は GUI_SOURCES をリンクしないため、selftest から
    // 直接検証できるようヘッダ内 inline 定義の static メソッドにしてある。
    static int syncFeedsFromSources(Project &p)
    {
        QVector<Feed> next;
        for (const AcousticSourceRow &r : p.acoustic().sources) {
            if (!r.enabled) continue;
            Feed f;                          // 向き Z・振幅/位相/Z0 は既定値
            f.x = r.x_m;
            f.y = r.y_m;
            f.z = r.z_m;
            next.push_back(f);
        }
        if (next.isEmpty()) return 0;        // feed は変更しない
        p.feeds() = next;
        return int(next.size());
    }

private slots:
    void refresh();              // model → widgets
    void onDomainChanged();      // 音響 ⇔ 水中 で音源リスト等を切替
    void applyWavPrep();         // WAV 前処理 widgets → model (+ プレビュー再計算)

private:
    void apply();
    QWidget *buildSourcesPage();
    QWidget *buildSignalPage();
    QWidget *buildDirectivityPage();
    QWidget *buildArrayPage();
    QWidget *buildAuralPage();
    bool isUnderwater() const;
    audioedit::SourcePrep wavPrep() const;   // モデル → 前処理設定
    // 音源リスト (Project::acoustic().sources / underwater().sources) と
    // 表の同期。ドメインで対象リストが切り替わる。
    QVector<AcousticSourceRow> &sourceList();
    void applySourceCell(int row, int col);  // widgets → model (1 セル)
    void refreshSourceCount();   // 有効音源数の注記を更新
    void refreshSourceTable();               // model → widgets
    void addSourceRow(const AcousticSourceRow &row);
    // 選択 WAV を実読込して包絡線とレベル指標を表示 (QThread で非同期)
    void loadWavPreview(const QString &path);
    // 選択された解析指向性モデルをポーラ図・帯域表・軸上特性へ反映
    void updateDirectivity();
    // アレイページ: 素子配置 + 遅延 → 遠方界パターン (acoustics/ArrayDirectivity)
    void updateArray();
    // 共通設定: 音源リストの位置と受音点から、距離・遅延・受音レベルを出す
    void updateDrive();
    // 畳み込み設定の受音点・入力WAV をモデルの実データで埋める
    void refreshConvBindings();
    // 可聴化品質指標: 実測 RIR (OperaAcousticSettings::rirPath) を非同期分析
    void computeAuralQuality();
    void clearAuralQuality();    // 未算出状態 ("—" + 説明) へ戻す

    Project    *m_p;
    bool        m_updating = false;
    QTabWidget *m_tabs;

    // sources
    QLabel       *m_srcHint;
    QTableWidget *m_srcTable;
    QPushButton  *m_presetBtn;
    QPushButton  *m_sigPickBtn  = nullptr;   // 選択行の信号 (WAV) を選ぶ
    QPushButton  *m_sigClearBtn = nullptr;   // 選択行の信号を解除
    QPushButton  *m_syncBtn  = nullptr;      // 有効行の位置 → feed へ反映
    QLabel       *m_syncNote = nullptr;      // 反映は位置のみ、の説明
    QLabel       *m_srcModelNote = nullptr;  // 「計算へは渡されない」注記
    QLabel       *m_srcCountNote = nullptr;  // 有効音源数と負荷の注記
    QLineEdit    *m_baseSpl;
    // 同時駆動の実計算 (距離補正の遅延 / クリップ余裕)
    QCheckBox    *m_clipPrevent = nullptr;
    QCheckBox    *m_distComp = nullptr;
    QCheckBox    *m_coherence = nullptr;
    QTableWidget *m_driveTable = nullptr;
    QLabel       *m_driveSummary = nullptr;
    QLabel       *m_baseSplUnit;

    // signal
    QComboBox    *m_sigKind;
    QLineEdit    *m_wavFile;
    // 入力信号の前処理 (.ofdx acoustic.source_wav の View)
    QLineEdit    *m_wavTrim0 = nullptr, *m_wavTrim1 = nullptr;
    QLineEdit    *m_wavGain = nullptr, *m_wavHpfHz = nullptr;
    QCheckBox    *m_wavHpf = nullptr;
    QTableWidget *m_libTable;
    MiniPlot     *m_wavePlot;
    QLabel       *m_wavStats = nullptr;    // RMS/Peak/Crest (実計算後に更新)
    QLabel       *m_previewNote = nullptr; // 見本表示の注記 (実読込後は隠す)
    QLabel       *m_srateValue = nullptr;  // サンプリングレート表示
    QString       m_previewPath;           // 実読込済みファイル (再読込防止)
    bool          m_previewBusy = false;   // 非同期読込中

    // directivity
    QComboBox    *m_dirModel, *m_dirSource;
    QLineEdit    *m_gllFile;
    MiniPlot     *m_freqResp;
    PolarPatternView *m_polar = nullptr;
    QLabel       *m_polarInfo = nullptr;    // ビーム幅 / F/B / Q (閉形式)
    QLabel       *m_polarClfNote = nullptr; // CLF/GLL 選択時のみ表示する注記
    QTableWidget *m_bandTable = nullptr;    // 帯域別指向性 (解析式 or "—")
    QLabel       *m_bandNote = nullptr;
    QLabel       *m_frNote = nullptr;       // 軸上周波数特性の状態注記

    // array (ラインアレイ / サブアレイ)
    QLineEdit    *m_arrElems = nullptr, *m_arrSpacing = nullptr;
    QLineEdit    *m_arrSplay = nullptr;
    QComboBox    *m_arrCurve = nullptr;
    QCheckBox    *m_arrSteer = nullptr;
    QLineEdit    *m_arrSteerDeg = nullptr;
    QCheckBox    *m_arrGrating = nullptr;
    QComboBox    *m_arrFreq = nullptr;
    MiniPlot     *m_arrPlot = nullptr;
    QTableWidget *m_arrTable = nullptr;
    QLabel       *m_arrNote = nullptr;      // グレーティングローブの警告
    QComboBox    *m_subLayout = nullptr;
    QCheckBox    *m_subRev = nullptr;
    QLineEdit    *m_subDelay = nullptr;
    QLabel       *m_subInfo = nullptr;

    // aural (畳み込み設定 — 実体は可聴化タブ。ここは同じモデルの View)
    QLineEdit    *m_convDry = nullptr;    // 入力WAV = auralizationDryFile
    QComboBox    *m_convRecv = nullptr;   // 受音点 = acoustic().receivers

    QComboBox    *m_renderRate;
    QTableWidget *m_qualTable = nullptr;    // 可聴化品質指標 (実測 RIR 由来)
    QLabel       *m_qualNote = nullptr;
    bool          m_qualBusy = false;       // 非同期分析中
};

} // namespace ofd
