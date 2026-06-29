<?php
/************************************************************************************
 *
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023,2026 Craig Edwards <support@sporks.gg>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/

require 'vendor/autoload.php';

$config = json_decode(file_get_contents("config.json"));

/* Retrieve all sentry envelopes written to temp directory, and post each one in turn to the API */
$files = glob("/tmp/.beholder_sentry_envelope_*.json");
sort($files);

$dsn = $config->sentry_dsn;
$environment = $config->environment;
$sampleRate = $config->sentry_sample_rate;
$dsn = parse_url($dsn);

foreach ($files as $envelope) {
    $content = file_get_contents($envelope);
    $postUrl = sprintf("%s://%s/api%s/envelope/?sentry_key=%s&sentry_version=7&sentry_client=sentry.native/7.77.0", $dsn["scheme"], $dsn["host"], $dsn["path"], $dsn["user"]);

    $curlHandle = curl_init($postUrl);
    curl_setopt($curlHandle, CURLOPT_POSTFIELDS, $content);
    curl_setopt($curlHandle, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curlHandle, CURLOPT_HTTPHEADER, ['Content-Type: application/json']);
    $curlResponse = curl_exec($curlHandle);
    curl_close($curlHandle);
    if (json_decode($curlResponse) !== null) {
           /* Successful posting results in json response. On success, delete the posted envelope */
           unlink($envelope);
    }
}
