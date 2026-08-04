// AcousticSourceTab.cpp
#include "AcousticSourceTab.h"
#include "../core/Project.h"
#include "../acoustics/qt/QtAcousticAdapter.h"
#include "../audio/AudioEditEngine.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTabWidget>
#include <QThread>
#include <QVBoxLayout>
#include <cmath>
#include <memory>
#include <vector>

using namespace ofd;

// ── file-local vocabulary (asrc_) ───────────────────────────────────────────
namespace {
const bool s_i18n = [] {
    using ofd::I18n;
    // sub tabs
    I18n::reg("asrc_tab_sources", "音源リスト", "Sources");
    I18n::reg("asrc_tab_signal", "入力信号 (WAV)", "Input signal (WAV)");
    I18n::reg("asrc_tab_directivity", "指向性", "Directivity");
    I18n::reg("asrc_tab_array", "アレイ・ライン音源", "Array / line sources");
    I18n::reg("asrc_tab_aural", "可聴化 / Auralization", "Auralization");
    // sources
    I18n::reg("asrc_sources_section", "音源一覧", "Sources");
    I18n::reg("asrc_src_hint_uw",
              "ソナー送信源・水中音源を配置。WAV/PCM・チャープ・トーンを直接入力可能。",
              "Place sonar transmitters and underwater sources. WAV/PCM, chirp "
              "and tone signals can be fed directly.");
    I18n::reg("asrc_src_hint_room",
              "AFMG EASE / Odeon 風のスピーカー配置。CLF/GLL指向性ファイルは"
              "パーサ未実装 — ファイル名の記録のみ。",
              "AFMG EASE / Odeon style loudspeaker placement. CLF/GLL "
              "directivity files: parser not implemented — only the file name "
              "is recorded.");
    I18n::reg("asrc_col_name", "名前", "Name");
    I18n::reg("asrc_col_kind", "種類", "Type");
    I18n::reg("asrc_col_pos", "位置 (x,y,z)", "Position (x,y,z)");
    I18n::reg("asrc_col_dir", "向き", "Aim");
    I18n::reg("asrc_col_sig", "信号", "Signal");
    I18n::reg("asrc_kind_card", "カーディオイド", "Cardioid");
    I18n::reg("asrc_kind_omni", "無指向性", "Omni");
    I18n::reg("asrc_kind_bipolar", "バイポーラ", "Bipolar");
    I18n::reg("asrc_kind_directional", "指向性", "Directional");
    I18n::reg("asrc_kind_omniall", "全方位", "Omni");
    I18n::reg("asrc_add_row", "＋ 音源を追加…", "+ Add source…");
    I18n::reg("asrc_btn_addfile", "＋ ファイルから追加", "+ Add from file");
    I18n::reg("asrc_btn_clflib", "CLF/GLL ライブラリ", "CLF/GLL library");
    I18n::reg("asrc_btn_preset", "プリセット", "Presets");
    I18n::reg("asrc_preset_sonar", "ソナー", "Sonar");
    I18n::reg("asrc_preset_speaker", "スピーカー", "Speakers");
    I18n::reg("asrc_common_section", "共通設定", "Common");
    I18n::reg("asrc_base_spl", "基準SPL @1m", "Reference SPL @1m");
    I18n::reg("asrc_normalize", "正規化", "Normalisation");
    I18n::reg("asrc_clip_prevent", "同時駆動でクリップ防止",
              "Prevent clipping when driven together");
    I18n::reg("asrc_delay", "遅延", "Delay");
    I18n::reg("asrc_dist_comp", "距離自動補正",
              "Automatic distance compensation");
    I18n::reg("asrc_phase", "位相", "Phase");
    I18n::reg("asrc_coherence", "音源間相互コヒーレンス",
              "Inter-source mutual coherence");
    // signal
    I18n::reg("asrc_signal_section", "入力信号", "Source signal");
    I18n::reg("asrc_sig_kind", "信号種別", "Signal type");
    I18n::reg("asrc_sig_impulse", "インパルス", "Impulse");
    I18n::reg("asrc_sig_sweep", "スイープ (TSP/ESS)", "Sweep (TSP/ESS)");
    I18n::reg("asrc_sig_chirp", "チャープ", "Chirp");
    I18n::reg("asrc_sig_tone", "正弦波", "Sine tone");
    I18n::reg("asrc_sig_noise", "ノイズ", "Noise");
    I18n::reg("asrc_sig_wav", "WAV ファイル", "WAV file");
    I18n::reg("asrc_wav_section", "WAV ファイル入力", "Audio file");
    I18n::reg("asrc_file", "ファイル", "File");
    I18n::reg("asrc_browse", "📁 参照…", "📁 Browse…");
    I18n::reg("asrc_listen", "▶ 試聴", "▶ Preview");
    I18n::reg("asrc_formats", "対応形式", "Supported formats");
    I18n::reg("asrc_channels", "チャンネル", "Channels");
    I18n::reg("asrc_ch_mono", "モノ → 1音源", "Mono → 1 source");
    I18n::reg("asrc_ch_stereo", "ステレオ → 2音源", "Stereo → 2 sources");
    I18n::reg("asrc_ch_51", "5.1ch → 6音源", "5.1ch → 6 sources");
    I18n::reg("asrc_srate", "サンプリングレート", "Sample rate");
    // 実ファイルを解析していないため「例」であることを明示 (絶対規則 5)
    I18n::reg("asrc_srate_sample", "例: 48000 Hz", "e.g. 48000 Hz");
    I18n::reg("asrc_resample", "自動リサンプル", "Auto resample");
    I18n::reg("asrc_loop", "ループ", "Loop");
    I18n::reg("asrc_loop_chk", "ファイル末尾でループ再生",
              "Loop playback at end of file");
    I18n::reg("asrc_trim", "トリミング", "Trim");
    I18n::reg("asrc_gain", "ゲイン", "Gain");
    I18n::reg("asrc_hpf", "ハイパス", "High-pass");
    I18n::reg("asrc_hpf_chk", "DC除去 20Hz", "DC removal 20 Hz");
    I18n::reg("asrc_lib_section", "ライブラリ", "Library");
    I18n::reg("asrc_lib_hint", "無響録音 (anechoic recordings) のプリセット",
              "Anechoic recording presets");
    I18n::reg("asrc_col_material", "素材", "Material");
    I18n::reg("asrc_col_len", "長さ", "Length");
    I18n::reg("asrc_col_use", "用途", "Use");
    I18n::reg("asrc_lib1", "男性スピーチ (日本語)", "Male speech (Japanese)");
    I18n::reg("asrc_lib1u", "STI評価", "STI evaluation");
    I18n::reg("asrc_lib2", "女性スピーチ (英語)", "Female speech (English)");
    I18n::reg("asrc_lib2u", "明瞭度比較", "Intelligibility comparison");
    I18n::reg("asrc_lib3", "クラシック (オーケストラ片チャネル)",
              "Classical (orchestra, one channel)");
    I18n::reg("asrc_lib3u", "音楽残響評価", "Music reverberation evaluation");
    I18n::reg("asrc_lib4", "ピアノ独奏", "Piano solo");
    I18n::reg("asrc_lib4u", "過渡応答試聴", "Transient response listening");
    I18n::reg("asrc_lib5", "クリック (極短パルス)", "Click (very short pulse)");
    I18n::reg("asrc_lib5u", "IRF直接観察", "Direct IRF observation");
    I18n::reg("asrc_lib6", "MLS / TSP スイープ", "MLS / TSP sweep");
    I18n::reg("asrc_lib6u", "正確なIRF測定", "Accurate IRF measurement");
    I18n::reg("asrc_lib7", "ピンクノイズ", "Pink noise");
    I18n::reg("asrc_lib7u", "RT60 / SPL測定", "RT60 / SPL measurement");
    I18n::reg("asrc_lib8_room", "拍手", "Applause");
    I18n::reg("asrc_lib8u_room", "残響推定", "Reverberation estimate");
    I18n::reg("asrc_lib8_uw", "クジラ鳴音", "Whale call");
    I18n::reg("asrc_lib8u_uw", "生物音響", "Bioacoustics");
    I18n::reg("asrc_preview_section", "信号プレビュー", "Waveform preview");
    I18n::reg("asrc_preview_fail", "読み込み失敗: %1", "Load failed: %1");
    // 書式は日英共通 (数値と単位のみ)
    I18n::reg("asrc_preview_stats",
              "RMS: %1 dBFS · Peak: %2 dBFS · Crest factor: %3 dB",
              "RMS: %1 dBFS · Peak: %2 dBFS · Crest factor: %3 dB");
    // WAV 試聴 (外部 CLI プレイヤ — QtMultimedia は使わない)
    I18n::reg("asrc_listen_title", "WAV 試聴", "WAV preview");
    I18n::reg("asrc_play_nofile", "ファイルが見つかりません: %1",
              "File not found: %1");
    I18n::reg("asrc_play_noplayer",
              "再生用プレイヤが見つかりません (PATH を確認)。ffplay / aplay / "
              "afplay のいずれかが必要です — macOS: 標準の afplay または "
              "brew install ffmpeg / Linux: apt install ffmpeg または "
              "alsa-utils。",
              "No audio player found on PATH. One of ffplay / aplay / afplay "
              "is required — macOS: built-in afplay or brew install ffmpeg / "
              "Linux: apt install ffmpeg or alsa-utils.");
    // directivity
    I18n::reg("asrc_dir_section", "指向性パターン", "Directivity");
    I18n::reg("asrc_dir_hint",
              "音源の放射特性。CLF (Common Loudspeaker Format) / GLL (Generic "
              "Loudspeaker Library) / 測定 polar データ / 解析モデルから選択 "
              "(CLF/GLL はパーサ未実装 — ファイル名の記録のみ)。",
              "Radiation characteristics of the source. Choose from CLF (Common "
              "Loudspeaker Format) / GLL (Generic Loudspeaker Library) / "
              "measured polar data / analytic models. (CLF/GLL parser not "
              "implemented — only the file name is recorded.)");
    I18n::reg("asrc_model_section", "モデル選択", "Model");
    I18n::reg("asrc_m_omni", "無指向性", "Omni");
    I18n::reg("asrc_m_card", "カーディオイド", "Cardioid");
    I18n::reg("asrc_m_super", "スーパーカーディオイド", "Supercardioid");
    I18n::reg("asrc_m_hyper", "ハイパーカーディオイド", "Hypercardioid");
    I18n::reg("asrc_m_fig8", "双指向性 (Fig-8)", "Figure-8");
    I18n::reg("asrc_s_piston", "円形ピストン", "Circular piston");
    I18n::reg("asrc_s_horn", "ホーン (CD型)", "Horn (CD type)");
    I18n::reg("asrc_s_line", "ラインアレイ要素", "Line array element");
    I18n::reg("asrc_s_clf", "CLF/GLL ファイル", "CLF/GLL file");
    I18n::reg("asrc_s_measured", "測定 polar (.txt)", "Measured polar (.txt)");
    I18n::reg("asrc_gll_section", "CLF/GLL ファイル", "Loudspeaker file");
    I18n::reg("asrc_origin", "参照点 (origin)", "Reference point (origin)");
    I18n::reg("asrc_o_acoustic", "音響中心", "Acoustic centre");
    I18n::reg("asrc_o_physical", "物理中心", "Physical centre");
    I18n::reg("asrc_o_custom", "カスタム", "Custom");
    I18n::reg("asrc_freqres", "周波数分解能", "Frequency resolution");
    I18n::reg("asrc_f_oct", "1オクターブ", "1 octave");
    I18n::reg("asrc_f_third", "1/3オクターブ", "1/3 octave");
    I18n::reg("asrc_f_sixth", "1/6オクターブ", "1/6 octave");
    I18n::reg("asrc_f_narrow", "狭帯域", "Narrow band");
    I18n::reg("asrc_angres", "角度分解能", "Angular resolution");
    I18n::reg("asrc_angres_val", "5° × 5° (球面)", "5° × 5° (spherical)");
    I18n::reg("asrc_band_section", "周波数別指向性", "Per-band directivity");
    I18n::reg("asrc_col_band", "帯域", "Band");
    I18n::reg("asrc_col_h6", "水平 -6dB", "Horizontal -6dB");
    I18n::reg("asrc_col_v6", "垂直 -6dB", "Vertical -6dB");
    I18n::reg("asrc_col_q", "Q値", "Q");
    I18n::reg("asrc_polar_section", "可視化", "Polar plot preview");
    I18n::reg("asrc_polar_hint",
              "水平面 (azimuth) ポーラパターン — 一次指向性モデル "
              "r = a + b·cosθ (周波数非依存)",
              "Horizontal (azimuth) polar pattern — first-order model "
              "r = a + b·cosθ (frequency-independent)");
    I18n::reg("asrc_polar_note_clf",
              "CLF/GLL・測定 polar のパーサは未実装 — 表示は選択中の解析モデル"
              " (一次指向性) の理論パターンです。",
              "CLF/GLL and measured-polar parsers are not implemented — the "
              "plot shows the theoretical pattern of the selected analytic "
              "(first-order) model.");
    I18n::reg("asrc_fr_section", "周波数特性",
              "Frequency response (on-axis)");
    // array
    I18n::reg("asrc_array_section", "ラインアレイ (EASE / D&B / L-Acoustics 風)",
              "Line array (EASE / D&B / L-Acoustics style)");
    I18n::reg("asrc_array_hint",
              "複数スピーカーをライン状に配置し、各要素の遅延・ゲインで指向性を制御。"
              "コンサートホール・PA システム設計に必須。",
              "Multiple loudspeakers placed in a line; directivity is controlled "
              "by per-element delay and gain. Essential for concert hall and PA "
              "system design.");
    I18n::reg("asrc_elems", "要素数", "Elements");
    I18n::reg("asrc_spacing", "要素間距離", "Element spacing");
    I18n::reg("asrc_curve", "カーブ", "Curve");
    I18n::reg("asrc_c_straight", "直線", "Straight");
    I18n::reg("asrc_c_j", "Jカーブ", "J-curve");
    I18n::reg("asrc_c_banana", "バナナ", "Banana");
    I18n::reg("asrc_c_custom", "カスタム", "Custom");
    I18n::reg("asrc_splay", "各要素の傾斜", "Per-element splay");
    I18n::reg("asrc_splay_unit", "° (上から)", "° (from top)");
    I18n::reg("asrc_dg_section", "遅延・ゲイン", "Delay & gain");
    I18n::reg("asrc_steer_auto", "自動算出 (受聴範囲指定)",
              "Automatic (specify listening range)");
    I18n::reg("asrc_coverage", "目標カバレッジ", "Target coverage");
    I18n::reg("asrc_uniform", "均一性目標", "Uniformity target");
    I18n::reg("asrc_grating", "蜂巣構造防止 (グレーティングローブ抑制)",
              "Grating lobe suppression");
    I18n::reg("asrc_airabs", "Air absorption 補償",
              "Air absorption compensation");
    I18n::reg("asrc_sub_section", "サブウーファアレイ", "Sub array");
    I18n::reg("asrc_layout", "配置", "Layout");
    I18n::reg("asrc_l_single", "単発", "Single");
    I18n::reg("asrc_l_endfire", "エンドファイア", "End-fire");
    I18n::reg("asrc_l_cardioid", "カーディオイド", "Cardioid");
    I18n::reg("asrc_l_gradient", "勾配", "Gradient");
    I18n::reg("asrc_rev_rear", "位相反転 (rear)", "Polarity invert (rear)");
    I18n::reg("asrc_rev_chk", "ON (前方放射重視)",
              "ON (front radiation priority)");
    I18n::reg("asrc_delay_rear", "遅延 (rear)", "Delay (rear)");
    // aural
    I18n::reg("asrc_aural_section", "可聴化", "Auralization");
    I18n::reg("asrc_aural_hint",
              "シミュレーション結果のIRF (Impulse Response) と入力WAVを畳み込み、"
              "その空間で聴いた音を再現。<br/>畳み込み処理は可聴化タブで実行 "
              "(このページは設定のみ・未実装)。",
              "Convolves the simulated IRF (impulse response) with the input WAV "
              "to reproduce the sound heard in that space.<br/>Convolution runs "
              "in the Auralization tab (this page is settings only — not "
              "implemented).");
    I18n::reg("asrc_conv_section", "畳み込み設定", "Convolution");
    I18n::reg("asrc_input_wav", "入力WAV", "Input WAV");
    I18n::reg("asrc_recv_irf", "受音点 (IRF)", "Receiver (IRF)");
    I18n::reg("asrc_p1", "P1_center (中央前列)", "P1_center (front centre)");
    I18n::reg("asrc_p2", "P2_left  (左サイド)", "P2_left (left side)");
    I18n::reg("asrc_p3", "P3_back  (後方)", "P3_back (rear)");
    I18n::reg("asrc_p4", "P4_balcony (バルコニー)", "P4_balcony (balcony)");
    I18n::reg("asrc_outch", "出力チャネル", "Output channels");
    I18n::reg("asrc_oc_mono", "モノ", "Mono");
    I18n::reg("asrc_oc_stereo", "ステレオ", "Stereo");
    I18n::reg("asrc_oc_binaural", "バイノーラル (HRTF)", "Binaural (HRTF)");
    I18n::reg("asrc_hrtf", "HRTFデータベース", "HRTF database");
    I18n::reg("asrc_hrtf_personal", "個人化 HRTF (.sofa)",
              "Personalised HRTF (.sofa)");
    I18n::reg("asrc_convmode", "畳み込み方式", "Convolution method");
    I18n::reg("asrc_cv_direct", "直接畳み込み", "Direct convolution");
    I18n::reg("asrc_cv_fft", "FFT (オフライン)", "FFT (offline)");
    I18n::reg("asrc_cv_part", "分割畳み込み (リアルタイム)",
              "Partitioned convolution (real-time)");
    I18n::reg("asrc_render_section", "出力", "Render");
    I18n::reg("asrc_outfile", "出力ファイル", "Output file");
    I18n::reg("asrc_bits", "ビット深度", "Bit depth");
    I18n::reg("asrc_btn_render", "🎧 レンダリング", "🎧 Render");
    I18n::reg("asrc_btn_listen2", "▶ 試聴 (ヘッドホン推奨)",
              "▶ Listen (headphones recommended)");
    I18n::reg("asrc_btn_ab", "A/B 比較 (素音 vs 残響付)",
              "A/B compare (dry vs reverberant)");
    I18n::reg("asrc_norender",
              "レンダリング済みの WAV がありません。畳み込み (レンダリング) は"
              "可聴化 (Auralization) タブで実行してください。",
              "No rendered WAV yet. Run the convolution (render) in the "
              "Auralization tab first.");
    I18n::reg("asrc_ab_handoff",
              "A/B 比較 (dry / wet の書き出しと波形比較) は可聴化 "
              "(Auralization) タブで実行します。入力WAV を可聴化タブの dry "
              "ファイルとして設定しました。",
              "A/B comparison (dry / wet rendering and waveform comparison) "
              "runs in the Auralization tab. The input WAV has been handed "
              "over as the Auralization tab's dry file.");
    I18n::reg("asrc_ab_delegate",
              "A/B 比較 (dry / wet の書き出しと波形比較) は可聴化 "
              "(Auralization) タブで実行します。入力WAV に実在するファイルを"
              "指定すると、ここから dry ファイルとして引き渡します。",
              "A/B comparison (dry / wet rendering and waveform comparison) "
              "runs in the Auralization tab. Point the input WAV to an "
              "existing file to hand it over as the dry file.");
    I18n::reg("asrc_ab_section", "A/B 試聴", "Listening test");
    I18n::reg("asrc_ab_target", "比較対象", "Compare");
    I18n::reg("asrc_ab_dry", "無響原音 (dry)", "Anechoic original (dry)");
    I18n::reg("asrc_ab_wet", "畳み込み済み (wet)", "Convolved (wet)");
    I18n::reg("asrc_ab_revonly", "シミュレーション残響のみ",
              "Simulated reverberation only");
    I18n::reg("asrc_seat", "座席切替", "Seat select");
    I18n::reg("asrc_seat1", "P1 — 中央前列", "P1 — front centre");
    I18n::reg("asrc_seat2", "P2 — サイド席", "P2 — side seat");
    I18n::reg("asrc_seat3", "P3 — 後方", "P3 — rear");
    I18n::reg("asrc_seat4", "P4 — バルコニー", "P4 — balcony");
    I18n::reg("asrc_play", "▶ 再生", "▶ Play");
    I18n::reg("asrc_abx", "ABXテスト", "ABX test");
    I18n::reg("asrc_abx_chk", "二重盲検モード", "Double-blind mode");
    I18n::reg("asrc_quality_section", "可聴化品質指標",
              "Auralization quality metrics");
    I18n::reg("asrc_col_item", "項目", "Item");
    I18n::reg("asrc_col_value", "値", "Value");
    I18n::reg("asrc_col_verdict", "判定", "Verdict");
    I18n::reg("asrc_q_irflen", "IRF長", "IRF length");
    I18n::reg("asrc_q_enough", "十分", "Sufficient");
    I18n::reg("asrc_q_density", "初期反射密度", "Early reflection density");
    I18n::reg("asrc_q_high", "高 (1st-50ms)", "High (1st-50ms)");
    I18n::reg("asrc_q_natural", "自然", "Natural");
    I18n::reg("asrc_q_good", "良好", "Good");
    I18n::reg("asrc_q_lf", "LF (側方音エネルギー)", "LF (lateral energy)");
    I18n::reg("asrc_q_spacious", "広がり感○", "Good spaciousness");
    I18n::reg("asrc_q_phase", "位相応答", "Phase response");
    I18n::reg("asrc_q_minphase", "最小位相補正済", "Minimum-phase corrected");
    return true;
}();

// 音響アクセント色 (mock の var(--acc-acoustic))
const QColor kAcc("#2E8B57");

// バッジ風 QLabel (badge / badge ok / badge acc 相当, スタイルは最小限)
QLabel *makeBadge(const QString &text, QWidget *parent,
                  const char *color = nullptr)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(QStringLiteral(
        "QLabel { border: 1px solid palette(mid); border-radius: 3px;"
        " padding: 1px 6px; %1 }")
        .arg(color ? QStringLiteral("color: %1;")
                         .arg(QString::fromLatin1(color))
                   : QString()));
    return l;
}

// 読取専用データ表 (q-table 相当) の共通初期化
void setupTable(QTableWidget *t, const QStringList &headers, int minH)
{
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(22);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setMinimumHeight(minH);
}

// WAV を外部 CLI プレイヤで再生する (QtMultimedia は依存に追加しない —
// H5ViewerTab の ffmpeg 探索と同じ流儀で PATH から実行ファイルを探す)。
// ffplay → aplay → afplay の順で最初に見つかったものを非同期起動し、
// どれも無ければ導入方法を案内する。
void playWavExternal(QWidget *parent, const QString &path)
{
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(parent, ofd::I18n::tr("asrc_listen_title"),
                             ofd::I18n::tr("asrc_play_nofile").arg(path));
        return;
    }
    const QString ffplay =
        QStandardPaths::findExecutable(QStringLiteral("ffplay"));
    if (!ffplay.isEmpty()) {
        // ウィンドウを開かず末尾で自動終了
        QProcess::startDetached(ffplay,
            { QStringLiteral("-nodisp"), QStringLiteral("-autoexit"),
              QStringLiteral("-loglevel"), QStringLiteral("quiet"), path });
        return;
    }
    const QString aplay =
        QStandardPaths::findExecutable(QStringLiteral("aplay"));
    if (!aplay.isEmpty()) {
        QProcess::startDetached(aplay, { QStringLiteral("-q"), path });
        return;
    }
    const QString afplay =
        QStandardPaths::findExecutable(QStringLiteral("afplay"));
    if (!afplay.isEmpty()) {
        QProcess::startDetached(afplay, { path });
        return;
    }
    QMessageBox::information(parent, ofd::I18n::tr("asrc_listen_title"),
                             ofd::I18n::tr("asrc_play_noplayer"));
}
} // namespace

// ── PolarPatternView — 一次指向性 r = a + b·cosθ を QPainter で描画 ────────
PolarPatternView::PolarPatternView(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(200, 200);
}

void PolarPatternView::setPattern(double a, double b)
{
    m_a = a;
    m_b = b;
    update();
}

void PolarPatternView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    // viewBox -110..110 → widget 座標
    const double s = qMin(width(), height()) / 220.0;
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(s, s);

    // 同心円 (dashed) + 放射線
    QPen grid(palette().mid().color(), 1);
    grid.setDashPattern({ 2, 2 });
    p.setPen(grid);
    p.setBrush(Qt::NoBrush);
    for (int r : { 20, 40, 60, 80, 100 })
        p.drawEllipse(QPointF(0, 0), double(r), double(r));
    grid.setDashPattern({ 1, 2 });
    p.setPen(grid);
    for (int a : { 0, 30, 60, 90, 120, 150 }) {
        const double rad = a * M_PI / 180.0;
        p.drawLine(QPointF(0, 0),
                   QPointF(100 * std::cos(rad), 100 * std::sin(rad)));
    }

    // 一次指向性 r = 100·|a + b·cosθ| / (a+b)  (θ = 0° を上向きに描く。
    // fig-8 の後方ローブは絶対値で表す — 音圧振幅の極座標表示)
    const double norm = std::max(std::fabs(m_a + m_b), 1e-9);
    QPainterPath path;
    for (int i = 0; i <= 180; ++i) {
        const double th = i * 2.0 * M_PI / 180.0;
        const double r =
            100.0 * std::fabs(m_a + m_b * std::cos(th)) / norm;
        const QPointF pt(r * std::sin(th), -r * std::cos(th));
        if (i == 0) path.moveTo(pt); else path.lineTo(pt);
    }
    path.closeSubpath();
    QColor fill = kAcc;
    fill.setAlphaF(0.3);
    p.setPen(QPen(kAcc, 1.5));
    p.setBrush(fill);
    p.drawPath(path);

    // 角度ラベル
    p.setPen(palette().text().color());
    QFont f = p.font();
    f.setPointSizeF(7.5);
    p.setFont(f);
    p.drawText(QRectF(-20, -100, 40, 12), Qt::AlignCenter, "0°");
    p.drawText(QRectF(70, -3, 40, 12), Qt::AlignCenter, "90°");
}

// ── AcousticSourceTab ───────────────────────────────────────────────────────
AcousticSourceTab::AcousticSourceTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    m_tabs = new QTabWidget(body);
    m_tabs->setDocumentMode(true);
    m_tabs->addTab(buildSourcesPage(),     I18n::tr("asrc_tab_sources"));
    m_tabs->addTab(buildSignalPage(),      I18n::tr("asrc_tab_signal"));
    m_tabs->addTab(buildDirectivityPage(), I18n::tr("asrc_tab_directivity"));
    m_tabs->addTab(buildArrayPage(),       I18n::tr("asrc_tab_array"));
    m_tabs->addTab(buildAuralPage(),       I18n::tr("asrc_tab_aural"));
    v->addWidget(m_tabs);

    v->addStretch(1);
    setWidget(body);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    connect(project, &Project::loaded, this, &AcousticSourceTab::refresh);
    connect(project, &Project::domainChanged,
            this, &AcousticSourceTab::onDomainChanged);
    onDomainChanged();
    refresh();
}

bool AcousticSourceTab::isUnderwater() const
{
    return m_p->activeDomain() == Domain::Underwater;
}

// ── page: sources ───────────────────────────────────────────────────────────
QWidget *AcousticSourceTab::buildSourcesPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("asrc_sources_section"), page);
    m_srcHint = new QLabel(s);
    m_srcHint->setWordWrap(true);
    s->vbox()->addWidget(m_srcHint);

    m_srcTable = new QTableWidget(0, 8, s);
    m_srcTable->setHorizontalHeaderLabels({
        "", "#", I18n::tr("asrc_col_name"), I18n::tr("asrc_col_kind"),
        I18n::tr("asrc_col_pos"), I18n::tr("asrc_col_dir"),
        I18n::tr("asrc_col_sig"), "SPL/SL" });
    m_srcTable->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    m_srcTable->horizontalHeader()->setStretchLastSection(true);
    m_srcTable->verticalHeader()->setVisible(false);
    m_srcTable->verticalHeader()->setDefaultSectionSize(22);
    m_srcTable->setMinimumHeight(190);
    s->vbox()->addWidget(m_srcTable);
    // 音源一覧は固定サンプル (モデル未接続) — 注記を明示 (絶対規則 5)
    s->vbox()->addWidget(tabhelp::sampleNote(s));

    auto *row = new QHBoxLayout();
    auto *addBtn = new QPushButton(I18n::tr("asrc_btn_addfile"), s);
    auto *libBtn = new QPushButton(I18n::tr("asrc_btn_clflib"), s);
    m_presetBtn = new QPushButton(s);
    tabhelp::markNotImplemented(addBtn);
    tabhelp::markNotImplemented(libBtn);
    tabhelp::markNotImplemented(m_presetBtn);
    row->addWidget(addBtn);
    row->addWidget(libBtn);
    row->addWidget(m_presetBtn);
    row->addStretch(1);
    s->vbox()->addLayout(row);
    v->addWidget(s);

    auto *sc = new SectionBox(I18n::tr("asrc_common_section"), page);
    auto *splRow = new QHBoxLayout();
    m_baseSpl = new QLineEdit(sc);
    m_baseSpl->setMaximumWidth(80);
    m_baseSplUnit = new QLabel(sc);
    splRow->addWidget(m_baseSpl);
    splRow->addWidget(m_baseSplUnit);
    splRow->addStretch(1);
    sc->form()->addRow(I18n::tr("asrc_base_spl"), splRow);
    auto *clip = new QCheckBox(I18n::tr("asrc_clip_prevent"), sc);
    clip->setChecked(true);
    sc->form()->addRow(I18n::tr("asrc_normalize"), clip);
    auto *dist = new QCheckBox(I18n::tr("asrc_dist_comp"), sc);
    dist->setChecked(true);
    sc->form()->addRow(I18n::tr("asrc_delay"), dist);
    auto *coh = new QCheckBox(I18n::tr("asrc_coherence"), sc);
    sc->form()->addRow(I18n::tr("asrc_phase"), coh);
    // 正規化/遅延/位相のチェックはどこにも読まれない — 注記 (基準SPLのみ有効)
    sc->vbox()->addWidget(tabhelp::unwiredNote(sc));
    v->addWidget(sc);
    v->addStretch(1);

    connect(m_baseSpl, &QLineEdit::editingFinished, this,
            [this] { apply(); });
    return page;
}

// 音源リスト表をドメインに合わせて再構築 (mock の isUW 分岐)
void AcousticSourceTab::fillSourceTable(bool underwater)
{
    struct Src {
        bool ck; const char *n; const char *kindKey;
        const char *pos, *d, *sig, *spl;
    };
    static const Src kUw[] = {
        { true,  "TX_sonar",  "asrc_kind_directional",
          "-1200, 50, 0", "+X", "chirp 3-5kHz", "220" },
        { false, "TX_pinger", "asrc_kind_omniall",
          "0, 100, 0", "—", "tone 12kHz", "195" },
    };
    static const Src kRoom[] = {
        { true,  "L_main",  "asrc_kind_card",
          "-3.0, 4.5, 5.0",  "-Z 30°", "speech.wav", "94" },
        { true,  "R_main",  "asrc_kind_card",
          " 3.0, 4.5, 5.0",  "-Z 30°", "speech.wav", "94" },
        { true,  "C_voice", "asrc_kind_card",
          " 0.0, 4.0, 5.5",  "-Z",     "speech.wav", "88" },
        { false, "SUB",     "asrc_kind_omni",
          " 0.0, 0.5, 5.0",  "—",      "pink.wav",   "100" },
        { false, "Surr_L",  "asrc_kind_bipolar",
          "-5.0, 3.0, 15.0", "+X",     "speech.wav", "82" },
        { false, "Surr_R",  "asrc_kind_bipolar",
          " 5.0, 3.0, 15.0", "-X",     "speech.wav", "82" },
    };
    const Src *rows = underwater ? kUw : kRoom;
    const int n = underwater ? 2 : 6;

    m_srcTable->clearSpans();
    m_srcTable->setRowCount(n + 1);
    for (int i = 0; i < n; ++i) {
        const Src &r = rows[i];
        auto *ck = new QTableWidgetItem;
        ck->setCheckState(r.ck ? Qt::Checked : Qt::Unchecked);
        ck->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        m_srcTable->setItem(i, 0, ck);
        auto *num = new QTableWidgetItem(QString::number(i + 1));
        num->setFlags(num->flags() & ~Qt::ItemIsEditable);
        m_srcTable->setItem(i, 1, num);
        m_srcTable->setItem(i, 2, new QTableWidgetItem(
            QString::fromUtf8(r.n)));                      // 名前は編集可
        auto ro = [this, i](int col, const QString &text) {
            auto *it = new QTableWidgetItem(text);
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            m_srcTable->setItem(i, col, it);
        };
        ro(3, I18n::tr(r.kindKey));
        ro(4, QString::fromUtf8(r.pos));
        ro(5, QString::fromUtf8(r.d));
        ro(6, QString::fromUtf8(r.sig));
        ro(7, QString::fromUtf8(r.spl) + " dB");
    }
    // 追加行 (＋ 音源を追加…)
    auto *ck = new QTableWidgetItem;
    ck->setCheckState(Qt::Unchecked);
    ck->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    m_srcTable->setItem(n, 0, ck);
    auto *add = new QTableWidgetItem(I18n::tr("asrc_add_row"));
    QFont f = add->font();
    f.setItalic(true);
    add->setFont(f);
    add->setForeground(palette().mid());
    add->setFlags(Qt::ItemIsEnabled);
    m_srcTable->setItem(n, 1, add);
    m_srcTable->setSpan(n, 1, 1, 7);
}

// ── page: signal ────────────────────────────────────────────────────────────
QWidget *AcousticSourceTab::buildSignalPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("asrc_signal_section"), page);
    m_sigKind = new QComboBox(s);
    m_sigKind->addItem(I18n::tr("asrc_sig_impulse"));
    m_sigKind->addItem(I18n::tr("asrc_sig_sweep"));
    m_sigKind->addItem(I18n::tr("asrc_sig_chirp"));
    m_sigKind->addItem(I18n::tr("asrc_sig_tone"));
    m_sigKind->addItem(I18n::tr("asrc_sig_noise"));
    m_sigKind->addItem(I18n::tr("asrc_sig_wav"));
    m_sigKind->setCurrentIndex(5);   // mock: value="wav"
    s->form()->addRow(I18n::tr("asrc_sig_kind"), m_sigKind);
    v->addWidget(s);

    auto *sw = new SectionBox(I18n::tr("asrc_wav_section"), page);
    auto *fileRow = new QHBoxLayout();
    m_wavFile = new QLineEdit("anechoic_speech_48k.wav", sw);
    auto *browse = new QPushButton(I18n::tr("asrc_browse"), sw);
    auto *listen = new QPushButton(I18n::tr("asrc_listen"), sw);
    fileRow->addWidget(m_wavFile, 1);
    fileRow->addWidget(browse);
    fileRow->addWidget(listen);
    sw->form()->addRow(I18n::tr("asrc_file"), fileRow);

    auto *fmtRow = new QHBoxLayout();
    for (const char *b : { "WAV", "FLAC", "AIFF", "OGG", "PCM", "24/32-bit" })
        fmtRow->addWidget(makeBadge(QString::fromUtf8(b), sw));
    fmtRow->addStretch(1);
    sw->form()->addRow(I18n::tr("asrc_formats"), fmtRow);

    auto *ch = new QComboBox(sw);
    ch->addItem(I18n::tr("asrc_ch_mono"));
    ch->addItem(I18n::tr("asrc_ch_stereo"));
    ch->addItem(I18n::tr("asrc_ch_51"));
    ch->addItem("Ambisonics (B-format)");
    sw->form()->addRow(I18n::tr("asrc_channels"), ch);

    auto *srRow = new QHBoxLayout();
    // ファイル未解析の間は固定値ではなく「例」と表示する
    // (プレビューで実読込に成功したら実測値へ置き換える)
    m_srateValue = new QLabel(I18n::tr("asrc_srate_sample"), sw);
    srRow->addWidget(m_srateValue);
    auto *resample = new QCheckBox(I18n::tr("asrc_resample"), sw);
    resample->setChecked(true);
    srRow->addWidget(resample);
    srRow->addStretch(1);
    sw->form()->addRow(I18n::tr("asrc_srate"), srRow);

    auto *loop = new QCheckBox(I18n::tr("asrc_loop_chk"), sw);
    sw->form()->addRow(I18n::tr("asrc_loop"), loop);

    auto *trimRow = new QHBoxLayout();
    auto *trim0 = new QLineEdit("0.0", sw); trim0->setMaximumWidth(60);
    auto *trim1 = new QLineEdit("5.0", sw); trim1->setMaximumWidth(60);
    trimRow->addWidget(trim0);
    trimRow->addWidget(new QLabel(QString::fromUtf8("〜"), sw));
    trimRow->addWidget(trim1);
    trimRow->addWidget(new QLabel("s", sw));
    trimRow->addStretch(1);
    sw->form()->addRow(I18n::tr("asrc_trim"), trimRow);

    auto *gainRow = new QHBoxLayout();
    auto *gain = new QLineEdit("0.0", sw); gain->setMaximumWidth(60);
    gainRow->addWidget(gain);
    gainRow->addWidget(new QLabel("dB", sw));
    gainRow->addStretch(1);
    sw->form()->addRow(I18n::tr("asrc_gain"), gainRow);

    auto *hpf = new QCheckBox(I18n::tr("asrc_hpf_chk"), sw);
    hpf->setChecked(true);
    sw->form()->addRow(I18n::tr("asrc_hpf"), hpf);
    // WAV 入力設定はまだどこにも読まれない
    sw->vbox()->addWidget(tabhelp::unwiredNote(sw));
    v->addWidget(sw);

    auto *sl = new SectionBox(I18n::tr("asrc_lib_section"), page);
    auto *libHint = new QLabel(I18n::tr("asrc_lib_hint"), sl);
    libHint->setWordWrap(true);
    sl->vbox()->addWidget(libHint);
    m_libTable = new QTableWidget(8, 4, sl);
    setupTable(m_libTable, { "", I18n::tr("asrc_col_material"),
                             I18n::tr("asrc_col_len"),
                             I18n::tr("asrc_col_use") }, 200);
    // レイアウトへの追加漏れがあると、表が親の左上に素置きされて
    // 上のヒントラベルに重なる (実際に表示崩れを起こしていた)
    sl->vbox()->addWidget(m_libTable);
    // 収録音源そのものは同梱していない — 一覧は想定内容の見本
    sl->vbox()->addWidget(tabhelp::sampleNote(sl));
    v->addWidget(sl);

    auto *sp = new SectionBox(I18n::tr("asrc_preview_section"), page);
    m_wavePlot = new MiniPlot(sp);
    m_wavePlot->setLabels("t [s]", "amplitude");
    m_wavePlot->setMinimumHeight(110);
    // ファイル未選択時のプレースホルダ波形。
    // モックは 440 Hz 搬送波だったが、5 s を 200 点 (= 40 Hz) で描くと
    // sin(2π·440·t) は全サンプルが節に当たって恒等的に 0 になり (実測 1e-13)、
    // 数値誤差だけの意味のない線が出ていた。描画点数で表現できる搬送波
    // (12 Hz) と点数 (1000) にして、波形として読める見本にする。
    MiniSeries wave;
    wave.color = kAcc;
    const int kPts = 1000;
    for (int i = 0; i < kPts; ++i) {
        const double t = i / double(kPts) * 5.0;
        const double env = std::exp(-std::pow((t - 1.5) / 0.6, 2.0));
        const double y = env * std::sin(2 * M_PI * 12 * t)
                       * (0.5 + 0.4 * std::sin(2 * M_PI * 1.5 * t));
        wave.pts.push_back({ t, y });
    }
    m_wavePlot->setSeries({ wave });
    sp->vbox()->addWidget(m_wavePlot);
    m_wavStats = new QLabel(
        QString::fromUtf8("RMS: -18 dBFS · Peak: -3 dBFS · Crest factor: 15 dB"),
        sp);
    sp->vbox()->addWidget(m_wavStats);
    // ファイル未選択の間は解析式による見本波形 (実読込に成功したら隠す)
    m_previewNote = tabhelp::sampleNote(sp);
    sp->vbox()->addWidget(m_previewNote);
    v->addWidget(sp);
    v->addStretch(1);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("asrc_wav_section"), QString(),
            "Audio (*.wav *.flac *.aiff *.ogg);;All files (*)");
        if (path.isEmpty()) return;
        m_wavFile->setText(path);
        loadWavPreview(path);
    });
    // 手入力パスも、確定時に実在すればプレビューへ反映する
    connect(m_wavFile, &QLineEdit::editingFinished, this, [this] {
        const QString path = m_wavFile->text();
        if (path != m_previewPath && QFileInfo::exists(path))
            loadWavPreview(path);
    });
    // 試聴: 外部 CLI プレイヤ (ffplay/aplay/afplay) に委ねる
    connect(listen, &QPushButton::clicked, this, [this] {
        playWavExternal(this, m_wavFile->text());
    });
    return page;
}

// 選択 WAV を実読込し、包絡線 (min/max) と RMS/Peak/Crest を実計算して表示。
// 読込と解析は QThread で非同期 (gui.md: 秒単位処理を GUI スレッドで同期
// 実行しない)。失敗時は見本表示のまま、エラーだけ統計行へ出す。
void AcousticSourceTab::loadWavPreview(const QString &path)
{
    if (m_previewBusy || path.trimmed().isEmpty()) return;
    m_previewBusy = true;

    struct PreviewData {
        bool ok = false;
        QString err;
        double fs = 0.0;
        std::vector<double> mono;          // 平均モノ (包絡線用)
        audioedit::LevelMetrics lv;
    };
    auto d = std::make_shared<PreviewData>();
    const std::string p = path.toStdString();
    QThread *th = QThread::create([p, d] {
        const acoustics::AcousticResult<acoustics::AudioBuffer> res =
            acoustics::readWavFile(p);
        if (!res.success()) {
            d->err = QString::fromStdString(res.message());
            return;
        }
        d->fs = res.value().sampleRateHz;
        d->mono = QtAcousticAdapter::selectChannel(res.value(), 2);
        // 指標も包絡線と同じ平均モノで測る (チャンネル間の齟齬を避ける)
        acoustics::AudioBuffer mb;
        mb.sampleRateHz = d->fs;
        mb.channels.push_back(d->mono);
        d->lv = audioedit::analyzeLevels(mb, 0, 0);   // a >= z → 全範囲
        d->ok = true;
    });
    connect(th, &QThread::finished, this, [this, th, d, path] {
        th->deleteLater();
        m_previewBusy = false;
        if (!d->ok) {
            m_wavStats->setText(I18n::tr("asrc_preview_fail").arg(d->err));
            return;   // 見本波形と注記はそのまま
        }
        QVector<QPointF> top, bottom;
        tabhelp::envelopeSeries(d->mono, d->fs, 1200,
                                tabhelp::TimeUnit::Seconds, top, bottom);
        MiniSeries hi;  hi.pts = top;     hi.color = kAcc;
        MiniSeries lo;  lo.pts = bottom;  lo.color = kAcc;
        m_wavePlot->setSeries({ hi, lo });
        m_wavStats->setText(
            I18n::tr("asrc_preview_stats")
                .arg(QString::number(d->lv.rmsDbfs, 'f', 1),
                     QString::number(d->lv.peakDbfs, 'f', 1),
                     QString::number(d->lv.crestDb, 'f', 1))
            + QStringLiteral(" · %1 s")
                  .arg(QString::number(d->lv.durationSec, 'f', 2)));
        m_srateValue->setText(QStringLiteral("%1 Hz").arg(qRound(d->fs)));
        m_previewNote->setVisible(false);   // 実データ表示 — 見本注記を外す
        m_previewPath = path;
    });
    th->start();
}

// ── page: directivity ───────────────────────────────────────────────────────
QWidget *AcousticSourceTab::buildDirectivityPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("asrc_dir_section"), page);
    auto *hint = new QLabel(I18n::tr("asrc_dir_hint"), s);
    hint->setWordWrap(true);
    s->vbox()->addWidget(hint);
    v->addWidget(s);

    auto *sm = new SectionBox(I18n::tr("asrc_model_section"), page);
    m_dirModel = new QComboBox(sm);
    m_dirModel->addItem(I18n::tr("asrc_m_omni"));
    m_dirModel->addItem(I18n::tr("asrc_m_card"));
    m_dirModel->addItem(I18n::tr("asrc_m_super"));
    m_dirModel->addItem(I18n::tr("asrc_m_hyper"));
    m_dirModel->addItem(I18n::tr("asrc_m_fig8"));
    m_dirModel->setCurrentIndex(1);   // ポーラプレビューは Cardioid
    sm->vbox()->addWidget(m_dirModel);
    m_dirSource = new QComboBox(sm);
    m_dirSource->addItem(I18n::tr("asrc_s_piston"));
    m_dirSource->addItem(I18n::tr("asrc_s_horn"));
    m_dirSource->addItem(I18n::tr("asrc_s_line"));
    m_dirSource->addItem(I18n::tr("asrc_s_clf"));
    m_dirSource->addItem(I18n::tr("asrc_s_measured"));
    m_dirSource->setCurrentIndex(3);   // mock: value="clf"
    sm->vbox()->addWidget(m_dirSource);
    v->addWidget(sm);

    auto *sf = new SectionBox(I18n::tr("asrc_gll_section"), page);
    auto *fileRow = new QHBoxLayout();
    m_gllFile = new QLineEdit("EAW_KF730_v2.gll", sf);
    auto *browse = new QPushButton(I18n::tr("asrc_browse"), sf);
    fileRow->addWidget(m_gllFile, 1);
    fileRow->addWidget(browse);
    sf->form()->addRow(I18n::tr("asrc_file"), fileRow);
    auto *fmtRow = new QHBoxLayout();
    fmtRow->addWidget(makeBadge(".CLF (CLF v2)", sf));
    fmtRow->addWidget(makeBadge(".GLL (Ease GLL)", sf, "#2E8B57"));
    fmtRow->addWidget(makeBadge(".XGLC", sf));
    fmtRow->addWidget(makeBadge(".SPK (Odeon)", sf));
    fmtRow->addWidget(makeBadge(".so8 (CATT)", sf));
    fmtRow->addStretch(1);
    sf->form()->addRow(I18n::tr("asrc_formats"), fmtRow);
    auto *origin = new QComboBox(sf);
    origin->addItem(I18n::tr("asrc_o_acoustic"));
    origin->addItem(I18n::tr("asrc_o_physical"));
    origin->addItem(I18n::tr("asrc_o_custom"));
    sf->form()->addRow(I18n::tr("asrc_origin"), origin);
    auto *freqres = new QComboBox(sf);
    freqres->addItem(I18n::tr("asrc_f_oct"));
    freqres->addItem(I18n::tr("asrc_f_third"));
    freqres->addItem(I18n::tr("asrc_f_sixth"));
    freqres->addItem(I18n::tr("asrc_f_narrow"));
    freqres->setCurrentIndex(1);   // mock: value="third"
    sf->form()->addRow(I18n::tr("asrc_freqres"), freqres);
    sf->form()->addRow(I18n::tr("asrc_angres"),
                       new QLabel(I18n::tr("asrc_angres_val"), sf));
    v->addWidget(sf);

    auto *sb = new SectionBox(I18n::tr("asrc_band_section"), page);
    auto *band = new QTableWidget(5, 5, sb);
    setupTable(band, { I18n::tr("asrc_col_band"), I18n::tr("asrc_col_h6"),
                       I18n::tr("asrc_col_v6"), I18n::tr("asrc_col_q"),
                       "DI [dB]" }, 150);
    static const char *kBand[5][5] = {
        { "125 Hz", "160°", "160°", "1.5",  "1.8" },
        { "500 Hz", "120°", "110°", "3.2",  "5.1" },
        { "1 kHz",  "90°",  "75°",  "7.4",  "8.7" },
        { "4 kHz",  "65°",  "45°",  "18.0", "12.6" },
        { "16 kHz", "35°",  "25°",  "55.0", "17.4" },
    };
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 5; ++c)
            band->setItem(r, c, new QTableWidgetItem(
                QString::fromUtf8(kBand[r][c])));
    sb->vbox()->addWidget(band);
    // 指向性表は固定サンプル (ファイル未解析)
    sb->vbox()->addWidget(tabhelp::sampleNote(sb));
    v->addWidget(sb);

    auto *sp = new SectionBox(I18n::tr("asrc_polar_section"), page);
    auto *polarHint = new QLabel(I18n::tr("asrc_polar_hint"), sp);
    polarHint->setWordWrap(true);
    sp->vbox()->addWidget(polarHint);
    auto *polarRow = new QHBoxLayout();
    m_polar = new PolarPatternView(sp);
    polarRow->addWidget(m_polar);
    m_polarInfo = new QLabel(sp);
    m_polarInfo->setTextFormat(Qt::RichText);
    m_polarInfo->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    polarRow->addSpacing(16);
    polarRow->addWidget(m_polarInfo);
    polarRow->addStretch(1);
    sp->vbox()->addLayout(polarRow);
    // CLF/GLL・測定 polar 選択時のみ「ファイルは未解析」の注記を出す
    m_polarClfNote = new QLabel(I18n::tr("asrc_polar_note_clf"), sp);
    m_polarClfNote->setWordWrap(true);
    m_polarClfNote->setStyleSheet("font-size:11px; color:#B8860B;");
    sp->vbox()->addWidget(m_polarClfNote);
    v->addWidget(sp);

    auto *sr = new SectionBox(I18n::tr("asrc_fr_section"), page);
    m_freqResp = new MiniPlot(sr);
    m_freqResp->setLabels("f [Hz] (log)", "SPL [dB]");
    m_freqResp->setXTickPow10(true);
    m_freqResp->setMinimumHeight(110);
    // mock: lowRoll = 1/(1+(80/f)⁴), highRoll = 1/(1+(f/18000)⁴)
    //       y = 20·log10(max(0.001, lowRoll·highRoll)) + sin(f/300)·1.5
    MiniSeries fr;
    fr.color = kAcc;
    for (int i = 0; i < 60; ++i) {
        const double f = 50.0 * std::pow(10.0, i / 15.0);
        const double lowRoll = 1.0 / (1.0 + std::pow(80.0 / f, 4.0));
        const double highRoll = 1.0 / (1.0 + std::pow(f / 18000.0, 4.0));
        const double y = 20.0 * std::log10(std::max(0.001, lowRoll * highRoll))
                       + std::sin(f / 300.0) * 1.5;
        fr.pts.push_back({ std::log10(f), y });
    }
    m_freqResp->setSeries({ fr });
    sr->vbox()->addWidget(m_freqResp);
    // 周波数特性は解析式による固定サンプル
    sr->vbox()->addWidget(tabhelp::sampleNote(sr));
    v->addWidget(sr);
    v->addStretch(1);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("asrc_gll_section"), QString(),
            "Loudspeaker (*.clf *.gll *.xglc *.spk *.so8);;All files (*)");
        if (!path.isEmpty()) m_gllFile->setText(path);
    });
    connect(m_dirModel, &QComboBox::currentIndexChanged, this,
            [this](int) { updateDirectivity(); });
    connect(m_dirSource, &QComboBox::currentIndexChanged, this,
            [this](int i) { m_polarClfNote->setVisible(i == 3 || i == 4); });
    m_polarClfNote->setVisible(m_dirSource->currentIndex() == 3 ||
                               m_dirSource->currentIndex() == 4);
    updateDirectivity();
    return page;
}

// 選択された一次指向性モデル r(θ) = a + b·cosθ をポーラ図へ反映し、
// ビーム幅・F/B 比・指向性係数 Q を閉形式で実計算する。
void AcousticSourceTab::updateDirectivity()
{
    // omni / cardioid / supercardioid / hypercardioid / fig-8 の係数
    static const double kAB[5][2] = {
        { 1.0, 0.0 }, { 0.5, 0.5 }, { 0.37, 0.63 },
        { 0.25, 0.75 }, { 0.0, 1.0 },
    };
    const int idx = qBound(0, m_dirModel->currentIndex(), 4);
    const double a = kAB[idx][0], b = kAB[idx][1];
    m_polar->setPattern(a, b);

    // ビーム幅: |r(θ)| が軸上値から dropDb 落ちる全角。
    // そこまで落ちない (omni 等) 場合は 0 を返し「—」表示にする
    auto beamDeg = [a, b](double dropDb) -> double {
        if (b <= 0.0) return 0.0;
        const double c =
            (std::pow(10.0, -dropDb / 20.0) * (a + b) - a) / b;
        if (c <= -1.0 || c >= 1.0) return 0.0;
        return 2.0 * std::acos(c) * 180.0 / M_PI;
    };
    auto beamText = [](double deg) {
        return deg > 0.0 ? QStringLiteral("%1°").arg(qRound(deg))
                         : QStringLiteral("—");
    };
    // F/B 比 = 20·log10(|r(0°)| / |r(180°)|)。背面ヌル (cardioid) は ∞
    const double back = std::fabs(a - b);
    const QString fb = back < 1e-9
        ? QStringLiteral("∞")
        : QStringLiteral("%1 dB").arg(QString::number(
              20.0 * std::log10((a + b) / back), 'f', 1));
    // 指向性係数 (回転対称 3D): Q = (a+b)² / (a² + b²/3)、DI = 10·log10 Q
    const double q = (a + b) * (a + b) / (a * a + b * b / 3.0);
    const double di = 10.0 * std::log10(q);
    m_polarInfo->setText(QStringLiteral(
        "<b>Type:</b> %1<br>"
        "<b>-3 dB beam:</b> %2<br>"
        "<b>-6 dB beam:</b> %3<br>"
        "<b>F/B ratio:</b> %4<br>"
        "<b>Q:</b> %5 (DI %6 dB)")
        .arg(m_dirModel->currentText(), beamText(beamDeg(3.0)),
             beamText(beamDeg(6.0)), fb, QString::number(q, 'f', 1),
             QString::number(di, 'f', 1)));
}

// ── page: array ─────────────────────────────────────────────────────────────
QWidget *AcousticSourceTab::buildArrayPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("asrc_array_section"), page);
    auto *hint = new QLabel(I18n::tr("asrc_array_hint"), s);
    hint->setWordWrap(true);
    s->vbox()->addWidget(hint);
    auto *elems = new QLineEdit("12", s);
    elems->setMaximumWidth(60);
    s->form()->addRow(I18n::tr("asrc_elems"), elems);
    auto *spacingRow = new QHBoxLayout();
    auto *spacing = new QLineEdit("0.35", s);
    spacing->setMaximumWidth(60);
    spacingRow->addWidget(spacing);
    spacingRow->addWidget(new QLabel("m", s));
    spacingRow->addStretch(1);
    s->form()->addRow(I18n::tr("asrc_spacing"), spacingRow);
    auto *curve = new QComboBox(s);
    curve->addItem(I18n::tr("asrc_c_straight"));
    curve->addItem(I18n::tr("asrc_c_j"));
    curve->addItem(I18n::tr("asrc_c_banana"));
    curve->addItem(I18n::tr("asrc_c_custom"));
    curve->setCurrentIndex(1);   // mock: value="jcurve"
    s->form()->addRow(I18n::tr("asrc_curve"), curve);
    auto *splayRow = new QHBoxLayout();
    auto *splay = new QLineEdit("0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14", s);
    splayRow->addWidget(splay, 1);
    splayRow->addWidget(new QLabel(I18n::tr("asrc_splay_unit"), s));
    s->form()->addRow(I18n::tr("asrc_splay"), splayRow);
    v->addWidget(s);

    auto *sd = new SectionBox(I18n::tr("asrc_dg_section"), page);
    auto *steer = new QCheckBox(I18n::tr("asrc_steer_auto"), sd);
    steer->setChecked(true);
    sd->form()->addRow("Beam steering", steer);
    auto *covRow = new QHBoxLayout();
    auto *cov0 = new QLineEdit("5", sd);  cov0->setMaximumWidth(60);
    auto *cov1 = new QLineEdit("30", sd); cov1->setMaximumWidth(60);
    covRow->addWidget(cov0);
    covRow->addWidget(new QLabel(QString::fromUtf8("〜"), sd));
    covRow->addWidget(cov1);
    covRow->addWidget(new QLabel("m", sd));
    covRow->addStretch(1);
    sd->form()->addRow(I18n::tr("asrc_coverage"), covRow);
    auto *uniRow = new QHBoxLayout();
    auto *uni = new QLineEdit("3", sd);
    uni->setMaximumWidth(60);
    uniRow->addWidget(new QLabel(QString::fromUtf8("±"), sd));
    uniRow->addWidget(uni);
    uniRow->addWidget(new QLabel("dB", sd));
    uniRow->addStretch(1);
    sd->form()->addRow(I18n::tr("asrc_uniform"), uniRow);
    auto *chkRow = new QHBoxLayout();
    auto *grating = new QCheckBox(I18n::tr("asrc_grating"), sd);
    grating->setChecked(true);
    auto *airabs = new QCheckBox(I18n::tr("asrc_airabs"), sd);
    chkRow->addWidget(grating);
    chkRow->addWidget(airabs);
    chkRow->addStretch(1);
    sd->vbox()->addLayout(chkRow);
    v->addWidget(sd);

    auto *ss = new SectionBox(I18n::tr("asrc_sub_section"), page);
    auto *layout = new QComboBox(ss);
    layout->addItem(I18n::tr("asrc_l_single"));
    layout->addItem(I18n::tr("asrc_l_endfire"));
    layout->addItem(I18n::tr("asrc_l_cardioid"));
    layout->addItem(I18n::tr("asrc_l_gradient"));
    layout->setCurrentIndex(2);   // mock: value="cardioid"
    ss->form()->addRow(I18n::tr("asrc_layout"), layout);
    auto *rev = new QCheckBox(I18n::tr("asrc_rev_chk"), ss);
    rev->setChecked(true);
    ss->form()->addRow(I18n::tr("asrc_rev_rear"), rev);
    auto *delayRow = new QHBoxLayout();
    auto *delay = new QLineEdit("3.5", ss);
    delay->setMaximumWidth(60);
    delayRow->addWidget(delay);
    delayRow->addWidget(new QLabel("ms", ss));
    delayRow->addStretch(1);
    ss->form()->addRow(I18n::tr("asrc_delay_rear"), delayRow);
    v->addWidget(ss);
    // アレイページの設定は全節ともまだどこにも読まれない
    v->addWidget(tabhelp::unwiredNote(page));
    v->addStretch(1);
    return page;
}

// ── page: aural ─────────────────────────────────────────────────────────────
QWidget *AcousticSourceTab::buildAuralPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setSpacing(8);

    auto *s = new SectionBox(I18n::tr("asrc_aural_section"), page);
    auto *hint = new QLabel(I18n::tr("asrc_aural_hint"), s);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::RichText);
    s->vbox()->addWidget(hint);
    v->addWidget(s);

    auto *sc = new SectionBox(I18n::tr("asrc_conv_section"), page);
    auto *inRow = new QHBoxLayout();
    auto *inWav = new QLineEdit("anechoic_speech_48k.wav", sc);
    auto *inBtn = new QPushButton("📁", sc);
    inBtn->setMaximumWidth(36);
    inRow->addWidget(inWav, 1);
    inRow->addWidget(inBtn);
    sc->form()->addRow(I18n::tr("asrc_input_wav"), inRow);
    auto *recv = new QComboBox(sc);
    recv->addItem(I18n::tr("asrc_p1"));
    recv->addItem(I18n::tr("asrc_p2"));
    recv->addItem(I18n::tr("asrc_p3"));
    recv->addItem(I18n::tr("asrc_p4"));
    sc->form()->addRow(I18n::tr("asrc_recv_irf"), recv);
    auto *outch = new QComboBox(sc);
    outch->addItem(I18n::tr("asrc_oc_mono"));
    outch->addItem(I18n::tr("asrc_oc_stereo"));
    outch->addItem(I18n::tr("asrc_oc_binaural"));
    outch->addItem("Ambisonics (B-format)");
    outch->addItem("5.1ch");
    outch->addItem("22.2ch");
    outch->setCurrentIndex(2);   // mock: value="binaural"
    sc->form()->addRow(I18n::tr("asrc_outch"), outch);
    auto *hrtf = new QComboBox(sc);
    hrtf->addItem("KEMAR (MIT)");
    hrtf->addItem("CIPIC (UC Davis)");
    hrtf->addItem("SADIE II (York)");
    hrtf->addItem(I18n::tr("asrc_hrtf_personal"));
    sc->form()->addRow(I18n::tr("asrc_hrtf"), hrtf);
    auto *conv = new QComboBox(sc);
    conv->addItem(I18n::tr("asrc_cv_direct"));
    conv->addItem(I18n::tr("asrc_cv_fft"));
    conv->addItem(I18n::tr("asrc_cv_part"));
    conv->setCurrentIndex(2);   // mock: value="partition"
    sc->form()->addRow(I18n::tr("asrc_convmode"), conv);
    // 畳み込み設定はまだどこにも読まれない
    sc->vbox()->addWidget(tabhelp::unwiredNote(sc));
    v->addWidget(sc);

    auto *sr = new SectionBox(I18n::tr("asrc_render_section"), page);
    auto *outFile = new QLineEdit("aural_concert_hall_P1.wav", sr);
    sr->form()->addRow(I18n::tr("asrc_outfile"), outFile);
    m_renderRate = new QComboBox(sr);
    m_renderRate->addItems({ "44.1 kHz", "48 kHz", "96 kHz", "192 kHz" });
    m_renderRate->setCurrentIndex(1);   // mock: value="48"
    sr->form()->addRow(I18n::tr("asrc_srate"), m_renderRate);
    auto *bits = new QComboBox(sr);
    bits->addItems({ "16-bit", "24-bit", "32-bit float" });
    bits->setCurrentIndex(1);   // mock: value="24"
    sr->form()->addRow(I18n::tr("asrc_bits"), bits);
    auto *btnRow = new QHBoxLayout();
    auto *renderBtn = new QPushButton(I18n::tr("asrc_btn_render"), sr);
    auto *listenBtn = new QPushButton(I18n::tr("asrc_btn_listen2"), sr);
    auto *abBtn     = new QPushButton(I18n::tr("asrc_btn_ab"), sr);
    // レンダリング (畳み込み) 自体は可聴化タブが担う — このページでは未実装
    tabhelp::markNotImplemented(renderBtn);
    btnRow->addWidget(renderBtn);
    btnRow->addWidget(listenBtn);
    btnRow->addWidget(abBtn);
    btnRow->addStretch(1);
    sr->vbox()->addLayout(btnRow);
    v->addWidget(sr);

    auto *sa = new SectionBox(I18n::tr("asrc_ab_section"), page);
    auto *abRow = new QHBoxLayout();
    auto *dry = new QCheckBox(I18n::tr("asrc_ab_dry"), sa);
    dry->setChecked(true);
    auto *wet = new QCheckBox(I18n::tr("asrc_ab_wet"), sa);
    wet->setChecked(true);
    auto *revOnly = new QCheckBox(I18n::tr("asrc_ab_revonly"), sa);
    abRow->addWidget(dry);
    abRow->addWidget(wet);
    abRow->addWidget(revOnly);
    abRow->addStretch(1);
    sa->form()->addRow(I18n::tr("asrc_ab_target"), abRow);
    auto *seatRow = new QHBoxLayout();
    auto *seat = new QComboBox(sa);
    seat->addItem(I18n::tr("asrc_seat1"));
    seat->addItem(I18n::tr("asrc_seat2"));
    seat->addItem(I18n::tr("asrc_seat3"));
    seat->addItem(I18n::tr("asrc_seat4"));
    seatRow->addWidget(seat, 1);
    auto *playBtn = new QPushButton(I18n::tr("asrc_play"), sa);
    tabhelp::markNotImplemented(playBtn);
    seatRow->addWidget(playBtn);
    sa->form()->addRow(I18n::tr("asrc_seat"), seatRow);
    auto *abx = new QCheckBox(I18n::tr("asrc_abx_chk"), sa);
    sa->form()->addRow(I18n::tr("asrc_abx"), abx);
    v->addWidget(sa);

    auto *sq = new SectionBox(I18n::tr("asrc_quality_section"), page);
    auto *q = new QTableWidget(5, 3, sq);
    setupTable(q, { I18n::tr("asrc_col_item"), I18n::tr("asrc_col_value"),
                    I18n::tr("asrc_col_verdict") }, 150);
    const struct { QString item, value, verdict; } kQ[5] = {
        { I18n::tr("asrc_q_irflen"), "3.2 s", I18n::tr("asrc_q_enough") },
        { I18n::tr("asrc_q_density"), I18n::tr("asrc_q_high"),
          I18n::tr("asrc_q_natural") },
        { "ITDG (Initial Time Delay Gap)", "23 ms", I18n::tr("asrc_q_good") },
        { I18n::tr("asrc_q_lf"), "0.32", I18n::tr("asrc_q_spacious") },
        { I18n::tr("asrc_q_phase"), I18n::tr("asrc_q_minphase"), "OK" },
    };
    for (int r = 0; r < 5; ++r) {
        q->setItem(r, 0, new QTableWidgetItem(kQ[r].item));
        q->setItem(r, 1, new QTableWidgetItem(kQ[r].value));
        auto *ver = new QTableWidgetItem(kQ[r].verdict);
        ver->setForeground(QBrush(kAcc));
        q->setItem(r, 2, ver);
    }
    sq->vbox()->addWidget(q);
    // 品質指標は固定サンプル — 実測値と誤認させない (絶対規則 5・6)
    sq->vbox()->addWidget(tabhelp::sampleNote(sq));
    v->addWidget(sq);
    v->addStretch(1);

    // 入力WAV の参照ボタン (隣の QLineEdit にパスを反映)
    connect(inBtn, &QPushButton::clicked, this, [this, inWav] {
        const QString path = QFileDialog::getOpenFileName(
            this, I18n::tr("asrc_input_wav"), QString(),
            "Audio (*.wav *.flac *.aiff *.ogg);;All files (*)");
        if (!path.isEmpty()) inWav->setText(path);
    });
    connect(m_renderRate, &QComboBox::currentIndexChanged, this,
            [this] { apply(); });
    // 試聴: 可聴化タブがレンダリングした出力 WAV を外部プレイヤで再生。
    // まだ出力が無い場合は明示的にその旨を伝える (虚偽の動作表示をしない)
    connect(listenBtn, &QPushButton::clicked, this, [this] {
        const QString out =
            m_p->operaAcoustic().auralizationOutputFile.trimmed();
        if (out.isEmpty() || !QFileInfo::exists(out)) {
            QMessageBox::information(this, I18n::tr("asrc_listen_title"),
                                     I18n::tr("asrc_norender"));
            return;
        }
        playWavExternal(this, out);
    });
    // A/B 比較: dry/wet の書き出し・比較は可聴化タブで実装済み —
    // 入力WAV を dry ファイルとして引き渡し、実行場所を案内する
    connect(abBtn, &QPushButton::clicked, this, [this, inWav] {
        const QString wav = inWav->text().trimmed();
        if (!wav.isEmpty() && QFileInfo::exists(wav)) {
            m_p->operaAcoustic().auralizationDryFile = wav;
            m_p->operaAcoustic().enabled = true;
            m_p->touch();
            QMessageBox::information(this, I18n::tr("asrc_btn_ab"),
                                     I18n::tr("asrc_ab_handoff"));
        } else {
            QMessageBox::information(this, I18n::tr("asrc_btn_ab"),
                                     I18n::tr("asrc_ab_delegate"));
        }
    });
    return page;
}

// ── domain switch (音響 ⇔ 水中) ─────────────────────────────────────────────
void AcousticSourceTab::onDomainChanged()
{
    const bool uw = isUnderwater();
    m_srcHint->setText(I18n::tr(uw ? "asrc_src_hint_uw" : "asrc_src_hint_room"));
    m_presetBtn->setText(QStringLiteral("%1 (%2)")
        .arg(I18n::tr("asrc_btn_preset"),
             I18n::tr(uw ? "asrc_preset_sonar" : "asrc_preset_speaker")));
    m_baseSplUnit->setText(uw ? QStringLiteral("dB re μPa·m")
                              : QStringLiteral("dB SPL"));
    fillSourceTable(uw);

    // ライブラリ表 (最終行のみドメイン依存)
    const struct { const char *badge, *mat, *len, *use; } kLib[8] = {
        { "●", "asrc_lib1", "15s",  "asrc_lib1u" },
        { "●", "asrc_lib2", "15s",  "asrc_lib2u" },
        { "○", "asrc_lib3", "30s",  "asrc_lib3u" },
        { "○", "asrc_lib4", "20s",  "asrc_lib4u" },
        { "○", "asrc_lib5", "1ms",  "asrc_lib5u" },
        { "○", "asrc_lib6", "1-10s","asrc_lib6u" },
        { "○", "asrc_lib7", "10s",  "asrc_lib7u" },
        { "○", nullptr,     nullptr, nullptr },
    };
    for (int r = 0; r < 8; ++r) {
        const bool last = (r == 7);
        auto *badge = new QTableWidgetItem(QString::fromUtf8(kLib[r].badge));
        if (r < 2) badge->setForeground(QBrush(kAcc));
        m_libTable->setItem(r, 0, badge);
        m_libTable->setItem(r, 1, new QTableWidgetItem(
            last ? I18n::tr(uw ? "asrc_lib8_uw" : "asrc_lib8_room")
                 : I18n::tr(kLib[r].mat)));
        m_libTable->setItem(r, 2, new QTableWidgetItem(
            last ? QString::fromUtf8(uw ? "5s" : "100ms")
                 : QString::fromUtf8(kLib[r].len)));
        m_libTable->setItem(r, 3, new QTableWidgetItem(
            last ? I18n::tr(uw ? "asrc_lib8u_uw" : "asrc_lib8u_room")
                 : I18n::tr(kLib[r].use)));
    }
    refresh();
}

// ── model ⇔ widgets ────────────────────────────────────────────────────────
void AcousticSourceTab::apply()
{
    if (m_updating) return;
    if (isUnderwater())
        m_p->underwater().sonarSL_dB = m_baseSpl->text().toDouble();
    else
        m_p->acoustic().srcSPL_dB = m_baseSpl->text().toDouble();
    static const int kRates[4] = { 44100, 48000, 96000, 192000 };
    m_p->acoustic().sampleRate =
        kRates[qBound(0, m_renderRate->currentIndex(), 3)];
    m_p->touch();
}

void AcousticSourceTab::refresh()
{
    m_updating = true;
    const double spl = isUnderwater() ? m_p->underwater().sonarSL_dB
                                      : m_p->acoustic().srcSPL_dB;
    m_baseSpl->setText(QString::number(spl, 'g', 6));
    const int sr = m_p->acoustic().sampleRate;
    m_renderRate->setCurrentIndex(
        sr == 44100 ? 0 : sr == 96000 ? 2 : sr == 192000 ? 3 : 1);
    m_updating = false;
}
