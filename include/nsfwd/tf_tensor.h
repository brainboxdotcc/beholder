#pragma once
#include <tensorflow/c/c_api.h>

class tf_tensor {
public:
	tf_tensor() = default;

	explicit tf_tensor(TF_Tensor *_tensor) {
		tensor = _tensor;
	}

	tf_tensor(TF_DataType type, const int64_t *dims, int num_dims, size_t len) {
		tensor = TF_AllocateTensor(type, dims, num_dims, len);
	}

	~tf_tensor() {
		TF_DeleteTensor(tensor);
	}

	tf_tensor(const tf_tensor&) = delete;
	tf_tensor& operator=(const tf_tensor&) = delete;
	tf_tensor(tf_tensor&&) = delete;
	tf_tensor& operator=(tf_tensor&&) = delete;

	operator TF_Tensor *() {
		return tensor;
	}

	TF_Tensor **out() {
		return &tensor;
	}

	void *data() {
		return TF_TensorData(tensor);
	}

	template<typename T> T *as() {
		return static_cast<T *>(TF_TensorData(tensor));
	}

	operator bool() const {
		return tensor != nullptr;
	}

	void copy_from(const void *data, size_t len) {
		memcpy(TF_TensorData(tensor), data, len);
	}

private:
	TF_Tensor *tensor = nullptr;
};