#include <iostream>
#include <fstream>
#include <vector>
#include <string>
using namespace std;

int main() {
    ifstream in("789.config");
    if (!in) {
        cout << "Cannot open input file.\n";
        return 1;
    }
    char str[255];
    while (in) {
        in.getline(str, 255);  // delim defaults to '\n'
        if (in) cout << str << endl;
        auto vec = split_str(str, ':');
        if (vec.size() != 2) {
            throw_line(string("failed to read the config file line: ") + str);
        }
    }
    in.close();

    return 0;
}