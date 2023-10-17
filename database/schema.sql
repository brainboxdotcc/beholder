SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";

CREATE TABLE guild_bypass_roles (
  guild_id bigint UNSIGNED NOT NULL,
  role_id bigint UNSIGNED NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Roles allowed to bypass image scanning in a guild';

CREATE TABLE guild_config (
  guild_id bigint UNSIGNED NOT NULL,
  log_channel bigint UNSIGNED DEFAULT NULL,
  embed_title varchar(256) DEFAULT NULL,
  embed_body text,
  premium_subscription varchar(200) DEFAULT NULL COMMENT 'chargebee subscription id',
  premium_title varchar(256) DEFAULT NULL,
  premium_body varchar(4096) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE guild_patterns (
  guild_id bigint UNSIGNED NOT NULL,
  pattern varchar(150) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE premium_filters (
  guild_id bigint UNSIGNED NOT NULL,
  pattern varchar(150) NOT NULL,
  score double DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE premium_filter_model (
  id int NOT NULL,
  category varchar(150) NOT NULL,
  model varchar(150) NOT NULL,
  description varchar(200) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;


ALTER TABLE guild_bypass_roles
  ADD PRIMARY KEY (guild_id,role_id) USING BTREE,
  ADD UNIQUE KEY role_id (role_id);

ALTER TABLE guild_config
  ADD PRIMARY KEY (guild_id);

ALTER TABLE guild_patterns
  ADD KEY guild_id (guild_id);

ALTER TABLE premium_filters
  ADD UNIQUE KEY guild_id (guild_id,pattern) USING BTREE,
  ADD KEY guild_id_2 (guild_id);

ALTER TABLE premium_filter_model
  ADD PRIMARY KEY (id),
  ADD UNIQUE KEY category (category),
  ADD KEY model (model);


ALTER TABLE premium_filter_model
  MODIFY id int NOT NULL AUTO_INCREMENT;
