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

