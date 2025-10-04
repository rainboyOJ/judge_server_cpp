//存储测试会话结果的容器

#pragma once
#include <vector>
#include <mutex>
#include <memory>
#include <filesystem>


#include "judgeInfo.h"
#include "memPool.h"
#include "utils.h"
#include "json.hpp"
#include "common/Types.h"

using namespace std::filesystem;
using json = nlohmann::json;

enum class readResultStatus {
    NOT_DATA,         // 没有数据
    NOT_NEW_DATA,     // 没有新的数据
    EXCEED_TESTBOX_ID, // 超出测试盒子ID范围
    SUCCESS,          // 成功读取数据
    FINISHED,         // 所有结果都已经读取完毕
};

// ============= 前向声明 =============
class testBox;
class resultContainer;

/**
 * @brief 测试会话类 - 管理单个完整测试任务的所有信息和状态
 * 
 * 设计职责：
 * - 管理一个完整评测会话的生命周期
 * - 存储所有测试用例的结果数据
 * - 跟踪评测进度和状态
 * - 提供线程安全的结果写入接口
 * - 支持JSON序列化输出
 * 
 * 使用场景：
 * 当用户提交代码进行评测时，会创建一个testSession来管理整个评测过程，
 * 包含多个测试用例的执行结果、编译信息、错误状态等。
 * 
 * 线程安全：
 * 所有公共接口都是线程安全的，内部使用互斥锁保护共享数据
 */
class testSession {
public:
    friend class resultContainer;
    // ============= 核心标识信息 =============
    int testBoxId;    // 测试盒子ID，用于标识不同的评测实例
    int uuid;         // 全局唯一评测ID
    testError err_type; // 评测错误类型（如编译错误、数据文件未找到等）

    // ============= 评测内容 =============
    std::unique_ptr<testProblem> test_problem_p; // 题目信息（代码、语言等）
    std::vector<TestCaseResult> TCR;             // 测试用例结果数组
    std::vector<TestCaseInfo> TCI;             // 测试用例信息数组,用于评测限制

protected:
    // ============= 进度跟踪 =============
    int data_size;      // 总测试用例数量
    int finish_cnt;     // 已完成的测试用例数量
    int readDone_cnt;   // 已读取的结果数量（用于增量读取）

    // ============= 线程安全 =============
    mutable std::mutex mtx_; // 互斥锁，保护所有成员变量

public:
    /**
     * @brief 默认构造函数
     * 初始化所有成员为默认状态
     */
    testSession();

    /**
     * @brief 写入单个测试用例结果
     * @param idx 测试用例索引（0开始）
     * @param tcr 测试用例结果对象
     * @return bool 是否所有测试用例都已完成
     * @note 线程安全，自动增加完成计数
     */
    bool writeResult(int idx, const TestCaseResult& tcr);

    /**
     * @brief 序列化测试结果为JSON格式
     * @return json 包含完整评测信息的JSON对象
     * @note 包含题目信息、所有测试用例结果、进度信息等
     */
    json serialize() const;

    /**
     * @brief 清空所有测试结果并重置状态
     * @note 将所有测试用例状态重置为INIT，清空计数器
     */
    void clear();

    /**
     * @brief 初始化测试会话
     * @param testBoxId 测试盒子ID
     * @param data_size 测试用例总数
     * @note 重置所有状态，准备开始新的评测
     */
    void init_by_test_id(int testBoxId, int data_size);

    /**
     * @brief 检查评测是否已完成
     * @return bool 是否所有测试用例都已完成
     */
    bool isFinished() const;

    /**
     * @brief 获取当前完成进度
     * @return double 完成百分比 (0.0 - 1.0)
     */
    double getProgress() const;
};

/**
 * @brief 结果容器类 - 管理多个测试会话的容器
 * 
 * 设计职责：
 * - 管理多个并发的测试会话
 * - 提供线程安全的结果写入和读取接口
 * - 管理内存池，优化内存分配
 * - 支持增量结果读取
 */
class resultContainer {
public:
    // 类型别名
    using Pointer = typename memoryPool<TestCaseResult>::Pointer;

    /**
     * @brief 构造函数
     * @param size 支持的最大并发测试会话数量
     */
    explicit resultContainer(int size, const testBox* testBoxPtr);

    /**
     * @brief 析构函数 - 清理所有资源
     */
    ~resultContainer();

    // ========= 得到一些相关信息 =====

    language getLanguage(int testBoxId)  {
        std::lock_guard<std::mutex> lck(sessions_[testBoxId].mtx_);
        return sessions_[testBoxId].test_problem_p->lang;
    }

    auto getPid(int testBoxId)  {
        std::lock_guard<std::mutex> lck(sessions_[testBoxId].mtx_);
        return sessions_[testBoxId].test_problem_p->pid;
    }

    std::string_view getCode(int testBoxId) {
        std::lock_guard<std::mutex> lck(sessions_[testBoxId].mtx_);
        return sessions_[testBoxId].test_problem_p->code;
    }

    std::string_view getExeName(int testBoxId) {
        std::lock_guard<std::mutex> lck(sessions_[testBoxId].mtx_);
        return sessions_[testBoxId].TCI[0].exe;
    }

    const testBox * getTestBoxPtr() const {
        return testBoxPtr_;
    }

    // ============= 会话管理接口 =============

    /**
     * @brief 初始化指定的测试会话
     * @param testId 测试会话ID
     * @param data_size 测试用例数量
     */
    void init_by_test_id(int testId, int data_size);

    /**
     * @brief 检查指定会话的评测是否完成
     * @param testBoxId 测试盒子ID
     * @return bool 是否已完成所有测试用例
     */
    bool isJudgeFinished(int testBoxId) const;

    /**
     * @brief 直接写入测试用例结果（推荐接口）
     * @param testBoxId 测试盒子ID
     * @param caseIndex 测试用例索引
     * @param result 测试用例结果
     * @return bool 是否所有测试用例都已完成
     */
    bool writeCaseResult(int testBoxId, int caseIndex, const TestCaseResult& result);

    // ============= 题目信息管理 =============

    /**
     * @brief 设置测试会话的题目信息
     * @param testBoxId 测试盒子ID
     * @param test_problem 题目信息（包含代码、语言等）
     */
    void init_testProblem(int testBoxId, std::unique_ptr<testProblem> test_problem);

    // 重载
    void init_testProblem(int testBoxId, int uuid, char pid[32], language lang, std::string&& code);

    /**
     * @brief 设置错误类型
     * @param testBoxId 测试盒子ID
     * @param err_type 错误类型
     */
    void setErrorType(int testBoxId, testError err_type);

    // ============= 结果读取接口 =============

    /**
     * @brief 读取JSON格式的测试结果
     * @param idx 测试会话索引
     * @param status 返回的读取状态
     * @return json 测试结果的JSON格式
     */
    json GetResultAsJson(int idx, readResultStatus& status);

    
    // 获取 testBoxId 对应工作目录(编译,测试)
    fs::path work_dir(int testBoxId) const {
        return fs::temp_directory_path() /
                           ("oj_compile_" + std::to_string(testBoxId));
    }

    // 获取 testBoxId 对应的可执行文件路径
    fs::path exe_path(int testBoxId) const {
        return work_dir(testBoxId) / "solution";
    }

    fs::path source_path(int testBoxId) {
        std::string source_name = "solution";
        auto lang = getLanguage(testBoxId);
        if (lang == language::cpp) {
            source_name += ".cpp";
        } else if (lang == language::python) {
            source_name += ".py";
        }
        return work_dir(testBoxId) / source_name;
    }

    // ============= 资源管理接口 =============

    /**
     * @brief 重置指定会话的所有数据
     * @param idx 会话索引
     */
    void ResetTestSessionById(int idx);

    /**
     * @brief 从内存池分配测试结果对象（兼容旧接口）
     * @param n 分配数量
     * @return testPointResult* 链表头指针
     */
    TestCaseResult* AllocateTestCaseResult_of_N(int n);

    void setTestCaseInfo(int testBoxId, int testCaseId, const TestCaseInfo& info);
    
    // 获取测试用例信息
    bool getTestCaseInfo(int testBoxId, int testCaseId, TestCaseInfo& info) const;

private:
    std::mutex mtx_;                              // 全局互斥锁
    memoryPool<TestCaseResult> mem_;              // 内存池
    std::vector<testSession> sessions_;           // 测试会话数组
    const testBox* testBoxPtr_;                   // 测试盒子指针（用于回调）
};
