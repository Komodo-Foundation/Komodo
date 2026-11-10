// A real C++ class (std::string + std::vector) and two free functions,
// reached from Komodo through extern "C" wrappers and a mangled-name alias.
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <string>
#include <vector>

class Counter {
public:
    explicit Counter(int64_t start) : total_(start) {}
    int64_t add(int64_t n) {
        history_.push_back(n);
        total_ += n;
        return total_;
    }
    std::string label() const {
        std::string s = "Counter[" + std::to_string(history_.size()) + " adds, total "
                      + std::to_string(total_) + "]";
        return s;
    }
private:
    int64_t total_;
    std::vector<int64_t> history_;
    std::string unused_;
};

// ── the extern "C" surface ──
extern "C" {
void *counter_new(int64_t start) { return new Counter(start); }
int64_t counter_add(void *c, int64_t n) { return static_cast<Counter *>(c)->add(n); }
// the std::string is copied into malloc'd memory the caller owns for the
// life of the process (a tiny, deliberate leak: this is a demo)
const char *counter_label(void *c) {
    std::string s = static_cast<Counter *>(c)->label();
    char *out = static_cast<char *>(std::malloc(s.size() + 1));
    std::char_traits<char>::copy(out, s.c_str(), s.size() + 1);
    return out;
}
void counter_free(void *c) { delete static_cast<Counter *>(c); }

// a C name that is a Komodo keyword
int64_t match(int64_t x) { return x * 100; }
}

// an ordinary (mangled) C++ function: `_Z6triplel`
int64_t triple(int64_t x) { return x * 3; }
