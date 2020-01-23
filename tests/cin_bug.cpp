#include <iostream>

using namespace std;

// it is astonished to find cin with a big usage bug that has seldom been mentioned by others
int main() {
    int a = 1;
    cin >> a;  //try 'a'
    cout << a << endl;
//    cout<<cin.fail();
//    cout << a;
    string b;
    cin >> b;
    cout << b << endl;
}