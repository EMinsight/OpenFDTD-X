/* mock_acoustic_solver.c — 外部音響ソルバー出力契約のモック (純 C99)。
 *
 * ADR-0007 の「契約の実行可能な仕様」: 実ソルバーは本リポジトリに存在しない
 * ため、この最小実装が出力契約 (metadata.json / rir.wav / metrics.json /
 * solver.log) の参照実装を兼ねる。契約を変更するときは本ファイルと
 * docs/adr/0007-acoustic-solver-contract.md を同時に更新すること。
 *
 * 使い方:  mock_acoustic_solver <working_dir>
 * 出力 (すべて <working_dir> 直下):
 *   rir.wav       — 16 bit PCM 48 kHz mono、1.5 s。直接音デルタ (10 ms) +
 *                   指数減衰白色雑音 (RT ≈ 0.8 s)。乱数は LCG で決定的。
 *   metadata.json — {"contract_version":1,"solver":"mock",...}
 *   metrics.json  — {"note":"mock"}
 *   solver.log    — 実行ログ (末尾 "mock solver: done")
 * 進捗は stdout に "progress a/b" 形式で出力する (AcousticRunner が解析)。
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOCK_SAMPLE_RATE 48000
#define MOCK_RT_SECONDS 0.8
#define MOCK_DURATION_SECONDS 1.5
#define MOCK_DIRECT_DELAY_SECONDS 0.010
#define MOCK_DIRECT_AMPLITUDE 0.5
#define MOCK_DECAY_NOISE_RMS 0.2
#define MOCK_PROGRESS_TOTAL 10

/* 決定的な擬似乱数 (LCG, Numerical Recipes 係数 — tests/acoustics/
 * test_common.h と同一のモデル)。 */
static uint32_t lcg_next(uint32_t *state) {
    *state = *state * 1664525u + 1013904223u;
    return *state;
}
/* [-1, 1] の一様乱数 */
static double lcg_uniform(uint32_t *state) {
    return ((double)(lcg_next(state) >> 8) / 8388608.0) - 1.0;
}

static void put_u32le(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
}
static void put_u16le(unsigned char *p, uint16_t v) {
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
}

/* 44 byte 標準 WAV ヘッダ (RIFF + fmt(16) + data)、16 bit PCM mono */
static int write_wav_header(FILE *fp, uint32_t sample_rate, uint32_t n_samples) {
    unsigned char h[44];
    const uint16_t channels = 1, bits = 16;
    const uint16_t block_align = (uint16_t)(channels * bits / 8);
    const uint32_t byte_rate = sample_rate * block_align;
    const uint32_t data_size = n_samples * block_align;

    memcpy(h + 0, "RIFF", 4);
    put_u32le(h + 4, 36u + data_size);
    memcpy(h + 8, "WAVE", 4);
    memcpy(h + 12, "fmt ", 4);
    put_u32le(h + 16, 16u);          /* fmt チャンクサイズ */
    put_u16le(h + 20, 1u);           /* WAVE_FORMAT_PCM */
    put_u16le(h + 22, channels);
    put_u32le(h + 24, sample_rate);
    put_u32le(h + 28, byte_rate);
    put_u16le(h + 32, block_align);
    put_u16le(h + 34, bits);
    memcpy(h + 36, "data", 4);
    put_u32le(h + 40, data_size);
    return fwrite(h, 1, sizeof(h), fp) == sizeof(h) ? 0 : -1;
}

static int16_t quantize16(double x) {
    double v = x * 32767.0;
    if (v > 32767.0) v = 32767.0;
    if (v < -32768.0) v = -32768.0;
    return (int16_t)(v >= 0.0 ? v + 0.5 : v - 0.5);
}

/* 合成 RIR を書き出しつつ stdout に進捗を出す */
static int write_rir_wav(const char *path) {
    const uint32_t fs = MOCK_SAMPLE_RATE;
    const uint32_t n = (uint32_t)(MOCK_DURATION_SECONDS * fs + 0.5);
    const uint32_t d0 = (uint32_t)(MOCK_DIRECT_DELAY_SECONDS * fs + 0.5);
    /* exp(-6.91 t / RT) は RT 秒で -60 dB (エネルギー) に対応する減衰 */
    const double k = 6.91 / (MOCK_RT_SECONDS * (double)fs);
    /* 一様乱数の RMS は 1/√3 なので √3 倍して指定 RMS にする */
    const double amp = MOCK_DECAY_NOISE_RMS * sqrt(3.0);
    uint32_t state = 20260716u; /* 固定シード — 出力は完全に決定的 */
    uint32_t i;
    int step_done = 0;
    FILE *fp = fopen(path, "wb");

    if (!fp) return -1;
    if (write_wav_header(fp, fs, n) != 0) {
        fclose(fp);
        return -1;
    }
    for (i = 0; i < n; ++i) {
        double x = 0.0;
        int step;
        if (i >= d0) {
            x = amp * lcg_uniform(&state) * exp(-k * (double)(i - d0));
            if (i == d0) x += MOCK_DIRECT_AMPLITUDE; /* 直接音デルタ */
        }
        {
            unsigned char s[2];
            put_u16le(s, (uint16_t)quantize16(x));
            if (fwrite(s, 1, 2, fp) != 2) {
                fclose(fp);
                return -1;
            }
        }
        /* 進捗: サンプル書き出しの 1/10 ごとに "progress a/b" */
        step = (int)(((uint64_t)(i + 1) * MOCK_PROGRESS_TOTAL) / n);
        for (; step_done < step; ) {
            ++step_done;
            printf("progress %d/%d\n", step_done, MOCK_PROGRESS_TOTAL);
            fflush(stdout);
        }
    }
    return fclose(fp) == 0 ? 0 : -1;
}

static int write_text_file(const char *path, const char *text) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    if (fputs(text, fp) < 0) {
        fclose(fp);
        return -1;
    }
    return fclose(fp) == 0 ? 0 : -1;
}

static int join_path(char *out, size_t cap, const char *dir, const char *name) {
    int r = snprintf(out, cap, "%s/%s", dir, name);
    return (r > 0 && (size_t)r < cap) ? 0 : -1;
}

int main(int argc, char **argv) {
    char path[4096];
    const char *dir;

    if (argc < 2) {
        fprintf(stderr, "usage: mock_acoustic_solver <working_dir>\n");
        return 2;
    }
    dir = argv[1];
    printf("mock acoustic solver (contract reference implementation)\n");
    printf("working dir: %s\n", dir);
    fflush(stdout);

    /* (a) rir.wav — 進捗行はこの中で出力する */
    if (join_path(path, sizeof(path), dir, "rir.wav") != 0 ||
        write_rir_wav(path) != 0) {
        fprintf(stderr, "mock solver: cannot write rir.wav in %s\n", dir);
        return 1;
    }

    /* (b) metadata.json (必須 — 未知キー無視・追加キーのみの互換規則) */
    if (join_path(path, sizeof(path), dir, "metadata.json") != 0 ||
        write_text_file(path,
                        "{\n"
                        "  \"contract_version\": 1,\n"
                        "  \"solver\": \"mock\",\n"
                        "  \"rt_nominal\": 0.8,\n"
                        "  \"sample_rate\": 48000\n"
                        "}\n") != 0) {
        fprintf(stderr, "mock solver: cannot write metadata.json in %s\n", dir);
        return 1;
    }

    /* (c) metrics.json (任意) */
    if (join_path(path, sizeof(path), dir, "metrics.json") != 0 ||
        write_text_file(path, "{\n  \"note\": \"mock\"\n}\n") != 0) {
        fprintf(stderr, "mock solver: cannot write metrics.json in %s\n", dir);
        return 1;
    }

    /* (d) solver.log (必須) */
    if (join_path(path, sizeof(path), dir, "solver.log") != 0 ||
        write_text_file(path,
                        "mock acoustic solver log\n"
                        "grid: n/a (synthetic RIR, no field solve)\n"
                        "rir: 48000 Hz, 16 bit PCM mono, RT nominal 0.8 s\n"
                        "mock solver: done\n") != 0) {
        fprintf(stderr, "mock solver: cannot write solver.log in %s\n", dir);
        return 1;
    }

    printf("mock solver: done\n");
    return 0;
}
