#include <aws/lambda-runtime/runtime.h>

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
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "include/rapidjson/document.h"

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
        sch.scheduleTask(task, bestProcessor, bestEST, bestEFT);
        updateGaps(gaps, gapCache, bestProcessor, bestEST, bestEFT, task);
      }
      return sch;
    }
  };

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

  void scheduleToString(const Schedule& sc, string& output) {
    rapidjson::Document d;
    d.SetObject();
    NBT i{1};
    for (auto [p, s, e] : sc.taskSchedule) {

      rapidjson::Value taskSch(rapidjson::kObjectType);

      rapidjson::Value processorNumber; processorNumber.SetInt(p);
      rapidjson::Value processorNV; processorNumber.SetString("processor");
      taskSch.AddMember(processorNV, processorNumber, d.GetAllocator());

      rapidjson::Value startNumber; startNumber.SetInt(s);
      rapidjson::Value startNV; startNV.SetString("start");
      taskSch.AddMember(startNV, startNumber, d.GetAllocator());

      rapidjson::Value endNumber; endNumber.SetInt(e);
      rapidjson::Value endNV; endNV.SetString("end");
      taskSch.AddMember(endNV, endNumber, d.GetAllocator());

      rapidjson::Value taskNameV; string taskName{"task"+to_string(i)};
      taskNameV.SetString(taskName.data(), taskName.length(), d.GetAllocator());

      d.AddMember(taskNameV, taskSch, d.GetAllocator());
      ++i;
    }
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    d.Accept(writer);
    string s(buf.GetString(), buf.GetSize());
    output = std::move(s);
    s = "";
  }
} // namespace HEFT_CPP
void cmdRun(const string& jsonPayload, string& response) {
  using namespace HEFT_CPP;
  NBT processorCount{20};
  optional<HomogenousTaskSchedulingProblemConfig> otspc{fromJson(jsonPayload, processorCount)};
  if (otspc == nullopt) {
    cerr << "Could not decode graph in json format, aborting.\n";
    return;
  }
  HomogenousHeftAlgorithm heft(&otspc.value());
  Schedule sch{heft.solve()};
  scheduleToString(sch, response);
}

int main() {
    auto handler = [](aws::lambda_runtime::invocation_request const& req) {
        string response;
      cmdRun(req.payload, response);
        return aws::lambda_runtime::invocation_response::success(
            response, "application/json");
    };

    aws::lambda_runtime::run_handler(handler);
    return 0;
}
