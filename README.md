![Social](https://beholder.cc/img/social.png)

This is a bot powered by the [D++ library](https://dpp.dev) which scans images posted on public channels through OCR and AI Image Recognition and uses pattern matching to determine what is in the images. If they match the admin's defined ruleset, it deletes the message and tells the user off with a customisable message.

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
	"sentry_dsn": "sentry dsn",
	"sentry_sample_rate": 0.2,
	"database": {
		"host": "localhost",
		"username": "mysql username",
		"password": "mysql password",
		"database": "mysql database",
		"port": 3306
	}
	,
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

**Note**: Leave the values under the `ir` key as empty strings. Self-hosting the premium image recognition API is not supported at this time.

Import the base mysql schema:

```bash
mysql -u <database-user> -p <database-name> < database/schema.sql
Enter password:
```

Insert data into database for your guild and patterns

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
* [Beholder NSFW Scanning container](https://github.com/brainboxdotcc/beholder-nsfw-server):
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
