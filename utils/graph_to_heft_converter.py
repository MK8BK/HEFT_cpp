import json
import sys

if len(sys.argv) < 3:
    print("Usage: python3 graph_to_heft_input.py graph.json <processor_count>", file=sys.stderr)
    sys.exit(1)

json_path = sys.argv[1]
q = int(sys.argv[2])

with open(json_path, "r") as f:
    graph = json.load(f)

tasks = graph["tasks"]
v = len(tasks)

# Map task id string -> index in [0, v)
id_to_index = {task["id"]: i for i, task in enumerate(tasks)}

# Build successors from dependencies
successors = [[] for _ in range(v)]
for task in tasks:
    current = id_to_index[task["id"]]
    for dep_id in task["dependencies"]:
        dep = id_to_index[dep_id]
        successors[dep].append(current)

# 1) v q
print(v, q)

# 2) successors list for each task
for i in range(v):
    row = [str(len(successors[i]))] + [str(s) for s in successors[i]]
    print(" ".join(row))

# 3) data matrix: all zero (no communication cost)
for _ in range(v):
    print(" ".join(["0"] * v))

# 4) B matrix: all ones (positive, but irrelevant because data is zero)
for _ in range(q):
    print(" ".join(["1"] * q))

# 5) W matrix: same duration on every processor (homogeneous)
for task in tasks:
    d = task["duration"]
    print(" ".join([str(d)] * q))

# 6) L vector: all zero startup costs
print(" ".join(["0"] * q))