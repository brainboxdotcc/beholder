![Social](https://beholder.cc/img/social.png)

Beholder is a Discord moderation bot that scans images for text and inappropriate content.

Using OCR, Beholder extracts text from uploaded images, stickers, embeds and linked images, then compares the results against moderator-defined pattern lists. Messages matching configured rules can be deleted automatically, logged for review, or trigger moderation actions.

Beholder also supports NSFW image detection and image hash blocking, allowing moderators to quickly remove known unwanted content from their communities.

Beholder focuses on fast, privacy-conscious image moderation without storing user images.

## Features

* OCR scanning of images, stickers, embeds and linked images
* Custom keyword and phrase matching
* Per-channel moderation policies
* NSFW image detection
* Image hash blocking and allow-listing
* Moderator review actions (block, unblock, timeout, kick, ban)
* Customisable moderation messages
* Discord log channel integration

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
* sentry-native
* libcrypto/libssl
* libtesseract-dev
* libleptonica-dev
* ImageMagick Convert
* libmysqlclient 8.x
* g++ 11.4 or later
* cmake
* fmtlib
* spdlog
* CxxUrl
* screen
* [Beholder NSFW Detection Service](https://github.com/brainboxdotcc/beholder-nsfw-server):

  * Docker
  * Docker Compose
  * node.js 20.x
  * Tensorflow
  * tfjs-node
  * express

## Starting the bot

```bash
cd beholder
screen -dmS beholder ./run.sh
```
