#include <iostream>
#include "stone.h"

int main(int argc, char* argv[]) {
	(void)argv;
	std::cout << "Stone - Fast Hardware LUT Network Inference & Synthesis Tool\n";
	std::cout << "Status: CLI tool and Verilog translation are currently under development (not yet implemented).\n";
	if (argc > 1) {
		std::cout << "Arguments provided: " << argc - 1 << "\n";
	}
	return 0;
}
