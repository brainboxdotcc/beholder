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
#include <openssl/evp.h>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <memory>

using evp_ctx = std::unique_ptr<EVP_MD_CTX, void (*)(EVP_MD_CTX *)>;

std::string sha256(const std::string &buffer) {
	std::vector<unsigned char> hash;
	evp_ctx evpCtx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
	EVP_DigestInit_ex(evpCtx.get(), EVP_sha256(), nullptr);
	EVP_DigestUpdate(evpCtx.get(), buffer.data(), buffer.length());
	hash.resize(32);
	std::fill(hash.begin(), hash.end(), 0);
	unsigned int len{0};
	EVP_DigestFinal_ex(evpCtx.get(), hash.data(), &len);

	std::stringstream out;
	for (size_t i = 0; i < hash.size(); i++) {
		out << std::setfill('0') << std::setw(2) << std::hex << int(hash[i]);
	}

	return out.str();
}

