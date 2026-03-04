#include <iostream>
#include <vector>
#include <cassert>
#include <numeric>
#include <unordered_set>
#include <algorithm>
#include <tuple>
using namespace std;

namespace HEFT_CPP{

// numeric base type used throughout
// possibly replace by `unsigned long long` later
using NBT = int;

// time duration type
using TDT = int;

// data size type
using DST = int;

// data transfer rate type
using DRT = int;

// Assuming a sparse DAG, this is an appropriate representation
class TaskSchedulingProblemConfig{
  public:

  // Each node has an ID in [0, v[
  const NBT v;

  // number of processors to accomplish tasks
  const NBT q;

  // The successors and predecessors are accessible in O(1)
  vector<vector<NBT>> successors;
  vector<vector<NBT>> predecessors;

  // matrix of communication data
  // possibly replace by c-style array later
  vector<vector<DST>> data;

  // matrix of data transfer rates between processors
  vector<vector<DRT>> B;

  // execution time of jobs on processors
  vector<vector<TDT>> W;

  // communication startup cost of processors
  vector<TDT> L;


  // disable the default constructor, v and q need to be defined first
  TaskSchedulingProblemConfig() = delete;

  // the only allowed constructor
  TaskSchedulingProblemConfig(NBT taskCount, NBT processorCount):
    v(taskCount), q(processorCount),
    successors(taskCount, vector<NBT>(0)),
    predecessors(taskCount, vector<NBT>(0)),
    data(taskCount, vector<DST>(taskCount, 0)),
    B(processorCount, vector<DRT>(processorCount, 0)),
    W(taskCount, vector<TDT>(processorCount, 0)),
    L(processorCount,0)
  {}

  // destructor, change if using dynamic c-style array
  ~TaskSchedulingProblemConfig() = default;

  // add an edge to the dag
  inline void addEdge(NBT sourceJob, NBT destJob){
    if(sourceJob<v && destJob<v) [[likely]]
      successors[sourceJob].push_back(destJob),
        predecessors[destJob].push_back(sourceJob);
  }

  // set the data transfer requirement between job source and dest
  // default is 0
  inline void setDataTransferRequirement(NBT sourceJob, NBT destJob,
      DST transferReq){
    if(sourceJob<v && destJob<v) [[likely]]
      data[sourceJob][destJob] = transferReq;
  }

  // set the data transfer rate between processor source and dest
  // default is 0
  inline void setDataTransferRate(NBT sourceProcessor, NBT destProcessor,
      DRT transferRate){
    if(sourceProcessor<q && destProcessor<q) [[likely]]
      B[sourceProcessor][destProcessor] = transferRate;
  }

  // set the execution time of a task on a particular processor
  inline void setExecutionTime(NBT task, NBT processor, TDT time){
    if(task<v && processor<q) [[likely]]
      W[task][processor] = time;
  }

  // set the communication startup cost of the processor `processor`
  inline void setCommunicationStartupCost(NBT processor, TDT commStartCost){
    if(processor<q) [[likely]]
      L[processor] = commStartCost;
  }
};

// class to represent the final schedule
class Schedule{
  public:
    // number of jobs/tasks
    NBT v;

    // number of available processors
    NBT q;

    // processor id: start time, end time, task id
    vector<vector<tuple<TDT,TDT,NBT>>> schedule;

    // disable default constructor
    Schedule() = delete;

    // constructor
    Schedule(NBT taskCount, NBT processorCount):
      v(taskCount), q(processorCount), schedule(processorCount,
          vector<tuple<TDT,TDT,NBT>>(0)){}

    // schedule a task to run on a specified processor 
    inline void scheduleTask(NBT task, NBT processor, TDT startTime,
        TDT endTime){
      if(task<v && processor<q && startTime<=endTime) [[likely]]
        schedule[processor].push_back({startTime, endTime, task});
    }
};

static void computeUprank(TaskSchedulingProblemConfig& tspc,
    vector<TDT>& uprank, unordered_set<NBT>& exitTasks,
    vector<vector<TDT>>& cmeans, vector<TDT>& Wmeans){
  unordered_set<NBT> visited;
  unordered_set<NBT> next;
  for(NBT exitTask : exitTasks){
    uprank[exitTask] = Wmeans[exitTask];
    visited.insert(exitTask);
    for(NBT task : tspc.predecessors[exitTask])
      next.insert(task);
  }
  auto nIt{next.begin()};
  while(static_cast<NBT>(visited.size())!=tspc.v-1){
    if(nIt==next.end()) break;
    bool flag;
    do{
      flag = true;
      for(NBT succ : tspc.successors[*nIt]){
        if(uprank[succ]==-1){
          flag = false; 
          break;
        }
      }
      if(flag) break;
      else nIt++;
    }while(true);
    NBT candidate{*nIt};
    uprank[candidate] = Wmeans[candidate];
    TDT maxBelow{-1};
    for(NBT nj : tspc.successors[candidate]){
      maxBelow = max(maxBelow, uprank[nj]+cmeans[candidate][nj]);
    }
    uprank[candidate] += maxBelow;
    visited.insert(candidate);
    for(NBT pred : tspc.predecessors[candidate])
      next.insert(pred);
    next.erase(candidate);
  }
}

// I have no limitations
void HeftSolve(TaskSchedulingProblemConfig& tspc, Schedule& sc){
  assert((void(" invalid schedule input"),
      (sc.v==tspc.v && sc.q==tspc.q)));

  // compute the mean communication startup cost
  TDT Lmean{accumulate(tspc.L.begin(), tspc.L.end(), 0)/tspc.q};

  // Wmeans[i] is the average execution time of task ni
  vector<TDT> Wmeans(tspc.v, 0);
  for(NBT ni{}; ni<tspc.v; ni++)
    // TODO: review cache friendliness
    Wmeans[ni] = accumulate(tspc.W[ni].begin(), tspc.W[ni].end(), 0)/tspc.q;

  // Bmeans is the average data transfer rate between processors
  DRT Bmean{};
  for(auto vect:tspc.B)
    Bmean+=accumulate(vect.begin(), vect.end(), 0)/(tspc.q * tspc.q);

  // cmeans[i][j] is the average communication cost between task i and j
  vector<vector<TDT>> cmeans(tspc.v, vector<TDT>(tspc.v, 0));
  for(NBT ni{}; ni<tspc.v; ni++){
    for(NBT nk{}; nk<tspc.v; nk++){
      if(ni==nk)[[unlikely]]
        continue;
      cmeans[ni][nk] = Lmean + (tspc.data[ni][nk]/Bmean);
    }
  }

  // EST[i][j] is the earliest execution start time of task i on processor j
  vector<vector<NBT>> EST(tspc.v,vector<NBT>(tspc.q, 0));

  // EFT[i][j] is the earliest execution finish time of task i on processor j
  vector<vector<NBT>> EFT(tspc.v,vector<NBT>(tspc.q, 0));

  // compute the exit and entry tasks
  unordered_set<NBT> entryTasks, exitTasks;
  for(NBT ni{}; ni<tspc.v; ni++){
    if(tspc.predecessors[ni].size()==0) entryTasks.insert(ni);
    if(tspc.successors[ni].size()==0) exitTasks.insert(ni);
  }

  // uprank[i] is the uprank of task i
  vector<TDT> uprank(tspc.v, -1);
  computeUprank(tspc, uprank, exitTasks, cmeans, Wmeans);
}

} // namespace HEFT_CPP

int main(){
  using namespace HEFT_CPP;
  TaskSchedulingProblemConfig tspc(20,50);
  return 0;
}
