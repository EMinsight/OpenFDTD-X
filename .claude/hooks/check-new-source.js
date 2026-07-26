#!/usr/bin/env node
// PostToolUse hook (Edit|Write) — 新規 .cpp/.h が CMakeLists.txt に登録されて
// いなければ警告を出す。このリポジトリは glob せず明示列挙しているため、
// 追加し忘れると「書いたのにビルドされない」事故が起きやすい。
// クロスプラットフォーム (Node.js) — ECC の流儀に合わせる。
const fs = require("fs");
const path = require("path");

let input = "";
try { input = fs.readFileSync(0, "utf8"); } catch { process.exit(0); }

let data;
try { data = JSON.parse(input); } catch { process.exit(0); }

const file = data?.tool_input?.file_path || "";
if (!/\/src\/.*\.(cpp|h)$/.test(file)) process.exit(0);

const root = process.env.CLAUDE_PROJECT_DIR || process.cwd();
const cml = path.join(root, "CMakeLists.txt");
let cmake = "";
try { cmake = fs.readFileSync(cml, "utf8"); } catch { process.exit(0); }

const rel = path.relative(root, file).replace(/\\/g, "/");
if (!cmake.includes(rel)) {
  // stderr + exit 2 → Claude へフィードバックされる
  console.error(
    `⚠ ${rel} は CMakeLists.txt に未登録です。` +
    `GUI_SOURCES / CORE_SOURCES に追加しないとビルドされません。`);
  process.exit(2);
}
process.exit(0);
