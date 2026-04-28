/**
 * @file Result.h
 * @brief 泛型 Result<T, E> 工具类，用于函数式风格的返回值/错误统一表达。
 */
#pragma once

#include <string>
#include <stdexcept> // 如果需要用于值访问的std::runtime_error
#include <utility>   // 用于std::move, std::in_place
#include <variant>   // 持有值或错误

// 定义一个简单的默认错误类型，可以自定义。
// 对于更复杂的错误，您可能使用错误代码的枚举类
// 或专用的错误结构体/类。
using DefaultErrorType = std::string;

template<typename T, typename E = DefaultErrorType>
class Result {
public:
    // 成功和失败状态的构造函数
    Result(T&& val) : data_(std::in_place_type<T>, std::move(val)) {} // 成功
    Result(const T& val) : data_(std::in_place_type<T>, val) {}      // 成功（拷贝）
    Result(E&& err) : data_(std::in_place_type<E>, std::move(err)) {} // 失败
    Result(const E& err) : data_(std::in_place_type<E>, err) {}      // 失败（拷贝）

    // 便利的静态工厂方法（可选但很好用）
    static Result<T, E> success(T&& value) {
        return Result<T, E>(std::move(value));
    }
    static Result<T, E> success(const T& value) {
        return Result<T, E>(value);
    }
    static Result<T, E> failure(E&& error_val) {
        return Result<T, E>(std::move(error_val));
    }
    static Result<T, E> failure(const E& error_val) {
        return Result<T, E>(error_val);
    }

    // 检查状态
    bool isSuccess() const {
        return std::holds_alternative<T>(data_);
    }
    bool isFailure() const {
        return std::holds_alternative<E>(data_);
    }

    // 访问器 - 如果访问错误状态会抛出异常
    // 或者你可以提供返回可选值/指针的版本
    const T& value() const {
        if (!isSuccess()) {
            // 考虑更具体的异常类型
            // throw std::runtime_error("尝试访问失败Result的值");
            throw std::runtime_error("Attempted to access value of a failed Result");
        }
        return std::get<T>(data_);
    }

    T& value() { // 非const版本
        if (!isSuccess()) {
            // throw std::runtime_error("尝试访问失败Result的值");
            throw std::runtime_error("Attempted to access value of a failed Result");
        }
        return std::get<T>(data_);
    }

    const E& error() const {
        if (!isFailure()) {
            //throw std::runtime_error("尝试访问成功Result的错误");
            throw std::runtime_error("Attempted to access error of a successful Result");
        }
        return std::get<E>(data_);
    }
    
    E& error() { // 非const版本
        if (!isFailure()) {
            //throw std::runtime_error("尝试访问成功Result的错误");
            throw std::runtime_error("Attempted to access error of a successful Result");
        }
        return std::get<E>(data_);
    }

    // 提供一种方式来获取值，如果是错误则返回默认值
    T value_or(T&& defaultValue) const {
        if (isSuccess()) {
            return std::get<T>(data_);
        }
        return std::move(defaultValue);
    }

    // operator bool用于快速检查成功
    explicit operator bool() const {
        return isSuccess();
    }

    // 单子操作（可选，用于函数式风格的链式调用）
    // 示例：map操作
    template<typename Func>
    auto map(Func&& func) -> Result<std::invoke_result_t<Func, T>, E> {
        if (isSuccess()) {
            try {
                return Result<std::invoke_result_t<Func, T>, E>::success(func(std::get<T>(data_)));
            } catch (const E& e) { // 如果func可以抛出错误类型E
                return Result<std::invoke_result_t<Func, T>, E>::failure(e);
            } catch (const std::exception& e) { // 或任何其他异常，转换为E
                 // 这种转换可能需要更复杂
                return Result<std::invoke_result_t<Func, T>, E>::failure(E(e.what())); 
            }
        }
        return Result<std::invoke_result_t<Func, T>, E>::failure(std::get<E>(data_));
    }

    // 示例：and_then（flatMap）
    template<typename Func>
    auto and_then(Func&& func) -> Result<typename std::invoke_result_t<Func, T>::ValueType, E> {
        // 假设Func返回一个Result<U, E>
        using NextResultType = std::invoke_result_t<Func, T>;
        if (isSuccess()) {
            try {
                return func(std::get<T>(data_));
            } catch (const E& e) {
                return NextResultType::failure(e);
            } catch (const std::exception& e) {
                return NextResultType::failure(E(e.what()));
            }
        }
        return NextResultType::failure(std::get<E>(data_));
    }

    // 值类型的类型别名，对and_then有用
    using ValueType = T;

private:
    std::variant<T, E> data_;
};

// 常见类型别名
using StringResult = Result<std::string>; // 使用DefaultErrorType（std::string）

// 对于不返回值但可能失败的操作，
// 常见做法是为T使用占位符类型，如`bool`或自定义的`Unit`类型。
struct Unit {}; // void类型成功的占位符
using VoidResult = Result<Unit>; // 表示成功或失败的操作

