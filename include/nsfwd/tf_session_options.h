#pragma once
#include <tensorflow/c/c_api.h>

class tf_session_options {
public:
	tf_session_options() {
		options = TF_NewSessionOptions();
	}

	~tf_session_options() {
		TF_DeleteSessionOptions(options);
	}

	tf_session_options(const tf_session_options&) = delete;
	tf_session_options& operator=(const tf_session_options&) = delete;
	tf_session_options(tf_session_options&&) = delete;
	tf_session_options& operator=(tf_session_options&&) = delete;

	operator TF_SessionOptions *() {
		return options;
	}

private:
	TF_SessionOptions *options = nullptr;
};