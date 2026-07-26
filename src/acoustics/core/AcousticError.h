// AcousticError.h — 音響分析コアのエラーコードと結果型 (指示書 §7.3)。
// 例外は送出しない (エラーコード方式)。Qt 非依存 / C++14。
#pragma once
#include <string>
#include <utility>

namespace ofd {
namespace acoustics {

// §7.3: エラーコード (16 値)
enum class AcousticErrorCode {
    Ok = 0,                   // 成功
    InvalidArgument,          // 引数不正
    EmptyInput,               // 入力が空
    InputTooShort,            // 入力長不足
    NonFiniteSample,          // NaN / Inf を含む
    ClippingDetected,         // クリッピング検出
    InsufficientDynamicRange, // 動的範囲不足
    NoiseFloorTooHigh,        // ノイズフロア過大
    DirectSoundNotFound,      // 直接音が検出できない
    RegressionFailed,         // 減衰カーブ回帰の失敗
    FilterDesignFailed,       // フィルタ設計失敗
    UnsupportedSampleRate,    // 非対応サンプリング周波数
    FileNotFound,             // ファイルが開けない
    FileReadError,            // ファイル読み込み失敗
    FileWriteError,           // ファイル書き込み失敗
    UnsupportedFormat         // 非対応フォーマット
};

inline const char *acousticErrorCodeName(AcousticErrorCode code) {
    switch (code) {
    case AcousticErrorCode::Ok:                       return "Ok";
    case AcousticErrorCode::InvalidArgument:          return "InvalidArgument";
    case AcousticErrorCode::EmptyInput:               return "EmptyInput";
    case AcousticErrorCode::InputTooShort:            return "InputTooShort";
    case AcousticErrorCode::NonFiniteSample:          return "NonFiniteSample";
    case AcousticErrorCode::ClippingDetected:         return "ClippingDetected";
    case AcousticErrorCode::InsufficientDynamicRange: return "InsufficientDynamicRange";
    case AcousticErrorCode::NoiseFloorTooHigh:        return "NoiseFloorTooHigh";
    case AcousticErrorCode::DirectSoundNotFound:      return "DirectSoundNotFound";
    case AcousticErrorCode::RegressionFailed:         return "RegressionFailed";
    case AcousticErrorCode::FilterDesignFailed:       return "FilterDesignFailed";
    case AcousticErrorCode::UnsupportedSampleRate:    return "UnsupportedSampleRate";
    case AcousticErrorCode::FileNotFound:             return "FileNotFound";
    case AcousticErrorCode::FileReadError:            return "FileReadError";
    case AcousticErrorCode::FileWriteError:           return "FileWriteError";
    case AcousticErrorCode::UnsupportedFormat:        return "UnsupportedFormat";
    }
    return "Unknown";
}

// 成功時は value を、失敗時は errorCode / message を保持する結果型。
// T はデフォルト構築可能であること。例外を使わないエラー伝搬に用いる。
template <typename T>
class AcousticResult {
public:
    // 既定構築は「未初期化エラー」扱い (誤用検出のため)
    AcousticResult()
        : m_code(AcousticErrorCode::InvalidArgument),
          m_message("uninitialized result"), m_value() {}

    static AcousticResult ok(T value) {
        AcousticResult r;
        r.m_code = AcousticErrorCode::Ok;
        r.m_message.clear();
        r.m_value = std::move(value);
        return r;
    }

    static AcousticResult error(AcousticErrorCode code, const std::string &message) {
        AcousticResult r;
        // Ok をエラーとして渡された場合は InvalidArgument に矯正する
        r.m_code = (code == AcousticErrorCode::Ok) ? AcousticErrorCode::InvalidArgument
                                                   : code;
        r.m_message = message;
        return r;
    }

    bool success() const { return m_code == AcousticErrorCode::Ok; }
    AcousticErrorCode errorCode() const { return m_code; }
    const std::string &message() const { return m_message; }

    // 失敗時はデフォルト構築値を返す (呼び出し側は success() を先に確認すること)
    T &value() { return m_value; }
    const T &value() const { return m_value; }

private:
    AcousticErrorCode m_code;
    std::string m_message;
    T m_value;
};

} // namespace acoustics
} // namespace ofd
