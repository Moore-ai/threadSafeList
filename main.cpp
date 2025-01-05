#include "threadSafeList.hpp"

#include <iostream>
#include <thread>

int main() {
    ThreadSafe_list<int>list;

    auto func1=[&list]() {
        for (int i=1;i<=3;++i) {
            list.push_back(i);
        }
    };

    auto func2=[&list]() {
        for (int i=4;i<=6;++i) {
            list.push_back(i);
        }
    };

    std::thread th1(func1),th2(func2);

    th1.join();
    th2.join();

    list.for_each([](int n) {
        std::cout<<n<<" ";
    });
    std::cout<<std::endl;

    return 0;
}