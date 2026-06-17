#!/bin/sh
sudo chown ocr:ocr ./tessd && sudo chmod 04755 ./tessd
chmod go-rwx ./beholder
chmod go-rwx ../config.json
chmod go-rwx ./logs ./logs/*
