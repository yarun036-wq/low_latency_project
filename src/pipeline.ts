import { readFile } from "node:fs/promises";

import { syncSource } from "./adapters.js";
import { fillApplicationForm, launchApplicationReview } from "./browser.js";
import { loadFormConfig, loadProfile } from "./loaders.js";
import { createOpenAIClient, generateApplicationDraft } from "./openai-client.js";
import { scoreJobCompatibility } from "./scorer.js";
import { loadSourceConfigs } from "./source-loaders.js";
import { loadDb, replaceApplications, updateApplication, upsertJobs } from "./store.js";
import type { ApplicationRecord, BatchApplicationRunResult, Job, Profile, RawJobFeedItem } from "./types.js";

function normalizeFeedJob(item: RawJobFeedItem): Job {
  return {
    id: `${item.source}-${item.externalId}`,
    title: item.title,
    company: item.company,
    location: item.location,
    salaryUsd: item.salaryUsd,
    employmentType: item.employmentType,
    description: item.description,
    requirements: item.requirements,
    preferredSkills: item.preferredSkills,
    applicationUrl: item.applicationUrl,
    source: item.source,
    applicationType: item.applicationType,
    formConfigPath: item.formConfigPath
  };
}

export async function ingestJobFeed(feedPath: string): Promise<Job[]> {
  const raw = await readFile(feedPath, "utf8");
  const feed = JSON.parse(raw) as RawJobFeedItem[];
  const jobs = feed.map(normalizeFeedJob);
  await upsertJobs(jobs);
  return jobs;
}

export async function syncConfiguredSources(sourceConfigPath: string): Promise<Job[]> {
  const sources = await loadSourceConfigs(sourceConfigPath);
  const results = await Promise.all(sources.map((source) => syncSource(source)));
  const jobs = results.flat().map(normalizeFeedJob);
  if (jobs.length > 0) {
    await upsertJobs(jobs);
  }
  return jobs;
}

export async function listStoredJobs(): Promise<Job[]> {
  const db = await loadDb();
  return db.jobs;
}

export async function buildApplicationQueue(profile: Profile): Promise<ApplicationRecord[]> {
  const db = await loadDb();
  const existingByJobId = new Map(db.applications.map((entry) => [entry.jobId, entry]));

  const applications = db.jobs.map((job) => {
    const compatibility = scoreJobCompatibility(profile, job);
    const prior = existingByJobId.get(job.id);
    const eligible = compatibility.score >= 70 && job.applicationType !== "manual_only";
    const status = prior?.status && prior.status !== "blocked" && prior.status !== "new"
      ? prior.status
      : eligible
        ? "queued"
        : compatibility.score >= 45
          ? "new"
          : "blocked";

    const notes = [
      `${compatibility.score}/100 ${compatibility.verdict}`,
      ...compatibility.explanation
    ];

    return {
      id: prior?.id ?? `app-${job.id}`,
      jobId: job.id,
      compatibilityScore: compatibility.score,
      compatibilityVerdict: compatibility.verdict,
      status,
      notes,
      draft: prior?.draft,
      createdAt: prior?.createdAt ?? new Date().toISOString(),
      updatedAt: new Date().toISOString()
    } satisfies ApplicationRecord;
  });

  await replaceApplications(applications);
  return applications;
}

export async function loadPipelineState(profile: Profile) {
  const db = await loadDb();
  const jobs = db.jobs;
  const jobsById = new Map(jobs.map((job) => [job.id, job]));
  const applications = db.applications
    .map((application) => ({
      ...application,
      job: jobsById.get(application.jobId),
      compatibility: jobsById.get(application.jobId)
        ? scoreJobCompatibility(profile, jobsById.get(application.jobId) as Job)
        : undefined
    }))
    .filter((entry) => entry.job);

  return {
    jobs,
    applications
  };
}

export async function prepareApplicationDraft(
  profilePath: string,
  applicationId: string
): Promise<ApplicationRecord> {
  const db = await loadDb();
  const application = db.applications.find((entry) => entry.id === applicationId);
  if (!application) {
    throw new Error(`Application not found: ${applicationId}`);
  }
  const job = db.jobs.find((entry) => entry.id === application.jobId);
  if (!job) {
    throw new Error(`Job not found for application ${applicationId}.`);
  }
  const profile = await loadProfile(profilePath);
  const compatibility = scoreJobCompatibility(profile, job);
  const draft = await generateApplicationDraft(createOpenAIClient(), profile, job, compatibility);
  const updated: ApplicationRecord = {
    ...application,
    draft,
    status: "draft_ready",
    updatedAt: new Date().toISOString(),
    notes: [...application.notes, "Draft generated."]
  };
  await updateApplication(updated);
  return updated;
}

export async function approveApplication(applicationId: string): Promise<ApplicationRecord> {
  const db = await loadDb();
  const application = db.applications.find((entry) => entry.id === applicationId);
  if (!application) {
    throw new Error(`Application not found: ${applicationId}`);
  }
  const updated: ApplicationRecord = {
    ...application,
    status: "approved",
    updatedAt: new Date().toISOString(),
    notes: [...application.notes, "Approved for submission."]
  };
  await updateApplication(updated);
  return updated;
}

export async function submitApplication(
  profile: Profile,
  applicationId: string
): Promise<ApplicationRecord> {
  const db = await loadDb();
  const application = db.applications.find((entry) => entry.id === applicationId);
  if (!application) {
    throw new Error(`Application not found: ${applicationId}`);
  }
  if (application.status !== "approved") {
    throw new Error("Application must be approved before submission.");
  }
  const job = db.jobs.find((entry) => entry.id === application.jobId);
  if (!job) {
    throw new Error(`Job not found for application ${applicationId}.`);
  }
  if (!application.draft) {
    throw new Error("Generate a draft before submission.");
  }

  if (job.applicationType === "manual_only") {
    throw new Error("Manual-only applications cannot be auto-submitted.");
  }

  if (job.applicationType === "easy_apply") {
    throw new Error("Easy Apply auto-submit is not supported. Use manual review for this job.");
  }

  if (job.applicationType === "external_form") {
    if (!job.formConfigPath) {
      throw new Error("External form applications require a form config before submission.");
    }
    const formConfig = await loadFormConfig(job.formConfigPath);
    await fillApplicationForm(formConfig, profile, application.draft, true);
  }

  const updated: ApplicationRecord = {
    ...application,
    status: "submitted",
    updatedAt: new Date().toISOString(),
    notes: [...application.notes, `Submitted via ${job.applicationType ?? "manual_only"} flow.`]
  };
  await updateApplication(updated);
  return updated;
}

export async function runAutoApply(profilePath: string): Promise<BatchApplicationRunResult[]> {
  const profile = await loadProfile(profilePath);
  const queue = await buildApplicationQueue(profile);
  const db = await loadDb();
  const jobsById = new Map(db.jobs.map((job) => [job.id, job]));
  const eligibleStatuses = new Set<ApplicationRecord["status"]>(["queued", "draft_ready", "approved"]);
  const results: BatchApplicationRunResult[] = [];

  for (const application of queue) {
    const job = jobsById.get(application.jobId);
    if (!job) {
      results.push({
        applicationId: application.id,
        jobId: application.jobId,
        title: "Unknown job",
        company: "Unknown company",
        outcome: "failed",
        reason: "Job record is missing from storage.",
        status: "error"
      });
      continue;
    }

    if (!eligibleStatuses.has(application.status)) {
      results.push({
        applicationId: application.id,
        jobId: job.id,
        title: job.title,
        company: job.company,
        outcome: "skipped",
        reason: `Skipped because status is ${application.status}.`,
        status: application.status
      });
      continue;
    }

    if (job.applicationType === "manual_only") {
      results.push({
        applicationId: application.id,
        jobId: job.id,
        title: job.title,
        company: job.company,
        outcome: "skipped",
        reason: "Manual-only job; no automatic submission path is configured.",
        status: application.status
      });
      continue;
    }

    if (job.applicationType === "easy_apply") {
      results.push({
        applicationId: application.id,
        jobId: job.id,
        title: job.title,
        company: job.company,
        outcome: "skipped",
        reason: "Easy Apply is review-assisted only in this project and is not auto-submitted.",
        status: application.status
      });
      continue;
    }

    try {
      let current = application;
      if (!current.draft) {
        current = await prepareApplicationDraft(profilePath, current.id);
      }
      if (current.status !== "approved") {
        current = await approveApplication(current.id);
      }
      current = await submitApplication(profile, current.id);
      results.push({
        applicationId: current.id,
        jobId: job.id,
        title: job.title,
        company: job.company,
        outcome: "submitted",
        reason: "Draft generated, approved, and submitted automatically.",
        status: current.status
      });
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      const failed: ApplicationRecord = {
        ...(await loadDb()).applications.find((entry) => entry.id === application.id) ?? application,
        status: "error",
        updatedAt: new Date().toISOString(),
        notes: [...application.notes, `Auto-apply failed: ${message}`]
      };
      await updateApplication(failed);
      results.push({
        applicationId: application.id,
        jobId: job.id,
        title: job.title,
        company: job.company,
        outcome: "failed",
        reason: message,
        status: "error"
      });
    }
  }

  return results;
}

export async function startManualReview(
  profilePath: string,
  applicationId: string
): Promise<ApplicationRecord> {
  const db = await loadDb();
  const application = db.applications.find((entry) => entry.id === applicationId);
  if (!application) {
    throw new Error(`Application not found: ${applicationId}`);
  }
  const job = db.jobs.find((entry) => entry.id === application.jobId);
  if (!job) {
    throw new Error(`Job not found for application ${applicationId}.`);
  }

  const profile = await loadProfile(profilePath);
  const compatibility = scoreJobCompatibility(profile, job);
  const draft = application.draft ?? await generateApplicationDraft(createOpenAIClient(), profile, job, compatibility);
  const formConfig = job.formConfigPath ? await loadFormConfig(job.formConfigPath) : undefined;

  await launchApplicationReview(job.applicationUrl, profile, draft, formConfig);

  const updated: ApplicationRecord = {
    ...application,
    draft,
    status: application.status === "submitted" ? application.status : "draft_ready",
    updatedAt: new Date().toISOString(),
    notes: [...application.notes, "Opened in manual review mode."]
  };
  await updateApplication(updated);
  return updated;
}
