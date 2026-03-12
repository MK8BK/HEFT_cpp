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
```

