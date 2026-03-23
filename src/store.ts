import { mkdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";

import type { ApplicationRecord, Job, LocalDatabase } from "./types.js";

const storageDir = path.resolve("storage");
const dbPath = path.join(storageDir, "db.json");

function defaultDb(): LocalDatabase {
  return {
    jobs: [],
    applications: []
  };
}

export async function loadDb(): Promise<LocalDatabase> {
  try {
    const raw = await readFile(dbPath, "utf8");
    const parsed = JSON.parse(raw) as LocalDatabase;
    return {
      jobs: parsed.jobs ?? [],
      applications: parsed.applications ?? [],
      lastIngestedAt: parsed.lastIngestedAt
    };
  } catch {
    return defaultDb();
  }
}

export async function saveDb(db: LocalDatabase): Promise<void> {
  await mkdir(storageDir, { recursive: true });
  await writeFile(dbPath, JSON.stringify(db, null, 2), "utf8");
}

export async function upsertJobs(jobs: Job[]): Promise<LocalDatabase> {
  const db = await loadDb();
  const existing = new Map(db.jobs.map((job) => [job.id, job]));
  for (const job of jobs) {
    existing.set(job.id, job);
  }
  db.jobs = [...existing.values()];
  db.lastIngestedAt = new Date().toISOString();
  await saveDb(db);
  return db;
}

export async function replaceApplications(applications: ApplicationRecord[]): Promise<LocalDatabase> {
  const db = await loadDb();
  db.applications = applications;
  await saveDb(db);
  return db;
}

export async function updateApplication(record: ApplicationRecord): Promise<LocalDatabase> {
  const db = await loadDb();
  const next = db.applications.filter((entry) => entry.id !== record.id);
  next.push(record);
  db.applications = next.sort((left, right) => left.createdAt.localeCompare(right.createdAt));
  await saveDb(db);
  return db;
}
