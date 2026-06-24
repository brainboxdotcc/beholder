#pragma once
#include <tensorflow/c/c_api.h>
#include <string>

class tf_status {
public:
	tf_status()
	{
		status = TF_NewStatus();
	}

	~tf_status()
	{
		TF_DeleteStatus(status);
	}

	// Prevent copies and moves to guarantee safe lifetime management
	tf_status(const tf_status&) = delete;
	tf_status& operator=(const tf_status&) = delete;
	tf_status(tf_status&&) = delete;
	tf_status& operator=(tf_status&&) = delete;

	operator TF_Status *() {
		return status;
	}

	bool ok() const {
		return TF_GetCode(status) == TF_OK;
	}

	std::string message() const {
		return TF_Message(status);
	}

private:
	TF_Status *status = nullptr;
};
