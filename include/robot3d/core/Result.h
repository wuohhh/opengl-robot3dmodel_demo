#pragma once
#include <string>
namespace robot3d { namespace core {
template<class T> struct Result {
    bool ok;
    T value;
    std::string error;
    static Result success(const T& value) { Result r = {true, value, std::string()}; return r; }
    static Result failure(const std::string& error) { Result r = {false, T(), error}; return r; }
};
}}
