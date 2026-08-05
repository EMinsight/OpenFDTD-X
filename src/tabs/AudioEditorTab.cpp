// AudioEditorTab.cpp
#include "AudioEditorTab.h"
#include "../acoustics/io/WavReader.h"
#include "../acoustics/io/WavWriter.h"
#include "../widgets/AudioWaveformView.h"
#include "../widgets/MiniPlot.h"
#include "../widgets/SectionBox.h"
#include "../I18n.h"
#include "TabHelpers.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>
#include <iterator>
#include <memory>

using namespace ofd;
using namespace ofd::audioedit;
using namespace ofd::tabhelp;

// ── タブ固有語彙 (ae_) — file-local 登録 ────────────────────────────────────
namespace {
const bool s_i18n = [] {
    I18n::reg("ae_title", "🎚 音響編集・解析 / Audio editor (内蔵・実動作)",
              "🎚 Audio editor & analysis (built-in)");
    I18n::reg("ae_hint",
        "波形/スペクトログラム編集・信号生成・エフェクト・音響指標解析を"
        "本体内で実行。測定用スイープ生成 → 実測IR読込 → RT算出 → 可聴化まで"
        "一貫。アプリ内のリアルタイム再生・録音は未対応 (再生は OS の"
        "プレーヤーで開く)。",
        "Edit waveforms/spectrograms, generate signals, apply effects and "
        "compute acoustic metrics in-app. Real-time in-app playback and "
        "recording are not implemented; playback opens the OS player.");
    I18n::reg("ae_load", "📁 WAV読込", "📁 Load WAV");
    I18n::reg("ae_record", "⏺ 録音", "⏺ Record");
    I18n::reg("ae_play", "▶ 再生 (OSプレーヤー)", "▶ Play (OS player)");
    I18n::reg("ae_undo", "↶ 元に戻す", "↶ Undo");
    I18n::reg("ae_export", "💾 WAV書出", "💾 Export WAV");
    I18n::reg("ae_view_wave", "波形", "Waveform");
    I18n::reg("ae_view_spec", "スペクトログラム", "Spectrogram");
    I18n::reg("ae_drag_hint", "▸ ドラッグで範囲選択 (編集・解析は選択範囲に適用)",
              "▸ Drag to select a range (edits and analysis apply to it)");
    I18n::reg("ae_clear_sel", "選択解除", "Clear selection");
    I18n::reg("ae_status_init", "信号を生成または読込してください",
              "Generate or load a signal to start");
    I18n::reg("ae_status_undo", "元に戻しました", "Undone");
    I18n::reg("ae_status_loaded", "%1 を読込", "Loaded %1");
    I18n::reg("ae_status_load_fail", "読込失敗: %1 (対応: WAV PCM16/24/32/float)",
              "Load failed: %1 (supported: WAV PCM16/24/32/float)");
    I18n::reg("ae_status_exported", "WAV を書出しました: %1", "Exported WAV: %1");
    I18n::reg("ae_status_export_fail", "書出失敗: %1", "Export failed: %1");
    I18n::reg("ae_status_playing", "OS のプレーヤーで開きました: %1",
              "Opened in the OS player: %1");
    I18n::reg("ae_status_generated", "%1 を生成 (%2s, %3Hz)",
              "Generated %1 (%2s, %3Hz)");
    I18n::reg("ae_status_analyzed", "解析完了", "Analysis complete");
    I18n::reg("ae_status_processing", "処理中…", "Processing…");
    I18n::reg("ae_sel_info", " · 選択 %1s", " · selection %1s");

    // 使い方
    I18n::reg("ae_howto", "📋 使い方 / 代表的な手順", "📋 How-to / typical workflows");
    I18n::reg("ae_howto_goal", "目的", "Goal");
    I18n::reg("ae_howto_steps", "手順", "Steps");
    I18n::reg("ae_h1g", "🏛 ホールのIR実測", "🏛 Measure a hall IR");
    I18n::reg("ae_h1s",
        "①「信号生成」で ESSスイープを生成 → ②「💾 WAV書出」でファイル化し、"
        "外部の再生・録音機材 (またはOSの録音アプリ) で放音・収音 → "
        "③ 録音WAVを「📁 WAV読込」→ ④「解析」で T20/T30/EDT 算出 → "
        "ホール解析タブの実測値と比較 (アプリ内録音は未実装)",
        "① Generate an ESS sweep → ② export it with \"Export WAV\" and "
        "play/record it with external gear (or the OS recorder) → ③ load the "
        "recorded WAV → ④ compute T20/T30/EDT in Analyze and compare with the "
        "hall analysis tab (in-app recording is not implemented)");
    I18n::reg("ae_h2g", "🎧 響きの試聴 (可聴化)", "🎧 Audition reverberance");
    I18n::reg("ae_h2s",
        "①「📁 WAV読込」で乾いた音源 (声・楽器) を開く → "
        "②「エフェクト > リバーブ」にホール解析の RT60 値を入力 → "
        "③「畳み込み適用」→「▶ 再生」でそのホールの響きを試聴",
        "① Load a dry source (voice/instrument) → ② enter the hall's RT60 in "
        "Effects > Reverb → ③ apply convolution and press Play to audition");
    I18n::reg("ae_h3g", "🔇 ノイズ除去", "🔇 Denoise");
    I18n::reg("ae_h3s",
        "① 波形上で無音部 (暗騒音のみ) をドラッグ選択 → "
        "②「エフェクト > ノイズリダクション > 🎙 学習」→ ③ 選択解除して「適用」",
        "① Drag-select a silent (noise-only) part → ② learn it in Effects > "
        "Spectral denoise → ③ clear the selection and apply");
    I18n::reg("ae_h4g", "📊 レベル・周波数確認", "📊 Check levels & spectrum");
    I18n::reg("ae_h4s",
        "① 解析したい範囲をドラッグ選択 (未選択なら全体) → "
        "②「解析」タブで窓関数を選ぶ (レベル計測は Flat-top) → "
        "③「📊 選択範囲を解析」で LUFS/スペクトル/オクターブバンド表示",
        "① Drag-select the range (or none for the whole clip) → ② pick a "
        "window in Analyze (Flat-top for level metering) → ③ run the analysis "
        "for LUFS / spectrum / octave bands");
    I18n::reg("ae_h5g", "✂ 切り出し・整音", "✂ Trim & clean up");
    I18n::reg("ae_h5s",
        "① 残す範囲をドラッグ選択 → ②「編集 > ✂ 切出し」→ ③「ノーマライズ」→ "
        "④「💾 WAV書出」で保存。失敗したら「↶ 元に戻す」",
        "① Drag-select the part to keep → ② Edit > Trim → ③ Normalize → "
        "④ Export WAV. Use Undo if something goes wrong");

    // サブタブ
    I18n::reg("ae_tab_edit", "編集", "Edit");
    I18n::reg("ae_tab_gen", "信号生成", "Generate");
    I18n::reg("ae_tab_fx", "エフェクト", "Effects");
    I18n::reg("ae_tab_analyze", "解析", "Analyze");

    // 編集
    I18n::reg("ae_sec_edit", "基本編集 / Basic edits", "Basic edits");
    I18n::reg("ae_trim", "✂ 選択範囲を切出し", "✂ Trim to selection");
    I18n::reg("ae_delete", "🗑 選択範囲を削除", "🗑 Delete selection");
    I18n::reg("ae_silence", "無音化", "Silence");
    I18n::reg("ae_reverse", "⇄ リバース", "⇄ Reverse");
    I18n::reg("ae_normalize", "📈 ノーマライズ (-0.2dBFS)", "📈 Normalize (-0.2 dBFS)");
    I18n::reg("ae_fadein", "フェードイン", "Fade in");
    I18n::reg("ae_fadeout", "フェードアウト", "Fade out");
    I18n::reg("ae_gain", "ゲイン", "Gain");
    I18n::reg("ae_dc", "DCオフセット除去", "Remove DC offset");
    I18n::reg("ae_smooth", "クリック除去 (平滑)", "De-click (smooth)");
    I18n::reg("ae_st_trim", "選択範囲を切出し (%1s)", "Trimmed to selection (%1s)");
    I18n::reg("ae_st_delete", "選択範囲を削除", "Deleted selection");
    I18n::reg("ae_st_silence", "無音化", "Silenced");
    I18n::reg("ae_st_reverse", "リバース", "Reversed");
    I18n::reg("ae_st_norm", "ノーマライズ (%1dB)", "Normalized (%1 dB)");
    I18n::reg("ae_st_fadein", "フェードイン", "Fade in applied");
    I18n::reg("ae_st_fadeout", "フェードアウト", "Fade out applied");
    I18n::reg("ae_st_gain", "ゲイン %1dB", "Gain %1 dB");
    I18n::reg("ae_st_dc", "DCオフセット除去", "DC offset removed");
    I18n::reg("ae_st_smooth", "スムージング", "Smoothed");
    I18n::reg("ae_ins_dur", "無音挿入", "Insert silence");
    I18n::reg("ae_ins_go", "➕ 挿入 (選択開始/末尾)",
              "➕ Insert (at selection start / end)");
    I18n::reg("ae_st_ins", "無音 %1s を挿入", "Inserted %1 s of silence");
    I18n::reg("ae_rep_count", "回数", "Count");
    I18n::reg("ae_repeat", "🔁 選択範囲をリピート", "🔁 Repeat selection");
    I18n::reg("ae_st_repeat", "選択範囲を %1 回リピート",
              "Repeated selection %1 times");

    // クロスフェード連結
    I18n::reg("ae_sec_concat", "クロスフェード連結 / Crossfade append",
              "Crossfade append");
    I18n::reg("ae_concat_overlap", "重なり", "Overlap");
    I18n::reg("ae_concat_load", "📁 WAVを読込んで末尾に連結",
              "📁 Load WAV & append");
    I18n::reg("ae_concat_note",
        "▸ 現在のバッファ末尾へ読込WAVを等パワー (sin/cos) クロスフェードで"
        "連結。fs が異なる場合は自動でサンプルレート変換します。",
        "▸ Appends the loaded WAV to the current buffer with an equal-power "
        "(sin/cos) crossfade. Sample rates are converted automatically when "
        "they differ.");
    I18n::reg("ae_st_concat", "クロスフェード連結: %1",
              "Crossfaded append: %1");

    // サンプルレート変換
    I18n::reg("ae_sec_src", "サンプルレート変換 / Sample-rate conversion",
              "Sample-rate conversion");
    I18n::reg("ae_src_rate", "変換先", "Target rate");
    I18n::reg("ae_src_custom", "任意…", "Custom…");
    I18n::reg("ae_src_go", "変換", "Convert");
    I18n::reg("ae_src_note",
        "▸ ポリフェーズ Kaiser sinc (阻止域 ~90 dB、群遅延補正) の高品質変換。"
        "音高は変わりません。速度変更 (ピッチ連動) は「エフェクト > 時間軸」。",
        "▸ High-quality polyphase Kaiser sinc (~90 dB stopband, group-delay "
        "compensated). Pitch is preserved; for speed change use Effects > "
        "Time.");
    I18n::reg("ae_st_src", "サンプルレート変換 %1 → %2 Hz",
              "Sample rate converted %1 -> %2 Hz");
    I18n::reg("ae_st_src_fail", "サンプルレート変換失敗: %1",
              "Sample-rate conversion failed: %1");
    I18n::reg("ae_st_src_same", "変換不要 (同一サンプルレート)",
              "No conversion needed (same sample rate)");

    // 信号生成
    I18n::reg("ae_sec_gen", "信号生成 / Signal generator", "Signal generator");
    I18n::reg("ae_gen_kind", "種類", "Kind");
    I18n::reg("ae_gen_sweep", "指数スイープ ESS (IR測定の標準)",
              "Exponential sweep ESS (standard for IR measurement)");
    I18n::reg("ae_gen_linsweep", "線形スイープ", "Linear sweep");
    I18n::reg("ae_gen_sine", "正弦波", "Sine");
    I18n::reg("ae_gen_white", "ホワイトノイズ", "White noise");
    I18n::reg("ae_gen_pink", "ピンクノイズ", "Pink noise");
    I18n::reg("ae_gen_mls", "MLS (擬似ランダム)", "MLS (pseudo-random)");
    I18n::reg("ae_gen_impulse", "単位インパルス", "Unit impulse");
    I18n::reg("ae_gen_click", "クリック (バルーン模擬)", "Click (balloon-pop substitute)");
    I18n::reg("ae_gen_freq", "周波数", "Frequency");
    I18n::reg("ae_gen_freq_to", "〜", "to");
    I18n::reg("ae_gen_len", "長さ", "Length");
    I18n::reg("ae_gen_amp", "振幅", "Amplitude");
    I18n::reg("ae_gen_fs", "サンプリング", "Sample rate");
    I18n::reg("ae_gen_go", "⚡ 生成", "⚡ Generate");
    I18n::reg("ae_gen_hint_sweep",
        "ESS: 逆フィルタ畳込で高調波歪を分離できる — ホール実測の標準手法",
        "ESS: inverse-filter convolution separates harmonic distortion — "
        "the standard method for hall measurements");
    I18n::reg("ae_gen_hint_mls", "MLS: 相関法でIR算出。定常騒音下で有利",
              "MLS: IR via correlation; robust under steady background noise");
    I18n::reg("ae_gen_hint_click", "風船破裂・ピストル音の代替 (簡易測定)",
              "Substitute for balloon pops / starter pistols (quick tests)");
    I18n::reg("ae_gen_sweep_bad",
              "スイープには 0 < 開始周波数 < 終了周波数 が必要です",
              "Sweeps need 0 < start frequency < end frequency");
    I18n::reg("ae_sec_hall", "ホール解析との連携 / Link to hall analysis",
              "Link to hall analysis");
    I18n::reg("ae_hall_hint",
        "生成したスイープを書出して実測に使用 → 録音WAVを読込 → 解析タブで "
        "T20/T30/EDT を算出 → ホール解析の実測欄と比較。",
        "Export the generated sweep for measurement, load the recorded WAV, "
        "compute T20/T30/EDT in the Analyze tab and compare with the hall "
        "analysis measurements.");
    I18n::reg("ae_hall_conv", "🎧 シミュレーションIRで可聴化 (畳み込み)",
              "🎧 Auralize with simulated IR (convolution)");

    // エフェクト
    I18n::reg("ae_sec_filter", "フィルタ / Filters", "Filters");
    I18n::reg("ae_fx_freq", "周波数", "Frequency");
    I18n::reg("ae_fx_gain", "ゲイン", "Gain");
    I18n::reg("ae_fx_eq", "ピーキングEQ", "Peaking EQ");
    I18n::reg("ae_fx_hp", "ハイパス", "High-pass");
    I18n::reg("ae_fx_lp", "ローパス", "Low-pass");
    I18n::reg("ae_fx_ls", "ローシェルフ", "Low shelf");
    I18n::reg("ae_fx_hs", "ハイシェルフ", "High shelf");
    I18n::reg("ae_fx_notch", "ノッチ", "Notch");
    I18n::reg("ae_fx_bp", "バンドパス", "Band-pass");
    I18n::reg("ae_st_eq", "EQ %1Hz %2dB", "EQ %1 Hz %2 dB");
    I18n::reg("ae_st_hp", "ハイパス %1Hz", "High-pass %1 Hz");
    I18n::reg("ae_st_lp", "ローパス %1Hz", "Low-pass %1 Hz");
    I18n::reg("ae_st_ls", "ローシェルフ %1Hz %2dB", "Low shelf %1 Hz %2 dB");
    I18n::reg("ae_st_hs", "ハイシェルフ %1Hz %2dB", "High shelf %1 Hz %2 dB");
    I18n::reg("ae_st_notch", "ノッチ %1Hz", "Notch %1 Hz");
    I18n::reg("ae_st_bp", "バンドパス %1Hz", "Band-pass %1 Hz");
    I18n::reg("ae_sec_dyn", "ダイナミクス / Dynamics", "Dynamics");
    I18n::reg("ae_fx_thr", "閾値", "Threshold");
    I18n::reg("ae_fx_ratio", "レシオ", "Ratio");
    I18n::reg("ae_fx_comp", "コンプレッサ適用", "Apply compressor");
    I18n::reg("ae_st_comp", "コンプレッサ %1dB %2:1", "Compressor %1 dB %2:1");
    I18n::reg("ae_sec_time_fx", "空間系 / Time-based", "Time-based");
    I18n::reg("ae_fx_delay", "ディレイ", "Delay");
    I18n::reg("ae_fx_fb", "FB", "FB");
    I18n::reg("ae_fx_mix", "Mix", "Mix");
    I18n::reg("ae_fx_apply", "適用", "Apply");
    I18n::reg("ae_st_delay", "ディレイ %1ms", "Delay %1 ms");
    I18n::reg("ae_fx_reverb", "リバーブ", "Reverb");
    I18n::reg("ae_fx_conv", "畳み込み適用", "Apply convolution");
    I18n::reg("ae_st_reverb", "畳み込みリバーブ RT=%1s", "Convolution reverb RT=%1 s");
    I18n::reg("ae_reverb_note",
        "▸ IRを合成して畳み込みます (ConvolutionEngine)。ホール解析のRT値を"
        "入れれば、そのホールの響きを試聴できます。",
        "▸ Synthesizes an IR and convolves it (ConvolutionEngine). Enter the "
        "RT from the hall analysis to audition that hall's reverberance.");
    I18n::reg("ae_sec_timescale", "時間軸 / Time", "Time");
    I18n::reg("ae_fx_rate", "速度", "Rate");
    I18n::reg("ae_fx_rate_apply", "適用 (ピッチ連動)", "Apply (pitch follows)");
    I18n::reg("ae_st_rate", "速度 ×%1", "Rate ×%1");
    I18n::reg("ae_fx_pitch", "ピッチ", "Pitch");
    I18n::reg("ae_fx_semi", "半音", "semitones");
    I18n::reg("ae_fx_pitch_apply", "ピッチシフト (長さ保持)",
              "Pitch shift (keeps length)");
    I18n::reg("ae_st_pitch", "ピッチ %1半音 (長さ保持)",
              "Pitch %1 semitones (length kept)");
    I18n::reg("ae_fx_stretch", "ストレッチ", "Stretch");
    I18n::reg("ae_fx_stretch_apply", "タイムストレッチ (ピッチ保持)",
              "Time stretch (keeps pitch)");
    I18n::reg("ae_st_stretch", "タイムストレッチ ×%1 (ピッチ保持)",
              "Time stretch ×%1 (pitch kept)");
    I18n::reg("ae_timescale_note",
        "▸ OLA/グラニュラー方式。可聴化の再生速度調整やドップラー模擬に。",
        "▸ OLA / granular method. For playback-speed adjustment of "
        "auralizations and Doppler emulation.");
    I18n::reg("ae_sec_nr", "ノイズリダクション / Spectral denoise",
              "Spectral denoise");
    I18n::reg("ae_nr_learn", "🎙 選択範囲をノイズとして学習",
              "🎙 Learn noise from selection");
    I18n::reg("ae_nr_amount", "低減量", "Reduction");
    I18n::reg("ae_nr_apply", "適用", "Apply");
    I18n::reg("ae_st_nr_learn", "ノイズプロファイルを学習 (選択範囲)",
              "Noise profile learned from selection");
    I18n::reg("ae_st_nr_fail",
              "学習失敗 — 選択範囲が短すぎます (2048 サンプル以上必要)",
              "Learning failed — selection too short (needs ≥ 2048 samples)");
    I18n::reg("ae_st_nr", "ノイズ除去 -%1dB 適用", "Denoise -%1 dB applied");
    I18n::reg("ae_nr_note",
        "▸ 手順: 無音部 (暗騒音のみ) を選択→学習→適用。スペクトルゲート方式 "
        "(2048点, 75%OL)。",
        "▸ Steps: select a silent (noise-only) part → learn → apply. "
        "Spectral gate (2048 pt, 75% OL).");
    I18n::reg("ae_sec_stereo", "ステレオ / Stereo & phase", "Stereo & phase");
    I18n::reg("ae_stereo_mono", "モノラル化", "Mono");
    I18n::reg("ae_stereo_swap", "L/R入替", "Swap L/R");
    I18n::reg("ae_stereo_side", "Side抽出", "Extract side");
    I18n::reg("ae_stereo_widen", "ワイド化 ×1.6", "Widen ×1.6");
    I18n::reg("ae_stereo_invl", "L位相反転", "Invert L phase");

    // 解析
    I18n::reg("ae_sec_win", "窓関数 / Window function", "Window function");
    I18n::reg("ae_win", "窓", "Window");
    I18n::reg("ae_win_info", "メインローブ %1 bin · サイドローブ %2 dB · %3",
              "Main lobe %1 bin · sidelobe %2 dB · %3");
    I18n::reg("ae_win_note",
        "▸ スペクトログラム表示とスペクトル解析の両方に適用。レベル計測は "
        "Flat-top、微小信号検出は Blackman-Harris 推奨。",
        "▸ Applies to both the spectrogram and spectrum analysis. Flat-top "
        "for level metering, Blackman-Harris for low-level detection.");
    I18n::reg("ae_sec_analyze", "解析 / Analysis", "Analysis");
    I18n::reg("ae_analyze_go", "📊 選択範囲を解析", "📊 Analyze selection");
    I18n::reg("ae_analyze_hint", "スペクトル + レベル + Schroeder残響指標",
              "Spectrum + levels + Schroeder reverberation metrics");
    I18n::reg("ae_m_metric", "指標", "Metric");
    I18n::reg("ae_m_value", "値", "Value");
    I18n::reg("ae_m_note", "備考", "Notes");
    I18n::reg("ae_m_dur", "長さ", "Duration");
    I18n::reg("ae_m_dur_note", "選択範囲", "Selected range");
    I18n::reg("ae_m_peak", "ピークレベル", "Peak level");
    I18n::reg("ae_m_clip", "クリップ疑い", "Possible clipping");
    I18n::reg("ae_m_ok", "OK", "OK");
    I18n::reg("ae_m_rms", "RMSレベル", "RMS level");
    I18n::reg("ae_m_rms_note", "実効値", "Effective value");
    I18n::reg("ae_m_crest", "クレストファクタ", "Crest factor");
    I18n::reg("ae_m_impulsive", "衝撃音的", "Impulsive");
    I18n::reg("ae_m_steady", "定常的", "Steady");
    I18n::reg("ae_m_dc", "DCオフセット", "DC offset");
    I18n::reg("ae_m_dc_bad", "除去推奨", "Removal recommended");
    I18n::reg("ae_m_dc_ok", "問題なし", "No issue");
    I18n::reg("ae_m_edt", "EDT (T10×6)", "EDT (T10×6)");
    I18n::reg("ae_m_ir_note", "IRとみなした場合", "Treating range as an IR");
    I18n::reg("ae_m_t20_note", "-5〜-25dB × 3", "-5 to -25 dB × 3");
    I18n::reg("ae_m_t30_note", "-5〜-35dB × 2", "-5 to -35 dB × 2");
    I18n::reg("ae_m_ratio", "T20/T30 比", "T20/T30 ratio");
    I18n::reg("ae_m_nonlinear", "非線形減衰 (カップリング疑い)",
              "Non-linear decay (possible coupling)");
    I18n::reg("ae_m_linear", "線形減衰", "Linear decay");
    I18n::reg("ae_m_range_short", "減衰が不足 (範囲を IR に合わせて選択)",
              "Insufficient decay (select an IR-like range)");
    I18n::reg("ae_sec_spectrum", "スペクトル / Spectrum", "Spectrum");
    I18n::reg("ae_spectrum_note", "▸ %1窓 4096点FFT。対数周波数軸。",
              "▸ %1 window, 4096-pt FFT. Log frequency axis.");
    I18n::reg("ae_freq_axis", "f [Hz]", "f [Hz]");
    I18n::reg("ae_db_axis", "dB", "dB");
    I18n::reg("ae_sec_loud", "ラウドネス / Loudness (ITU-R BS.1770)",
              "Loudness (ITU-R BS.1770)");
    I18n::reg("ae_l_target", "目安", "Guideline");
    I18n::reg("ae_l_integrated", "統合ラウドネス", "Integrated loudness");
    I18n::reg("ae_l_integrated_note", "配信 -14 / 放送 -24",
              "Streaming -14 / broadcast -24");
    I18n::reg("ae_l_range", "ラウドネスレンジ LRA", "Loudness range LRA");
    I18n::reg("ae_l_range_note", "音楽 6〜12 LU", "Music 6-12 LU");
    I18n::reg("ae_l_mom", "モーメンタリー最大", "Momentary max");
    I18n::reg("ae_l_tp", "True Peak (簡易4x OS)", "True peak (simple 4x OS)");
    I18n::reg("ae_l_tp_over", "⚠ -1dBTP超過", "⚠ exceeds -1 dBTP");
    I18n::reg("ae_l_tp_ok", "OK (≦-1dBTP)", "OK (≤ -1 dBTP)");
    I18n::reg("ae_sec_bands", "オクターブバンド / 1/1 octave (IEC 61260)",
              "1/1 octave bands (IEC 61260)");
    I18n::reg("ae_bands_note",
        "▸ 中心周波数 [Hz] 別相対レベル [dB]。吸音材選定・NC曲線照合に使用。",
        "▸ Relative level [dB] per center frequency [Hz]. For absorber "
        "selection and NC-curve checks.");
    return true;
}();

// 使い方テーブル (実装済みの手順のみ — 録音は未実装のため外部録音を案内)
struct HowTo { const char *goalKey; const char *stepsKey; };
const HowTo kHowTo[] = {
    { "ae_h1g", "ae_h1s" },
    { "ae_h2g", "ae_h2s" },
    { "ae_h3g", "ae_h3s" },
    { "ae_h4g", "ae_h4s" },
    { "ae_h5g", "ae_h5s" },
};

QTableWidget *makeTable(QWidget *parent, const QStringList &headers)
{
    auto *t = new QTableWidget(0, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    return t;
}

void fitTable(QTableWidget *t)
{
    t->resizeRowsToContents();
    int h = t->horizontalHeader()->height() + 2;
    for (int r = 0; r < t->rowCount(); ++r) h += t->rowHeight(r);
    t->setFixedHeight(h + 4);
}

QDoubleSpinBox *spin(QWidget *parent, double lo, double hi, double val,
                     int decimals = 1, double step = 1.0)
{
    auto *s = new QDoubleSpinBox(parent);
    s->setRange(lo, hi);
    s->setDecimals(decimals);
    s->setSingleStep(step);
    s->setValue(val);
    return s;
}

} // namespace

// ── construction ────────────────────────────────────────────────────────────
AudioEditorTab::AudioEditorTab(Project *project, QWidget *parent)
    : QScrollArea(parent), m_p(project)
{
    auto *body = new QWidget(this);
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    // ── ヘッダ: 概要 + ツールバー + 波形ビュー ─────────────────────────────
    auto *sTop = new SectionBox(I18n::tr("ae_title"), body);
    auto *hint = new QLabel(I18n::tr("ae_hint"), sTop);
    hint->setWordWrap(true);
    sTop->vbox()->addWidget(hint);

    auto *bar = new QHBoxLayout();
    auto *btnLoad = new QPushButton(I18n::tr("ae_load"), sTop);
    auto *btnRec = new QPushButton(I18n::tr("ae_record"), sTop);
    markNotImplemented(btnRec);   // アプリ内録音は未実装 (Qt Multimedia 不使用)
    m_btnPlay = new QPushButton(I18n::tr("ae_play"), sTop);
    m_btnUndo = new QPushButton(I18n::tr("ae_undo"), sTop);
    m_btnExport = new QPushButton(I18n::tr("ae_export"), sTop);
    bar->addWidget(btnLoad);
    bar->addWidget(btnRec);
    bar->addWidget(m_btnPlay);
    bar->addWidget(m_btnUndo);
    bar->addWidget(m_btnExport);
    bar->addStretch(1);
    sTop->vbox()->addLayout(bar);

    auto *viewRow = new QHBoxLayout();
    m_viewMode = new QComboBox(sTop);
    m_viewMode->addItem(I18n::tr("ae_view_wave"));
    m_viewMode->addItem(I18n::tr("ae_view_spec"));
    viewRow->addWidget(m_viewMode);
    m_info = new QLabel(QStringLiteral("—"), sTop);
    viewRow->addWidget(m_info, 1);
    sTop->vbox()->addLayout(viewRow);

    m_view = new AudioWaveformView(sTop);
    sTop->vbox()->addWidget(m_view);

    auto *selRow = new QHBoxLayout();
    auto *dragHint = new QLabel(I18n::tr("ae_drag_hint"), sTop);
    selRow->addWidget(dragHint);
    m_btnClearSel = new QPushButton(I18n::tr("ae_clear_sel"), sTop);
    m_btnClearSel->setVisible(false);
    selRow->addWidget(m_btnClearSel);
    selRow->addStretch(1);
    m_status = new QLabel(I18n::tr("ae_status_init"), sTop);
    m_status->setWordWrap(true);
    selRow->addWidget(m_status);
    sTop->vbox()->addLayout(selRow);
    v->addWidget(sTop);

    // ── 使い方 ──────────────────────────────────────────────────────────────
    auto *sHow = new SectionBox(I18n::tr("ae_howto"), body);
    auto *howTable = makeTable(sHow,
        { I18n::tr("ae_howto_goal"), I18n::tr("ae_howto_steps") });
    for (const HowTo &h : kHowTo) {
        const int r = howTable->rowCount();
        howTable->insertRow(r);
        howTable->setItem(r, 0, roItem(I18n::tr(h.goalKey)));
        howTable->setItem(r, 1, roItem(I18n::tr(h.stepsKey)));
    }
    howTable->setWordWrap(true);
    fitTable(howTable);
    sHow->vbox()->addWidget(howTable);
    v->addWidget(sHow);

    // ── サブタブ ────────────────────────────────────────────────────────────
    auto *tabs = new QTabWidget(body);
    v->addWidget(tabs);

    // ============ 編集 ============
    auto *pageEdit = new QWidget(tabs);
    auto *ve = new QVBoxLayout(pageEdit);
    {
        auto *s = new SectionBox(I18n::tr("ae_sec_edit"), pageEdit);
        auto *r1 = new QHBoxLayout();
        auto *btnTrim = new QPushButton(I18n::tr("ae_trim"), s);
        auto *btnDel = new QPushButton(I18n::tr("ae_delete"), s);
        auto *btnSil = new QPushButton(I18n::tr("ae_silence"), s);
        auto *btnRev = new QPushButton(I18n::tr("ae_reverse"), s);
        r1->addWidget(btnTrim); r1->addWidget(btnDel);
        r1->addWidget(btnSil);  r1->addWidget(btnRev);
        r1->addStretch(1);
        s->vbox()->addLayout(r1);
        auto *r2 = new QHBoxLayout();
        auto *btnNorm = new QPushButton(I18n::tr("ae_normalize"), s);
        auto *btnFi = new QPushButton(I18n::tr("ae_fadein"), s);
        auto *btnFo = new QPushButton(I18n::tr("ae_fadeout"), s);
        r2->addWidget(btnNorm); r2->addWidget(btnFi); r2->addWidget(btnFo);
        r2->addStretch(1);
        s->vbox()->addLayout(r2);
        auto *r3 = new QHBoxLayout();
        r3->addWidget(new QLabel(I18n::tr("ae_gain") + ":", s));
        for (int gDb : { -12, -6, -3, 3, 6, 12 }) {
            auto *b = new QPushButton(
                (gDb > 0 ? QStringLiteral("+%1dB") : QStringLiteral("%1dB"))
                    .arg(gDb), s);
            connect(b, &QPushButton::clicked, this, [this, gDb] {
                if (!hasBuf()) return;
                const auto [a, z] = range();
                pushBuffer(gainRange(m_buf, a, z, gDb),
                    I18n::tr("ae_st_gain").arg(gDb > 0
                        ? QStringLiteral("+%1").arg(gDb)
                        : QString::number(gDb)));
            });
            m_needBuf.push_back(b);
            r3->addWidget(b);
        }
        r3->addStretch(1);
        s->vbox()->addLayout(r3);
        auto *r4 = new QHBoxLayout();
        auto *btnDc = new QPushButton(I18n::tr("ae_dc"), s);
        auto *btnSm = new QPushButton(I18n::tr("ae_smooth"), s);
        r4->addWidget(btnDc); r4->addWidget(btnSm);
        r4->addStretch(1);
        s->vbox()->addLayout(r4);
        // 無音挿入 (選択開始位置、未選択時は末尾) + 選択範囲のリピート展開
        auto *r5 = new QHBoxLayout();
        r5->addWidget(new QLabel(I18n::tr("ae_ins_dur") + QStringLiteral(":"),
                                 s));
        auto *insDur = spin(s, 0.01, 600, 1.0, 2, 0.1);
        r5->addWidget(insDur);
        r5->addWidget(new QLabel(QStringLiteral("s"), s));
        auto *btnIns = new QPushButton(I18n::tr("ae_ins_go"), s);
        r5->addWidget(btnIns);
        r5->addSpacing(12);
        r5->addWidget(new QLabel(I18n::tr("ae_rep_count") +
                                 QStringLiteral(":"), s));
        auto *repCount = spin(s, 2, 99, 4, 0, 1);
        r5->addWidget(repCount);
        auto *btnRep = new QPushButton(I18n::tr("ae_repeat"), s);
        r5->addWidget(btnRep);
        r5->addStretch(1);
        s->vbox()->addLayout(r5);
        ve->addWidget(s);

        // クロスフェード連結 (2 つ目の WAV を読込んで末尾へ結合)
        auto *sx = new SectionBox(I18n::tr("ae_sec_concat"), pageEdit);
        auto *xr = new QHBoxLayout();
        xr->addWidget(new QLabel(I18n::tr("ae_concat_overlap"), sx));
        auto *xfOv = spin(sx, 0.0, 10.0, 0.05, 2, 0.01);
        xr->addWidget(xfOv);
        xr->addWidget(new QLabel(QStringLiteral("s"), sx));
        auto *btnConcat = new QPushButton(I18n::tr("ae_concat_load"), sx);
        xr->addWidget(btnConcat);
        xr->addStretch(1);
        sx->vbox()->addLayout(xr);
        auto *xNote = new QLabel(I18n::tr("ae_concat_note"), sx);
        xNote->setWordWrap(true);
        sx->vbox()->addWidget(xNote);
        ve->addWidget(sx);

        // サンプルレート変換 (音響コアのポリフェーズ Kaiser sinc)
        auto *ssr = new SectionBox(I18n::tr("ae_sec_src"), pageEdit);
        auto *rr = new QHBoxLayout();
        auto *srcCombo = new QComboBox(ssr);
        for (int fs : { 44100, 48000, 88200, 96000 })
            srcCombo->addItem(QStringLiteral("%1 Hz").arg(fs), fs);
        srcCombo->addItem(I18n::tr("ae_src_custom"), 0);
        srcCombo->setCurrentIndex(1);   // 48 kHz 既定
        rr->addWidget(srcCombo);
        auto *srcSpin = spin(ssr, 1000, 384000, 48000, 0, 1000);
        srcSpin->setEnabled(false);
        rr->addWidget(srcSpin);
        rr->addWidget(new QLabel(QStringLiteral("Hz"), ssr));
        auto *btnSrc = new QPushButton(I18n::tr("ae_src_go"), ssr);
        rr->addWidget(btnSrc);
        rr->addStretch(1);
        ssr->form()->addRow(I18n::tr("ae_src_rate"), rr);
        auto *srcNote = new QLabel(I18n::tr("ae_src_note"), ssr);
        srcNote->setWordWrap(true);
        ssr->vbox()->addWidget(srcNote);
        ve->addWidget(ssr);
        ve->addStretch(1);

        m_needSel.insert(m_needSel.end(), { btnTrim, btnDel, btnRep });
        m_needBuf.insert(m_needBuf.end(),
            { btnSil, btnRev, btnNorm, btnFi, btnFo, btnDc, btnSm,
              btnIns, btnConcat, btnSrc });

        connect(btnIns, &QPushButton::clicked, this, [this, insDur] {
            if (!hasBuf()) return;
            // 挿入位置 = 選択開始 (未選択時は末尾)
            const std::size_t at = m_view->hasSelection()
                ? m_view->selectionStart() : m_buf.sampleCount();
            pushBuffer(insertSilence(m_buf, at, insDur->value()),
                       I18n::tr("ae_st_ins").arg(insDur->value(), 0, 'f', 2));
        });
        connect(btnRep, &QPushButton::clicked, this, [this, repCount] {
            if (!hasBuf() || !m_view->hasSelection()) return;
            const auto [a, z] = range();
            const int cnt = static_cast<int>(repCount->value());
            pushBuffer(repeatRange(m_buf, a, z, cnt),
                       I18n::tr("ae_st_repeat").arg(cnt));
            m_view->clearSelection();
        });
        connect(btnConcat, &QPushButton::clicked, this, [this, xfOv] {
            if (!hasBuf() || m_busy) return;
            const QString path = QFileDialog::getOpenFileName(this,
                I18n::tr("ae_concat_load"), QString(),
                QStringLiteral("WAV (*.wav)"));
            if (path.isEmpty()) return;
            const acoustics::AcousticResult<acoustics::AudioBuffer> res =
                acoustics::readWavFile(path.toStdString());
            if (!res.success()) {
                setStatus(I18n::tr("ae_status_load_fail")
                              .arg(QString::fromStdString(res.message())));
                return;
            }
            // fs 不一致時のリサンプルは秒単位になり得る — 非同期で実行
            const audioedit::AudioBuffer bufA = m_buf;
            const audioedit::AudioBuffer bufB = res.value();
            const double ov = xfOv->value();
            runHeavy([bufA, bufB, ov] {
                         return crossfadeConcat(bufA, bufB, ov);
                     },
                     I18n::tr("ae_st_concat").arg(QFileInfo(path).fileName()));
        });
        connect(srcCombo, &QComboBox::currentIndexChanged, this,
                [srcCombo, srcSpin](int) {
            srcSpin->setEnabled(srcCombo->currentData().toInt() == 0);
        });
        connect(btnSrc, &QPushButton::clicked, this,
                [this, srcCombo, srcSpin] {
            if (!hasBuf() || m_busy) return;
            const int preset = srcCombo->currentData().toInt();
            const double dst =
                (preset > 0) ? double(preset) : srcSpin->value();
            const double src = m_buf.sampleRateHz;
            if (dst == src) {   // 恒等変換は undo を積まない
                setStatus(I18n::tr("ae_st_src_same"));
                return;
            }
            // resampleTo は失敗時に入力を変更せず error を返す —
            // 成功したときだけバッファを差し替える (成功と偽らない)
            m_busy = true;
            setStatus(I18n::tr("ae_status_processing"));
            updateInfo();
            auto result = std::make_shared<audioedit::AudioBuffer>();
            auto err = std::make_shared<std::string>();
            const audioedit::AudioBuffer buf = m_buf;
            QThread *th = QThread::create([buf, dst, result, err] {
                *result = audioedit::resampleTo(buf, dst, err.get());
            });
            connect(th, &QThread::finished, this,
                    [this, th, result, err, src, dst] {
                th->deleteLater();
                m_busy = false;
                if (!err->empty()) {
                    setStatus(I18n::tr("ae_st_src_fail")
                                  .arg(QString::fromStdString(*err)));
                    updateInfo();
                } else {
                    pushBuffer(std::move(*result),
                               I18n::tr("ae_st_src")
                                   .arg(src, 0, 'f', 0).arg(dst, 0, 'f', 0));
                }
            });
            th->start();
        });

        connect(btnTrim, &QPushButton::clicked, this, [this] {
            if (!hasBuf() || !m_view->hasSelection()) return;
            const auto [a, z] = range();
            const double sec = (z - a) / m_buf.sampleRateHz;
            pushBuffer(trimToRange(m_buf, a, z),
                       I18n::tr("ae_st_trim").arg(sec, 0, 'f', 3));
            m_view->clearSelection();
        });
        connect(btnDel, &QPushButton::clicked, this, [this] {
            if (!hasBuf() || !m_view->hasSelection()) return;
            const auto [a, z] = range();
            pushBuffer(deleteRange(m_buf, a, z), I18n::tr("ae_st_delete"));
            m_view->clearSelection();
        });
        connect(btnSil, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            const auto [a, z] = range();
            pushBuffer(silenceRange(m_buf, a, z), I18n::tr("ae_st_silence"));
        });
        connect(btnRev, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            const auto [a, z] = range();
            pushBuffer(reverseRange(m_buf, a, z), I18n::tr("ae_st_reverse"));
        });
        connect(btnNorm, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            const auto [a, z] = range();
            double gainDb = 0.0;
            audioedit::AudioBuffer b =
                normalizeRange(m_buf, a, z, 0.98, &gainDb);
            pushBuffer(std::move(b),
                       I18n::tr("ae_st_norm").arg(gainDb, 0, 'f', 1));
        });
        connect(btnFi, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            const auto [a, z] = range();
            pushBuffer(fadeRange(m_buf, a, z, true), I18n::tr("ae_st_fadein"));
        });
        connect(btnFo, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            const auto [a, z] = range();
            pushBuffer(fadeRange(m_buf, a, z, false), I18n::tr("ae_st_fadeout"));
        });
        connect(btnDc, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            const auto [a, z] = range();
            pushBuffer(removeDcRange(m_buf, a, z), I18n::tr("ae_st_dc"));
        });
        connect(btnSm, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            const auto [a, z] = range();
            pushBuffer(smoothRange(m_buf, a, z), I18n::tr("ae_st_smooth"));
        });
    }
    tabs->addTab(pageEdit, I18n::tr("ae_tab_edit"));

    // ============ 信号生成 ============
    auto *pageGen = new QWidget(tabs);
    auto *vg = new QVBoxLayout(pageGen);
    {
        auto *s = new SectionBox(I18n::tr("ae_sec_gen"), pageGen);
        m_genKind = new QComboBox(s);
        m_genKind->addItem(I18n::tr("ae_gen_sweep"));      // 0 ExpSweep
        m_genKind->addItem(I18n::tr("ae_gen_linsweep"));   // 1 LinSweep
        m_genKind->addItem(I18n::tr("ae_gen_sine"));       // 2 Sine
        m_genKind->addItem(I18n::tr("ae_gen_white"));      // 3 White
        m_genKind->addItem(I18n::tr("ae_gen_pink"));       // 4 Pink
        m_genKind->addItem(I18n::tr("ae_gen_mls"));        // 5 Mls
        m_genKind->addItem(I18n::tr("ae_gen_impulse"));    // 6 Impulse
        m_genKind->addItem(I18n::tr("ae_gen_click"));      // 7 Click
        s->form()->addRow(I18n::tr("ae_gen_kind"), m_genKind);

        auto *fr = new QHBoxLayout();
        m_genF1 = spin(s, 1, 96000, 20, 0, 10);
        m_genF2 = spin(s, 1, 96000, 20000, 0, 100);
        fr->addWidget(m_genF1);
        fr->addWidget(new QLabel(I18n::tr("ae_gen_freq_to"), s));
        fr->addWidget(m_genF2);
        fr->addWidget(new QLabel(QStringLiteral("Hz"), s));
        fr->addStretch(1);
        s->form()->addRow(I18n::tr("ae_gen_freq"), fr);

        auto *lr = new QHBoxLayout();
        m_genDur = spin(s, 0.01, 600, 3, 2, 0.5);
        lr->addWidget(m_genDur);
        lr->addWidget(new QLabel(QStringLiteral("s"), s));
        lr->addSpacing(12);
        lr->addWidget(new QLabel(I18n::tr("ae_gen_amp"), s));
        m_genAmp = spin(s, 0.0, 1.0, 0.7, 2, 0.05);
        lr->addWidget(m_genAmp);
        lr->addStretch(1);
        s->form()->addRow(I18n::tr("ae_gen_len"), lr);

        auto *gr = new QHBoxLayout();
        auto *btnGen = new QPushButton(I18n::tr("ae_gen_go"), s);
        gr->addWidget(btnGen);
        m_genHint = new QLabel(I18n::tr("ae_gen_hint_sweep"), s);
        m_genHint->setWordWrap(true);
        gr->addWidget(m_genHint, 1);
        s->vbox()->addLayout(gr);
        vg->addWidget(s);

        connect(btnGen, &QPushButton::clicked, this,
                &AudioEditorTab::generateSignal);
        connect(m_genKind, &QComboBox::currentIndexChanged, this, [this](int i) {
            m_genHint->setText(i == 0 ? I18n::tr("ae_gen_hint_sweep")
                             : i == 5 ? I18n::tr("ae_gen_hint_mls")
                             : i == 7 ? I18n::tr("ae_gen_hint_click")
                                      : QString());
        });

        auto *sHall = new SectionBox(I18n::tr("ae_sec_hall"), pageGen);
        auto *hallHint = new QLabel(I18n::tr("ae_hall_hint"), sHall);
        hallHint->setWordWrap(true);
        sHall->vbox()->addWidget(hallHint);
        auto *btnSim = new QPushButton(I18n::tr("ae_hall_conv"), sHall);
        m_needBuf.push_back(btnSim);
        sHall->vbox()->addWidget(btnSim, 0, Qt::AlignLeft);
        vg->addWidget(sHall);
        vg->addStretch(1);
        connect(btnSim, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            // mock の applySimIR: RT=2.0s の合成 IR で可聴化
            m_revRT->setValue(2.0);
            const audioedit::AudioBuffer buf = m_buf;
            const double mix = m_revMix->value();
            runHeavy([buf, mix] { return applyReverb(buf, 2.0, mix); },
                     I18n::tr("ae_st_reverb").arg(2.0, 0, 'f', 1));
        });
    }
    tabs->addTab(pageGen, I18n::tr("ae_tab_gen"));

    // ============ エフェクト ============
    auto *pageFx = new QWidget(tabs);
    auto *vf = new QVBoxLayout(pageFx);
    {
        // フィルタ
        auto *s = new SectionBox(I18n::tr("ae_sec_filter"), pageFx);
        auto *fr = new QHBoxLayout();
        m_eqF = spin(s, 10, 96000, 1000, 0, 100);
        fr->addWidget(m_eqF);
        fr->addWidget(new QLabel(QStringLiteral("Hz · Q"), s));
        m_eqQ = spin(s, 0.1, 30, 1, 2, 0.1);
        fr->addWidget(m_eqQ);
        fr->addWidget(new QLabel(I18n::tr("ae_fx_gain"), s));
        m_eqG = spin(s, -40, 40, 0, 1, 1);
        fr->addWidget(m_eqG);
        fr->addWidget(new QLabel(QStringLiteral("dB"), s));
        fr->addStretch(1);
        s->form()->addRow(I18n::tr("ae_fx_freq"), fr);
        auto *br = new QHBoxLayout();
        auto *btnEq = new QPushButton(I18n::tr("ae_fx_eq"), s);
        auto *btnHp = new QPushButton(I18n::tr("ae_fx_hp"), s);
        auto *btnLp = new QPushButton(I18n::tr("ae_fx_lp"), s);
        br->addWidget(btnEq); br->addWidget(btnHp); br->addWidget(btnLp);
        br->addStretch(1);
        s->vbox()->addLayout(br);
        // RBJ Cookbook の補完分 (シェルフ / ノッチ / バンドパス)
        auto *br2 = new QHBoxLayout();
        auto *btnLs = new QPushButton(I18n::tr("ae_fx_ls"), s);
        auto *btnHs = new QPushButton(I18n::tr("ae_fx_hs"), s);
        auto *btnNotch = new QPushButton(I18n::tr("ae_fx_notch"), s);
        auto *btnBp = new QPushButton(I18n::tr("ae_fx_bp"), s);
        br2->addWidget(btnLs); br2->addWidget(btnHs);
        br2->addWidget(btnNotch); br2->addWidget(btnBp);
        br2->addStretch(1);
        s->vbox()->addLayout(br2);
        vf->addWidget(s);
        m_needBuf.insert(m_needBuf.end(),
            { btnEq, btnHp, btnLp, btnLs, btnHs, btnNotch, btnBp });
        // ゲイン表示の符号付き整形 (+6 / -6)
        auto fmtDb = [](double v) {
            return v > 0 ? QStringLiteral("+%1").arg(v, 0, 'f', 0)
                         : QString::number(v, 'f', 0);
        };
        connect(btnLs, &QPushButton::clicked, this, [this, fmtDb] {
            if (!hasBuf()) return;
            pushBuffer(applyBiquad(m_buf, BiquadKind::LowShelf,
                                   m_eqF->value(), m_eqQ->value(),
                                   m_eqG->value()),
                I18n::tr("ae_st_ls").arg(m_eqF->value(), 0, 'f', 0)
                                    .arg(fmtDb(m_eqG->value())));
        });
        connect(btnHs, &QPushButton::clicked, this, [this, fmtDb] {
            if (!hasBuf()) return;
            pushBuffer(applyBiquad(m_buf, BiquadKind::HighShelf,
                                   m_eqF->value(), m_eqQ->value(),
                                   m_eqG->value()),
                I18n::tr("ae_st_hs").arg(m_eqF->value(), 0, 'f', 0)
                                    .arg(fmtDb(m_eqG->value())));
        });
        connect(btnNotch, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            pushBuffer(applyBiquad(m_buf, BiquadKind::Notch, m_eqF->value(),
                                   m_eqQ->value(), 0),
                       I18n::tr("ae_st_notch").arg(m_eqF->value(), 0, 'f', 0));
        });
        connect(btnBp, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            pushBuffer(applyBiquad(m_buf, BiquadKind::BandPass, m_eqF->value(),
                                   m_eqQ->value(), 0),
                       I18n::tr("ae_st_bp").arg(m_eqF->value(), 0, 'f', 0));
        });
        connect(btnEq, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            pushBuffer(applyBiquad(m_buf, BiquadKind::Peaking, m_eqF->value(),
                                   m_eqQ->value(), m_eqG->value()),
                I18n::tr("ae_st_eq").arg(m_eqF->value(), 0, 'f', 0)
                    .arg(m_eqG->value() > 0
                        ? QStringLiteral("+%1").arg(m_eqG->value(), 0, 'f', 0)
                        : QString::number(m_eqG->value(), 'f', 0)));
        });
        connect(btnHp, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            pushBuffer(applyBiquad(m_buf, BiquadKind::HighPass, m_eqF->value(),
                                   m_eqQ->value(), 0),
                       I18n::tr("ae_st_hp").arg(m_eqF->value(), 0, 'f', 0));
        });
        connect(btnLp, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            pushBuffer(applyBiquad(m_buf, BiquadKind::LowPass, m_eqF->value(),
                                   m_eqQ->value(), 0),
                       I18n::tr("ae_st_lp").arg(m_eqF->value(), 0, 'f', 0));
        });

        // ダイナミクス
        auto *sd = new SectionBox(I18n::tr("ae_sec_dyn"), pageFx);
        auto *dr = new QHBoxLayout();
        m_thr = spin(sd, -80, 0, -24, 0, 1);
        dr->addWidget(m_thr);
        dr->addWidget(new QLabel(QStringLiteral("dB ·"), sd));
        dr->addWidget(new QLabel(I18n::tr("ae_fx_ratio"), sd));
        m_ratio = spin(sd, 1, 40, 4, 1, 0.5);
        dr->addWidget(m_ratio);
        dr->addWidget(new QLabel(QStringLiteral(":1"), sd));
        auto *btnComp = new QPushButton(I18n::tr("ae_fx_comp"), sd);
        dr->addWidget(btnComp);
        dr->addStretch(1);
        sd->form()->addRow(I18n::tr("ae_fx_thr"), dr);
        vf->addWidget(sd);
        m_needBuf.push_back(btnComp);
        connect(btnComp, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            pushBuffer(applyCompressor(m_buf, m_thr->value(), m_ratio->value()),
                I18n::tr("ae_st_comp").arg(m_thr->value(), 0, 'f', 0)
                                      .arg(m_ratio->value(), 0, 'f', 0));
        });

        // 空間系
        auto *st = new SectionBox(I18n::tr("ae_sec_time_fx"), pageFx);
        auto *tr1 = new QHBoxLayout();
        m_dly = spin(st, 1, 5000, 250, 0, 10);
        tr1->addWidget(m_dly);
        tr1->addWidget(new QLabel(QStringLiteral("ms ·"), st));
        tr1->addWidget(new QLabel(I18n::tr("ae_fx_fb"), st));
        m_fb = spin(st, 0, 0.95, 0.35, 2, 0.05);
        tr1->addWidget(m_fb);
        tr1->addWidget(new QLabel(I18n::tr("ae_fx_mix"), st));
        m_mix = spin(st, 0, 1, 0.3, 2, 0.05);
        tr1->addWidget(m_mix);
        auto *btnDelay = new QPushButton(I18n::tr("ae_fx_apply"), st);
        tr1->addWidget(btnDelay);
        tr1->addStretch(1);
        st->form()->addRow(I18n::tr("ae_fx_delay"), tr1);
        auto *tr2 = new QHBoxLayout();
        tr2->addWidget(new QLabel(QStringLiteral("RT60"), st));
        m_revRT = spin(st, 0.1, 20, 1.8, 1, 0.1);
        tr2->addWidget(m_revRT);
        tr2->addWidget(new QLabel(QStringLiteral("s ·"), st));
        tr2->addWidget(new QLabel(I18n::tr("ae_fx_mix"), st));
        m_revMix = spin(st, 0, 1, 0.35, 2, 0.05);
        tr2->addWidget(m_revMix);
        auto *btnRev2 = new QPushButton(I18n::tr("ae_fx_conv"), st);
        tr2->addWidget(btnRev2);
        tr2->addStretch(1);
        st->form()->addRow(I18n::tr("ae_fx_reverb"), tr2);
        auto *revNote = new QLabel(I18n::tr("ae_reverb_note"), st);
        revNote->setWordWrap(true);
        st->vbox()->addWidget(revNote);
        vf->addWidget(st);
        m_needBuf.insert(m_needBuf.end(), { btnDelay, btnRev2 });
        connect(btnDelay, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            pushBuffer(applyDelay(m_buf, m_dly->value(), m_fb->value(),
                                  m_mix->value()),
                       I18n::tr("ae_st_delay").arg(m_dly->value(), 0, 'f', 0));
        });
        connect(btnRev2, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            const audioedit::AudioBuffer buf = m_buf;
            const double rt = m_revRT->value(), mix = m_revMix->value();
            runHeavy([buf, rt, mix] { return applyReverb(buf, rt, mix); },
                     I18n::tr("ae_st_reverb").arg(rt, 0, 'f', 1));
        });

        // 時間軸
        auto *sc = new SectionBox(I18n::tr("ae_sec_timescale"), pageFx);
        auto *cr1 = new QHBoxLayout();
        m_rate = spin(sc, 0.25, 4, 1.0, 2, 0.05);
        cr1->addWidget(m_rate);
        cr1->addWidget(new QLabel(QStringLiteral("×"), sc));
        auto *btnRate = new QPushButton(I18n::tr("ae_fx_rate_apply"), sc);
        cr1->addWidget(btnRate);
        cr1->addStretch(1);
        sc->form()->addRow(I18n::tr("ae_fx_rate"), cr1);
        auto *cr2 = new QHBoxLayout();
        m_semi = spin(sc, -24, 24, 0, 0, 1);
        cr2->addWidget(m_semi);
        cr2->addWidget(new QLabel(I18n::tr("ae_fx_semi"), sc));
        auto *btnPitch = new QPushButton(I18n::tr("ae_fx_pitch_apply"), sc);
        cr2->addWidget(btnPitch);
        cr2->addStretch(1);
        sc->form()->addRow(I18n::tr("ae_fx_pitch"), cr2);
        auto *cr3 = new QHBoxLayout();
        m_stretch = spin(sc, 0.25, 4, 1.0, 2, 0.05);
        cr3->addWidget(m_stretch);
        cr3->addWidget(new QLabel(QStringLiteral("×"), sc));
        auto *btnStretch = new QPushButton(I18n::tr("ae_fx_stretch_apply"), sc);
        cr3->addWidget(btnStretch);
        cr3->addStretch(1);
        sc->form()->addRow(I18n::tr("ae_fx_stretch"), cr3);
        auto *tsNote = new QLabel(I18n::tr("ae_timescale_note"), sc);
        tsNote->setWordWrap(true);
        sc->vbox()->addWidget(tsNote);
        vf->addWidget(sc);
        m_needBuf.insert(m_needBuf.end(), { btnRate, btnPitch, btnStretch });
        connect(btnRate, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            pushBuffer(applyRate(m_buf, m_rate->value()),
                       I18n::tr("ae_st_rate").arg(m_rate->value(), 0, 'f', 2));
        });
        connect(btnPitch, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            const audioedit::AudioBuffer buf = m_buf;
            const double semi = m_semi->value();
            runHeavy([buf, semi] { return pitchShift(buf, semi); },
                I18n::tr("ae_st_pitch").arg(semi > 0
                    ? QStringLiteral("+%1").arg(semi, 0, 'f', 0)
                    : QString::number(semi, 'f', 0)));
        });
        connect(btnStretch, &QPushButton::clicked, this, [this] {
            if (!hasBuf()) return;
            const audioedit::AudioBuffer buf = m_buf;
            const double factor = m_stretch->value();
            runHeavy([buf, factor] { return timeStretch(buf, factor); },
                     I18n::tr("ae_st_stretch").arg(factor, 0, 'f', 2));
        });

        // ノイズリダクション
        auto *sn = new SectionBox(I18n::tr("ae_sec_nr"), pageFx);
        auto *nr1 = new QHBoxLayout();
        m_nrLearn = new QPushButton(I18n::tr("ae_nr_learn"), sn);
        nr1->addWidget(m_nrLearn);
        nr1->addWidget(new QLabel(I18n::tr("ae_nr_amount"), sn));
        m_nrDb = spin(sn, 1, 60, 12, 0, 1);
        nr1->addWidget(m_nrDb);
        nr1->addWidget(new QLabel(QStringLiteral("dB"), sn));
        m_nrApply = new QPushButton(I18n::tr("ae_nr_apply"), sn);
        m_nrApply->setEnabled(false);
        nr1->addWidget(m_nrApply);
        nr1->addStretch(1);
        sn->vbox()->addLayout(nr1);
        auto *nrNote = new QLabel(I18n::tr("ae_nr_note"), sn);
        nrNote->setWordWrap(true);
        sn->vbox()->addWidget(nrNote);
        vf->addWidget(sn);
        m_needSel.push_back(m_nrLearn);
        connect(m_nrLearn, &QPushButton::clicked, this, [this] {
            if (!hasBuf() || !m_view->hasSelection()) return;
            const auto [a, z] = range();
            m_noiseProfile = noiseProfile(m_buf, a, z);
            // 2048 サンプル未満の選択では学習できない — 成功と偽らない
            setStatus(I18n::tr(m_noiseProfile.empty() ? "ae_st_nr_fail"
                                                      : "ae_st_nr_learn"));
            updateInfo();
        });
        connect(m_nrApply, &QPushButton::clicked, this, [this] {
            if (!hasBuf() || m_noiseProfile.empty()) return;
            const audioedit::AudioBuffer buf = m_buf;
            const std::vector<double> prof = m_noiseProfile;
            const double db = m_nrDb->value();
            runHeavy([buf, prof, db] { return denoise(buf, prof, db); },
                     I18n::tr("ae_st_nr").arg(db, 0, 'f', 0));
        });

        // ステレオ
        auto *ss = new SectionBox(I18n::tr("ae_sec_stereo"), pageFx);
        auto *sr = new QHBoxLayout();
        const std::pair<StereoOp, const char *> kOps[] = {
            { StereoOp::Mono,       "ae_stereo_mono" },
            { StereoOp::Swap,       "ae_stereo_swap" },
            { StereoOp::Side,       "ae_stereo_side" },
            { StereoOp::Widen,      "ae_stereo_widen" },
            { StereoOp::InvertLeft, "ae_stereo_invl" },
        };
        for (const auto &op : kOps) {
            auto *b = new QPushButton(I18n::tr(op.second), ss);
            const StereoOp kind = op.first;
            const QString label = I18n::tr(op.second);
            connect(b, &QPushButton::clicked, this, [this, kind, label] {
                if (!hasBuf()) return;
                pushBuffer(applyStereoOp(m_buf, kind), label);
            });
            m_needBuf.push_back(b);
            sr->addWidget(b);
        }
        sr->addStretch(1);
        ss->vbox()->addLayout(sr);
        vf->addWidget(ss);
        vf->addStretch(1);
    }
    tabs->addTab(pageFx, I18n::tr("ae_tab_fx"));

    // ============ 解析 ============
    auto *pageAn = new QWidget(tabs);
    auto *va = new QVBoxLayout(pageAn);
    {
        auto *sw = new SectionBox(I18n::tr("ae_sec_win"), pageAn);
        m_winCombo = new QComboBox(sw);
        const bool en = I18n::instance().lang() == QStringLiteral("en");
        for (const WindowInfo &w : windowInfos())
            m_winCombo->addItem(QString::fromUtf8(en ? w.nameEn : w.nameJa),
                                QString::fromLatin1(w.id));
        m_winCombo->setCurrentIndex(1);   // Hann 既定
        sw->form()->addRow(I18n::tr("ae_win"), m_winCombo);
        m_winInfo = new QLabel(sw);
        m_winInfo->setWordWrap(true);
        sw->vbox()->addWidget(m_winInfo);
        auto *winNote = new QLabel(I18n::tr("ae_win_note"), sw);
        winNote->setWordWrap(true);
        sw->vbox()->addWidget(winNote);
        va->addWidget(sw);
        auto updateWinInfo = [this, en] {
            const int i = m_winCombo->currentIndex();
            const std::vector<WindowInfo> &infos = windowInfos();
            if (i < 0 || i >= static_cast<int>(infos.size())) return;
            const WindowInfo &w = infos[i];
            m_winInfo->setText(I18n::tr("ae_win_info")
                .arg(QString::fromUtf8(w.mainLobe),
                     QString::fromUtf8(w.sideLobe),
                     QString::fromUtf8(en ? w.useEn : w.useJa)));
            m_view->setWindowKind(w.kind);
        };
        connect(m_winCombo, &QComboBox::currentIndexChanged, this,
                updateWinInfo);
        updateWinInfo();

        auto *sa = new SectionBox(I18n::tr("ae_sec_analyze"), pageAn);
        auto *ar = new QHBoxLayout();
        auto *btnAnalyze = new QPushButton(I18n::tr("ae_analyze_go"), sa);
        ar->addWidget(btnAnalyze);
        ar->addWidget(new QLabel(I18n::tr("ae_analyze_hint"), sa));
        ar->addStretch(1);
        sa->vbox()->addLayout(ar);
        m_metricsTable = makeTable(sa, { I18n::tr("ae_m_metric"),
            I18n::tr("ae_m_value"), I18n::tr("ae_m_note") });
        m_metricsTable->setVisible(false);
        sa->vbox()->addWidget(m_metricsTable);
        va->addWidget(sa);
        m_needBuf.push_back(btnAnalyze);
        connect(btnAnalyze, &QPushButton::clicked, this,
                &AudioEditorTab::runAnalysis);

        auto *sp = new SectionBox(I18n::tr("ae_sec_spectrum"), pageAn);
        m_spectrumPlot = new MiniPlot(sp);
        m_spectrumPlot->setLabels(I18n::tr("ae_freq_axis"),
                                  I18n::tr("ae_db_axis"));
        m_spectrumPlot->setXTickPow10(true);
        m_spectrumPlot->setMinimumHeight(150);
        sp->vbox()->addWidget(m_spectrumPlot);
        m_spectrumNote = new QLabel(sp);
        m_spectrumNote->setWordWrap(true);
        sp->vbox()->addWidget(m_spectrumNote);
        sp->setVisible(false);
        va->addWidget(sp);

        auto *sl = new SectionBox(I18n::tr("ae_sec_loud"), pageAn);
        m_loudTable = makeTable(sl, { I18n::tr("ae_m_metric"),
            I18n::tr("ae_m_value"), I18n::tr("ae_l_target") });
        sl->vbox()->addWidget(m_loudTable);
        sl->setVisible(false);
        va->addWidget(sl);

        auto *sb = new SectionBox(I18n::tr("ae_sec_bands"), pageAn);
        m_bandTable = new QTableWidget(1, 10, sb);
        m_bandTable->verticalHeader()->setVisible(false);
        m_bandTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_bandTable->setSelectionMode(QAbstractItemView::NoSelection);
        sb->vbox()->addWidget(m_bandTable);
        auto *bandNote = new QLabel(I18n::tr("ae_bands_note"), sb);
        bandNote->setWordWrap(true);
        sb->vbox()->addWidget(bandNote);
        sb->setVisible(false);
        va->addWidget(sb);
        va->addStretch(1);
    }
    tabs->addTab(pageAn, I18n::tr("ae_tab_analyze"));

    // ── 接続: ヘッダ部 ──────────────────────────────────────────────────────
    connect(btnLoad, &QPushButton::clicked, this, &AudioEditorTab::loadWav);
    connect(m_btnPlay, &QPushButton::clicked, this,
            &AudioEditorTab::playViaSystemPlayer);
    connect(m_btnUndo, &QPushButton::clicked, this, &AudioEditorTab::undoLast);
    connect(m_btnExport, &QPushButton::clicked, this,
            &AudioEditorTab::exportWav);
    connect(m_viewMode, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_view->setViewMode(i == 1 ? AudioWaveformView::ViewMode::Spectrogram
                                   : AudioWaveformView::ViewMode::Waveform);
    });
    connect(m_view, &AudioWaveformView::selectionChanged, this,
            [this](std::size_t, std::size_t) { updateInfo(); });
    connect(m_btnClearSel, &QPushButton::clicked, this, [this] {
        m_view->clearSelection();
        updateInfo();
    });

    updateInfo();
    setWidget(body);
    setWidgetResizable(true);
}

// ── 状態管理 ────────────────────────────────────────────────────────────────
void AudioEditorTab::pushBuffer(audioedit::AudioBuffer next,
                                const QString &status)
{
    if (hasBuf()) {
        m_undo.push_back(m_buf);
        if (m_undo.size() > 12)
            m_undo.erase(m_undo.begin());
    }
    m_buf = std::move(next);
    m_view->setBuffer(hasBuf() ? &m_buf : nullptr);
    setStatus(status);
    updateInfo();
}

void AudioEditorTab::undoLast()
{
    if (m_undo.empty()) return;
    m_buf = std::move(m_undo.back());
    m_undo.pop_back();
    m_view->setBuffer(hasBuf() ? &m_buf : nullptr);
    setStatus(I18n::tr("ae_status_undo"));
    updateInfo();
}

std::pair<std::size_t, std::size_t> AudioEditorTab::range() const
{
    if (m_view->hasSelection())
        return { m_view->selectionStart(), m_view->selectionEnd() };
    return { 0, m_buf.sampleCount() };
}

void AudioEditorTab::setStatus(const QString &s)
{
    m_status->setText(s);
}

void AudioEditorTab::updateInfo()
{
    if (hasBuf()) {
        QString t = QStringLiteral("%1ch · %2Hz · %3s")
            .arg(m_buf.channelCount())
            .arg(m_buf.sampleRateHz, 0, 'f', 0)
            .arg(m_buf.durationSeconds(), 0, 'f', 3);
        if (m_view->hasSelection())
            t += I18n::tr("ae_sel_info").arg(
                (m_view->selectionEnd() - m_view->selectionStart())
                    / m_buf.sampleRateHz, 0, 'f', 3);
        m_info->setText(t);
    } else {
        m_info->setText(QStringLiteral("—"));
    }
    // 非同期処理中は全操作を止める (バッファ差し替え中の競合防止)
    const bool has = hasBuf() && !m_busy;
    const bool sel = has && m_view->hasSelection();
    m_btnPlay->setEnabled(has);
    m_btnExport->setEnabled(has);
    m_btnUndo->setEnabled(!m_undo.empty() && !m_busy);
    m_btnUndo->setText(m_undo.empty()
        ? I18n::tr("ae_undo")
        : I18n::tr("ae_undo") + QStringLiteral(" (%1)").arg(m_undo.size()));
    m_btnClearSel->setVisible(sel);
    for (QPushButton *b : m_needBuf) b->setEnabled(has);
    for (QPushButton *b : m_needSel) b->setEnabled(sel);
    if (m_nrApply)
        m_nrApply->setEnabled(has && !m_noiseProfile.empty());
}

// 重い処理の非同期実行。op はバッファをコピーで捕捉済みの純関数
void AudioEditorTab::runHeavy(
    const std::function<audioedit::AudioBuffer()> &op,
    const QString &doneStatus)
{
    if (m_busy) return;
    m_busy = true;
    setStatus(I18n::tr("ae_status_processing"));
    updateInfo();
    auto result = std::make_shared<audioedit::AudioBuffer>();
    QThread *th = QThread::create([op, result] { *result = op(); });
    connect(th, &QThread::finished, this, [this, th, result, doneStatus] {
        th->deleteLater();
        m_busy = false;
        pushBuffer(std::move(*result), doneStatus);
    });
    th->start();
}

// ── 入出力 ──────────────────────────────────────────────────────────────────
void AudioEditorTab::loadWav()
{
    const QString path = QFileDialog::getOpenFileName(this,
        I18n::tr("ae_load"), QString(), QStringLiteral("WAV (*.wav)"));
    if (path.isEmpty()) return;
    const acoustics::AcousticResult<acoustics::AudioBuffer> res =
        acoustics::readWavFile(path.toStdString());
    if (!res.success()) {
        setStatus(I18n::tr("ae_status_load_fail")
                      .arg(QString::fromStdString(res.message())));
        return;
    }
    m_view->clearSelection();
    pushBuffer(res.value(),
               I18n::tr("ae_status_loaded").arg(QFileInfo(path).fileName()));
}

void AudioEditorTab::exportWav()
{
    if (!hasBuf()) return;
    const QString path = QFileDialog::getSaveFileName(this,
        I18n::tr("ae_export"), QStringLiteral("openfdtd_audio.wav"),
        QStringLiteral("WAV (*.wav)"));
    if (path.isEmpty()) return;
    const acoustics::AcousticResult<bool> res = acoustics::writeWavFile(
        path.toStdString(), m_buf, acoustics::WavSampleFormat::Pcm16);
    if (!res.success()) {
        setStatus(I18n::tr("ae_status_export_fail")
                      .arg(QString::fromStdString(res.message())));
        return;
    }
    setStatus(I18n::tr("ae_status_exported").arg(QFileInfo(path).fileName()));
}

void AudioEditorTab::playViaSystemPlayer()
{
    if (!hasBuf()) return;
    // アプリ内再生は未対応 (Qt Multimedia 不使用) — 一時 WAV を OS に委ねる。
    // 選択範囲があればその部分だけを書き出す
    const auto [a, z] = range();
    const audioedit::AudioBuffer part =
        m_view->hasSelection() ? trimToRange(m_buf, a, z) : m_buf;
    const QString path =
        QDir::temp().filePath(QStringLiteral("ofdx_audioedit_play.wav"));
    const acoustics::AcousticResult<bool> res = acoustics::writeWavFile(
        path.toStdString(), part, acoustics::WavSampleFormat::Pcm16);
    if (!res.success()) {
        setStatus(I18n::tr("ae_status_export_fail")
                      .arg(QString::fromStdString(res.message())));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    setStatus(I18n::tr("ae_status_playing").arg(path));
}

// ── 生成・解析 ──────────────────────────────────────────────────────────────
void AudioEditorTab::generateSignal()
{
    static const SignalKind kKinds[] = {
        SignalKind::ExpSweep, SignalKind::LinSweep, SignalKind::Sine,
        SignalKind::White, SignalKind::Pink, SignalKind::Mls,
        SignalKind::Impulse, SignalKind::Click
    };
    const int i = m_genKind->currentIndex();
    if (i < 0 || i >= static_cast<int>(std::size(kKinds))) return;
    // ESS は 0 < f1 < f2 が前提 — 満たさない入力を黙って無音にしない
    if (kKinds[i] == SignalKind::ExpSweep &&
        (m_genF1->value() <= 0.0 || m_genF2->value() <= m_genF1->value())) {
        setStatus(I18n::tr("ae_gen_sweep_bad"));
        return;
    }
    // fs は既存バッファに合わせる (無ければ 48 kHz)
    const double sr = hasBuf() ? m_buf.sampleRateHz : 48000.0;
    audioedit::AudioBuffer b = audioedit::generateSignal(
        kKinds[i], m_genF1->value(), m_genF2->value(), m_genDur->value(),
        m_genAmp->value(), sr);
    m_view->clearSelection();
    pushBuffer(std::move(b),
        I18n::tr("ae_status_generated").arg(m_genKind->currentText())
            .arg(m_genDur->value(), 0, 'f', 2).arg(sr, 0, 'f', 0));
}

audioedit::WindowKind AudioEditorTab::currentWindow() const
{
    const int i = m_winCombo->currentIndex();
    const std::vector<WindowInfo> &infos = windowInfos();
    if (i < 0 || i >= static_cast<int>(infos.size()))
        return WindowKind::Hann;
    return infos[i].kind;
}

void AudioEditorTab::runAnalysis()
{
    if (!hasBuf() || m_busy) return;
    m_busy = true;
    setStatus(I18n::tr("ae_status_processing"));
    updateInfo();
    // 構造化束縛はラムダに捕捉できない (C++17) — 素の変数に受ける
    const std::pair<std::size_t, std::size_t> rz = range();
    const std::size_t a = rz.first, z = rz.second;
    const WindowKind win = currentWindow();
    const audioedit::AudioBuffer buf = m_buf;

    struct AnalysisData {
        LevelMetrics m;
        std::vector<SpectrumPoint> spec;
        LoudnessMetrics loud;
        std::vector<OctaveBand> bands;
    };
    auto data = std::make_shared<AnalysisData>();
    QThread *th = QThread::create([buf, a, z, win, data] {
        data->m = analyzeLevels(buf, a, z);
        data->spec = spectrum(buf, a, win);
        data->loud = analyzeLoudness(buf);
        data->bands = octaveBands(buf);
    });
    connect(th, &QThread::finished, this, [this, th, data] {
        th->deleteLater();
        m_busy = false;
        showAnalysis(data->m, data->spec, data->loud, data->bands);
        updateInfo();
    });
    th->start();
}

void AudioEditorTab::showAnalysis(
    const LevelMetrics &m, const std::vector<SpectrumPoint> &spec,
    const LoudnessMetrics &loud, const std::vector<OctaveBand> &bands)
{
    // 指標テーブル
    m_metricsTable->setRowCount(0);
    auto addRow = [this](const QString &name, const QString &value,
                         const QString &note) {
        const int r = m_metricsTable->rowCount();
        m_metricsTable->insertRow(r);
        m_metricsTable->setItem(r, 0, roItem(name));
        m_metricsTable->setItem(r, 1, roItem(value));
        m_metricsTable->setItem(r, 2, roItem(note));
    };
    // I18n::tr は静的関数なので this の捕捉は不要
    // (Apple clang が -Wunused-lambda-capture で警告する)
    auto rtVal = [](bool has, double sec) {
        return has ? QStringLiteral("%1 s").arg(sec, 0, 'f', 3)
                   : I18n::tr("ae_m_range_short");
    };
    addRow(I18n::tr("ae_m_dur"),
           QStringLiteral("%1 s").arg(m.durationSec, 0, 'f', 3),
           I18n::tr("ae_m_dur_note"));
    addRow(I18n::tr("ae_m_peak"),
           QStringLiteral("%1 dBFS").arg(m.peakDbfs, 0, 'f', 2),
           m.peakDbfs > -0.1 ? I18n::tr("ae_m_clip") : I18n::tr("ae_m_ok"));
    addRow(I18n::tr("ae_m_rms"),
           QStringLiteral("%1 dBFS").arg(m.rmsDbfs, 0, 'f', 2),
           I18n::tr("ae_m_rms_note"));
    addRow(I18n::tr("ae_m_crest"),
           QStringLiteral("%1 dB").arg(m.crestDb, 0, 'f', 2),
           m.crestDb > 14 ? I18n::tr("ae_m_impulsive")
                          : I18n::tr("ae_m_steady"));
    addRow(I18n::tr("ae_m_dc"),
           QString::number(m.dcOffset, 'e', 2),
           std::fabs(m.dcOffset) > 1e-3 ? I18n::tr("ae_m_dc_bad")
                                        : I18n::tr("ae_m_dc_ok"));
    addRow(I18n::tr("ae_m_edt"), rtVal(m.hasEdt, m.edtSec),
           I18n::tr("ae_m_ir_note"));
    addRow(QStringLiteral("T20"), rtVal(m.hasT20, m.t20Sec),
           I18n::tr("ae_m_t20_note"));
    addRow(QStringLiteral("T30"), rtVal(m.hasT30, m.t30Sec),
           I18n::tr("ae_m_t30_note"));
    if (m.hasT20 && m.hasT30 && m.t30Sec > 0) {
        const double ratio = m.t20Sec / m.t30Sec;
        addRow(I18n::tr("ae_m_ratio"), QString::number(ratio, 'f', 3),
               std::fabs(ratio - 1.0) > 0.1 ? I18n::tr("ae_m_nonlinear")
                                            : I18n::tr("ae_m_linear"));
    }
    fitTable(m_metricsTable);
    m_metricsTable->setVisible(true);

    // スペクトル
    QVector<QPointF> pts;
    for (const SpectrumPoint &p : spec)
        pts.append(QPointF(p.logF, p.db));
    MiniSeries series;
    series.pts = pts;
    m_spectrumPlot->setSeries({ series });
    m_spectrumNote->setText(
        I18n::tr("ae_spectrum_note").arg(m_winCombo->currentText()));
    m_spectrumPlot->parentWidget()->setVisible(true);

    // ラウドネス
    m_loudTable->setRowCount(0);
    auto addLoud = [this](const QString &name, const QString &value,
                          const QString &note) {
        const int r = m_loudTable->rowCount();
        m_loudTable->insertRow(r);
        m_loudTable->setItem(r, 0, roItem(name));
        m_loudTable->setItem(r, 1, roItem(value));
        m_loudTable->setItem(r, 2, roItem(note));
    };
    addLoud(I18n::tr("ae_l_integrated"),
        QStringLiteral("%1 LUFS").arg(loud.integratedLufs, 0, 'f', 1),
        I18n::tr("ae_l_integrated_note"));
    addLoud(I18n::tr("ae_l_range"),
        QStringLiteral("%1 LU").arg(loud.rangeLu, 0, 'f', 1),
        I18n::tr("ae_l_range_note"));
    addLoud(I18n::tr("ae_l_mom"),
        QStringLiteral("%1 LUFS").arg(loud.momentaryMaxLufs, 0, 'f', 1),
        QStringLiteral("—"));
    addLoud(I18n::tr("ae_l_tp"),
        QStringLiteral("%1 dBTP").arg(loud.truePeakDbtp, 0, 'f', 2),
        loud.truePeakDbtp > -1.0 ? I18n::tr("ae_l_tp_over")
                                 : I18n::tr("ae_l_tp_ok"));
    fitTable(m_loudTable);
    m_loudTable->parentWidget()->setVisible(true);

    // オクターブバンド
    m_bandTable->setColumnCount(static_cast<int>(bands.size()));
    QStringList headers;
    for (const OctaveBand &b : bands)
        headers << (b.fcHz >= 1000
            ? QStringLiteral("%1k").arg(b.fcHz / 1000.0, 0, 'g', 3)
            : QString::number(b.fcHz, 'g', 3));
    m_bandTable->setHorizontalHeaderLabels(headers);
    for (int c = 0; c < static_cast<int>(bands.size()); ++c)
        m_bandTable->setItem(0, c,
            roItem(QString::number(bands[c].db, 'f', 1)));
    fitTable(m_bandTable);
    m_bandTable->parentWidget()->setVisible(true);

    setStatus(I18n::tr("ae_status_analyzed"));
}
