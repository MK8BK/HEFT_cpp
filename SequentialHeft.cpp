#include <iostream>
#include <vector>
#include <cassert>
#include <cstdint>
#include <numeric>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <tuple>
#include <stack>
using namespace std;

namespace HEFT_CPP {
  // numeric base type used throughout
  // possibly replace by `unsigned long long` later
  using NBT = int64_t;

  // time duration type
  using TDT = int64_t;

  // data size type
  using DST = int64_t;

  // data transfer rate type
  using DRT = int64_t;

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
    TaskSchedulingProblemConfig(NBT taskCount, NBT processorCount) : v(taskCount), q(processorCount),
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
    inline void addEdge(NBT sourceJob, NBT destJob) {
      if (sourceJob < v && destJob < v) [[likely]]
          successors[sourceJob].push_back(destJob),
          predecessors[destJob].push_back(sourceJob);
    }

    // set the data transfer requirement between job source and dest
    // default is 0
    inline void setDataTransferRequirement(NBT sourceJob, NBT destJob,
                                           DST transferReq) {
      if (sourceJob < v && destJob < v) [[likely]]
          data[sourceJob][destJob] = transferReq;
    }

    // set the data transfer rate between processor source and dest
    // default is 0
    inline void setDataTransferRate(NBT sourceProcessor, NBT destProcessor,
                                    DRT transferRate) {
      if (sourceProcessor < q && destProcessor < q) [[likely]]
          B[sourceProcessor][destProcessor] = transferRate;
    }

    // set the execution time of a task on a particular processor
    inline void setExecutionTime(NBT task, NBT processor, TDT time) {
      if (task < v && processor < q) [[likely]]
          W[task][processor] = time;
    }

    // set the communication startup cost of the processor `processor`
    inline void setCommunicationStartupCost(NBT processor, TDT commStartCost) {
      if (processor < q) [[likely]]
          L[processor] = commStartCost;
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
    vector<set<tuple<NBT, TDT, TDT> > > _processorSchedule;

    vector<bool> scheduled;

    // disable default constructor
    Schedule() = delete;

    // constructor
    Schedule(NBT taskCount, NBT processorCount) : v(taskCount), q(processorCount),
                                                  taskSchedule(taskCount), _processorSchedule(processorCount),
                                                  scheduled(taskCount, false) {
    }

    // schedule a task to run on a specified processor
    inline void scheduleTask(NBT task, NBT processor, TDT startTime,
                             TDT endTime) {
      if (task < v && processor < q && startTime <= endTime) [[likely]]{
        _processorSchedule[processor].insert({startTime, endTime, task});
        taskSchedule[task] = {processor, startTime, endTime};
        scheduled[task] = true;
      }
    }
  };


  /**
   *
   * @param tspc /
   * @param t1
   * @param t2
   * @return true if t2 depends on t1 (using dfs)
   */
  bool taskPrecedes(TaskSchedulingProblemConfig &tspc, NBT t1, NBT t2) {
    vector<bool> visited;
    stack<NBT> s;
    s.push(t1);
    while (s.size() != static_cast<size_t>(0)) {
      NBT candidate{s.top()};
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
    HeftAlgorithm(TaskSchedulingProblemConfig *tspcp) : tspc(tspcp),
                                                        sch(tspcp->v, tspcp->q) {
    }

    TaskSchedulingProblemConfig *tspc;
    Schedule sch;

    void computeUprank(vector<TDT> &uprank, unordered_set<NBT> &exitTasks) {
      // compute the mean communication startup cost
      TDT Lmean{accumulate(tspc->L.begin(), tspc->L.end(), 0) / tspc->q};

      // Wmeans[i] is the average execution time of task ni
      vector<TDT> Wmeans(tspc->v, 0);
      for (NBT ni{}; ni < tspc->v; ni++)
        // TODO: review cache friendliness
          Wmeans[ni] = accumulate(tspc->W[ni].begin(), tspc->W[ni].end(), 0) / tspc->q;

      // Bmeans is the average data transfer rate between processors
      DRT Bmean{};
      for (auto vect: tspc->B)
        Bmean += accumulate(vect.begin(), vect.end(), 0) / (tspc->q * tspc->q);

      // cmeans[i][j] is the average communication cost between task i and j
      vector<vector<TDT> > cmeans(tspc->v, vector<TDT>(tspc->v, 0));
      for (NBT ni{}; ni < tspc->v; ni++) {
        for (NBT nk{}; nk < tspc->v; nk++) {
          if (ni == nk)[[unlikely]]
              continue;
          cmeans[ni][nk] = Lmean + (tspc->data[ni][nk] / Bmean);
        }
      }

      unordered_set<NBT> computed;
      unordered_set<NBT> predPool;
      for (NBT exitTask: exitTasks) {
        uprank[exitTask] = Wmeans[exitTask];
        computed.insert(exitTask);
        for (NBT task: tspc->predecessors[exitTask])
          predPool.insert(task);
      }
      auto canCompute = [this, &computed](NBT task) {
        return all_of(tspc->successors[task].begin(), tspc->successors[task].end(),
                      [&computed](NBT pred) {
                        return computed.find(pred) != computed.end();
                      }
        );
      };
      TDT maxBelow;
      // compute uprank of all tasks
      while (static_cast<NBT>(computed.size()) < tspc->v) {
        // find next uprank computation candidate
        auto It{predPool.begin()};
        while (It != predPool.end() && !canCompute(*It)) ++It;

        // compute uprank of candidate
        NBT candidate{*It};
        uprank[candidate] = Wmeans[candidate];
        maxBelow = 0;
        for (NBT succ: tspc->successors[candidate])
          maxBelow = max(maxBelow, cmeans[candidate][succ] + uprank[succ]);
        uprank[candidate] += maxBelow;
        computed.insert(candidate);
        predPool.erase(It);
      }
    }

    void computeEST(NBT task, NBT processor, vector<vector<TDT> > &EST,
                    vector<vector<TDT> > &EFT) {
      for (NBT pred: tspc->predecessors[task])
        if (!sch.scheduled[pred]) {
          cerr << "FATAL: cant compute EST" << endl;
          exit(EXIT_FAILURE);
        }
      TDT tmpEst{};
      for (NBT pred: tspc->predecessors[task]) {
        auto [predProcessor, predStart, predEnd] = sch.taskSchedule[pred];
        tmpEst = max(tmpEst, predEnd + tspc->L[predProcessor]
                             + (tspc->data[pred][task]) / (tspc->B[predProcessor][processor]));
      }

      auto prevIt{sch._processorSchedule[processor].begin()};
      auto it{sch._processorSchedule[processor].begin()};
      if (it != sch._processorSchedule[processor].end()) ++it;
      else {
        EST[task][processor] = tmpEst;
        return;
      }
      while (it!=sch._processorSchedule[processor].end() && get<2>(*it)<tmpEst)
        ++it, ++prevIt;
      if (it==sch._processorSchedule[processor].end()) {
        EST[task][processor] = tmpEst;
        return;
      }
      while (it != sch._processorSchedule[processor].end()) {
        auto [t1, s1, e1] = (*prevIt);
        auto [t2, s2, e2] = (*it);
        if ((s2 - e1) >= tspc->W[task][processor]) {
          EST[task][processor] = e1;
          return;
        }
        ++it, ++prevIt;
      }
      // if insert at the end
      EST[task][processor] = get<2>(*prevIt);
      // TODO: work on all this here, fishy
    }



    Schedule &solve() {
      assert((void(" invalid schedule input"),
        (sch.v==tspc->v && sch.q==tspc->q)));


      // compute the exit and entry tasks
      unordered_set<NBT> entryTasks, exitTasks;
      for (NBT ni{}; ni < tspc->v; ni++) {
        if (tspc->predecessors[ni].size() == 0) entryTasks.insert(ni);
        if (tspc->successors[ni].size() == 0) exitTasks.insert(ni);
      }

      // uprank[i] is the uprank of task i
      vector<TDT> uprank(tspc->v, -1);
      computeUprank(uprank, exitTasks);

      // EST[i][j] is the earliest execution start time of task i on processor j
      vector<vector<TDT> > EST(tspc->v, vector<TDT>(tspc->q, 0));

      // EFT[i][j] is the earliest execution finish time of task i on processor j
      vector<vector<TDT> > EFT(tspc->v, vector<TDT>(tspc->q, 0));

      // AFT[i] is the actual execution finish time of task i
      vector<TDT> AFT(tspc->v, 0);

      // avail[j] is the earliest time at which processor j
      // is ready for task execution
      vector<TDT> avail(tspc->q);

      // sorting tasks in nonincreasing uprank order
      vector<pair<TDT, NBT> > uprankTaskNum(tspc->v, {0, 0});
      for (NBT task{}; task < tspc->v; ++task)
        uprankTaskNum[task] = {uprank[task], task};
      sort(uprankTaskNum.begin(), uprankTaskNum.end(), greater<>());

      for (NBT i{}; i < tspc->v; ++i) {
        NBT task{uprankTaskNum[i].second};
        for (NBT processor{}; processor < tspc->q; ++processor) {
          // compute EST using insertion based scheduling policy
          computeEST(task, processor, EST, EFT);
          // compute EFT[task][processor]
          EFT[task][processor] = EFT[task][processor] + tspc->W[task][processor];
        }
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
      cin >> successorNum;
      tspc.successors.reserve(successorNum);
      for (NBT i{}; i < successorNum; ++i) {
        cin >> succ;
        tspc.successors[task].push_back(succ);
        tspc.predecessors[succ].push_back(task);
      }
    }
    for (NBT task1{}; task1 < v; ++task1)
      for (NBT task2{}; task2 < v; ++task2)
        cin >> tspc.data[task1][task2];
    for (NBT processor1{}; processor1 < q; ++processor1)
      for (NBT processor2{}; processor2 < q; ++processor2)
        cin >> tspc.B[processor1][processor2];
    for (NBT task{}; task < v; ++task)
      for (NBT processor{}; processor < q; ++processor)
        cin >> tspc.W[task][processor];
    for (NBT processor{}; processor < q; ++processor)
      cin >> tspc.L[processor];
    return tspc;
  }
} // namespace HEFT_CPP

int main() {
  using namespace HEFT_CPP;
  TaskSchedulingProblemConfig tspc{readTspc(cin)};
  return 0;
}
