#include <fstream>

int main() {
    const char *path = "789.config";
    if ((file_fd_ = open(path, O_RDWR)) == -1) {
        std::ostringstream oss;
        oss << "无法打开配置文件: " << path;
        string s = oss.str();
        throw std::logic_error(s);
    }


}