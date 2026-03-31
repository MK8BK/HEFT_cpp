# python ../graph_generator.py --num_tasks 20 --max_dependencies 5

task_counts=(1000 5000 10000 15000 20000 25000 30000 35000 40000 45000 50000)

max_deps=(30)

for task_num in "${task_counts[@]}"; do
  for max_dep in "${max_deps[@]}"; do
    python ../graph_generator.py --num_tasks $task_num --max_dependencies $max_dep
  done
done

# processors
# 5
# 10
# 50
# 100
# 500
# 1000
# 5000
# 10000
