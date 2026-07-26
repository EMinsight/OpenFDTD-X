// ArrayView.h — 非所有の連続メモリビュー (指示書 §7.2)。
// Qt 非依存 / C++14。所有権を持たず、参照先の寿命は呼び出し側が保証する。
#pragma once
#include <cstddef>
#include <type_traits>
#include <vector>

namespace ofd {
namespace acoustics {

template <typename T>
class ArrayView {
public:
    typedef T value_type;
    typedef T *iterator;
    typedef T *pointer;

    ArrayView() : m_data(nullptr), m_size(0) {}
    ArrayView(T *data, std::size_t size) : m_data(data), m_size(size) {}

    // 非 const な std::vector から (T = double / const double のいずれでも可)
    template <typename U>
    ArrayView(std::vector<U> &v,
              typename std::enable_if<
                  std::is_same<typename std::remove_const<T>::type, U>::value>::type * = nullptr)
        : m_data(v.empty() ? nullptr : v.data()), m_size(v.size()) {}

    // const な std::vector から (T が const 修飾のときのみ有効)
    template <typename U>
    ArrayView(const std::vector<U> &v,
              typename std::enable_if<std::is_same<T, const U>::value>::type * = nullptr)
        : m_data(v.empty() ? nullptr : v.data()), m_size(v.size()) {}

    // ArrayView<double> → ArrayView<const double> の暗黙変換
    template <typename U>
    ArrayView(const ArrayView<U> &other,
              typename std::enable_if<std::is_convertible<U *, T *>::value>::type * = nullptr)
        : m_data(other.data()), m_size(other.size()) {}

    T *data() const { return m_data; }
    std::size_t size() const { return m_size; }
    bool empty() const { return m_size == 0; }

    T &operator[](std::size_t i) const { return m_data[i]; }
    T *begin() const { return m_data; }
    T *end() const { return m_data + m_size; }
    T &front() const { return m_data[0]; }
    T &back() const { return m_data[m_size - 1]; }

    // 範囲外は自動的に切り詰める (例外を送出しない)
    ArrayView subview(std::size_t offset, std::size_t count) const {
        if (offset > m_size) offset = m_size;
        if (count > m_size - offset) count = m_size - offset;
        return ArrayView(m_data + offset, count);
    }
    ArrayView first(std::size_t n) const { return subview(0, n); }
    ArrayView last(std::size_t n) const {
        return subview(m_size > n ? m_size - n : 0, n);
    }

private:
    T *m_data;
    std::size_t m_size;
};

// 音声サンプル列の読み取り専用ビュー
typedef ArrayView<const double> ConstSampleView;

} // namespace acoustics
} // namespace ofd
