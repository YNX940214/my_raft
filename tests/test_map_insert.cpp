#include <map>
#include <string>
#include <iostream>

using namespace std;

bool compare_server(const std::tuple<string,int> &a, const std::tuple<string,int> &b){
    int port1=get<1>(a);
    int port2=get<1>(b);
    if (port1 < port2)
        return true;
    else
        return false;
}
int main() {
    map<std::tuple<string, int>, int> map1;
    std::tuple<string, int> server = std::make_tuple(string("1.2.3"), 123);
    auto temp = map1.insert({server, 1});
    cout<< map1[server];
    return 0;
}