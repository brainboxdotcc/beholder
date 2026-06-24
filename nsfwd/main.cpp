#include <malloc.h>
#include <nsfwd/stbi_image.h>
#include <nsfwd/tf_graph.h>
#include <nsfwd/tf_session_options.h>
#include <nsfwd/tf_buffer.h>
#include <nsfwd/tf_session.h>
#include <nsfwd/tf_tensor.h>
#include <nsfwd/tf_status.h>
#include <nsfwd/tf_operation.h>
#include <beholder/logger.h>
#include <nsfwd/log_aggregator.h>
#include <nsfwd/nsfwd.h>

using namespace drogon;

int run_server() {

	server_log_init();

	tf_status status;
	tf_graph graph;
	tf_session_options session_options;
	tf_buffer meta_graph;

	const char *tags[] = { "serve" };

	tf_session session(TF_LoadSessionFromSavedModel(session_options, nullptr, "../nsfw_model", tags, 1, graph, meta_graph, status));
	if (!status.ok()) {
		LOG_FATAL << "Failed to load model: " << status.message();
		return 1;
	}

	tf_operation input_op(graph, "serve_input_1");
	tf_operation output_op(graph, "StatefulPartitionedCall");

	LOG_INFO << "Loaded model";

	app().setThreadNum(1).setClientMaxBodySize(32 * 1024 * 1024).registerHandler( "/",
		[&input_op, &output_op, &session](const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {

			auto json_error = [&](drogon::HttpStatusCode code, std::string_view message) {
				auto body = fmt::format("{{\"error\":\"{}\"}}", message);
				auto resp = drogon::HttpResponse::newHttpResponse();
				resp->setStatusCode(code);
				resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
				resp->setBody(body);
				LOG_WARN << message;
				callback(resp);
			};

			stbi_image image(req->body());
			if (!image) {
				json_error(drogon::k400BadRequest, "invalid image");
				return;
			}

			LOG_INFO << "POST / -> Image: " << image.get_width() << "x" << image.get_height() << "x" << image.get_channels();

			float input[INPUT_SIZE];
			image.resize_and_normalise(input, INPUT_WIDTH, INPUT_HEIGHT);

			int64_t input_dims[] = { 1, INPUT_HEIGHT, INPUT_WIDTH, INPUT_CHANNELS };

			tf_tensor input_tensor(TF_FLOAT, input_dims, 4, INPUT_SIZE * sizeof(float));
			if (!input_tensor) {
				json_error(drogon::k500InternalServerError, "Tensor allocation failed");
				return;
			}

			input_tensor.copy_from(input, INPUT_SIZE * sizeof(float));

			tf_tensor output_tensor;
			tf_status status;

			session.run(input_op, input_tensor, output_op, output_tensor, status);
			if (!status.ok()) {
				json_error(drogon::k500InternalServerError, "Inference failed: " + status.message());
				return;
			}

			float *results = output_tensor.as<float>();

			auto reply_body = fmt::format(
				"{{\"drawing\":{:.6f},\"hentai\":{:.6f},\"neutral\":{:.6f},\"porn\":{:.6f},\"sexy\":{:.6f}}}\n",
				results[INDEX_DRAWING],
				results[INDEX_HENTAI],
				results[INDEX_NEUTRAL],
				results[INDEX_PORN],
				results[INDEX_SEXY]
			);
			auto resp = drogon::HttpResponse::newHttpResponse();
			resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
			resp->setBody(reply_body);

			callback(resp);
		},
		{ drogon::Post });

	drogon::app().addListener("127.0.0.1", 6969).run();

	return 0;
}

int main(int argc, char **argv) {
	if (argc > 1 && std::string_view(argv[1]) == "--child") {
		return run_server();
	}
	return run_supervisor(argv[0]);
}