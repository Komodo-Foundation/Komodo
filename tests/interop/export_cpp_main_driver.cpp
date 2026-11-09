#include <cstdio>
#include <string>
#include <vector>
#include "export_cpp_main.h"

int main() {
    std::printf("add: %ld\n", (long)add(40, 2));
    std::string s = shout("cpp", 3);
    std::printf("shout: %s (%zu chars)\n", s.c_str(), s.size());
    std::printf("lerp: %.2f\n", lerp(10.0, 20.0, 0.25));
    std::printf("is_pos: %d %d\n", (int)is_pos(5), (int)is_pos(-5));
    std::vector<int64_t> v;
    for (int i = 0; i < 5; i++) v.push_back(add(i, i));
    int64_t sum = 0;
    for (int64_t x : v) sum += x;
    std::printf("sum: %ld\n", (long)sum);
    Token t = (Token)(void *)0x1234;
    std::printf("token: %p\n", (void *)pass_through(t));
    std::printf("calls: %ld\n", (long)call_count());
    reset();
    std::printf("after reset: %ld\n", (long)call_count());
    return 0;
}
