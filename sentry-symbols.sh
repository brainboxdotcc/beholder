#!/bin/sh
objcopy --only-keep-debug --compress-debug-sections=zlib ./beholder ./beholder.debug
objcopy --only-keep-debug --compress-debug-sections=zlib ./tessd ./tessd.debug
sentry-cli debug-files upload -p beholder-bot --log-level=info ./*.debug

