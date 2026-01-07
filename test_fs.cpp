#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;
int main() {
    std::cout << fs::current_path() << std::endl;
    return 0;
}
