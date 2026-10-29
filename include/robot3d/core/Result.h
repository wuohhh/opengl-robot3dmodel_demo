#pragma once
#include <string>
namespace robot3d
{
namespace core
{
/** @brief 携带成功状态、返回值或错误原因的通用操作结果。 */
template <class T> struct Result
{
    bool ok;           ///< 操作是否成功；仅为 true 时应使用 value。
    T value;           ///< 成功时的返回值；failure() 将其初始化为 T()。
    std::string error; ///< 失败原因；success() 将其置为空字符串。
    static Result success(const T &value)
    {
        Result r = {true, value, std::string()};
        return r;
    }
    static Result failure(const std::string &error)
    {
        Result r = {false, T(), error};
        return r;
    }
};
}
}
