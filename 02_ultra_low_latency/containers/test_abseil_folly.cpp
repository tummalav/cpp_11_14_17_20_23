#include <iostream>
#include "absl/container/flat_hash_map.h"
#include <folly/FBVector.h>
int main() {
    absl::flat_hash_map<int, int> map;
    map[1] = 100;
    folly::fbvector<int> vec;
    vec.push_back(42);
    std::cout << "Abseil map[1] = " << map[1] << std::endl;
    std::cout << "Folly vec[0] = " << vec[0] << std::endl;
    std::cout << "SUCCESS: Both libraries are working!" << std::endl;
    return 0;
}
