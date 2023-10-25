#pragma once

namespace tessd {

	enum class exit_code : int {
		no_error =	0,
		read =		1,
		tess_init =	2,
		pix_read_mem =	3,
		image_size =	4,
		no_output =	5,
		timeout =	6,
		waitpid = 	7,
		exec_fail =	8,
		max = 		9,
	};

	inline const char* tessd_error[static_cast<int>(exit_code::max)] = {
		"No error",
		"Error reading from STDIN",
		"Error initialising Tesseract",
		"Error reading image",
		"Image dimensions too large",
		"No OCR output",
		"Program timeout"
		"waitpid() on tessd failed",
		"exec() failed to launch tessd",
		"", // Marks the end of the array!
	};

	inline void status(exit_code e) {
		exit(static_cast<int>(e));
	}

};