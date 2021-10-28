/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _AFL_FUZZER_UTILS
#define _AFL_FUZZER_UTILS

#include <string>
#include <fstream>

// Redirect AFL input to given file
void fuzzFile(std::string filePath) {
	// Write to file
	std::string line;
	std::ofstream outfile;
	outfile.open(filePath);

	while (std::getline(std::cin, line))
	{
		outfile << line << std::endl;
	}
	outfile.close();
}

#endif
