#!/bin/sh
sudo chown ocr:ocr ./tessd && sudo chmod 04755 ./tessd
sudo chmod go-rwx ./beholder
sudo chmod go-rwx ../config.json
sudo chmod go-rwx ./logs ./logs/*
sudo chown nsfwd:nsfwd ./nsfwd && sudo chmod 04755 ./nsfwd
sudo chown nsfwd nsfwd-logs
sudo chmod o-rwx,g+rwx ./nsfwd-logs ./nsfwd-logs/*

