#include <iostream>


using namespace std;

class A {
public:
    void print() {
        cout << (void *) reinterpret_cast<char *>(this + 1) << endl;
    }

    int a;
    int b;
    int c;
};

int main() {
    A a;
    cout << (void *) &a << endl;
    a.print();
}