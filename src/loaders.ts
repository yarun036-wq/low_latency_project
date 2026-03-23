import { readFile } from "node:fs/promises";

import { formConfigSchema, jobsSchema, profileSchema } from "./schema.js";
import type { FormConfig, Job, Profile } from "./types.js";

async function readJsonFile(path: string): Promise<unknown> {
  const content = await readFile(path, "utf8");
  return JSON.parse(content);
}

export async function loadProfile(path: string): Promise<Profile> {
  return profileSchema.parse(await readJsonFile(path));
}

export async function loadJobs(path: string): Promise<Job[]> {
  return jobsSchema.parse(await readJsonFile(path));
}

export async function loadFormConfig(path: string): Promise<FormConfig> {
  return formConfigSchema.parse(await readJsonFile(path));
}
