#include <tensorflow/c/c_api.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <cstdint>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

int main()
{
	TF_Status *status = TF_NewStatus();
	TF_Graph *graph = TF_NewGraph();
	TF_SessionOptions *session_options = TF_NewSessionOptions();

	const char *tags[] = { "serve" };

	TF_Buffer *meta_graph = TF_NewBuffer();

	TF_Session *session = TF_LoadSessionFromSavedModel(
		session_options,
		nullptr,
		"../nsfw_model",
		tags,
		1,
		graph,
		meta_graph,
		status);

	if (TF_GetCode(status) != TF_OK) {
		std::cerr << "Failed to load model: "
			  << TF_Message(status)
			  << std::endl;
		return 1;
	}

	std::cout << "Loaded model OK" << std::endl;

	int width;
	int height;
	int channels;

	unsigned char *image = stbi_load(
		"/home/brain/test.png",
		&width,
		&height,
		&channels,
		3);

	if (!image) {
		std::cerr << "Failed to load image" << std::endl;
		return 1;
	}

	std::cout << "Loaded image "
		  << width
		  << "x"
		  << height
		  << std::endl;

	std::vector<unsigned char> resized(299 * 299 * 3);

	stbir_resize_uint8_linear(
		image,
		width,
		height,
		0,
		resized.data(),
		299,
		299,
		0,
		STBIR_RGB);

	stbi_image_free(image);

	std::vector<float> input(299 * 299 * 3);

	for (size_t i = 0; i < input.size(); ++i) {
		input[i] = static_cast<float>(resized[i]) / 255.0f;
	}

	std::int64_t input_dims[] = {
		1,
		299,
		299,
		3
	};

	TF_Tensor *input_tensor = TF_AllocateTensor(
		TF_FLOAT,
		input_dims,
		4,
		input.size() * sizeof(float));

	memcpy(
		TF_TensorData(input_tensor),
		input.data(),
		input.size() * sizeof(float));

	TF_Operation *input_op =
		TF_GraphOperationByName(graph, "serve_input_1");

	TF_Operation *output_op =
		TF_GraphOperationByName(graph, "StatefulPartitionedCall");

	if (!input_op) {
		std::cerr << "Could not find input operation" << std::endl;
		return 1;
	}

	if (!output_op) {
		std::cerr << "Could not find output operation" << std::endl;
		return 1;
	}

	TF_Output input_port;
	input_port.oper = input_op;
	input_port.index = 0;

	TF_Output output_port;
	output_port.oper = output_op;
	output_port.index = 0;

	TF_Tensor *output_tensor = nullptr;

	TF_SessionRun(
		session,
		nullptr,
		&input_port,
		&input_tensor,
		1,
		&output_port,
		&output_tensor,
		1,
		nullptr,
		0,
		nullptr,
		status);

	if (TF_GetCode(status) != TF_OK) {
		std::cerr << "Inference failed: "
			  << TF_Message(status)
			  << std::endl;
		return 1;
	}

	float *results =
		static_cast<float *>(TF_TensorData(output_tensor));

	std::cout << std::endl;
	std::cout << "Drawing : " << results[0] << std::endl;
	std::cout << "Hentai  : " << results[1] << std::endl;
	std::cout << "Neutral : " << results[2] << std::endl;
	std::cout << "Porn    : " << results[3] << std::endl;
	std::cout << "Sexy    : " << results[4] << std::endl;

	TF_DeleteTensor(output_tensor);
	TF_DeleteTensor(input_tensor);

	TF_DeleteSession(session, status);
	TF_DeleteGraph(graph);
	TF_DeleteSessionOptions(session_options);
	TF_DeleteBuffer(meta_graph);
	TF_DeleteStatus(status);

	return 0;
}
