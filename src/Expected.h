#pragma once

#include <string>

class Error {
public:
    Error() = default;
    Error(std::string trace) : mTrace(trace){};

    friend std::ostream& operator<<(std::ostream& outs, const Error& e) { return outs << e.mTrace; }

private:
    std::string mTrace;
};

template <class T>
class Expected {
public:
    Expected(T value) : mExpected(value), mIsExpected(true){};
    Expected(Error error) : mExpected(T()), mError(error), mIsExpected(false){};

    static Expected<T> error(std::string trace) { return Expected(Error(trace)); }

    const T& operator*() const { return mExpected; }

    bool isExpected() const { return mIsExpected; }
    const T& value() const { return mExpected; }
    const Error& error() const { return mError; }

private:
    const T mExpected;
    const Error mError;
    bool mIsExpected;
};
