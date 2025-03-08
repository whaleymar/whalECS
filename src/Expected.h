#pragma once

#include <string>

class Error {
public:
    Error() = default;
    Error(const std::string& trace) : mTrace(trace) {};

    friend std::ostream& operator<<(std::ostream& outs, const Error& e) { return outs << e.mTrace; }

private:
    std::string mTrace;
};

template <class T>
class Expected {
public:
    Expected() : mExpected(T()), mIsExpected(true) {}
    Expected(T value) : mExpected(value), mIsExpected(true) {}
    Expected(Error error) : mError(error), mIsExpected(false) {}

    static Expected<std::string> error(const std::string& trace) { return Expected<std::string>(Error(trace)); }

    const T& operator*() const { return mExpected; }
    const T* operator->() const { return &mExpected; }

    bool isExpected() const { return mIsExpected; }
    const T& value() const { return mExpected; }
    const Error& error() const { return mError; }

private:
    // this should be a union, but it's kinda complicated and I should just switch to a third party lib
    T mExpected;
    Error mError;
    bool mIsExpected;
};

template <>
class Expected<void> {
public:
    Expected() : mIsExpected(true) {}
    Expected(Error error) : mError(error), mIsExpected(false) {}

    static Expected<void> error(const std::string& trace) { return Expected(Error(trace)); }

    bool isExpected() const { return mIsExpected; }
    const Error& error() const { return mError; }

private:
    Error mError;
    bool mIsExpected;
};
