// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include "CommandRouter.h"
#include "nlohmann/json.hpp"

struct EditorContext;
using json = nlohmann::json;

// Register all built-in core commands with the command router.
// Tum yerlesik temel komutlari komut yonlendiricisine kaydet.
// Includes: input.key, input.char, cursor.*, buffer.*, edit.*, file.*, tab.*, mode.*, app.*
// Icerir: input.key, input.char, cursor.*, buffer.*, edit.*, file.*, tab.*, mode.*, app.*
void RegisterCommands(CommandRouter& router, EditorContext* ctx);
