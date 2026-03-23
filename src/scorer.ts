import type { CompatibilityReport, Job, Profile } from "./types.js";

function normalize(value: string): string {
  return value.trim().toLowerCase();
}

function includesLoose(haystack: string[], needle: string): boolean {
  const normalizedNeedle = normalize(needle);
  return haystack.some((entry) => normalize(entry).includes(normalizedNeedle) || normalizedNeedle.includes(normalize(entry)));
}

export function scoreJobCompatibility(profile: Profile, job: Job): CompatibilityReport {
  const profileSkills = profile.skills.map(normalize);
  const matchedSkills = job.requirements.filter((skill) => includesLoose(profile.skills, skill));
  const missingSkills = job.requirements.filter((skill) => !includesLoose(profile.skills, skill));

  const titleMatch = profile.targetRoles.some((role) => normalize(job.title).includes(normalize(role)) || normalize(role).includes(normalize(job.title)));
  const locationMatch = profile.preferredLocations.some((location) => normalize(job.location).includes(normalize(location)) || normalize(location).includes(normalize(job.location)));
  const salaryMatch = job.salaryUsd === undefined || job.salaryUsd >= profile.minimumSalaryUsd;

  let score = 0;
  score += Math.round((matchedSkills.length / job.requirements.length) * 60);
  score += titleMatch ? 15 : 0;
  score += locationMatch ? 10 : 0;
  score += salaryMatch ? 10 : -5;
  score += profile.yearsOfExperience >= 1 ? 5 : 0;
  score = Math.max(0, Math.min(score, 100));

  const verdict: CompatibilityReport["verdict"] =
    score >= 75 ? "Strong match" : score >= 45 ? "Possible match" : "Weak match";

  const explanation = [
    `${matchedSkills.length} of ${job.requirements.length} required skills match your profile.`,
    titleMatch ? "The job title aligns with your target roles." : "The job title is outside your primary target roles.",
    locationMatch ? "The location fits your preferences." : "The location is outside your stated preferences.",
    salaryMatch ? "The listed compensation is above your current floor or not specified." : "The listed compensation is below your stated minimum."
  ];

  if (missingSkills.length > 0) {
    explanation.push(`Main gaps: ${missingSkills.join(", ")}.`);
  }

  const preferredHits = (job.preferredSkills ?? []).filter((skill) => profileSkills.includes(normalize(skill)));
  if (preferredHits.length > 0) {
    explanation.push(`You also match preferred skills: ${preferredHits.join(", ")}.`);
  }

  return {
    score,
    verdict,
    matchedSkills,
    missingSkills,
    titleMatch,
    locationMatch,
    salaryMatch,
    explanation
  };
}
