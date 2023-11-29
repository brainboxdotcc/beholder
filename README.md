# D++ Beholder Bot

This is a bot powered by the [D++ library](https://dpp.dev) which scans images posted on public channels through OCR and AI Image Recognition and uses pattern matching to determine what is in the images. If they match the admin's defined ruleset, it deletes the message and tells the user off with a customisable message.

### Example of OCR detection
![Screenshot_20231020-193744](https://github.com/brainboxdotcc/beholder/assets/1556794/692c11d1-181f-4d58-a95b-35c8fa831bac)
![Screenshot_20231020-193722](https://github.com/brainboxdotcc/beholder/assets/1556794/6b96883d-152a-4706-9a72-3301705d1659)

### Example of Image Recognition Detection
![image](https://github.com/brainboxdotcc/beholder/assets/1556794/8039114c-3ff3-4b7e-846a-07415d8f8b5e)

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
	"tunnel_interface": "GRE tunnel interface IP",
	"log": "log path goes here",
	"environment": "environment name",
	"sentry_dsn": "sentry dsn",
	"sentry_sample_rate": 0.2,
	"ir": {
		"host": "image recognition endpoint",
		"path": "image recognition endpoint",
		"credentials": {
			"username": "username",
			"password": "password"
		},
		"fields": [],
		"label_runner": "resnet-50 endpoint url",
		"label_key": "resnet key"
	},
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

## Other Dependencies

To offer premium services, various paid subscription APIs are required. These are configured in the `ir` tag in `config.json` You should also subscribe to an **anti-DDOS tunnel service**, which can be bound to as an interface for making unsafe web requests to fetch image content. Without this, your server's IP is left exposed to the neer-do-wells of Discord. Configure this in `tunnel_interface`.

## Starting the bot

```bash
cd beholder
screen -dmS beholder ./run.sh
```
