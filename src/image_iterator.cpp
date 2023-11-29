#include <dpp/dpp.h>
#include <beholder/image_iterator.h>
#include <beholder/beholder.h>
#include <beholder/config.h>
#include <CxxUrl/url.hpp>
#include "3rdparty/httplib.h"
#include <set>

namespace image {

	void iterate(const dpp::message msg, dpp::message_context_menu_t event, on_image image_event, on_end end_event) {
		std::vector<dpp::attachment> attaches;

		if (msg.attachments.size() > 0) {
			for (const dpp::attachment& attach : msg.attachments) {
				attaches.emplace_back(attach);
			}
		}

		std::vector<std::string> parts = dpp::utility::tokenize(replace_string(msg.content, "\n", " "), " ");
		if (msg.embeds.size() > 0) {
			for (const dpp::embed& embed : msg.embeds) {
				if (!embed.url.empty()) {
					parts.emplace_back(embed.url);
				}
				if (embed.thumbnail.has_value() && !embed.thumbnail->url.empty()) {
					parts.emplace_back(embed.thumbnail->url);
				}
				if (embed.footer.has_value() && !embed.footer->icon_url.empty()) {
					parts.emplace_back(embed.footer->icon_url);
				}
				if (embed.image.has_value() && !embed.image->url.empty()) {
					parts.emplace_back(embed.image->url);
				}
				if (embed.video.has_value() && !embed.video->url.empty()) {
					parts.emplace_back(embed.video->url);
				}
				if (embed.author.has_value()) {
					if (!embed.author->icon_url.empty()) {
						parts.emplace_back(embed.author->icon_url);
					}
					if (!embed.author->url.empty()) {
						parts.emplace_back(embed.author->url);
					}
				}
				auto spaced = dpp::utility::tokenize(replace_string(embed.description, "\n", " "), " ");
				if (!spaced.empty()) {
					parts.insert(parts.end(), spaced.begin(), spaced.end());
				}
				for (const dpp::embed_field& field : embed.fields) {
					auto spaced = dpp::utility::tokenize(replace_string(field.value, "\n", " "), " ");
					if (!spaced.empty()) {
						parts.insert(parts.end(), spaced.begin(), spaced.end());
					}
				}
			}
		}

		if (msg.stickers.size() > 0) {
			for (const dpp::sticker& sticker : msg.stickers) {
				if (!sticker.id.empty()) {
					parts.emplace_back(sticker.get_url());
				}
			}
		}

		std::set<std::string> checked;
		for (std::string& possibly_url : parts) {
			size_t size = possibly_url.length();
			possibly_url = dpp::lowercase(possibly_url);
			auto cloaked_url_pos = possibly_url.find("](http");
			if (cloaked_url_pos != std::string::npos && possibly_url.length() - cloaked_url_pos - 3 > 7) {
				possibly_url = possibly_url.substr(cloaked_url_pos + 2, possibly_url.length() - cloaked_url_pos - 3);
			}
			if ((size >= 9 && possibly_url.substr(0, 8) == "https://") ||
			(size >= 8 && possibly_url.substr(0, 7) == "http://")) {
				dpp::attachment attach((dpp::message*)&msg);
				attach.url = possibly_url;
				try {
					Url u(possibly_url);
					attach.filename = fs::path(u.path()).filename();
				}
				catch (const std::exception&) {
					attach.filename = fs::path(possibly_url).filename();
				}
				if (checked.find(possibly_url) == checked.end()) {
					attaches.emplace_back(attach);
					checked.emplace(possibly_url);
				}
			}
		}

		size_t added = 0;
		std::vector<std::string> add;
		for (const dpp::attachment& attach : attaches) {
			Url url;
			try {
				Url u(attach.url);
				url = u;
			}
			catch (const std::exception&) {
				continue;
			}
			std::string host = url.scheme() + "://" + url.host();
			httplib::Client cli(host.c_str());
			cli.enable_server_certificate_verification(false);
			cli.set_interface(config::get("tunnel_interface"));
			auto res = cli.Get(url.path());
			if (res) {
				std::string hash = sha256(res->body);
				add.emplace_back(attach.url);
				if (image_event(attach.url, hash, event)) {
					break;
				}
				added++;
			}
		}
		end_event(add, added);
	}		
}
