#include <iostream>
#include <vector>
#include <task_thread_pool.hpp>

using namespace std;


int main() {
  vector<int> responses(147);
  auto compute = [](vector<int>& resps, const int x){resps[x] = x*x;};
  for (const int value : responses) cout << value << ' ';
  cout << endl;
  task_thread_pool::task_thread_pool tp;
  for (int i{}; i<147; ++i) tp.submit_detach(compute, std::ref(responses), i);
  tp.wait_for_tasks();
  for (const int value : responses) cout << value << ' ';
  cout << endl;
  return EXIT_SUCCESS;
}
