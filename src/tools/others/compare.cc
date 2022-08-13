
#include <unordered_set>
#include <string>
#include <iostream>
#include <fstream>

#include <stdio.h>
#include <stdint.h>

int32_t main(int32_t argc, char* argv[])
{
	std::unordered_set<std::string> source_set;
	std::string filename = argv[1];
	std::fstream fin;
	fin.open(filename.c_str(), std::ios::in);

	std::string line;
	while (getline(fin, line)) {
    	source_set.emplace(line);

    	if (argc == 4) {
    		fprintf(stderr, "source: %s\n", line.c_str());
    	}
	}
	fin.close();

	filename = argv[2];
	std::fstream fin2;
	fin2.open(filename.c_str(), std::ios::in);

	while (getline(fin2, line)) {
    	auto iter = source_set.find(line);
    	if (iter != source_set.end()) {
    		fprintf(stderr, "found: %s\n", line.c_str());
    	}

    	if (argc == 4) {
    		fprintf(stderr, "dst: %s\n", line.c_str());
    	}
	}
	fin2.close();

	return 0;
}