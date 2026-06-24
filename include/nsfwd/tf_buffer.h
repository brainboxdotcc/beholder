#pragma once
#include <tensorflow/c/c_api.h>

class tf_buffer {
public:
	tf_buffer() {
		buffer = TF_NewBuffer();
	}

	~tf_buffer() {
		TF_DeleteBuffer(buffer);
	}

	tf_buffer(const tf_buffer&) = delete;
	tf_buffer& operator=(const tf_buffer&) = delete;
	tf_buffer(tf_buffer&&) = delete;
	tf_buffer& operator=(tf_buffer&&) = delete;

	operator TF_Buffer *() {
		return buffer;
	}

private:
	TF_Buffer *buffer = nullptr;
};