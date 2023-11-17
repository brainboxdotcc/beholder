SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";

-- --------------------------------------------------------

--
-- Table structure for table api_cache_gore
--

DROP TABLE IF EXISTS api_cache_gore;
CREATE TABLE api_cache_gore (
  hash varchar(64) NOT NULL,
  api text,
  cached_at datetime DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
PARTITION BY KEY (`hash`)
(
PARTITION p0 ENGINE=InnoDB,
PARTITION p1 ENGINE=InnoDB,
PARTITION p2 ENGINE=InnoDB,
PARTITION p3 ENGINE=InnoDB,
PARTITION p4 ENGINE=InnoDB,
PARTITION p5 ENGINE=InnoDB,
PARTITION p6 ENGINE=InnoDB,
PARTITION p7 ENGINE=InnoDB,
PARTITION p8 ENGINE=InnoDB,
PARTITION p9 ENGINE=InnoDB,
PARTITION p10 ENGINE=InnoDB,
PARTITION p11 ENGINE=InnoDB,
PARTITION p12 ENGINE=InnoDB,
PARTITION p13 ENGINE=InnoDB,
PARTITION p14 ENGINE=InnoDB,
PARTITION p15 ENGINE=InnoDB,
PARTITION p16 ENGINE=InnoDB,
PARTITION p17 ENGINE=InnoDB,
PARTITION p18 ENGINE=InnoDB,
PARTITION p19 ENGINE=InnoDB,
PARTITION p20 ENGINE=InnoDB,
PARTITION p21 ENGINE=InnoDB,
PARTITION p22 ENGINE=InnoDB,
PARTITION p23 ENGINE=InnoDB,
PARTITION p24 ENGINE=InnoDB,
PARTITION p25 ENGINE=InnoDB
);

-- --------------------------------------------------------

--
-- Table structure for table api_cache_nudity
--

DROP TABLE IF EXISTS api_cache_nudity;
CREATE TABLE api_cache_nudity (
  hash varchar(64) NOT NULL,
  api text,
  cached_at datetime DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
PARTITION BY KEY (`hash`)
(
PARTITION p0 ENGINE=InnoDB,
PARTITION p1 ENGINE=InnoDB,
PARTITION p2 ENGINE=InnoDB,
PARTITION p3 ENGINE=InnoDB,
PARTITION p4 ENGINE=InnoDB,
PARTITION p5 ENGINE=InnoDB,
PARTITION p6 ENGINE=InnoDB,
PARTITION p7 ENGINE=InnoDB,
PARTITION p8 ENGINE=InnoDB,
PARTITION p9 ENGINE=InnoDB,
PARTITION p10 ENGINE=InnoDB,
PARTITION p11 ENGINE=InnoDB,
PARTITION p12 ENGINE=InnoDB,
PARTITION p13 ENGINE=InnoDB,
PARTITION p14 ENGINE=InnoDB,
PARTITION p15 ENGINE=InnoDB,
PARTITION p16 ENGINE=InnoDB,
PARTITION p17 ENGINE=InnoDB,
PARTITION p18 ENGINE=InnoDB,
PARTITION p19 ENGINE=InnoDB,
PARTITION p20 ENGINE=InnoDB,
PARTITION p21 ENGINE=InnoDB,
PARTITION p22 ENGINE=InnoDB,
PARTITION p23 ENGINE=InnoDB,
PARTITION p24 ENGINE=InnoDB,
PARTITION p25 ENGINE=InnoDB
);

-- --------------------------------------------------------

--
-- Table structure for table api_cache_offensive
--

DROP TABLE IF EXISTS api_cache_offensive;
CREATE TABLE api_cache_offensive (
  hash varchar(64) NOT NULL,
  api text,
  cached_at datetime DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
PARTITION BY KEY (`hash`)
(
PARTITION p0 ENGINE=InnoDB,
PARTITION p1 ENGINE=InnoDB,
PARTITION p2 ENGINE=InnoDB,
PARTITION p3 ENGINE=InnoDB,
PARTITION p4 ENGINE=InnoDB,
PARTITION p5 ENGINE=InnoDB,
PARTITION p6 ENGINE=InnoDB,
PARTITION p7 ENGINE=InnoDB,
PARTITION p8 ENGINE=InnoDB,
PARTITION p9 ENGINE=InnoDB,
PARTITION p10 ENGINE=InnoDB,
PARTITION p11 ENGINE=InnoDB,
PARTITION p12 ENGINE=InnoDB,
PARTITION p13 ENGINE=InnoDB,
PARTITION p14 ENGINE=InnoDB,
PARTITION p15 ENGINE=InnoDB,
PARTITION p16 ENGINE=InnoDB,
PARTITION p17 ENGINE=InnoDB,
PARTITION p18 ENGINE=InnoDB,
PARTITION p19 ENGINE=InnoDB,
PARTITION p20 ENGINE=InnoDB,
PARTITION p21 ENGINE=InnoDB,
PARTITION p22 ENGINE=InnoDB,
PARTITION p23 ENGINE=InnoDB,
PARTITION p24 ENGINE=InnoDB,
PARTITION p25 ENGINE=InnoDB
);

-- --------------------------------------------------------

--
-- Table structure for table api_cache_wad
--

DROP TABLE IF EXISTS api_cache_wad;
CREATE TABLE api_cache_wad (
  hash varchar(64) NOT NULL,
  api text,
  cached_at datetime DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
PARTITION BY KEY (`hash`)
(
PARTITION p0 ENGINE=InnoDB,
PARTITION p1 ENGINE=InnoDB,
PARTITION p2 ENGINE=InnoDB,
PARTITION p3 ENGINE=InnoDB,
PARTITION p4 ENGINE=InnoDB,
PARTITION p5 ENGINE=InnoDB,
PARTITION p6 ENGINE=InnoDB,
PARTITION p7 ENGINE=InnoDB,
PARTITION p8 ENGINE=InnoDB,
PARTITION p9 ENGINE=InnoDB,
PARTITION p10 ENGINE=InnoDB,
PARTITION p11 ENGINE=InnoDB,
PARTITION p12 ENGINE=InnoDB,
PARTITION p13 ENGINE=InnoDB,
PARTITION p14 ENGINE=InnoDB,
PARTITION p15 ENGINE=InnoDB,
PARTITION p16 ENGINE=InnoDB,
PARTITION p17 ENGINE=InnoDB,
PARTITION p18 ENGINE=InnoDB,
PARTITION p19 ENGINE=InnoDB,
PARTITION p20 ENGINE=InnoDB,
PARTITION p21 ENGINE=InnoDB,
PARTITION p22 ENGINE=InnoDB,
PARTITION p23 ENGINE=InnoDB,
PARTITION p24 ENGINE=InnoDB,
PARTITION p25 ENGINE=InnoDB
);

-- --------------------------------------------------------

--
-- Table structure for table discord_access_tokens
--

DROP TABLE IF EXISTS discord_access_tokens;
CREATE TABLE discord_access_tokens (
  id bigint UNSIGNED NOT NULL,
  access_token varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  refresh_token varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  token_type varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  expires_in int NOT NULL,
  expires_at timestamp NOT NULL,
  scope varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  user_id bigint UNSIGNED NOT NULL,
  created_at timestamp NULL DEFAULT NULL,
  updated_at timestamp NULL DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- --------------------------------------------------------

--
-- Table structure for table failed_jobs
--

DROP TABLE IF EXISTS failed_jobs;
CREATE TABLE failed_jobs (
  id bigint UNSIGNED NOT NULL,
  uuid varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  connection text COLLATE utf8mb4_unicode_ci NOT NULL,
  queue text COLLATE utf8mb4_unicode_ci NOT NULL,
  payload longtext COLLATE utf8mb4_unicode_ci NOT NULL,
  exception longtext COLLATE utf8mb4_unicode_ci NOT NULL,
  failed_at timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- --------------------------------------------------------

--
-- Table structure for table guild_bypass_roles
--

DROP TABLE IF EXISTS guild_bypass_roles;
CREATE TABLE guild_bypass_roles (
  guild_id bigint UNSIGNED NOT NULL COMMENT 'PK Guild ID',
  role_id bigint UNSIGNED NOT NULL COMMENT 'PK Role ID'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Roles allowed to bypass image scanning in a guild';

-- --------------------------------------------------------

--
-- Table structure for table guild_config
--

DROP TABLE IF EXISTS guild_config;
CREATE TABLE guild_config (
  guild_id bigint UNSIGNED NOT NULL COMMENT 'PK guild ID',
  log_channel bigint UNSIGNED DEFAULT NULL COMMENT 'Log channel ID',
  embeds_disabled tinyint UNSIGNED DEFAULT '0' COMMENT 'True if warning embeds are disabled',
  embed_title varchar(256) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT 'Blocked image embed title',
  embed_body text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci COMMENT 'Blocked image embed body',
  premium_subscription varchar(200) DEFAULT NULL COMMENT 'chargebee subscription id',
  premium_title varchar(256) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT 'IR blocked image embed title',
  premium_body varchar(4096) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT 'IR blocked image embed body',
  calls_this_month int UNSIGNED DEFAULT NULL COMMENT 'Number of IR lookups performed this month',
  calls_limit int UNSIGNED DEFAULT NULL COMMENT 'Number of IR calls allowed per month'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Per-guild configuration';

-- --------------------------------------------------------

--
-- Table structure for table guild_ignored_channels
--

DROP TABLE IF EXISTS guild_ignored_channels;
CREATE TABLE guild_ignored_channels (
  guild_id bigint UNSIGNED NOT NULL COMMENT 'Guild ID PK',
  channel_id bigint UNSIGNED NOT NULL COMMENT 'Channel ID PK'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- --------------------------------------------------------

--
-- Table structure for table guild_patterns
--

DROP TABLE IF EXISTS guild_patterns;
CREATE TABLE guild_patterns (
  guild_id bigint UNSIGNED NOT NULL COMMENT 'PK Guild ID',
  pattern varchar(150) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'Wildcard OCR pattern'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Per guild OCR patterns';

-- --------------------------------------------------------

--
-- Table structure for table migrations
--

DROP TABLE IF EXISTS migrations;
CREATE TABLE migrations (
  id int UNSIGNED NOT NULL,
  migration varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  batch int NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

--
-- Dumping data for table migrations
--

INSERT INTO migrations (id, migration, batch) VALUES
(1, '2014_10_12_000000_create_users_table', 1),
(2, '2014_10_12_100000_create_password_reset_tokens_table', 1),
(3, '2019_08_19_000000_create_failed_jobs_table', 1),
(4, '2019_12_14_000001_create_personal_access_tokens_table', 1),
(5, '2023_05_25_121158_add_remember_token_to_users_table', 2),
(6, '2023_05_26_165816_create_discord_access_tokens_table', 2),
(7, '2023_05_27_055058_remove_refresh_token_from_users_table', 3),
(8, '2023_06_11_062809_update_users_table', 4),
(9, '2023_10_23_141335_create_premium_plans', 5),
(10, '2023_10_23_141841_create_premium_credits', 6);

-- --------------------------------------------------------

--
-- Table structure for table password_reset_tokens
--

DROP TABLE IF EXISTS password_reset_tokens;
CREATE TABLE password_reset_tokens (
  email varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  token varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  created_at timestamp NULL DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- --------------------------------------------------------

--
-- Table structure for table personal_access_tokens
--

DROP TABLE IF EXISTS personal_access_tokens;
CREATE TABLE personal_access_tokens (
  id bigint UNSIGNED NOT NULL,
  tokenable_type varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  tokenable_id bigint UNSIGNED NOT NULL,
  name varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  token varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL,
  abilities text COLLATE utf8mb4_unicode_ci,
  last_used_at timestamp NULL DEFAULT NULL,
  expires_at timestamp NULL DEFAULT NULL,
  created_at timestamp NULL DEFAULT NULL,
  updated_at timestamp NULL DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- --------------------------------------------------------

--
-- Table structure for table premium_credits
--

DROP TABLE IF EXISTS premium_credits;
CREATE TABLE premium_credits (
  user_id bigint UNSIGNED NOT NULL,
  subscription_id varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  guild_id bigint UNSIGNED NOT NULL,
  active tinyint(1) NOT NULL DEFAULT '1',
  since datetime NOT NULL,
  plan_id varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  cancel_date datetime DEFAULT NULL,
  manual_expiry_date datetime DEFAULT NULL,
  payment_failed tinyint(1) NOT NULL DEFAULT '0',
  payment_failed_date datetime DEFAULT NULL,
  created_at timestamp NULL DEFAULT NULL,
  updated_at timestamp NULL DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- --------------------------------------------------------

--
-- Table structure for table premium_filters
--

DROP TABLE IF EXISTS premium_filters;
CREATE TABLE premium_filters (
  guild_id bigint UNSIGNED NOT NULL COMMENT 'PK Guild ID',
  pattern varchar(150) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'Pattern JSON path, from premium_filter_model table',
  score double DEFAULT NULL COMMENT 'Minimum trigger score, 0 to 1 inclusive'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Per guild IR patterns';

-- --------------------------------------------------------

--
-- Table structure for table premium_filter_model
--

DROP TABLE IF EXISTS premium_filter_model;
CREATE TABLE premium_filter_model (
  id int NOT NULL COMMENT 'PK Autoincrement ID',
  category varchar(150) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'JSON path into API result',
  model varchar(150) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'API model name',
  description varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT 'Description of filter',
  detail varchar(150) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT 'Detail about filter',
  default_threshold double DEFAULT '0.5' COMMENT 'Default trigger %'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='IR filter models';

-- --------------------------------------------------------

--
-- Table structure for table premium_plans
--

DROP TABLE IF EXISTS premium_plans;
CREATE TABLE premium_plans (
  id varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  name varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  price decimal(8,2) NOT NULL,
  quota int UNSIGNED NOT NULL,
  description text COLLATE utf8mb4_unicode_ci,
  period int UNSIGNED NOT NULL,
  period_unit enum('year','month') COLLATE utf8mb4_unicode_ci NOT NULL,
  currency enum('GBP','USD') COLLATE utf8mb4_unicode_ci NOT NULL,
  created_at timestamp NULL DEFAULT NULL,
  updated_at timestamp NULL DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- --------------------------------------------------------

--
-- Table structure for table scan_cache
--

DROP TABLE IF EXISTS scan_cache;
CREATE TABLE scan_cache (
  hash varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'SHA256 of image url',
  ocr text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci COMMENT 'Previous OCR results',
  cached_at timestamp NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Last update time/date'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='OCR and IR previous cached scans hashed by sha256 of file content'
PARTITION BY KEY (`hash`)
(
PARTITION p0 ENGINE=InnoDB,
PARTITION p1 ENGINE=InnoDB,
PARTITION p2 ENGINE=InnoDB,
PARTITION p3 ENGINE=InnoDB,
PARTITION p4 ENGINE=InnoDB,
PARTITION p5 ENGINE=InnoDB,
PARTITION p6 ENGINE=InnoDB,
PARTITION p7 ENGINE=InnoDB,
PARTITION p8 ENGINE=InnoDB,
PARTITION p9 ENGINE=InnoDB,
PARTITION p10 ENGINE=InnoDB,
PARTITION p11 ENGINE=InnoDB,
PARTITION p12 ENGINE=InnoDB,
PARTITION p13 ENGINE=InnoDB,
PARTITION p14 ENGINE=InnoDB,
PARTITION p15 ENGINE=InnoDB
);

-- --------------------------------------------------------

--
-- Table structure for table users
--

DROP TABLE IF EXISTS users;
CREATE TABLE users (
  id bigint UNSIGNED NOT NULL,
  name varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  email varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  username varchar(255) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  avatar varchar(150) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  locale varchar(150) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  mfa_enabled tinyint(1) DEFAULT NULL,
  premium_type varchar(255) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  public_flags varchar(255) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  verified tinyint(1) DEFAULT NULL,
  banner varchar(255) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  banner_color varchar(255) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  accent_color varchar(255) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  discriminator varchar(255) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  global_name varchar(255) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  email_verified_at timestamp NULL DEFAULT NULL,
  password varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  remember_token varchar(100) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  created_at timestamp NULL DEFAULT NULL,
  updated_at timestamp NULL DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

--
-- Indexes for dumped tables
--

--
-- Indexes for table api_cache_gore
--
ALTER TABLE api_cache_gore
  ADD PRIMARY KEY (hash),
  ADD KEY cached_at_idx (cached_at);

--
-- Indexes for table api_cache_nudity
--
ALTER TABLE api_cache_nudity
  ADD PRIMARY KEY (hash),
  ADD KEY cached_at_idx (cached_at);

--
-- Indexes for table api_cache_offensive
--
ALTER TABLE api_cache_offensive
  ADD PRIMARY KEY (hash),
  ADD KEY cached_at_idx (cached_at);

--
-- Indexes for table api_cache_wad
--
ALTER TABLE api_cache_wad
  ADD PRIMARY KEY (hash),
  ADD KEY cached_at_idx (cached_at);

--
-- Indexes for table discord_access_tokens
--
ALTER TABLE discord_access_tokens
  ADD PRIMARY KEY (id),
  ADD KEY discord_access_tokens_user_id_foreign (user_id);

--
-- Indexes for table failed_jobs
--
ALTER TABLE failed_jobs
  ADD PRIMARY KEY (id),
  ADD UNIQUE KEY failed_jobs_uuid_unique (uuid);

--
-- Indexes for table guild_bypass_roles
--
ALTER TABLE guild_bypass_roles
  ADD PRIMARY KEY (guild_id,role_id) USING BTREE,
  ADD UNIQUE KEY role_id (role_id);

--
-- Indexes for table guild_config
--
ALTER TABLE guild_config
  ADD PRIMARY KEY (guild_id);

--
-- Indexes for table guild_ignored_channels
--
ALTER TABLE guild_ignored_channels
  ADD PRIMARY KEY (guild_id,channel_id),
  ADD KEY guild_id (guild_id);

--
-- Indexes for table guild_patterns
--
ALTER TABLE guild_patterns
  ADD PRIMARY KEY (guild_id,pattern),
  ADD KEY guild_id (guild_id);

--
-- Indexes for table migrations
--
ALTER TABLE migrations
  ADD PRIMARY KEY (id);

--
-- Indexes for table password_reset_tokens
--
ALTER TABLE password_reset_tokens
  ADD PRIMARY KEY (email);

--
-- Indexes for table personal_access_tokens
--
ALTER TABLE personal_access_tokens
  ADD PRIMARY KEY (id),
  ADD UNIQUE KEY personal_access_tokens_token_unique (token),
  ADD KEY personal_access_tokens_tokenable_type_tokenable_id_index (tokenable_type,tokenable_id);

--
-- Indexes for table premium_credits
--
ALTER TABLE premium_credits
  ADD UNIQUE KEY premium_credits_user_id_subscription_id_unique (user_id,subscription_id),
  ADD UNIQUE KEY premium_credits_guild_id_unique (guild_id),
  ADD KEY premium_credits_plan_id_foreign (plan_id);

--
-- Indexes for table premium_filters
--
ALTER TABLE premium_filters
  ADD UNIQUE KEY guild_id (guild_id,pattern) USING BTREE,
  ADD KEY guild_id_2 (guild_id),
  ADD KEY model_link (pattern);

--
-- Indexes for table premium_filter_model
--
ALTER TABLE premium_filter_model
  ADD PRIMARY KEY (id),
  ADD UNIQUE KEY category (category),
  ADD KEY model (model);

--
-- Indexes for table premium_plans
--
ALTER TABLE premium_plans
  ADD PRIMARY KEY (id);

--
-- Indexes for table scan_cache
--
ALTER TABLE scan_cache
  ADD PRIMARY KEY (hash),
  ADD KEY cached_at (cached_at);

--
-- Indexes for table users
--
ALTER TABLE users
  ADD PRIMARY KEY (id),
  ADD UNIQUE KEY users_email_unique (email);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table discord_access_tokens
--
ALTER TABLE discord_access_tokens
  MODIFY id bigint UNSIGNED NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table failed_jobs
--
ALTER TABLE failed_jobs
  MODIFY id bigint UNSIGNED NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table migrations
--
ALTER TABLE migrations
  MODIFY id int UNSIGNED NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=11;

--
-- AUTO_INCREMENT for table personal_access_tokens
--
ALTER TABLE personal_access_tokens
  MODIFY id bigint UNSIGNED NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table premium_filter_model
--
ALTER TABLE premium_filter_model
  MODIFY id int NOT NULL AUTO_INCREMENT COMMENT 'PK Autoincrement ID';

--
-- AUTO_INCREMENT for table users
--
ALTER TABLE users
  MODIFY id bigint UNSIGNED NOT NULL AUTO_INCREMENT;

--
-- Constraints for dumped tables
--

--
-- Constraints for table discord_access_tokens
--
ALTER TABLE discord_access_tokens
  ADD CONSTRAINT discord_access_tokens_user_id_foreign FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE;

--
-- Constraints for table premium_credits
--
ALTER TABLE premium_credits
  ADD CONSTRAINT premium_credits_plan_id_foreign FOREIGN KEY (plan_id) REFERENCES premium_plans (id),
  ADD CONSTRAINT premium_credits_user_id_foreign FOREIGN KEY (user_id) REFERENCES users (id);

--
-- Constraints for table premium_filters
--
ALTER TABLE premium_filters
  ADD CONSTRAINT model_link FOREIGN KEY (pattern) REFERENCES premium_filter_model (category) ON DELETE RESTRICT ON UPDATE RESTRICT;
COMMIT;
