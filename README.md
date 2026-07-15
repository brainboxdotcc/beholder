![Social](https://beholder.cc/img/social.png)

Beholder is a Discord moderation bot that scans images for text and inappropriate content.

Using OCR, Beholder extracts text from uploaded images, stickers, embeds and linked images, then compares the results against moderator-defined pattern lists. Messages matching configured rules can be deleted automatically, logged for review, or trigger moderation actions.

Beholder also supports NSFW image detection and image hash blocking, allowing moderators to quickly remove known unwanted content from their communities.

Beholder focuses on fast, privacy-conscious image moderation without storing user images.

## Features

* OCR scanning of images, stickers, embeds and linked images
* Custom keyword and phrase matching
* Per-channel moderation rules
* NSFW image detection
* Image hash blocking and allow-listing
* Moderator review actions (block, unblock, timeout, kick, ban)
* Customisable moderation messages
* Scanning of entire GIFs or AVIFs by change detection
* MP4, MKV, WEBM video scanning

## Architecture

Beholder is split into separate processes for isolation.

The main bot connects to Discord and handles moderation decisions. NSFW detection is provided by `nsfwd`, a small local web service which keeps the TensorFlow model loaded in memory and accepts image scan requests from the bot.

OCR and image scanning are performed by `tessd`, which is spawned per request. This keeps untrusted image processing isolated from the main bot process. `tessd` runs under a separate user without access to the configuration file, and is constrained with memory and execution time limits, so malformed or hostile images cannot take down the bot or leak state between scans.

## Compilation

```bash
mkdir build
cd build
cmake ..
make -j
```

## Configuring the bot

Create a config.json in the directory above the build directory:

```json
{
	"token": "token goes here",
	"log": "log path goes here",
	"environment": "environment name",
	"database": {
		"host": "localhost",
		"username": "mysql username",
		"password": "mysql password",
		"database": "mysql database",
		"port": 3306
	},
	"botlists": {
		"top.gg": {
			"token": "top.gg bot list token"
		},
		"other compatible bot list": {
			"token": "their token..."
		}
	}
}
```

Import the base MySQL schema:

```bash
mysql -u <database-user> -p <database-name> < database/schema.sql
Enter password:
```

Insert data into the database for your guild and moderation patterns.

## Software Dependencies

* [D++](https://github.com/brainboxdotcc/dpp) v10.0.28 or later
* libcrypto/libssl
* libtesseract-dev
* libleptonica-dev
* libopencv-dev
* pkg-config
* libavformat-dev
* libavcodec-dev
* libavutil-dev
* libswscale-dev
* libmysqlclient 8.x
* g++ 11.4 or later
* libjsoncpp-dev
* [tensorflow](https://storage.googleapis.com/tensorflow/versions/2.18.0/libtensorflow-cpu-linux-x86_64.tar.gz) - [Installation](https://www.tensorflow.org/install/lang_c)
* [drogon](https://github.com/drogonframework/drogon)
* libuuid
* zlib
* cmake
* fmtlib
* libwebp-dev
* libavif-dev
* spdlog
* CxxUrl
* libtre-dev
* screen

## Starting the bot

```bash
cd beholder
screen -dmS beholder ./run.sh
```

## Starting the nsfw server

This daemon must be running to detect and delete NSFW imagery

```bash
cd beholder
screen -dmS nsfwd ./nsfwd.sh
```
