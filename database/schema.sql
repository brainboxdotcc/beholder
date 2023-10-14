SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";
CREATE DATABASE IF NOT EXISTS `yeet` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
USE `yeet`;

CREATE TABLE `guild_bypass_roles` (
  `guild_id` bigint UNSIGNED NOT NULL,
  `role_id` bigint UNSIGNED NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Roles allowed to bypass image scanning in a guild';

CREATE TABLE `guild_patterns` (
  `guild_id` bigint UNSIGNED NOT NULL,
  `pattern` varchar(150) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE `guild_config` (
  `guild_id` bigint UNSIGNED NOT NULL,
  `log_channel` bigint UNSIGNED NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

--
-- Indexes for dumped tables
--

ALTER TABLE `guild_config`
  ADD PRIMARY KEY (`guild_id`);
COMMIT;

ALTER TABLE `guild_bypass_roles`
  ADD PRIMARY KEY (`guild_id`,`role_id`) USING BTREE,
  ADD UNIQUE KEY `role_id` (`role_id`);

ALTER TABLE `guild_patterns`
  ADD KEY `guild_id` (`guild_id`);
COMMIT;
