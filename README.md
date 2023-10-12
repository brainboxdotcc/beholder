# D++ Yeet Bot

This is a bot powered by the [D++ library](https://dpp.dev) which runs images on public channels through OCR and uses pattern matching to determine if they might be images of code or code output. If they are, it deletes them and tells the user off.

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
"homeserver": "server id of server where the bot should run"
}
```

Start the bot:

    cd build
    ./yeet

