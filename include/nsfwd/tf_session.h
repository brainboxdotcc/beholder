#pragma once
#include <tensorflow/c/c_api.h>
#include <nsfwd/tf_status.h>
#include <nsfwd/tf_operation.h>
#include <nsfwd/tf_tensor.h>

class tf_session {
public:
	tf_session() = default;

	explicit tf_session(TF_Session *session) {
		this->session = session;
	}

	~tf_session() {
		if (session) {
			tf_status status;
			TF_DeleteSession(session, status);
		}
	}

	tf_session(const tf_session&) = delete;
	tf_session& operator=(const tf_session&) = delete;
	tf_session(tf_session&&) = delete;
	tf_session& operator=(tf_session&&) = delete;

	operator TF_Session *() {
		return session;
	}

	void run(tf_operation& input_op, tf_tensor& input_tensor, tf_operation& output_op, tf_tensor& output_tensor, tf_status& status) {
		TF_Tensor* raw_input = input_tensor;
		TF_SessionRun(session, nullptr, input_op.port(), &raw_input, 1, output_op.port(), output_tensor.out(), 1, nullptr, 0, nullptr, status);
	}

private:
	TF_Session *session = nullptr;
};