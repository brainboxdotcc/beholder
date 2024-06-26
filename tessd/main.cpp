/************************************************************************************
 * 
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023 Craig Edwards <support@sporks.gg>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/
#include <dpp/dpp.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <iostream>
#include <array>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>
#include <beholder/tessd.h>
#include <beholder/config.h>
#include <beholder/sentry.h>

/**
 * @brief Tesseract Daemon
 * 
 * This is a separate standalone program which scans an image whos data is given via stdin,
 * and outputs the text via stdout once EOF is received on stdin.
 * 
 * This was originally done within the main bot process as part of ocr.cpp, and although it was
 * a fair bit faster, libtesseract has some huge memory leaks, which happen when you call
 * tesseract::TessBaseAPI::GetUTF8Text(). These are internal leaks, which I do not have the
 * inclination or technical knowledge in image processing internals to fix. Checking on their
 * issue tracker on github shows 400 issues, dating back as far as 2017 which are still open.
 * This means that if i report the leak, it won't get fixed any time in a reasonable timeframe.
 * As the main program which uses tesseract is a short lived program like this, it is likely
 * the never noticed the issue.
 * 
 * By isolating tesseract in its own program like this, we ensure that Linux can free up the
 * memory leak for us. We can also pre-launch copies of this program, each waiting for stdin
 * in a similar way to how fastcgi works. This would improve performance if needed.
 */

void tessd_timeout(int sig)
{
	tessd::status(tessd::exit_code::timeout);
}

void set_limit(int type, uint64_t max)
{
	rlimit64 r;
	if (getrlimit64(type, &r) != -1) {
		if (r.rlim_cur > max || r.rlim_max > max || r.rlim_max == 0) {
			r.rlim_cur = r.rlim_max = max;
			setrlimit64(type, &r);
		}
	}
}

int main()
{
	/* Program has a hard coded maximum runtime of 1 minute */
	config::init("../config.json");
	dpp::cluster bot("");
	sentry::init(bot);
	void* read_image = sentry::register_transaction_type("OCR", "read image");
	void* init_tesseract = sentry::register_transaction_type("OCR", "init tesseract");
	void* do_tesseract = sentry::register_transaction_type("OCR", "run tesseract");
	signal(SIGALRM, tessd_timeout);
	alarm(60);

	/* Set process memory limit to 1gb */
	const uint64_t ONE_GIGABYTE = 1073741824;
	set_limit(RLIMIT_DATA, ONE_GIGABYTE);
	set_limit(RLIMIT_RSS, ONE_GIGABYTE);
	
	/* Tesseract outputs errors to terminal instead of letting us capture them via
	 * an error code. This is supremely dumb, but we can't do anything with these
	 * errors and they pollute our OCR output. So, we silence them.
	 */
	if (!std::freopen(nullptr, "rb", stdin)) {
		tessd::status(tessd::exit_code::read);
	}
	fclose(stderr);

	std::size_t len{0};
	constexpr std::size_t INIT_BUFFER_SIZE{10240};
	std::array<char, INIT_BUFFER_SIZE> buf;
	std::vector<char> input;

	/**
	 * Buffered read from stdin to vector of char
	 */
	void* tx_read_image = sentry::start_transaction(read_image);
	while((len = std::fread(buf.data(), sizeof(buf[0]), buf.size(), stdin)) > 0) {
		if(std::ferror(stdin) && !std::feof(stdin)) {
			sentry::log_critical("tessd", "Unexpected end of file on stdin");
			sentry::end_transaction(tx_read_image);
			sentry::close();
			tessd::status(tessd::exit_code::read);
		}
		input.insert(input.end(), buf.data(), buf.data() + len);
	}
	sentry::end_transaction(tx_read_image);
	
	/**
	 * RAII isnt a thing in tesseract land. We can't just initialise the object
	 * by `new`, we have to separately call an Init method. One of many
	 * anti-patterns.
	 */
	void* tx_init_tesseract = sentry::start_transaction(init_tesseract);
	tesseract::TessBaseAPI* api = new tesseract::TessBaseAPI();
	if (api->Init(NULL, "eng", tesseract::OcrEngineMode::OEM_DEFAULT)) {
		delete api;
		sentry::log_critical("tessd", "tesseract::TessBaseAPI::Init failed");
		sentry::end_transaction(tx_init_tesseract);
		sentry::close();
		tessd::status(tessd::exit_code::tess_init);
	}
	sentry::end_transaction(tx_init_tesseract);

	/**
	 * This tells tesseract we want the whole image to be treated as one
	 * block of text
	 */
	api->SetPageSegMode(tesseract::PageSegMode::PSM_SINGLE_BLOCK);

	void* tx_do_tesseract = sentry::start_transaction(do_tesseract);
	/**
	 * We have to use leptonica to load images into tesseract.
	 * Leptonica is ugly and pretty undocumented. Here be hairy
	 * dragons.
	 */
	Pix* image = pixReadMem((l_uint8*)input.data(), input.size());
	if (!image) {
		delete api;
		sentry::end_transaction(tx_do_tesseract);
		sentry::close();
		tessd::status(tessd::exit_code::pix_read_mem);
	}

	image = pixConvertRGBToGrayFast(image);

	/**
	 * We may have already checked this value if discord gave it us as attachment metadata.
	 * Just to be sure, and also in case we're processing an image given in a raw url, we check the
	 * width and height again here.
	 */
	if (!image || image->w * image->h > 33554432) {
		delete api;
		if (image) {
			pixDestroy(&image);
		}
		sentry::end_transaction(tx_do_tesseract);
		sentry::close();
		tessd::status(tessd::exit_code::image_size);
	}

	api->SetImage(image);

	/**
	 * Leptonica is a C library so we have to manually release the image
	 */
	pixDestroy(&image);

	const char* output = api->GetUTF8Text();

	/**
	 * We have to call Clear to get rid of the data we loaded in SetImage.
	 * Destroying the api object isnt enough. RAII, whats that? herp derp.
	 */
	api->Clear();
	delete api;
	if (!output) {
		sentry::end_transaction(tx_do_tesseract);
		sentry::close();
		tessd::status(tessd::exit_code::no_output);
	}

	/**
	 * Parent process picks up this output
	 */
	std::cout << output << std::endl;

	/**
	 * Send transaction record to sentry.io
	 */
	sentry::end_transaction(tx_do_tesseract);

	/**
	 * We have to delete[] this content. Why couldnt it just return
	 * a std::string? i dont know...
	 */
	delete[] output;

	sentry::close();
	tessd::status(tessd::exit_code::no_error);
}
