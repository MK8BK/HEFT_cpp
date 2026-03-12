# HEFT_cpp

Heft algorithm implementation in c++

```bash
g++ -o SequentialHeft src/SequentialHeft.cpp  -Wall -Werror -std=c++20 -I include/
# for help
./SequentialHeft -h 
# for a stdin based reading
./SequentialHeft < example_graphs/article_example1.txt
# for a json file based reading, see class_ressources folder for examples
./SequentialHeft -p <processor_count> -f <json_file_path>

# to profile
g++ -o SequentialHeft src/SequentialHeft.cpp -Wall -Werror -std=c++20 -I include/ -O0 -pg
time ./SequentialHeft -p 20 -f test/task_graph_10000_1000_seed_80905.json
gprof SequentialHeft gmon.out | gprof2dot -s -w | dot -Gdpi=200 -Tpng -o test/profile.png

# to optimize
g++ -o SequentialHeft src/SequentialHeft.cpp -Wall -Werror -std=c++20 -I include/ -O2
time ./SequentialHeft -p 20 -f test/task_graph_10000_1000_seed_80905.json
```

