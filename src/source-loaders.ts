import { readFile } from "node:fs/promises";

import type { SourceConfig } from "./types.js";

export async function loadSourceConfigs(path: string): Promise<SourceConfig[]> {
  const raw = await readFile(path, "utf8");
  return JSON.parse(raw) as SourceConfig[];
}
