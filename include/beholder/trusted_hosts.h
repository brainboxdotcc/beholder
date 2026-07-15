/************************************************************************************
 *
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023,2026 Craig Edwards <support@sporks.gg>
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
#pragma once

/**
 * Hosts which may be contacted directly rather than via the local SSRF proxy.
 *
 * These are large, trusted first-party media providers or infrastructure under
 * our control. All other hosts are fetched via the local proxy to prevent
 * disclosure of the scanner's public IP address.
 */
inline static const char* trusted[] = {
	/* Brainbox */
	"https://brainbox.cc",
	"https://*.brainbox.cc",

	/* Beholder */
	"https://*.beholder.cc",
	"https://beholder.cc",

	/* TriviaBot */
	"https://triviabot.co.uk",
	"https://*.triviabot.co.uk",

	/* Sporks */
	"https://sporks.gg",
	"https://*.sporks.gg",

	/* SSOD */
	"https://ssod.org",
	"https://*.ssod.org",

	/* Discord */
	"https://cdn.discordapp.com",
	"https://media.discordapp.net",
	"https://images-ext-*.discordapp.net",

	/* GIF providers */
	"https://tenor.com",
	"https://*.tenor.com",
	"https://giphy.com",
	"https://*.giphy.com",
	"https://klipy.com",
	"https://*.klipy.com",

	/* Google and YouTube */
	"https://youtube.com",
	"https://*.youtube.com",
	"https://youtu.be",
	"https://*.ytimg.com",
	"https://*.googlevideo.com",
	"https://*.ggpht.com",
	"https://*.googleusercontent.com",

	/* Reddit */
	"https://reddit.com",
	"https://*.reddit.com",
	"https://redd.it",
	"https://*.redditmedia.com",
	"https://*.redd.it",
	"https://*.redditstatic.com",

	/* Imgur */
	"https://imgur.com",
	"https://*.imgur.com",

	/* Twitch */
	"https://twitch.tv",
	"https://*.twitch.tv",
	"https://*.twitchcdn.net",
	"https://*.jtvnw.net",

	/* Tumblr */
	"https://tumblr.com",
	"https://*.tumblr.com",

	/* Medal */
	"https://medal.tv",
	"https://*.medal.tv",

	/* Streamable */
	"https://streamable.com",
	"https://*.streamable.com",

	/* Apple */
	"https://*.mzstatic.com",

	/* X/Twitter media */
	"https://*.twimg.com",
	"https://video.twimg.com",
	"https://*.x.com",
	"https://gif.fxtwitter.com",
	"https://assets.fxembed.com",

	/* GitHub-hosted media */
	"https://raw.githubusercontent.com",
	"https://user-images.githubusercontent.com",
	"https://objects.githubusercontent.com",
	"https://avatars.githubusercontent.com",

	/* Wikimedia */
	"https://upload.wikimedia.org",

	/* Steam */
	"https://*.steamstatic.com",
	"https://*.steamusercontent.com",
	"https://steamuserimages-a.akamaihd.net",
	"https://steamcommunity.com",
	"https://*.steampowered.com",

	/* TikTok */
	"https://*.tiktokcdn.com",
	"https://*.tiktokcdn-us.com",

	/* Spotify */
	"https://*.spotifycdn.com",
	"https://mosaic.scdn.co",

	/* SoundCloud */
	"https://*.sndcdn.com",

	/* Facebook/Instagram CDN */
	"https://*.fbcdn.net",
	"https://*.cdninstagram.com",

	/* Pinterest */
	"https://i.pinimg.com",

	/* Fandom */
	"https://static.wikia.nocookie.net",

	/* Nexus Mods */
	"https://staticdelivery.nexusmods.com",

	/* MediaFire */
	"https://static.mediafire.com",

	/* Misc */
	"https://image.tmdb.org",
	"https://opengraph.githubassets.com",
	"https://raw.githubusercontent.com",
	"https://avatars*.githubusercontent.com",
	"https://user-images.githubusercontent.com",
	"https://m.media-amazon.com",
	"https://a0.muscache.com",
};