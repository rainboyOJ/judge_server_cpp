#include <iostream>
#include <memory>
#include <vector>
#include <cstring>
#include <cassert>
#include "judgeInfo.h"
#include "utils.h"
#include "client_sockets.h"
#include "Logger.h"

// using namespace std;
using namespace std;

// Test helper function to create a sample testProblem
std::unique_ptr<testProblem> createSampleTestProblem() {
    auto tp = std::make_unique<testProblem>();
    tp->uuid = 12345;
    strcpy(tp->pid, "P1001");
    tp->lang = language::cpp;
    tp->code = "#include <iostream>\nint main() {\n    std::std::cout << \"Hello World!\" << std::endl;\n    return 0;\n}";
    return tp;
}

// Test JSON serialization functions
void test_json_serialization_functions() {
    std::cout << "=== Testing JSON Serialization Functions ===" << endl;
    
    // Create test problem
    auto original_tp = createSampleTestProblem();
    
    // Test JSON serialization
    json j = serializeTestProblemToJson(*original_tp);
    std::cout << "Serialized JSON:" << endl;
    std::cout << j.dump(2) << endl;
    
    // Test JSON deserialization
    auto deserialized_tp = deserializeTestProblemFromJson(j);
    
    // Verify data integrity
    assert(deserialized_tp != nullptr);
    assert(deserialized_tp->uuid == original_tp->uuid);
    assert(strcmp(deserialized_tp->pid, original_tp->pid) == 0);
    assert(deserialized_tp->lang == original_tp->lang);
    assert(deserialized_tp->code == original_tp->code);
    
    std::cout << "✅ JSON serialization/deserialization functions work correctly!" << endl;
    std::cout << endl;
}

// Test JSON string parsing
void test_json_string_parsing() {
    std::cout << "=== Testing JSON String Parsing ===" << endl;
    
    // Create a JSON string manually
    string json_str = R"DELIM({
        "uuid": 9999,
        "pid": "P2002",
        "lang": 2,
        "code": "print('Hello from Python!')"
    })DELIM";
    
    std::cout << "Input JSON string:" << endl;
    std::cout << json_str << endl;
    
    // Test deserialization from string
    auto tp = deserializeTestProblemFromJsonString(json_str);
    
    // Verify results
    assert(tp != nullptr);
    assert(tp->uuid == 9999);
    assert(strcmp(tp->pid, "P2002") == 0);
    assert(tp->lang == language::python);
    assert(tp->code == "print('Hello from Python!')");
    
    std::cout << "✅ JSON string parsing works correctly!" << endl;
    std::cout << "  UUID: " << tp->uuid << endl;
    std::cout << "  PID: " << tp->pid << endl;
    std::cout << "  Language: " << static_cast<int>(tp->lang) << endl;
    std::cout << "  Code: " << tp->code << endl;
    std::cout << endl;
}

// Test error handling for malformed JSON
void test_json_error_handling() {
    std::cout << "=== Testing JSON Error Handling ===" << endl;
    
    // Test malformed JSON strings
    vector<string> malformed_json = {
        "{\"uuid\": 123, \"invalid\":",  // Incomplete JSON
        "{uuid: 123}",                   // Missing quotes
        "not json at all",               // Not JSON
        "",                              // Empty string
        "null"                           // Valid JSON but wrong structure
    };
    
    for (const auto& bad_json : malformed_json) {
        std::cout << "Testing malformed JSON: " << bad_json.substr(0, 20) << "..." << endl;
        auto tp = deserializeTestProblemFromJsonString(bad_json);
        assert(tp == nullptr);  // Should return nullptr for malformed JSON
        std::cout << "  ✅ Correctly handled malformed JSON" << endl;
    }
    
    std::cout << "✅ JSON error handling works correctly!" << endl;
    std::cout << endl;
}

// Test binary vs JSON format detection logic
void test_format_detection() {
    std::cout << "=== Testing Format Detection Logic ===" << endl;
    
    auto tp = createSampleTestProblem();
    
    // Test binary format
    vector<uint8_t> binary_data = serializeTestProblem(*tp);
    std::cout << "Binary data starts with: " << static_cast<int>(binary_data[0]) << endl;
    assert(binary_data[0] != '{');  // Should not start with '{'
    
    // Test JSON format
    json j = serializeTestProblemToJson(*tp);
    string json_str = j.dump();
    std::cout << "JSON data starts with: '" << json_str[0] << "'" << endl;
    assert(json_str[0] == '{');  // Should start with '{'
    
    std::cout << "✅ Format detection logic criteria work correctly!" << endl;
    std::cout << endl;
}

// Test round-trip compatibility
void test_round_trip_compatibility() {
    std::cout << "=== Testing Round-trip Compatibility ===" << endl;
    
    auto original_tp = createSampleTestProblem();
    
    // Test Binary round-trip
    vector<uint8_t> binary_data = serializeTestProblem(*original_tp);
    testProblem binary_tp;
    deserializeTestProblem(binary_data.data(), binary_tp);
    
    // Test JSON round-trip
    json j = serializeTestProblemToJson(*original_tp);
    auto json_tp = deserializeTestProblemFromJson(j);
    
    // Verify both produce identical results
    assert(binary_tp.uuid == json_tp->uuid);
    assert(strcmp(binary_tp.pid, json_tp->pid) == 0);
    assert(binary_tp.lang == json_tp->lang);
    assert(binary_tp.code == json_tp->code);
    
    std::cout << "✅ Binary and JSON round-trips produce identical results!" << endl;
    std::cout << endl;
}

// Test various languages and special characters
void test_language_and_encoding() {
    std::cout << "=== Testing Language Support and Encoding ===" << endl;
    
    struct TestCase {
        language lang;
        string code;
        string description;
    };
    
    vector<TestCase> test_cases = {
        {language::cpp, "#include <iostream>\nint main() { return 0; }", "C++"},
        {language::python, "print('Hello, 世界!')", "Python with Unicode"},
        {language::c, "#include <stdio.h>\nint main() { printf(\"Hello\"); return 0; }", "C"}
    };
    
    for (const auto& test_case : test_cases) {
        std::cout << "Testing " << test_case.description << "..." << endl;
        
        auto tp = std::make_unique<testProblem>();
        tp->uuid = 1000;
        strcpy(tp->pid, "TEST");
        tp->lang = test_case.lang;
        tp->code = test_case.code;
        
        // JSON round-trip
        json j = serializeTestProblemToJson(*tp);
        auto restored_tp = deserializeTestProblemFromJson(j);
        
        assert(restored_tp != nullptr);
        assert(restored_tp->lang == test_case.lang);
        assert(restored_tp->code == test_case.code);
        
        std::cout << "  ✅ " << test_case.description << " handled correctly" << endl;
    }
    
    std::cout << "✅ All languages and encodings work correctly!" << endl;
    std::cout << endl;
}

// Test large code content
void test_large_content() {
    std::cout << "=== Testing Large Content Handling ===" << endl;
    
    auto tp = std::make_unique<testProblem>();
    tp->uuid = 99999;
    strcpy(tp->pid, "LARGE");
    tp->lang = language::cpp;
    
    // Create large code content
    string large_code = "#include <iostream>\n";
    for (int i = 0; i < 1000; i++) {
        large_code += "// This is line " + to_string(i) + " of a very long program\n";
    }
    large_code += "int main() { return 0; }";
    tp->code = large_code;
    
    std::cout << "Testing with " << large_code.length() << " character code..." << endl;
    
    // JSON round-trip
    json j = serializeTestProblemToJson(*tp);
    auto restored_tp = deserializeTestProblemFromJson(j);
    
    assert(restored_tp != nullptr);
    assert(restored_tp->uuid == 99999);
    assert(restored_tp->code == large_code);
    assert(restored_tp->code.length() == large_code.length());
    
    std::cout << "✅ Large content handled correctly!" << endl;
    std::cout << endl;
}

int main() {
    std::cout << "🚀 Starting Client Sockets JSON Functionality Tests" << endl;
    std::cout << "=================================================" << endl;
    std::cout << endl;
    
    try {
        // Run all tests
        test_json_serialization_functions();
        test_json_string_parsing();
        test_json_error_handling();
        test_format_detection();
        test_round_trip_compatibility();
        test_language_and_encoding();
        test_large_content();
        
        std::cout << "🎉 All tests passed successfully!" << endl;
        std::cout << "✅ JSON functionality in client_sockets is working correctly" << endl;
        std::cout << "✅ Format detection (binary vs JSON) is functioning properly" << endl;
        std::cout << "✅ Error handling for malformed JSON is robust" << endl;
        std::cout << "✅ Backward compatibility with binary format is maintained" << endl;
        
    } catch (const exception& e) {
        std::cout << "❌ Test failed with exception: " << e.what() << endl;
        return 1;
    } catch (...) {
        std::cout << "❌ Test failed with unknown exception" << endl;
        return 1;
    }
    
    return 0;
}
