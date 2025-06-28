#pragma once

#include <string>
#include <vector>
#include <cstdint> // 用于固定宽度整数如int32_t, uint64_t
#include <chrono>  // 用于时间相关类型

// 可能在应用程序中使用的基本类型定义。
// 这有助于维护一致性。

// 示例：如果经常使用ID，定义一个ID类型
using EntityId = uint64_t;

// 示例：如果需要为特定上下文定义特定的字符串类型
// （虽然std::string通常就够了）
// using SpecialStringType = std::string;

// 网络相关类型（如果变得很多，可以移动到网络特定的类型头文件中）
// using PortNumber = uint16_t;
// using IpAddress = std::string;

// 时间相关（通常直接使用std::chrono类型更好）
// using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

// 广泛使用的常量也可以放在这里，如果它们不适合
// 特定的类或模块上下文。
// 但是，在适当的地方更喜欢类枚举或静态const成员。

// namespace Constants {
//     constexpr int MAX_CONNECTIONS = 100;
//     constexpr double PI = 3.1415926535;
// }

// 如果您有跨模块使用的特定错误代码，可以在这里定义
// enum class GlobalErrorCode {
//     SUCCESS = 0,
//     UNKNOWN_ERROR = 1,
//     NETWORK_ERROR = 100,
//     FILE_IO_ERROR = 200
// };

// 这个文件旨在成为非常通用的类型和可能的常量的集合。
// 避免在这里放置复杂的类定义。保持简单。

