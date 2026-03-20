#include <iostream>
#include <thread>
#include <fstream>
#include <vector>
#include <numeric>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <filesystem>
#include <optional>
#include <tuple>
#include <rapidjson/document.h>

#define PARALLELIZED_FOR_PROCESSORS
#undef PARALLELIZED_FOR_PROCESSORS

#ifdef PARALLELIZED_FOR_PROCESSORS
#include <task_thread_pool.hpp>
#endif

#define JSON_VERIFICATION
// comment below to enable json verification
#undef JSON_VERIFICATION


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

  class HomogenousTaskSchedulingProblemConfig {
  public:
    // Each node has an ID in [0, v[
    const NBT v;

    // number of processors to accomplish tasks
    const NBT q;

    // The successors and predecessors are accessible in O(1)
    vector<vector<NBT> > successors;
    vector<vector<NBT> > predecessors;

    // vector of communication data to send to successors
    vector<DST> data;

    // execution time of jobs on homogenous processors
    vector<TDT> W;

    // disable the default constructor
    HomogenousTaskSchedulingProblemConfig() = delete;

    // the only allowed constructor
    HomogenousTaskSchedulingProblemConfig(const NBT taskCount, const NBT processorCount) : v(taskCount),
      q(processorCount),
      successors(taskCount, vector<NBT>(0)),
      predecessors(taskCount, vector<NBT>(0)),
      data(taskCount, 0),
      W(taskCount, 0) {
    }

    // destructor, change if using dynamic c-style array
    ~HomogenousTaskSchedulingProblemConfig() = default;
  };

  struct TaskSchedule {
    NBT id;
    TDT start, end;
    TaskSchedule():id(-1), start(-1), end(-1){}
    TaskSchedule(const NBT id, const TDT start, const TDT end):id(id), start(start), end(end){}
  };

  ostream& operator<<(ostream& os, const TaskSchedule& tsc) {
    return os << "tsc[" << tsc.id+1 << ',' << tsc.start << ','<<tsc.end << ']';
  }

  bool operator<(const TaskSchedule& t1, const TaskSchedule& t2) {
    // assuming no intersections of schedules
    return t1.end<t2.end;
  }

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

  ostream& operator<<(ostream& os, const Gap& gap) {
    return os << "gap["<<gap.start<< ',' << gap.end << ']';
  }

  bool operator<(const Gap& gap1, const Gap& gap2) {
    // assume that no intersections between gaps, ok in this problem
    return gap1.end<gap2.end;
  }

  class HomogenousHeftAlgorithm {
  public:
    explicit HomogenousHeftAlgorithm(HomogenousTaskSchedulingProblemConfig *tspcp) : tspc(tspcp),
      sch(tspcp->v, tspcp->q) {
    }

  private:
    HomogenousTaskSchedulingProblemConfig *tspc;
    Schedule sch;

    void computeRank(vector<TDT> &uprank, vector<bool> &computed, const NBT task) const {
      if (computed[task]) return;
      for (NBT succ: tspc->successors[task]) {
        if (!computed[succ])[[unlikely]]
            computeRank(uprank, computed, succ);
        uprank[task] = max(uprank[task], uprank[succ]);
      }
      uprank[task] += tspc->data[task] + tspc->W[task];
      computed[task] = true;
    }

    void computeUprank(vector<TDT> &uprank) const {
      unordered_set<NBT> exitTasks;
      unordered_set<NBT> entryTasks;
      for (NBT ni{}; ni < tspc->v; ni++) {
        if (tspc->successors[ni].empty()) exitTasks.insert(ni);
        if (tspc->predecessors[ni].empty()) entryTasks.insert(ni);
      }

      vector<bool> computed(tspc->v, false);
      for (const NBT exitTask: exitTasks) {
        uprank[exitTask] = tspc->W[exitTask];
        computed[exitTask] = true;
      }
      for (const NBT task: entryTasks)
        computeRank(uprank, computed, task);
    }


    [[nodiscard]] TDT computeEST(const NBT task, const NBT processor,
                                 const vector<set<Gap>> &gaps, vector<Gap>& gapCache) const {
      TDT tmpEst{};
      for (const NBT pred: tspc->predecessors[task]) {
        auto [predProcessor, predStart, predEnd] = sch.taskSchedule[pred];
        if (predProcessor != processor)[[likely]]
            tmpEst = max(tmpEst, predEnd + tspc->data[pred]);
        else[[unlikely]]
            tmpEst = max(tmpEst, predEnd);
      }
      if (sch._processorSchedule[processor].empty()) return tmpEst;
      const Gap goalGap{tmpEst, tmpEst};
      auto searchStart{upper_bound(gaps[processor].begin(), gaps[processor].end(), goalGap)};
      TDT s,e;
      while (searchStart!=gaps[processor].end()) {
        s = searchStart->start, e = searchStart->end;
        if (max(tmpEst, s)+tspc->W[task]<=e) {
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
      //if (task==10078) {
      //  if (gapCache[processor].start==-1) {
      //    cout << "task 10078 was scheduled at the end\n" ;
      //    cout << sch._processorSchedule[processor].size() << endl;
      //  }else {
      //    cout << "task 10078 was not scheduled at the end\n" ;
      //  }
      //}
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
          //if (task==10078) {
          //  cout << "task 10078 is not the first task\n" ;
          //  cout << "added prevGap " << Gap{prevEnd, start} << endl;
          //}
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

      TDT bestEFT, bestEST;
      NBT bestProcessor;
      vector<TDT> EST(tspc->q);
      vector<TDT> EFT(tspc->q);
      // moving from v to log(v) soon using gap abstraction
      vector<set<Gap> > gaps(tspc->q);
      vector<Gap> gapCache(tspc->q);
#ifdef PARALLELIZED_FOR_PROCESSORS
      auto thCompute = [this](const NBT task, const NBT processor, vector<TDT> &EST, vector<TDT> &EFT) {
        EST[processor] = computeEST(task, processor);
        EFT[processor] = EST[processor] + tspc->W[task];
      };
      const int platformThreads{static_cast<int>(thread::hardware_concurrency())};
      int numThreads{max(1, min(platformThreads - 1, 2))};
      task_thread_pool::task_thread_pool tp(numThreads);
#endif
      //int count{};
      for (NBT i{}; i < tspc->v; ++i) {
        const NBT task{uprankTaskNum[i].second};
        for (NBT processor = 0; processor < tspc->q; ++processor) {
#ifdef PARALLELIZED_FOR_PROCESSORS
          tp.submit_detach(thCompute, task, processor, std::ref(EST), std::ref(EFT));
#else
          EST[processor] = computeEST(task, processor, gaps, gapCache);
          EFT[processor] = EST[processor] + tspc->W[task];
#endif
        }
#ifdef PARALLELIZED_FOR_PROCESSORS
        tp.wait_for_tasks();
#endif
        auto smallestEFT{min_element(EFT.begin(), EFT.end())};
        bestEFT = *smallestEFT;
        bestProcessor = smallestEFT - EFT.begin();
        bestEST = EST[bestProcessor];
        //if (task==10080 || task==10078) {
        //  count++;
        //  cout << task << "------------" << bestEST << ' ' << bestEFT << endl;
        //  if (count==2) exit(EXIT_FAILURE);
        //}
        sch.scheduleTask(task, bestProcessor, bestEST, bestEFT);
        updateGaps(gaps, gapCache, bestProcessor, bestEST, bestEFT, task);
      }
      return sch;
    }
  };

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
        if (max(tmpEst,s)+tspc->W[task][processor]<=e) {
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
#ifdef PARALLELIZED_FOR_PROCESSORS
      auto thCompute = [this](const NBT task, const NBT processor, vector<TDT> &EST, vector<TDT> &EFT) {
        EST[processor] = computeEST(task, processor);
        EFT[processor] = EST[processor] + tspc->W[task];
      };
      const int platformThreads{static_cast<int>(thread::hardware_concurrency())};
      int numThreads{max(1, min(platformThreads - 1, 2))};
      task_thread_pool::task_thread_pool tp(numThreads);
#endif
      for (NBT i{}; i < tspc->v; ++i) {
        const NBT task{uprankTaskNum[i].second};
        for (NBT processor = 0; processor < tspc->q; ++processor) {
#ifdef PARALLELIZED_FOR_PROCESSORS
          tp.submit_detach(thCompute, task, processor, std::ref(EST), std::ref(EFT));
#else
          EST[processor] = computeEST(task, processor, gaps, gapCache);
          EFT[processor] = EST[processor] + tspc->W[task][processor];
#endif
        }
#ifdef PARALLELIZED_FOR_PROCESSORS
        tp.wait_for_tasks();
#endif
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
    TaskSchedule t1, t2;
    // check non intersection of tasks on same processor
    for (NBT processor{}; processor<tspc.q; ++processor) {
      auto t1It{sc._processorSchedule[processor].begin()};
      // no tasks
      if(t1It==sc._processorSchedule[processor].end()) continue;
      auto t2It{t1It};
      ++t2It;
      // only one task
      if(t2It==sc._processorSchedule[processor].end()) continue;
      while (t2It!=sc._processorSchedule[processor].end()) {
        t1 = *t1It, t2 = *t2It;
        if (t2.start<t1.end) return false;
        ++t2It, ++t1It;
      }
    }
    // check dependency constraints
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


  bool checkHomogenousSchedule(HomogenousTaskSchedulingProblemConfig &tspc, Schedule &sc) {
    TaskSchedule t1, t2;
    for (NBT processor{}; processor<tspc.q; ++processor) {
      auto t1It{sc._processorSchedule[processor].begin()};
      // no tasks
      if(t1It==sc._processorSchedule[processor].end()) continue;
      auto t2It{t1It};
      ++t2It;
      // only one task
      if(t2It==sc._processorSchedule[processor].end()) continue;
      while (t2It!=sc._processorSchedule[processor].end()) {
        t1 = *t1It, t2 = *t2It;
        if (t2.start<t1.end) {
          cout << t1 << ' ' << t2 << endl;
          exit(EXIT_FAILURE);
          return false;
        }
        ++t2It, ++t1It;
      }
    }
    for (NBT task{}; task < sc.v; ++task) {
      auto [p, s, e] = sc.taskSchedule[task];
      for (NBT succ: tspc.successors[task]) {
        auto [sp, ss, se] = sc.taskSchedule[succ];
        if (sp == p && ss < e) return false;
        if (sp != p && e + tspc.data[task] > ss) return false;
      }
    }
    return true;
  }

  optional<HomogenousTaskSchedulingProblemConfig> fromJson(const string &jsonSource, const NBT processorCount) {
    using namespace rapidjson;
    Document document;
    document.Parse(jsonSource.c_str());
#ifdef JSON_VERIFICATION
    if (document.HasParseError()) [[unlikely]] {
      cerr << "[Json Decoding error]\n";
      return nullopt;
    }
    if (!document.IsObject()) [[unlikely]] {
      cerr << "[Json error: not json object]\n";
      return nullopt;
    }
    if (!document.HasMember("tasks")) [[unlikely]] {
      cerr << "[Json error: no tasks entry]\n";
      return nullopt;
    }
    if (!document["tasks"].IsArray()) [[unlikely]] {
      cerr << "[Json error: tasks entry is not an array]\n";
      return nullopt;
    }
#endif
    const Value &tasks{document["tasks"]};
    NBT taskCount{tasks.Size()};
    HomogenousTaskSchedulingProblemConfig tspc(taskCount, processorCount);
    NBT task, pred;
    TDT duration;
    vector<DST> memories(taskCount, 0);
    auto taskNameToId = [](const string &s) { return static_cast<NBT>(stol(s.substr(4)) - 1); };
#ifdef JSON_VERIFICATION
    NBT count{};
#endif
    for (auto &taskV: tasks.GetArray()) {
#ifdef JSON_VERIFICATION
      ++count;
      if (!taskV.IsObject() || !taskV.HasMember("id") ||
          !taskV.HasMember("duration") || !taskV.HasMember("memory") ||
          !taskV.HasMember("dependencies") || !taskV["duration"].IsInt64()
          || !taskV["id"].IsString() || !taskV["memory"].IsInt64() ||
          !taskV["dependencies"].IsArray()) [[unlikely]] {
        cerr << "[Json error: task" << count << " has an invalid structure]\n";
        return nullopt;
      }
#endif
      task = taskNameToId(taskV["id"].GetString());
      memories[task] = taskV["memory"].GetInt64();
      duration = taskV["duration"].GetInt64();
      tspc.W[task] = duration;
      tspc.predecessors[task].reserve(taskV["dependencies"].GetArray().Size());
      for (auto &taskId: taskV["dependencies"].GetArray()) {
#ifdef JSON_VERIFICATION
        if (!taskId.IsString()) [[unlikely]] {
          cerr << "[Json error: task" << count << " has an invalid structure]\n";
          return nullopt;
        }
#endif
        pred = taskNameToId(taskId.GetString());
        tspc.successors[pred].push_back(task);
        tspc.predecessors[task].push_back(pred);
      }
    }
    for (task = 0; task < tspc.v; ++task)
      tspc.data[task] = memories[task];
    return tspc;
  }
} // namespace HEFT_CPP

void description_message() {
  cout << "Usage:\n";
  cout << "  -p <processor_count> -f <json_file_path> [-schedule_verification] [-compute_makespan]\n\n";
  cout << "   or interactively redirect .txt file using the following format\n";
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
  if (argc >= 5 && strcmp(argv[1], "-p") == 0 && strcmp(argv[3], "-f") == 0) {
    NBT processorCount{stol(argv[2])};
    ifstream fs(argv[4], ios::in);
    const size_t fileSize{filesystem::file_size(argv[4])};
    string fileContents(fileSize, '\0');
    fs.read(fileContents.data(), static_cast<streamsize>(fileSize));
    optional<HomogenousTaskSchedulingProblemConfig> otspc{fromJson(fileContents, processorCount)};
    fileContents.clear(); // deallocate
    if (otspc == nullopt) {
      cerr << "Could not decode graph in json format, aborting.\n";
      return EXIT_FAILURE;
    }
    HomogenousHeftAlgorithm heft(&otspc.value());
    Schedule sch{heft.solve()};
    cout << sch << endl;
    for (int i{5}; i < argc; ++i) {
      if (strcmp(argv[i], "-schedule_verification") == 0) {
        if (checkHomogenousSchedule(otspc.value(), sch))
          cout << "Schedule satisfies constraints." << endl;
        else
          cout << "Schedule does not satisfy constraints." << endl;
      } else if (strcmp(argv[i], "-compute_makespan") == 0) {
        cout << "Makespan is " << sch.getMakeSpan() << '.' << endl;
      }
    }
    return EXIT_SUCCESS;
  }

  // else, read form stdin
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
