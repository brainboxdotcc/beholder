#pragma once
#include <stdexcept>
#include <string_view>
#include <tensorflow/c/c_api.h>

class tf_operation {
public:
	tf_operation(TF_Graph *graph, std::string_view name, int index = 0) {
		operation = TF_GraphOperationByName(graph, name.data());
		if (!operation) {
			throw std::runtime_error("Failed to find graph operation " + std::string(name));
		}
		output.oper = operation;
		output.index = index;
	}

	TF_Output *port() {
		return &output;
	}

private:
	TF_Operation *operation = nullptr;
	TF_Output output;
};