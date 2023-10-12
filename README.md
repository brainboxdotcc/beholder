# D++ Yeet Bot

This is a bot powered by the [D++ library](https://dpp.dev) which runs images on public channels through OCR and uses pattern matching to determine if they might be images of code or code output. If they are, it deletes them and tells the user off.

![image](https://github.com/brainboxdotcc/yeet/assets/1556794/1366d2c3-9c4f-46ac-82d0-ad698d994487) ![image](https://github.com/brainboxdotcc/yeet/assets/1556794/8b4b0173-db2e-4489-a50f-a4582c7de228)


## Compilation

    mkdir build
    cd build
    cmake ..
    make -j

If DPP is installed in a different location you can specify the root directory to look in while running cmake 

    cmake .. -DDPP_ROOT_DIR=<your-path>

## Running the template bot

Create a config.json in the directory above the build directory:

```json
{
    "token": "your bot token here", 
    "homeserver": "server id of server where the bot should run",
    "logchannel": "server id where logs go",
    "patterns": [
        "wildcard patterns",
        "..."
    ]
}
```

## Dependencies

* Imagemagick command line tools
* Tesseract v5.x
* wget
* g++ 8.0 or later
* cmake
* [D++](https://github.com/brainboxdotcc/dpp) v10.0.26 or later

Start the bot:

    cd build
    ./yeet

