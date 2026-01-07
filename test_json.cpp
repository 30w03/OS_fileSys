#include <iostream>
#include <string>
#include <vector>
#include <cstring>

std::string parseJsonField(const std::string& json, const std::string& field) {
    std::string key = "\"" + field + "\"";
    size_t keyPos = json.find(key);
    if (keyPos == std::string::npos) {
        return "";
    }
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) {
        return "";
    }
    
    size_t valueStart = json.find('"', colonPos);
    if (valueStart == std::string::npos) {
        return "";
    }
    valueStart++; // Skip quote
    
    // Find end quote, handling escaped quotes
    size_t valueEnd = valueStart;
    while (true) {
        valueEnd = json.find('"', valueEnd);
        if (valueEnd == std::string::npos) {
            return "";
        }
        
        // Count backslashes before valueEnd to determine if quote is escaped
        size_t backslashCount = 0;
        size_t checkPos = valueEnd - 1;
        while (checkPos >= valueStart && json[checkPos] == '\\') {
            backslashCount++;
            if (checkPos == 0) break;
            checkPos--;
        }
        
        if (backslashCount % 2 == 0) {
            break; // Even number of backslashes means quote is NOT escaped
        }
        
        valueEnd++; // Skip escaped quote
    }
    
    return json.substr(valueStart, valueEnd - valueStart);
}

int main() {
    std::string hugeData(1024 * 1024, 'A'); // 1MB of 'A'
    std::string json = "{\"decision\":\"ACCEPT\",\"fileData\":\"" + hugeData + "\",\"filename\":\"test.pdf\"}";
    
    std::string filename = parseJsonField(json, "filename");
    std::cout << "Filename: '" << filename << "'" << std::endl;
    
    if (filename == "test.pdf") {
        std::cout << "SUCCESS" << std::endl;
    } else {
        std::cout << "FAILURE" << std::endl;
    }
    
    return 0;
}
