import { loadJobs, loadProfile } from "./loaders.js";
import { createOpenAIClient, generateApplicationDraft } from "./openai-client.js";
import { scoreJobCompatibility } from "./scorer.js";

export async function buildDashboardData(profilePath: string, jobsPath: string) {
  const profile = await loadProfile(profilePath);
  const jobs = await loadJobs(jobsPath);
  const reports = jobs
    .map((job) => ({
      job,
      compatibility: scoreJobCompatibility(profile, job)
    }))
    .sort((left, right) => right.compatibility.score - left.compatibility.score);

  return { profile, jobs, reports };
}

export async function buildJobDraft(profilePath: string, jobsPath: string, jobId: string) {
  const { profile, reports } = await buildDashboardData(profilePath, jobsPath);
  const selected = reports.find((entry) => entry.job.id === jobId);
  if (!selected) {
    throw new Error(`Job not found: ${jobId}`);
  }

  const draft = await generateApplicationDraft(
    createOpenAIClient(),
    profile,
    selected.job,
    selected.compatibility
  );

  return {
    profile,
    selected,
    draft
  };
}
