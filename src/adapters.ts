import { readFile } from "node:fs/promises";

import type { RawJobFeedItem, SourceConfig } from "./types.js";

type GreenhouseJob = {
  id: number;
  title: string;
  absolute_url: string;
  location?: { name?: string };
  content?: string;
};

type LeverJob = {
  id: string;
  text: string;
  hostedUrl: string;
  categories?: {
    location?: string;
    team?: string;
    commitment?: string;
  };
  descriptionPlain?: string;
  lists?: Array<{
    text?: string;
    content?: string;
  }>;
};

type WorkdayFeedItem = {
  id?: string;
  title?: string;
  company?: string;
  location?: string;
  url?: string;
  description?: string;
  requirements?: string[];
  employmentType?: string;
};

function htmlToText(value: string): string {
  return value.replace(/<[^>]+>/g, " ").replace(/\s+/g, " ").trim();
}

function inferRequirements(text: string): string[] {
  const candidates = [
    "C++",
    "C",
    "Python",
    "Linux",
    "Concurrency",
    "Multithreading",
    "GDB",
    "Build Systems",
    "Git",
    "REST APIs",
    "System Design",
    "Swift",
    "iOS",
    "UIKit",
    "CI/CD"
  ];
  return candidates.filter((item) => text.toLowerCase().includes(item.toLowerCase()));
}

async function fetchJson(url: string) {
  const response = await fetch(url, {
    headers: {
      "User-Agent": "job-application-agent/0.1"
    }
  });
  if (!response.ok) {
    throw new Error(`Failed to fetch ${url}: ${response.status}`);
  }
  return response.json();
}

async function syncGreenhouse(source: Extract<SourceConfig, { type: "greenhouse" }>): Promise<RawJobFeedItem[]> {
  const url = `https://boards-api.greenhouse.io/v1/boards/${source.boardToken}/jobs?content=true`;
  const payload = await fetchJson(url) as { jobs?: GreenhouseJob[] };
  return (payload.jobs ?? []).map((job) => {
    const description = htmlToText(job.content ?? "");
    return {
      source: source.id,
      externalId: String(job.id),
      title: job.title,
      company: source.boardToken,
      location: job.location?.name ?? "Unknown",
      employmentType: "Full-time",
      description,
      requirements: inferRequirements(description),
      preferredSkills: [],
      applicationUrl: job.absolute_url,
      applicationType: "external_form"
    } satisfies RawJobFeedItem;
  });
}

async function syncLever(source: Extract<SourceConfig, { type: "lever" }>): Promise<RawJobFeedItem[]> {
  const url = `https://api.lever.co/v0/postings/${source.site}?mode=json`;
  const payload = await fetchJson(url) as LeverJob[];
  return payload.map((job) => {
    const description = htmlToText(job.descriptionPlain ?? job.lists?.map((item) => item.content ?? item.text ?? "").join(" ") ?? "");
    return {
      source: source.id,
      externalId: job.id,
      title: job.text,
      company: source.site,
      location: job.categories?.location ?? "Unknown",
      employmentType: job.categories?.commitment ?? "Full-time",
      description,
      requirements: inferRequirements(description),
      preferredSkills: [],
      applicationUrl: job.hostedUrl,
      applicationType: "external_form"
    } satisfies RawJobFeedItem;
  });
}

async function syncWorkday(source: Extract<SourceConfig, { type: "workday" }>): Promise<RawJobFeedItem[]> {
  const payload = await fetchJson(source.feedUrl) as WorkdayFeedItem[];
  return payload.map((job, index) => {
    const description = htmlToText(job.description ?? "");
    return {
      source: source.id,
      externalId: String(job.id ?? index + 1),
      title: job.title ?? "Unknown role",
      company: job.company ?? source.label,
      location: job.location ?? "Unknown",
      employmentType: job.employmentType ?? "Full-time",
      description,
      requirements: job.requirements?.length ? job.requirements : inferRequirements(description),
      preferredSkills: [],
      applicationUrl: job.url ?? source.feedUrl,
      applicationType: "external_form"
    } satisfies RawJobFeedItem;
  });
}

async function syncLinkedInManual(source: Extract<SourceConfig, { type: "linkedin_manual" }>): Promise<RawJobFeedItem[]> {
  const raw = await readFile(source.filePath, "utf8");
  const payload = JSON.parse(raw) as RawJobFeedItem[];
  return payload.map((job) => ({
    ...job,
    source: source.id
  }));
}

export async function syncSource(source: SourceConfig): Promise<RawJobFeedItem[]> {
  if (!source.enabled) {
    return [];
  }

  if (source.type === "greenhouse") {
    return syncGreenhouse(source);
  }
  if (source.type === "lever") {
    return syncLever(source);
  }
  if (source.type === "workday") {
    return syncWorkday(source);
  }
  return syncLinkedInManual(source);
}
