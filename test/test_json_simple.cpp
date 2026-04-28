#include <iostream>
#include <cassert>
#include <cstring>
#include <string>
#include "../include/utils.h"
#include "../include/legacy/judgeInfo.h"

using namespace std;

// Helper function to create a test problem
testProblem* create_test_problem() {
    testProblem* tp = new testProblem();
    tp->uuid = 12345;
    strcpy(tp->pid, "P1001");
    tp->lang = language::cpp;
    tp->code = "#include <iostream>\nint main() { return 0; }";
    return tp;
}

// Test basic JSON functions
void test_basic_json() {
    cout << "=== Testing Basic JSON Functions ===" << endl;
    
    // Create test data
    auto original_tp = create_test_problem();
    
    // Test serialization
    cout << "Testing JSON serialization..." << endl;
    auto j = serializeTestProblemToJson(*original_tp);
    cout << "Serialized JSON: " << j.dump(2) << endl;
    
    // Test deserialization
    cout << "Testing JSON deserialization..." << endl;
    auto deserialized_tp = deserializeTestProblemFromJson(j);
    
    // Verify data integrity
    assert(deserialized_tp != nullptr);
    assert(deserialized_tp->uuid == original_tp->uuid);
    assert(strcmp(deserialized_tp->pid, original_tp->pid) == 0);
    assert(deserialized_tp->lang == original_tp->lang);
    assert(deserialized_tp->code == original_tp->code);
    
    cout << "✅ Basic JSON functions work correctly!" << endl;
    cout << "  UUID: " << deserialized_tp->uuid << endl;
    cout << "  PID: " << deserialized_tp->pid << endl;
    cout << "  Language: " << static_cast<int>(deserialized_tp->lang) << endl;
    cout << "  Code length: " << deserialized_tp->code.length() << " chars" << endl;
    
    delete original_tp;
    // delete deserialized_tp;
    cout << endl;
}

// Test JSON string parsing
void test_json_string() {
    cout << "=== Testing JSON String Parsing ===" << endl;
    
    // Create a simple JSON string
    string json_str = "{\"uuid\":9999,\"pid\":\"P2002\",\"lang\":2,\"code\":\"print('Hello!')\"}";
    
    cout << "Input JSON: " << json_str << endl;
    
    // Test deserialization from string
    auto tp = deserializeTestProblemFromJsonString(json_str);
    
    // Verify results
    assert(tp != nullptr);
    assert(tp->uuid == 9999);
    assert(strcmp(tp->pid, "P2002") == 0);
    assert(tp->lang == language::python);
    assert(tp->code == "print('Hello!')");
    
    cout << "✅ JSON string parsing works correctly!" << endl;
    cout << "  UUID: " << tp->uuid << endl;
    cout << "  PID: " << tp->pid << endl;
    cout << "  Language: " << static_cast<int>(tp->lang) << endl;
    cout << "  Code: " << tp->code << endl;
    
    // delete tp;
    cout << endl;
}

int main() {
    cout << "Starting JSON functionality tests..." << endl << endl;
    
    try {
        test_basic_json();
        test_json_string();
        
        cout << "🎉 All JSON tests passed successfully!" << endl;
        return 0;
    } catch (const exception& e) {
        cout << "❌ Test failed with exception: " << e.what() << endl;
        return 1;
    } catch (...) {
        cout << "❌ Test failed with unknown exception" << endl;
        return 1;
    }
}
