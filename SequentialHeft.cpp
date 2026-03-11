#include <iostream>
#include <vector>
#include <cstdint>
#include <numeric>
#include <set>
#include <algorithm>
#include <limits>
#include <tuple>
#include <functional>

using namespace std;

namespace HEFT_CPP {

    using NBT = int64_t;      // integer index / count type
    using TDT = long double;  // time duration type
    using DST = long double;  // data size type
    using DRT = long double;  // data rate type

    class TaskSchedulingProblemConfig {
    public:
        const NBT v; // number of tasks
        const NBT q; // number of processors

        vector<vector<NBT>> successors;
        vector<vector<NBT>> predecessors;

        // data[i][j] = amount of data sent from task i to task j
        vector<vector<DST>> data;

        // B[p][r] = transfer rate from processor p to processor r
        vector<vector<DRT>> B;

        // W[i][p] = execution time of task i on processor p
        vector<vector<TDT>> W;

        // L[p] = communication startup cost of processor p
        vector<TDT> L;

        TaskSchedulingProblemConfig() = delete;

        TaskSchedulingProblemConfig(NBT taskCount, NBT processorCount)
            : v(taskCount),
              q(processorCount),
              successors(taskCount),
              predecessors(taskCount),
              data(taskCount, vector<DST>(taskCount, 0)),
              B(processorCount, vector<DRT>(processorCount, 0)),
              W(taskCount, vector<TDT>(processorCount, 0)),
              L(processorCount, 0) {}

        void addEdge(NBT sourceJob, NBT destJob) {
            if (0 <= sourceJob && sourceJob < v &&
                0 <= destJob   && destJob   < v) {
                successors[sourceJob].push_back(destJob);
                predecessors[destJob].push_back(sourceJob);
            }
        }

        void setDataTransferRequirement(NBT sourceJob, NBT destJob, DST transferReq) {
            if (0 <= sourceJob && sourceJob < v &&
                0 <= destJob   && destJob   < v) {
                data[sourceJob][destJob] = transferReq;
            }
        }

        void setDataTransferRate(NBT sourceProcessor, NBT destProcessor, DRT transferRate) {
            if (0 <= sourceProcessor && sourceProcessor < q &&
                0 <= destProcessor   && destProcessor   < q) {
                B[sourceProcessor][destProcessor] = transferRate;
            }
        }

        void setExecutionTime(NBT task, NBT processor, TDT time) {
            if (0 <= task      && task      < v &&
                0 <= processor && processor < q) {
                W[task][processor] = time;
            }
        }

        void setCommunicationStartupCost(NBT processor, TDT commStartCost) {
            if (0 <= processor && processor < q) {
                L[processor] = commStartCost;
            }
        }
    };

    struct SchedComp {
        bool operator()(const tuple<NBT, TDT, TDT>& a,
                        const tuple<NBT, TDT, TDT>& b) const {
            if (get<1>(a) != get<1>(b)) return get<1>(a) < get<1>(b); // start time
            if (get<2>(a) != get<2>(b)) return get<2>(a) < get<2>(b); // end time
            return get<0>(a) < get<0>(b);                             // task id
        }
    };

    class Schedule {
    public:
        NBT v;
        NBT q;

        // taskSchedule[task] = {processor, start, end}
        vector<tuple<NBT, TDT, TDT>> taskSchedule;

        // for each processor: ordered set of {task, start, end}
        vector<set<tuple<NBT, TDT, TDT>, SchedComp>> processorSchedule;

        vector<bool> scheduled;

        Schedule() = delete;

        Schedule(NBT taskCount, NBT processorCount)
            : v(taskCount),
              q(processorCount),
              taskSchedule(taskCount, {-1, 0, 0}),
              processorSchedule(processorCount),
              scheduled(taskCount, false) {}

        void scheduleTask(NBT task, NBT processor, TDT startTime, TDT endTime) {
            if (0 <= task      && task      < v &&
                0 <= processor && processor < q &&
                startTime <= endTime) {
                processorSchedule[processor].insert({task, startTime, endTime});
                taskSchedule[task] = {processor, startTime, endTime};
                scheduled[task] = true;
            }
        }
    };

    ostream& operator<<(ostream& os, const Schedule& sc) {
        for (NBT processor = 0; processor < sc.q; ++processor) {
            for (const auto& [task, start, end] : sc.processorSchedule[processor]) {
                os << '[' << task + 1 << ":[" << start << ',' << end << "]] ";
            }
            if (processor + 1 < sc.q) {
                os << '\n';
            }
        }
        return os;
    }

    class HeftAlgorithm {
    public:
        explicit HeftAlgorithm(TaskSchedulingProblemConfig* tspcp)
            : tspc(tspcp), sch(tspcp->v, tspcp->q) {}

        TaskSchedulingProblemConfig* tspc;
        Schedule sch;

        TDT meanComputationCost(NBT task) const {
            TDT sum = accumulate(tspc->W[task].begin(), tspc->W[task].end(), static_cast<TDT>(0));
            return sum / static_cast<TDT>(tspc->q);
        }

        TDT meanStartupCost() const {
            TDT sum = accumulate(tspc->L.begin(), tspc->L.end(), static_cast<TDT>(0));
            return sum / static_cast<TDT>(tspc->q);
        }

        TDT meanTransferRate() const {
            if (tspc->q <= 1) {
                return 1; // avoid division by zero in degenerate case
            }

            TDT sum = 0;
            NBT count = 0;

            for (NBT p = 0; p < tspc->q; ++p) {
                for (NBT r = 0; r < tspc->q; ++r) {
                    if (p == r) continue;
                    sum += tspc->B[p][r];
                    ++count;
                }
            }

            if (count == 0) return 1;
            return sum / static_cast<TDT>(count);
        }

        TDT meanCommunicationCost(NBT fromTask, NBT toTask, TDT Lmean, TDT Bmean) const {
            if (fromTask == toTask) return 0;
            if (Bmean == 0) {
                return numeric_limits<TDT>::infinity();
            }
            return Lmean + tspc->data[fromTask][toTask] / Bmean;
        }

        void computeUprank(vector<TDT>& uprank) const {
            const TDT Lmean = meanStartupCost();
            const TDT Bmean = meanTransferRate();

            vector<bool> done(tspc->v, false);

            function<TDT(NBT)> dfs = [&](NBT task) -> TDT {
                if (done[task]) {
                    return uprank[task];
                }

                TDT wbar = meanComputationCost(task);

                if (tspc->successors[task].empty()) {
                    uprank[task] = wbar;
                    done[task] = true;
                    return uprank[task];
                }

                TDT bestSucc = 0;
                for (NBT succ : tspc->successors[task]) {
                    TDT cbar = meanCommunicationCost(task, succ, Lmean, Bmean);
                    bestSucc = max(bestSucc, cbar + dfs(succ));
                }

                uprank[task] = wbar + bestSucc;
                done[task] = true;
                return uprank[task];
            };

            for (NBT task = 0; task < tspc->v; ++task) {
                dfs(task);
            }
        }

        TDT communicationCost(NBT predTask, NBT task, NBT predProcessor, NBT processor) const {
            if (predProcessor == processor) {
                return 0;
            }

            DRT rate = tspc->B[predProcessor][processor];
            if (rate == 0) {
                return numeric_limits<TDT>::infinity();
            }

            return tspc->L[predProcessor] + tspc->data[predTask][task] / rate;
        }

        TDT computeReadyTime(NBT task, NBT processor) const {
            TDT ready = 0;

            for (NBT pred : tspc->predecessors[task]) {
                auto [predProcessor, predStart, predEnd] = sch.taskSchedule[pred];

                // In HEFT this should already be scheduled because of the rank order.
                if (predProcessor < 0) {
                    return numeric_limits<TDT>::infinity();
                }

                ready = max(
                    ready,
                    predEnd + communicationCost(pred, task, predProcessor, processor)
                );
            }

            return ready;
        }

        TDT computeEST(NBT task, NBT processor) const {
            TDT ready = computeReadyTime(task, processor);
            TDT duration = tspc->W[task][processor];

            // Start from the data-ready time.
            TDT start = ready;

            // Scan the already occupied intervals on this processor.
            // If we find a large enough hole, place the task there.
            for (const auto& [otherTask, s, e] : sch.processorSchedule[processor]) {
                if (start + duration <= s) {
                    return start; // fits before this existing task
                }
                start = max(start, e); // otherwise move start to after this task
            }

            // If no gap was large enough, place it after the last scheduled task.
            return start;
        }

        Schedule& solve() {
            vector<TDT> uprank(tspc->v, 0);
            computeUprank(uprank);

            // Order tasks by NONINCREASING rank_u.
            // If equal rank, smaller task id first (deterministic tie-break).
            vector<NBT> order(tspc->v);
            for (NBT i = 0; i < tspc->v; ++i) {
                order[i] = i;
            }

            sort(order.begin(), order.end(),
                 [&](NBT a, NBT b) {
                     if (uprank[a] != uprank[b]) return uprank[a] > uprank[b];
                     return a < b;
                 });

            for (NBT task : order) {
                TDT bestEST = 0;
                TDT bestEFT = numeric_limits<TDT>::infinity();
                NBT bestProcessor = -1;

                for (NBT processor = 0; processor < tspc->q; ++processor) {
                    TDT est = computeEST(task, processor);
                    TDT eft = est + tspc->W[task][processor];

                    if (eft < bestEFT) {
                        bestEFT = eft;
                        bestEST = est;
                        bestProcessor = processor;
                    }
                }

                sch.scheduleTask(task, bestProcessor, bestEST, bestEFT);
            }

            return sch;
        }
    };

    TaskSchedulingProblemConfig readTspc(istream& in) {
        NBT v, q;
        in >> v >> q;

        TaskSchedulingProblemConfig tspc(v, q);

        for (NBT task = 0; task < v; ++task) {
            NBT successorNum;
            in >> successorNum;

            tspc.successors[task].reserve(successorNum);

            for (NBT i = 0; i < successorNum; ++i) {
                NBT succ;
                in >> succ;
                tspc.successors[task].push_back(succ);
                tspc.predecessors[succ].push_back(task);
            }
        }

        for (NBT task1 = 0; task1 < v; ++task1) {
            for (NBT task2 = 0; task2 < v; ++task2) {
                in >> tspc.data[task1][task2];
            }
        }

        for (NBT processor1 = 0; processor1 < q; ++processor1) {
            for (NBT processor2 = 0; processor2 < q; ++processor2) {
                in >> tspc.B[processor1][processor2];
            }
        }

        for (NBT task = 0; task < v; ++task) {
            for (NBT processor = 0; processor < q; ++processor) {
                in >> tspc.W[task][processor];
            }
        }

        for (NBT processor = 0; processor < q; ++processor) {
            in >> tspc.L[processor];
        }

        return tspc;
    }

} // namespace HEFT_CPP

int main() {
    using namespace HEFT_CPP;

    TaskSchedulingProblemConfig tspc = readTspc(cin);
    HeftAlgorithm heft(&tspc);
    Schedule schedule = heft.solve();

    cout << schedule << '\n';
    return 0;
}