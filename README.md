# D++ Beholder Bot

This is a bot powered by the [D++ library](https://dpp.dev) which scans images posted on public channels through OCR and AI Image Recognition and uses pattern matching to determine what is in the images. If they match the admin's defined ruleset, it deletes the message and tells the user off with a customisable message.

![image](https://github.com/brainboxdotcc/beholder/assets/1556794/cfeecfb3-0a4d-4d23-bc1c-55c1c924ff6a)
![image](https://github.com/brainboxdotcc/beholder/assets/1556794/0bd8e6d3-1bcf-458b-af8f-c3243b916b2a)

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
	"ir": {
		"endpoint": "image recognition endpoint",
		"credentials": {
			"username": "username",
			"password": "password"
		},
		"fields": []
	},
	"database": {
		"host": "localhost",
		"username": "mysql username",
		"password": "mysql password",
		"database": "mysql database",
		"port": 3306
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

## Dependencies

* Tesseract (libtesseract-dev)
* Leptonica (libleptonica-dev)
* libmysqlclient
* g++ 11.4 or later
* cmake
* fmtlib
* spdlog
* [D++](https://github.com/brainboxdotcc/dpp) v10.0.27 or later

Start the bot:

```bash
cd build
./beholder
```
