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
* CxxUrl
* sentry-native
* libcrypto/libssl
* [D++](https://github.com/brainboxdotcc/dpp) v10.0.27 or later

Start the bot:

```bash
cd build
./beholder
```
