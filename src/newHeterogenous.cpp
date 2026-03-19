#include <iostream>
#include <fstream>
#include <vector>
#include <numeric>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <filesystem>
#include <cstring>



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
    TaskSchedulingProblemConfig() = delete;

    // the only allowed constructor
    TaskSchedulingProblemConfig(const NBT taskCount, const NBT processorCount) : v(taskCount), q(processorCount),
      successors(taskCount, vector<NBT>(0)),
      predecessors(taskCount, vector<NBT>(0)),
      data(taskCount, vector<DST>(taskCount, 0)),
      B(processorCount,
        vector<DRT>(processorCount, 1)),
      W(taskCount, vector<TDT>(processorCount, 0)),
      L(processorCount, 0) {
      for (NBT p{}; p < processorCount; ++p) B[p][p] = 0;
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

  struct TaskSchedule {
    NBT id;
    TDT start, end;
    TaskSchedule():id(-1), start(-1), end(-1){}
    TaskSchedule(const NBT id, const TDT start, const TDT end):id(id), start(start), end(end){}
  };

  bool operator<(const TaskSchedule& t1, const TaskSchedule& t2) {
    // assuming no intersections of schedules
    return t1.end<t2.end;
  }

  class Schedule {

  public:
    // number of jobs/tasks
    NBT v;

    // number of available processors
    NBT q;

    // task id: processor id, start time, end time
    vector<tuple<NBT, TDT, TDT> > taskSchedule;


    // processor id: ordered_set of {task id , start time, end time}
    vector<set<TaskSchedule>> _processorSchedule;

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

    [[nodiscard]] TDT getMakeSpan() const {
      TDT res{};
      for (NBT p{}; p < q; ++p)
        if (!_processorSchedule[p].empty())
          res = max(res, _processorSchedule[p].rbegin()->end);
      return res;
    }
  };


  ostream &operator<<(ostream &os, const Schedule &sc) {
    //for (size_t i{}; i<sc.taskSchedule.size(); ++i) {
    //  auto [processor, start, end] = sc.taskSchedule[i];
    //  os << '[' << i+1 << ":[" << start << ',' << end << ',' << processor << "]] " << '\n';
    //}
    for (NBT processor{}; processor < sc.q; ++processor) {
      os << 'P' << processor << ": ";
      for (auto [t, s, e]: sc._processorSchedule[processor])
        os << '[' << t + 1 << ":[" << s << ',' << e << "]] ";
      if (processor < sc.q - 1)[[likely]]
          os << '\n';
    }
    return os;
  }

  struct Gap {
    TDT start, end;
    Gap():start(-1), end(-1){}
    Gap(const TDT start, const TDT end):start(start), end(end){}
  };

  bool operator<(const Gap& gap1, const Gap& gap2) {
    // assume that no intersections between gaps, ok in this problem
    return gap1.end<gap2.end;
  }

  class HeftAlgorithm {
  public:
    explicit HeftAlgorithm(TaskSchedulingProblemConfig *tspcp) : tspc(tspcp),
                                                                 sch(tspcp->v, tspcp->q) {
    }

  private:
    TaskSchedulingProblemConfig *tspc;
    Schedule sch;

    void computeRank(vector<TDT> &uprank, vector<bool> &computed, vector<TDT> &Wmeans, vector<vector<TDT> > &cmeans,
                     const NBT task) const {
      if (computed[task]) return;
      for (const NBT succ: tspc->successors[task]) {
        if (!computed[succ])[[unlikely]]
            computeRank(uprank, computed, Wmeans, cmeans, succ);
        uprank[task] = max(uprank[task], cmeans[task][succ] + uprank[succ]);
      }
      uprank[task] += Wmeans[task];
      computed[task] = true;
    }

    void computeUprank(vector<TDT> &uprank) const {
      // compute the mean communication startup cost
      const TDT Lmean{accumulate(tspc->L.begin(), tspc->L.end(), static_cast<TDT>(0)) / tspc->q};

      // Wmeans[i] is the average execution time of task ni
      vector<TDT> Wmeans(tspc->v, 0);
      for (NBT ni{}; ni < tspc->v; ni++)
        Wmeans[ni] = accumulate(tspc->W[ni].begin(), tspc->W[ni].end(), static_cast<TDT>(0)) / tspc->q;

      // Bmeans is the average data transfer rate between processors
      DRT Bmean{};
      if (tspc->q == static_cast<NBT>(0)) Bmean = 1;
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
      unordered_set<NBT> entryTasks;
      for (NBT ni{}; ni < tspc->v; ni++) {
        if (tspc->successors[ni].empty()) exitTasks.insert(ni);
        if (tspc->predecessors[ni].empty()) entryTasks.insert(ni);
      }

      vector<bool> computed(tspc->v, false);
      for (const NBT exitTask: exitTasks) {
        uprank[exitTask] = Wmeans[exitTask];
        computed[exitTask] = true;
      }
      for (const NBT entryTask: entryTasks)
        computeRank(uprank, computed, Wmeans, cmeans, entryTask);
    }

    TDT computeEST(NBT task, NBT processor, vector<set<Gap>>& gaps, vector<Gap>& gapCache) {
      TDT tmpEst{};
      for (const NBT pred: tspc->predecessors[task]) {
        auto [predProcessor, predStart, predEnd] = sch.taskSchedule[pred];
        if (predProcessor != processor)[[likely]]
            tmpEst = max(tmpEst, predEnd + tspc->L[predProcessor]
                                 + tspc->data[pred][task] / tspc->B[predProcessor][processor]);
        else[[unlikely]]
            tmpEst = max(tmpEst, predEnd);
      }
      if (sch._processorSchedule[processor].empty()) return tmpEst;
      const Gap goalGap{tmpEst, tmpEst};
      auto searchStart{upper_bound(gaps[processor].begin(), gaps[processor].end(), goalGap)};
      TDT s,e;
      while (searchStart!=gaps[processor].end()) {
        s = searchStart->start, e = searchStart->end;
        if (tmpEst+tspc->W[task][processor]<=e) {
          gapCache[processor] = {s, e};
          return max(tmpEst, s);
        }
        ++searchStart;
      }
      const TDT processorEnd{sch._processorSchedule[processor].rbegin()->end};
      gapCache[processor] = {-1,-1};
      return max(tmpEst, processorEnd);
    }

    void updateGaps(vector<set<Gap>> &gaps, vector<Gap>& gapCache,
      const NBT processor, const TDT start, const TDT end,
                    const NBT task) {
      if (gapCache[processor].start==-1) {
        // was scheduled at the end
        auto lastTaskIt{sch._processorSchedule[processor].rbegin()};
        lastTaskIt++;
        if (lastTaskIt==sch._processorSchedule[processor].rend()) [[unlikely]]{
          // also is first task
          if (start>0)
            gaps[processor].insert({0, start});
        }else {
          // add possible gap before
          const TDT prevEnd{lastTaskIt->end};
          if (prevEnd<start)
            gaps[processor].insert({prevEnd, start});
        }
        return;
      }
      // was added to some gap
      gaps[processor].erase(gapCache[processor]);
      if (gapCache[processor].start<start)
        gaps[processor].insert({gapCache[processor].start, start});
      if (gapCache[processor].end>end)
        gaps[processor].insert({end, gapCache[processor].end});
    }

  public:
    Schedule &solve() {
      // uprank[i] is the uprank of task i
      vector<TDT> uprank(tspc->v, -1);
      computeUprank(uprank);

      // sorting tasks in nonincreasing uprank order
      vector<pair<TDT, NBT> > uprankTaskNum(tspc->v, {0, 0});
      for (NBT task{}; task < tspc->v; ++task)
        uprankTaskNum[task] = {uprank[task], task};
      sort(uprankTaskNum.begin(), uprankTaskNum.end(), greater<>());

      TDT bestEFT{}, bestEST{};
      NBT bestProcessor{};
      vector<TDT> EST(tspc->q);
      vector<TDT> EFT(tspc->q);
      // moving from v to log(v) soon using gap abstraction
      vector<set<Gap> > gaps(tspc->q);
      vector<Gap> gapCache(tspc->q);
      for (NBT i{}; i < tspc->v; ++i) {
        const NBT task{uprankTaskNum[i].second};
        for (NBT processor = 0; processor < tspc->q; ++processor) {
          EST[processor] = computeEST(task, processor, gaps, gapCache);
          EFT[processor] = EST[processor] + tspc->W[task][processor];
        }
        auto smallestEFT{min_element(EFT.begin(), EFT.end())};
        bestEFT = *smallestEFT;
        bestProcessor = smallestEFT - EFT.begin();
        bestEST = EST[bestProcessor];
        sch.scheduleTask(task, bestProcessor, bestEST, bestEFT);
        updateGaps(gaps, gapCache, bestProcessor, bestEST, bestEFT, task);
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

  bool checkSchedule(TaskSchedulingProblemConfig &tspc, Schedule &sc) {
    for (NBT task{}; task < sc.v; ++task) {
      auto [p, s, e] = sc.taskSchedule[task];
      for (NBT succ: tspc.successors[task]) {
        auto [sp, ss, se] = sc.taskSchedule[succ];
        if (sp == p && ss < e) return false;
        if (sp != p && e + tspc.L[p] + tspc.data[task][succ] / tspc.B[p][sp] > ss) return false;
      }
    }
    return true;
  }

} // namespace HEFT_CPP

void description_message() {
  cout << "Usage:\n";
  cout << "   interactively redirect .txt file using the following format\n";
  cout <<
      "      - two integers representing the number of tasks and the number of processors\n"
      "      - v lines are read, each one starts with the number of task successors\n"
      "      then the ids of said successors\n"
      "      - then vxv matrix data is read, the data transfer sizes between tasks\n"
      "      - then qxq matrix B is read, the data transfer rate between processors\n"
      "      - then vxq matrix W is read, the execution time of tasks on processors\n"
      "      - then q number form the vector L, the communication startup costs of processors\n";
}

int main(int argc, char *argv[]) {
  using namespace HEFT_CPP;

  if (argc == 2 && strcmp(argv[1], "-h") == 0) {
    description_message();
    return EXIT_SUCCESS;
  }

  TaskSchedulingProblemConfig tspc{readTspc(cin)};
  HeftAlgorithm heft(&tspc);
  Schedule schedule{heft.solve()};
  cout << schedule << endl;
  for (int i{1}; i < argc; ++i) {
    if (strcmp(argv[i], "-schedule_verification") == 0) {
      if (checkSchedule(tspc, schedule))
        cout << "Schedule satisfies constraints." << endl;
      else
        cout << "Schedule does not satisfy constraints." << endl;
    } else if (strcmp(argv[i], "-compute_makespan") == 0) {
      cout << "Makespan is " << schedule.getMakeSpan() << '.' << endl;
    }
  }
  return EXIT_SUCCESS;
}
