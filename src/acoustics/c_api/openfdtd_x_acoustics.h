/* openfdtd_x_acoustics.h — OpenFDTD-X 音響分析コアの安定 C API。
 *
 * 純 C (C99) から利用可能。STL / Qt / C++ クラス / 例外 / テンプレートは
 * 一切露出しない。ABI 互換性は ofdx_ac_metrics.struct_size と
 * ofdx_ac_metrics.api_version による前方互換検査で担保する:
 * 呼び出し側は analyze の前に必ず
 *     metrics.struct_size = sizeof(ofdx_ac_metrics);
 *     metrics.api_version = OFDX_AC_API_VERSION;
 * を設定すること。実装は既知のレイアウトと一致しない要求を
 * OFDX_AC_INVALID_ARGUMENT で拒否する。
 *
 * スレッド安全性: 異なる context を使う限り並行呼び出し可。同一 context の
 * 共有には外部同期が必要 (last_error バッファが context 内にあるため)。
 */
#ifndef OPENFDTD_X_ACOUSTICS_H
#define OPENFDTD_X_ACOUSTICS_H

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint32_t */

#ifdef __cplusplus
extern "C" {
#endif

/* この API のバージョン。構造体レイアウトが変わったら増やす。 */
#define OFDX_AC_API_VERSION 1u

/* エラーコード */
typedef enum ofdx_ac_error {
    OFDX_AC_SUCCESS = 0,
    OFDX_AC_INVALID_ARGUMENT = 1,            /* NULL / struct_size 不一致など */
    OFDX_AC_INVALID_AUDIO = 2,               /* 短すぎる / NaN / クリッピング等 */
    OFDX_AC_DIRECT_SOUND_NOT_FOUND = 3,      /* 直接音が検出できない */
    OFDX_AC_INSUFFICIENT_DYNAMIC_RANGE = 4,  /* 動的範囲不足 */
    OFDX_AC_NUMERICAL_FAILURE = 5,           /* 回帰 / フィルタ設計の失敗 */
    OFDX_AC_INTERNAL_ERROR = 6               /* 想定外の内部エラー */
} ofdx_ac_error;

/* 指標の品質区分 (ofdx_ac_metric.quality の値) */
typedef enum ofdx_ac_quality {
    OFDX_AC_QUALITY_INVALID = 0, /* 無効 (評価不能) */
    OFDX_AC_QUALITY_WARNING = 1, /* 参考値 (決定係数不足など) */
    OFDX_AC_QUALITY_VALID = 2    /* 有効 */
} ofdx_ac_quality;

/* 非所有の音声ビュー。samples の寿命は呼び出し側が保証する。 */
typedef struct ofdx_ac_audio_view {
    const double *samples;   /* モノラルサンプル列 (振幅, フルスケール ±1) */
    size_t sample_count;     /* サンプル数 */
    double sample_rate_hz;   /* サンプリング周波数 [Hz] (> 0) */
} ofdx_ac_audio_view;

/* 単一指標の値と品質 */
typedef struct ofdx_ac_metric {
    double value; /* 指標値 (単位は指標ごと: 秒 / dB / 比) */
    int valid;    /* 0 = 評価不能, 非 0 = 評価可能 */
    int quality;  /* ofdx_ac_quality の値 */
} ofdx_ac_metric;

/* 分析結果 (広帯域 = フィルタなし全帯域の ISO 3382-1 指標)。
 * struct_size / api_version は呼び出し側が analyze の前に設定する入力
 * フィールド。将来フィールドが追加されても、古い呼び出し側は自分の
 * struct_size を渡すことで検出できる。 */
typedef struct ofdx_ac_metrics {
    size_t struct_size;      /* [in] sizeof(ofdx_ac_metrics) を設定 */
    uint32_t api_version;    /* [in] OFDX_AC_API_VERSION を設定 */
    ofdx_ac_metric edt;         /* Early Decay Time [s] */
    ofdx_ac_metric t20;         /* 残響時間 T20 [s] */
    ofdx_ac_metric t30;         /* 残響時間 T30 [s] */
    ofdx_ac_metric c50;         /* 明瞭度 C50 [dB] */
    ofdx_ac_metric c80;         /* 明瞭度 C80 [dB] */
    ofdx_ac_metric d50;         /* Definition D50 [0..1] */
    ofdx_ac_metric center_time; /* 重心時間 Ts [s] */
} ofdx_ac_metrics;

/* 不透明な分析コンテキスト (last_error バッファ等を保持) */
typedef struct ofdx_ac_context ofdx_ac_context;

/* コンテキスト生成。失敗時 (メモリ不足) は NULL。 */
ofdx_ac_context *ofdx_ac_context_create(void);

/* コンテキスト破棄。NULL は無視する。 */
void ofdx_ac_context_destroy(ofdx_ac_context *ctx);

/* 室内インパルス応答 (RIR) を分析し、広帯域の ISO 3382-1 指標を
 * out_metrics に書き込む。呼び出し前に out_metrics->struct_size と
 * out_metrics->api_version を設定すること。
 * 成功時 OFDX_AC_SUCCESS。失敗時はエラーコードを返し、詳細メッセージを
 * ofdx_ac_last_error(ctx) で取得できる。 */
ofdx_ac_error ofdx_ac_analyze_rir(ofdx_ac_context *ctx,
                                  const ofdx_ac_audio_view *audio,
                                  ofdx_ac_metrics *out_metrics);

/* 直近のエラーメッセージ (UTF-8, NUL 終端)。エラーがなければ空文字列。
 * 返るポインタは ctx が破棄されるか次の API 呼び出しまで有効。
 * ctx == NULL でも静的な空文字列を返す (NULL は返さない)。 */
const char *ofdx_ac_last_error(const ofdx_ac_context *ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OPENFDTD_X_ACOUSTICS_H */
