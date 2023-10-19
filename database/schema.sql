SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";

CREATE TABLE `guild_bypass_roles` (
  `guild_id` bigint UNSIGNED NOT NULL COMMENT 'PK Guild ID',
  `role_id` bigint UNSIGNED NOT NULL COMMENT 'PK Role ID'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Roles allowed to bypass image scanning in a guild';

CREATE TABLE `guild_config` (
  `guild_id` bigint UNSIGNED NOT NULL COMMENT 'PK guild ID',
  `log_channel` bigint UNSIGNED DEFAULT NULL COMMENT 'Log channel ID',
  `embed_title` varchar(256) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT 'Blocked image embed title',
  `embed_body` text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci COMMENT 'Blocked image embed body',
  `premium_subscription` varchar(200) DEFAULT NULL COMMENT 'chargebee subscription id',
  `premium_title` varchar(256) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT 'IR blocked image embed title',
  `premium_body` varchar(4096) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT 'IR blocked image embed body',
  `calls_this_month` int UNSIGNED DEFAULT NULL COMMENT 'Number of IR lookups performed this month',
  `calls_limit` int UNSIGNED DEFAULT NULL COMMENT 'Number of IR calls allowed per month'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Per-guild configuration';

CREATE TABLE `guild_patterns` (
  `guild_id` bigint UNSIGNED NOT NULL COMMENT 'PK Guild ID',
  `pattern` varchar(150) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'Wildcard OCR pattern'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Per guild OCR patterns';

CREATE TABLE `premium_filters` (
  `guild_id` bigint UNSIGNED NOT NULL COMMENT 'PK Guild ID',
  `pattern` varchar(150) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'Pattern JSON path, from premium_filter_model table',
  `score` double DEFAULT NULL COMMENT 'Minimum trigger score, 0 to 1 inclusive'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Per guild IR patterns';

CREATE TABLE `premium_filter_model` (
  `id` int NOT NULL COMMENT 'PK Autoincrement ID',
  `category` varchar(150) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'JSON path into API result',
  `model` varchar(150) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'API model name',
  `description` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT 'Description of filter',
  `detail` varchar(150) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT 'Detail about filter'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='IR filter models';

CREATE TABLE `scan_cache` (
  `hash` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'SHA256 of image url',
  `ocr` text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci COMMENT 'Previous OCR results',
  `api` text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci COMMENT 'Previous API results',
  `cached_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Last update time/date'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='OCR and IR previous cached scans hashed by sha256 of file content';


ALTER TABLE `guild_bypass_roles`
  ADD PRIMARY KEY (`guild_id`,`role_id`) USING BTREE,
  ADD UNIQUE KEY `role_id` (`role_id`);

ALTER TABLE `guild_config`
  ADD PRIMARY KEY (`guild_id`);

ALTER TABLE `guild_patterns`
  ADD KEY `guild_id` (`guild_id`);

ALTER TABLE `premium_filters`
  ADD UNIQUE KEY `guild_id` (`guild_id`,`pattern`) USING BTREE,
  ADD KEY `guild_id_2` (`guild_id`),
  ADD KEY `model_link` (`pattern`);

ALTER TABLE `premium_filter_model`
  ADD PRIMARY KEY (`id`),
  ADD UNIQUE KEY `category` (`category`),
  ADD KEY `model` (`model`);

ALTER TABLE `scan_cache`
  ADD PRIMARY KEY (`hash`),
  ADD KEY `cached_at` (`cached_at`);


ALTER TABLE `premium_filter_model`
  MODIFY `id` int NOT NULL AUTO_INCREMENT COMMENT 'PK Autoincrement ID';


ALTER TABLE `premium_filters`
  ADD CONSTRAINT `model_link` FOREIGN KEY (`pattern`) REFERENCES `premium_filter_model` (`category`) ON DELETE RESTRICT ON UPDATE RESTRICT;

