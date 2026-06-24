#include <tensorflow/c/c_api.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <stdint.h>
#include <fmt/format.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#include <drogon/drogon.h>
#include <emmintrin.h>
#include <beholder/logger.h>

using namespace drogon;

int main()
{
	logger::init("logs/nsfwd.log");
	trantor::Logger::setOutputFunction([](const char *msg, uint64_t len) {
		dpp::log_t l;
		l.severity = dpp::ll_info;
		if (len < 43) {
			l.message = std::string(msg, len);
		} else {
			std::string s = std::string(msg + 43, len - 43);
			while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
				s.pop_back();
			}
			size_t colon = s.rfind(':');
			if (colon != std::string::npos) {
				bool numeric = true;
				for (size_t i = colon + 1; i < s.size(); ++i) {
					if (!isdigit(static_cast<unsigned char>(s[i]))) {
						numeric = false;
						break;
					}
				}
				if (numeric) {
					size_t dash = s.rfind(" - ", colon);
					if (dash != std::string::npos) {
						s.erase(dash);
					}
				}
			}
			l.message = s;
		}
		logger::log(l);
	}, []() {});

	int pipefd[2];
	pipe(pipefd);
	dup2(pipefd[1], STDERR_FILENO);
	close(pipefd[1]);
	std::thread([fd = pipefd[0]] {
		char buffer[4096];
		while (true) {
			ssize_t n = read(fd, buffer, sizeof(buffer));
			if (n <= 9) {
				break;
			}
			dpp::log_t l;
			l.severity = dpp::ll_warning;
			l.message = std::string(buffer + 9, n - 9);
			logger::log(l);
		}
	}).detach();

	setenv("TF_CPP_MIN_LOG_LEVEL", "1", 1);

	TF_Status *status = TF_NewStatus();
	TF_Graph *graph = TF_NewGraph();
	TF_SessionOptions *session_options = TF_NewSessionOptions();
	const char *tags[] = { "serve" };

	TF_Buffer *meta_graph = TF_NewBuffer();
	TF_Session *session = TF_LoadSessionFromSavedModel(session_options, nullptr, "../nsfw_model", tags, 1, graph, meta_graph, status);
	if (TF_GetCode(status) != TF_OK) {
		std::cerr << "Failed to load model: " << TF_Message(status) << std::endl;
		return 1;
	}

	LOG_INFO << "Loaded model";

	app().setClientMaxBodySize(32 * 1024 * 1024).registerHandler(
		"/",
		[graph, session, status](const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {

			auto json_error = [&](drogon::HttpStatusCode code, std::string_view message) {
				auto body = fmt::format("{{\"error\":\"{}\"}}", message);
				auto resp = drogon::HttpResponse::newHttpResponse();
				resp->setStatusCode(code);
				resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
				resp->setBody(body);
				callback(resp);
			};

			int width, height, channels;
			const std::string_view body = req->body();
			unsigned char *image = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(body.data()), body.size(), &width, &height, &channels, 3);

			if (!image) {
				auto resp = drogon::HttpResponse::newHttpResponse();
				json_error(drogon::k400BadRequest, "invalid image");
				return;
			}

			std::vector<unsigned char> resized(299 * 299 * 3);
			stbir_resize_uint8_linear(image, width, height, 0, resized.data(), 299, 299, 0, STBIR_RGB);
			stbi_image_free(image);

			std::vector<float> input(299 * 299 * 3);


			const __m128 scale = _mm_set1_ps(1.0f / 255.0f);
			size_t i = 0;
			for (; i + 16 <= resized.size(); i += 16) {
				__m128i bytes = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&resized[i]));
				__m128i zero = _mm_setzero_si128();
				__m128i lo16 = _mm_unpacklo_epi8(bytes, zero);
				__m128i hi16 = _mm_unpackhi_epi8(bytes, zero);
				__m128i lo32a = _mm_unpacklo_epi16(lo16, zero);
				__m128i lo32b = _mm_unpackhi_epi16(lo16, zero);
				__m128i hi32a = _mm_unpacklo_epi16(hi16, zero);
				__m128i hi32b = _mm_unpackhi_epi16(hi16, zero);
				_mm_storeu_ps(&input[i], _mm_mul_ps(_mm_cvtepi32_ps(lo32a), scale));
				_mm_storeu_ps(&input[i + 4], _mm_mul_ps(_mm_cvtepi32_ps(lo32b), scale));
				_mm_storeu_ps(&input[i + 8], _mm_mul_ps(_mm_cvtepi32_ps(hi32a), scale));
				_mm_storeu_ps(&input[i + 12], _mm_mul_ps(_mm_cvtepi32_ps(hi32b), scale));
			}
			for (; i < resized.size(); ++i) {
				input[i] = static_cast<float>(resized[i]) * (1.0f / 255.0f);
			}

			int64_t input_dims[] = { 1, 299, 299, 3 };
			TF_Tensor *input_tensor = TF_AllocateTensor(TF_FLOAT, input_dims, 4, input.size() * sizeof(float));
			memcpy(TF_TensorData(input_tensor), input.data(), input.size() * sizeof(float));

			TF_Operation *input_op = TF_GraphOperationByName(graph, "serve_input_1");
			TF_Operation *output_op = TF_GraphOperationByName(graph, "StatefulPartitionedCall");

			if (!input_op) {
				json_error(drogon::k500InternalServerError, "could not find input operation");
				return;
			}

			if (!output_op) {
				json_error(drogon::k500InternalServerError, "could not find output operation");
				return;
			}

			TF_Output input_port;
			input_port.oper = input_op;
			input_port.index = 0;

			TF_Output output_port;
			output_port.oper = output_op;
			output_port.index = 0;

			TF_Tensor *output_tensor = nullptr;

			TF_SessionRun(session, nullptr, &input_port, &input_tensor, 1, &output_port, &output_tensor, 1, nullptr, 0, nullptr, status);
			if (TF_GetCode(status) != TF_OK) {
				json_error(drogon::k500InternalServerError, "Inference failed: " + std::string(TF_Message(status)));
				return;
			}

			float *results = static_cast<float *>(TF_TensorData(output_tensor));

			auto reply_body = fmt::format("{{\"drawing\":{:.6f},\"hentai\":{:.6f},\"neutral\":{:.6f},\"porn\":{:.6f},\"sexy\":{:.6f}}}\n", results[0], results[1], results[2], results[3], results[4]);
			auto resp = drogon::HttpResponse::newHttpResponse();
			resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
			resp->setBody(reply_body);

			TF_DeleteTensor(output_tensor);
			TF_DeleteTensor(input_tensor);

			callback(resp);
		},
		{ drogon::Post });

	drogon::app().addListener("127.0.0.1", 6969).run();

	TF_DeleteSession(session, status);
	TF_DeleteGraph(graph);
	TF_DeleteSessionOptions(session_options);
	TF_DeleteBuffer(meta_graph);
	TF_DeleteStatus(status);

	return 0;
}
