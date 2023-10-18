# D++ Beholder Bot

This is a bot powered by the [D++ library](https://dpp.dev) which runs images on public channels through OCR and uses pattern matching to determine if they might be images of code or code output. If they are, it deletes them and tells the user off.

![image](https://github.com/brainboxdotcc/yeet/assets/1556794/2e12e40e-1a01-4689-bcf7-707ae167505f)
![image](https://github.com/brainboxdotcc/yeet/assets/1556794/b62d9713-8c74-43c1-9b2b-a0f44ae3851f)


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
	"token": "your bot token here",
	"ir": {
		"endpoint": "",
		"credentials": {
			"username": "",
			"password": ""
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
