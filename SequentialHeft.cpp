#include <iostream>
#include <vector>
#include <cstdint>
#include <numeric>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <limits>
#include <tuple>
#include <stack>
using namespace std;

namespace HEFT_CPP {
  // numeric base type used throughout
  // possibly replace by `unsigned long long` later
  using NBT = int64_t;

  // time duration type, should be floating point number
  using TDT = long double;

  // data size type
  using DST = long double;

  // data transfer rate type
  using DRT = long double;

  // Assuming a sparse DAG, this is an appropriate representation
  class TaskSchedulingProblemConfig {
  public:
    // Each node has an ID in [0, v[
    const NBT v;

    // number of processors to accomplish tasks
    const NBT q;

    // The successors and predecessors are accessible in O(1)
    vector<vector<NBT> > successors;
    vector<vector<NBT> > predecessors;

    // matrix of communication data
    // possibly replace by c-style array later
    vector<vector<DST> > data;

    // matrix of data transfer rates between processors
    vector<vector<DRT> > B;

    // execution time of jobs on processors
    vector<vector<TDT> > W;

    // communication startup cost of processors
    vector<TDT> L;


    // disable the default constructor
    TaskSchedulingProblemConfig();

    // the only allowed constructor
    TaskSchedulingProblemConfig(const NBT taskCount, const NBT processorCount) : v(taskCount), q(processorCount),
      successors(taskCount, vector<NBT>(0)),
      predecessors(taskCount, vector<NBT>(0)),
      data(taskCount, vector<DST>(taskCount, 0)),
      B(processorCount,
        vector<DRT>(processorCount, 0)),
      W(taskCount, vector<TDT>(processorCount, 0)),
      L(processorCount, 0) {
    }

    // destructor, change if using dynamic c-style array
    ~TaskSchedulingProblemConfig() = default;

    // add an edge to the dag
    void addEdge(const NBT sourceJob, const NBT destJob) {
      if (sourceJob < v && destJob < v) [[likely]]
          successors[sourceJob].push_back(destJob),
          predecessors[destJob].push_back(sourceJob);
    }

    // set the data transfer requirement between job source and dest
    // default is 0
    void setDataTransferRequirement(const NBT sourceJob, const NBT destJob,
                                    const DST transferReq) {
      if (sourceJob < v && destJob < v) [[likely]]
          data[sourceJob][destJob] = transferReq;
    }

    // set the data transfer rate between processor source and dest
    // default is 0
    void setDataTransferRate(const NBT sourceProcessor, const NBT destProcessor,
                             const DRT transferRate) {
      if (sourceProcessor < q && destProcessor < q) [[likely]]
          B[sourceProcessor][destProcessor] = transferRate;
    }

    // set the execution time of a task on a particular processor
    void setExecutionTime(const NBT task, const NBT processor, const TDT time) {
      if (task < v && processor < q) [[likely]]
          W[task][processor] = time;
    }

    // set the communication startup cost of the processor `processor`
    void setCommunicationStartupCost(const NBT processor, const TDT commStartCost) {
      if (processor < q) [[likely]]
          L[processor] = commStartCost;
    }
  };

  struct schedComp {
    bool operator()(const tuple<NBT, TDT, TDT> &t1, const tuple<NBT, TDT, TDT> &t2) const {
      return get<1>(t1) < get<1>(t2);
    }
  };

  // class to represent the final schedule
  class Schedule {
  public:
    // number of jobs/tasks
    NBT v;

    // number of available processors
    NBT q;

    // task id: processor id, start time, end time
    vector<tuple<NBT, TDT, TDT> > taskSchedule;


    // processor id: ordered_set of {task id , start time, end time}
    vector<set<tuple<NBT, TDT, TDT>, schedComp> > _processorSchedule;

    vector<bool> scheduled;

    // disable default constructor
    Schedule() = delete;

    // constructor
    Schedule(const NBT taskCount, const NBT processorCount) : v(taskCount), q(processorCount),
                                                              taskSchedule(taskCount),
                                                              _processorSchedule(processorCount),
                                                              scheduled(taskCount, false) {
    }

    // schedule a task to run on a specified processor
    void scheduleTask(NBT task, NBT processor, TDT startTime,
                      TDT endTime) {
      if (task < v && processor < q && startTime <= endTime) [[likely]]{
        _processorSchedule[processor].insert({task, startTime, endTime});
        taskSchedule[task] = {processor, startTime, endTime};
        scheduled[task] = true;
      }
    }
  };

  ostream &operator<<(ostream &os, const Schedule &sc) {
    //for (size_t i{}; i<sc.taskSchedule.size(); ++i) {
    //  auto [processor, start, end] = sc.taskSchedule[i];
    //  os << '[' << i+1 << ":[" << start << ',' << end << ',' << processor << "]] " << '\n';
    //}
    for (NBT processor{}; processor < sc.q; ++processor) {
      for (auto [t, s, e]: sc._processorSchedule[processor])
        os << '[' << t + 1 << ":[" << s << ',' << e << "]] ";
      if (processor < sc.q - 1)[[likely]]
          os << '\n';
    }
    return os;
  }

  /**
   * deprecate soon
   * @param tspc /
   * @param t1
   * @param t2
   * @return true if t2 depends on t1 (using dfs)
   */
  bool taskPrecedes(const TaskSchedulingProblemConfig &tspc, const NBT t1, const NBT t2) {
    vector<bool> visited;
    stack<NBT> s;
    s.push(t1);
    while (s.size() != static_cast<size_t>(0)) {
      const NBT candidate{s.top()};
      if (candidate == t2) return true;
      s.pop();
      if (visited[candidate]) continue;
      for (NBT succ: tspc.successors[candidate]) {
        if (succ == t2) return true;
        if (!visited[succ])
          s.push(succ);
      }
    }
    return false;
  }


  class HeftAlgorithm {
  public:
    explicit HeftAlgorithm(TaskSchedulingProblemConfig *tspcp) : tspc(tspcp),
                                                                 sch(tspcp->v, tspcp->q) {
    }

    TaskSchedulingProblemConfig *tspc;
    Schedule sch;

    void computeUprank(vector<TDT> &uprank) const {
      // compute the mean communication startup cost
      const TDT Lmean{accumulate(tspc->L.begin(), tspc->L.end(), static_cast<TDT>(0)) / tspc->q};

      // Wmeans[i] is the average execution time of task ni
      vector<TDT> Wmeans(tspc->v, 0);
      for (NBT ni{}; ni < tspc->v; ni++)
        Wmeans[ni] = accumulate(tspc->W[ni].begin(), tspc->W[ni].end(), static_cast<TDT>(0)) / tspc->q;

      // Bmeans is the average data transfer rate between processors
      DRT Bmean{};
      if (tspc->q==static_cast<NBT>(0)) Bmean = 1;
      else {
        for (auto vect: tspc->B)
          Bmean += accumulate(vect.begin(), vect.end(), static_cast<DRT>(0));
        Bmean /= tspc->q * (tspc->q - 1);
      }

      // cmeans[i][j] is the average communication cost between task i and j
      vector<vector<TDT> > cmeans(tspc->v, vector<TDT>(tspc->v, 0));
      for (NBT ni{}; ni < tspc->v; ni++) {
        for (NBT nk{}; nk < tspc->v; nk++) {
          if (ni == nk)[[unlikely]]
              continue;
          cmeans[ni][nk] = Lmean + tspc->data[ni][nk] / Bmean;
        }
      }

      unordered_set<NBT> exitTasks;
      for (NBT ni{}; ni < tspc->v; ni++) {
        if (tspc->successors[ni].empty()) exitTasks.insert(ni);
      }

      vector<bool> computed(tspc->v, false);
      unordered_set<NBT> predPool;
      for (const NBT exitTask: exitTasks) {
        uprank[exitTask] = Wmeans[exitTask];
        computed[exitTask] = true;
        for (NBT task: tspc->predecessors[exitTask])
          predPool.insert(task);
      }
      auto canCompute = [&computed, this](const NBT task) {
        return all_of(tspc->successors[task].begin(), tspc->successors[task].end(),
                      [&computed](const NBT pred) {
                        return computed[pred];
                      }
        );
      };
      // compute uprank of all tasks
      while (!predPool.empty()) {
        // find next uprank computation candidate
        auto it{predPool.begin()};
        while (it != predPool.end() && !canCompute(*it)) ++it;

        // compute uprank of candidate
        const NBT candidate{*it};
        uprank[candidate] = 0;
        for (const NBT succ: tspc->successors[candidate]) {
          uprank[candidate] = max(uprank[candidate], cmeans[candidate][succ] + uprank[succ]);
        }
        for (NBT pred: tspc->predecessors[candidate])
          if (predPool.find(pred) == predPool.end() && !computed[pred])
            predPool.insert(pred);
        uprank[candidate] += Wmeans[candidate];
        computed[candidate] = true;
        predPool.erase(it);
      }
    }

    TDT computeEST(NBT task, NBT processor) {
      TDT tmpEst{};
      for (const NBT pred: tspc->predecessors[task]) {
        auto [predProcessor, predStart, predEnd] = sch.taskSchedule[pred];
        if (predProcessor != processor)[[likely]]
            tmpEst = max(tmpEst, predEnd + tspc->L[predProcessor]
                                 + tspc->data[pred][task] / tspc->B[predProcessor][processor]);
        else[[unlikely]]
            tmpEst = max(tmpEst, predEnd);
      }
      if (sch._processorSchedule[processor].empty() || get<2>(*sch._processorSchedule[processor].rbegin()) < tmpEst)
        return tmpEst;
      auto prevIt{sch._processorSchedule[processor].begin()};
      auto it{prevIt};
      ++it;
      if (sch._processorSchedule[processor].end() == it) {
        // only one element scheduled on processor
        return max(tmpEst, get<2>(*prevIt));
      }
      while (it != sch._processorSchedule[processor].end() && get<1>(*it) < tmpEst + tspc->W[task][processor])
        ++prevIt, ++it;
      if (it == sch._processorSchedule[processor].end())
        return max(tmpEst, get<2>(*prevIt));
      TDT gap;
      while (it != sch._processorSchedule[processor].end()) {
        gap = get<1>(*it) - get<2>(*prevIt);
        if (gap > 0 && get<2>(*prevIt) >= tmpEst && gap >= tmpEst + tspc->W[task][processor]) {
          return get<2>(*prevIt);
        }
        ++prevIt, ++it;
      }
      return max(tmpEst, get<2>(*prevIt));
    }


    Schedule &solve() {
      // uprank[i] is the uprank of task i
      vector<TDT> uprank(tspc->v, -1);
      computeUprank(uprank);
      for (NBT i{}; i<tspc->v; ++i)
        cout << i << ' ' << uprank[i] << endl;

      // sorting tasks in nonincreasing uprank order
      vector<pair<TDT, NBT> > uprankTaskNum(tspc->v, {0, 0});
      for (NBT task{}; task < tspc->v; ++task)
        uprankTaskNum[task] = {uprank[task], task};
      sort(uprankTaskNum.begin(), uprankTaskNum.end(), greater<>());

      TDT bestEFT{}, currentEFT{}, currentEST{}, bestEST{};
      NBT bestProcessor{};
      for (NBT i{}; i < tspc->v; ++i) {
        const NBT task{uprankTaskNum[i].second};
        bestEFT = numeric_limits<TDT>::max();
        for (NBT processor{}; processor < tspc->q; ++processor) {
          // compute EST using insertion based scheduling policy
          currentEST = computeEST(task, processor);
          // compute EFT[task][processor]
          currentEFT = currentEST + tspc->W[task][processor];
          if (bestEFT > currentEFT)
            bestEFT = currentEFT,
                bestProcessor = processor,
                bestEST = currentEST;
        }
        sch.scheduleTask(task, bestProcessor, bestEST, bestEFT);
      }
      return sch;
    }
  };

  /**
   * Function to read a tspc, the format is specified here, in order, read:
   * - two integers representing the number of tasks and the number of processors
   * - v lines are read, each one starts with the number of task successors
   * then the ids of said successors
   * - then vxv matrix data is read, the data transfer sizes between tasks
   * - then qxq matrix B is read, the data transfer rate between processors
   * - then vxq matrix W is read, the execution time of tasks on processors
   * - then q number form the vector L, the communication startup costs of processors
  */
  TaskSchedulingProblemConfig readTspc(istream &in) {
    NBT v, q;
    in >> v >> q;
    TaskSchedulingProblemConfig tspc(v, q);
    NBT successorNum, succ;
    for (NBT task{}; task < v; ++task) {
      in >> successorNum;
      tspc.successors.reserve(successorNum);
      for (NBT i{}; i < successorNum; ++i) {
        in >> succ;
        tspc.successors[task].push_back(succ);
        tspc.predecessors[succ].push_back(task);
      }
    }
    for (NBT task1{}; task1 < v; ++task1)
      for (NBT task2{}; task2 < v; ++task2)
        in >> tspc.data[task1][task2];
    for (NBT processor1{}; processor1 < q; ++processor1)
      for (NBT processor2{}; processor2 < q; ++processor2)
        in >> tspc.B[processor1][processor2];
    for (NBT task{}; task < v; ++task)
      for (NBT processor{}; processor < q; ++processor)
        in >> tspc.W[task][processor];
    for (NBT processor{}; processor < q; ++processor)
      in >> tspc.L[processor];
    return tspc;
  }
} // namespace HEFT_CPP

int main() {
  using namespace HEFT_CPP;
  TaskSchedulingProblemConfig tspc{readTspc(cin)};
  HeftAlgorithm heft(&tspc);
  Schedule schedule{heft.solve()};
  cout << schedule << endl;
  return 0;
}
