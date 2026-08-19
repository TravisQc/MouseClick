#pragma once

#include <cstddef>
#include <cstdint>

struct WideTextView {
    const wchar_t* data = nullptr;
    std::size_t length = 0;

    WideTextView() = default;

    WideTextView(const wchar_t* text) : data(text), length(Measure(text)) {}

    WideTextView(const wchar_t* text, std::size_t textLength)
        : data(text), length(textLength) {}

    bool empty() const {
        return length == 0;
    }

private:
    static std::size_t Measure(const wchar_t* text) {
        if (text == nullptr) {
            return 0;
        }
        std::size_t result = 0;
        while (text[result] != L'\0') {
            ++result;
        }
        return result;
    }
};

template <std::size_t Capacity>
class BoundedWideString final {
public:
    BoundedWideString() = default;

    BoundedWideString(const BoundedWideString& other) {
        Assign(other.view());
    }

    BoundedWideString& operator=(const BoundedWideString& other) {
        if (this != &other) {
            Assign(other.view());
        }
        return *this;
    }

    bool Assign(WideTextView text) {
        if ((text.data == nullptr && text.length != 0) || text.length > Capacity) {
            return false;
        }
        for (std::size_t index = 0; index < text.length; ++index) {
            storage_[index] = text.data[index];
        }
        storage_[text.length] = L'\0';
        length_ = text.length;
        return true;
    }

    bool Assign(const wchar_t* text) {
        return Assign(WideTextView(text));
    }

    bool Append(WideTextView text) {
        if ((text.data == nullptr && text.length != 0) || text.length > Capacity - length_) {
            return false;
        }
        for (std::size_t index = 0; index < text.length; ++index) {
            storage_[length_ + index] = text.data[index];
        }
        length_ += text.length;
        storage_[length_] = L'\0';
        return true;
    }

    bool Append(const wchar_t* text) {
        return Append(WideTextView(text));
    }

    bool Append(wchar_t character) {
        if (length_ == Capacity) {
            return false;
        }
        storage_[length_] = character;
        ++length_;
        storage_[length_] = L'\0';
        return true;
    }

    void Clear() {
        length_ = 0;
        storage_[0] = L'\0';
    }

    const wchar_t* c_str() const {
        return storage_;
    }

    wchar_t* data() {
        return storage_;
    }

    std::size_t length() const {
        return length_;
    }

    bool empty() const {
        return length_ == 0;
    }

    static constexpr std::size_t capacity() {
        return Capacity;
    }

    WideTextView view() const {
        return WideTextView{storage_, length_};
    }

    void SetLength(std::size_t length) {
        if (length <= Capacity) {
            length_ = length;
            storage_[length_] = L'\0';
        }
    }

private:
    wchar_t storage_[Capacity + 1]{};
    std::size_t length_ = 0;
};

inline bool ParseUnsigned(WideTextView text, std::uint32_t* value) {
    if (text.empty() || text.data == nullptr || value == nullptr) {
        return false;
    }

    std::uint32_t result = 0;
    constexpr std::uint32_t maximum = 0xffffffffu;
    for (std::size_t index = 0; index < text.length; ++index) {
        const wchar_t character = text.data[index];
        if (character < L'0' || character > L'9') {
            return false;
        }
        const std::uint32_t digit = static_cast<std::uint32_t>(character - L'0');
        if (result > (maximum - digit) / 10u) {
            return false;
        }
        result = result * 10u + digit;
    }
    *value = result;
    return true;
}

template <std::size_t Capacity>
bool FormatUnsigned(std::uint32_t value, BoundedWideString<Capacity>* text) {
    if (text == nullptr) {
        return false;
    }

    wchar_t reversed[10]{};
    std::size_t length = 0;
    do {
        reversed[length] = static_cast<wchar_t>(L'0' + value % 10u);
        ++length;
        value /= 10u;
    } while (value != 0);

    if (length > Capacity) {
        return false;
    }
    wchar_t formatted[11]{};
    for (std::size_t index = 0; index < length; ++index) {
        formatted[index] = reversed[length - index - 1];
    }
    return text->Assign(WideTextView{formatted, length});
}
