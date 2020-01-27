#include <string>       // std::string
#include <iostream>     // std::cout
#include <sstream>      // std::ostringstream

bool file_exists(const std::string &path) {
    FILE *fp = fopen(path.c_str(), "r");
    if (fp) {
        return true;
    } else {
        return false;
    }
}

int main () {
    std::cout << file_exists("non_exist_file");
    return 0;
}