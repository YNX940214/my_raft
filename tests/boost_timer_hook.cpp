#include "boost/asio.hpp"
#include <iostream>

using namespace std;
using namespace boost::asio;

int main() {
    io_service io;
    deadline_timer t1(io);
    t1.expires_from_now(boost::posix_time::seconds(1));
    auto sp = std::make_shared<deadline_timer>(std::move(t1));

    sp->async_wait([sp](const boost::system::error_code &error) {
        if (error) {
            cout << "error: " << error.message() << endl;
        } else {
            int hooks = sp->expires_from_now(boost::posix_time::seconds(1));
            cout << "after the first timer expires, let's hook the second timer:\n";
            cout << " now, there are " << hooks << " on the timer" << endl;
        }
    });
    int c = sp->expires_from_now(boost::posix_time::seconds(1));
    cout << "waiting :" << c << endl;
    io.run();
//    int a;
//    cin>>a;
//    cout<<a;
    return 0;
}