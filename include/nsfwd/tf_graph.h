#pragma once
#include <tensorflow/c/c_api.h>

class tf_graph {
public:
	tf_graph() {
		graph = TF_NewGraph();
	}

	~tf_graph() {
		TF_DeleteGraph(graph);
	}

	tf_graph(const tf_graph&) = delete;
	tf_graph& operator=(const tf_graph&) = delete;
	tf_graph(tf_graph&&) = delete;
	tf_graph& operator=(tf_graph&&) = delete;

	operator TF_Graph *() {
		return graph;
	}

private:
	TF_Graph *graph = nullptr;
};