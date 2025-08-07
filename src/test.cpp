#include <iostream>
#include <vector>
#include <ctime>
#include "../inc/ObjectPool.h"

using std::cout;
using std::endl;

struct TreeNode {
    int _val;
    TreeNode* _left;
    TreeNode* _right;
    TreeNode() : _val(0), _left(nullptr), _right(nullptr) {}
};

void TestObjectPool() {
    const std::size_t Rounds = 5;
    const std::size_t N = 100000;

    std::vector<TreeNode*> v1;
    v1.reserve(N);

    std::clock_t begin1 = std::clock();
    for (std::size_t j = 0; j < Rounds; ++j) {
        for (std::size_t i = 0; i < N; ++i) v1.push_back(new TreeNode);
        for (std::size_t i = 0; i < N; ++i) delete v1[i];
        v1.clear();
    }
    std::clock_t end1 = std::clock();

    std::vector<TreeNode*> v2;
    v2.reserve(N);

    ObjectPool<TreeNode> TNPool;
    std::clock_t begin2 = std::clock();
    for (std::size_t j = 0; j < Rounds; ++j) {
        for (std::size_t i = 0; i < N; ++i) v2.push_back(TNPool.New());
        for (std::size_t i = 0; i < N; ++i) TNPool.Delete(v2[i]);
        v2.clear();
    }
    std::clock_t end2 = std::clock();

    cout << "new/delete time (ticks): " << (end1 - begin1) << endl;
    cout << "object pool time (ticks): " << (end2 - begin2) << endl;
}

int main() {
    TestObjectPool();
    return 0;
}

// g++ test.cpp -std=c++17 -Wall -Wextra -O2 -o test
