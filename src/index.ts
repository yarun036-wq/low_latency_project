import "dotenv/config";

import { requestApproval } from "./approval.js";
import { buildDashboardData } from "./agent.js";
import { fillApplicationForm } from "./browser.js";
import { parseArgs } from "./cli.js";
import { loadFormConfig } from "./loaders.js";
import { createOpenAIClient, generateApplicationDraft } from "./openai-client.js";

async function main(): Promise<void> {
  const args = parseArgs(process.argv.slice(2));
  const { profile, reports } = await buildDashboardData(args.profilePath, args.jobsPath);
  const openai = createOpenAIClient();

  console.log("\nCompatibility ranking:\n");
  for (const report of reports) {
    console.log(`${report.job.id} | ${report.job.title} @ ${report.job.company}`);
    console.log(`Compatibility: ${report.compatibility.score}/100 (${report.compatibility.verdict})`);
    console.log(`Matched skills: ${report.compatibility.matchedSkills.join(", ") || "None"}`);
    console.log(`Missing skills: ${report.compatibility.missingSkills.join(", ") || "None"}`);
    console.log(`Why: ${report.compatibility.explanation.join(" ")}`);
    console.log("");
  }

  const selected = args.jobId ? reports.find((item) => item.job.id === args.jobId) : reports[0];
  if (!selected) {
    throw new Error("No job available to evaluate.");
  }

  const draft = await generateApplicationDraft(openai, profile, selected.job, selected.compatibility);

  console.log("Selected job:\n");
  console.log(`${selected.job.title} @ ${selected.job.company}`);
  console.log(`Compatibility verdict: ${selected.compatibility.verdict}`);
  console.log(`Fit summary: ${draft.fitSummary}\n`);
  console.log("Tailored highlights:");
  for (const item of draft.tailoredHighlights) {
    console.log(`- ${item}`);
  }
  console.log("\nCover letter draft:\n");
  console.log(draft.coverLetter);
  console.log("");

  if (!args.apply) {
    return;
  }

  if (!args.formConfigPath) {
    throw new Error("Missing --form-config for application automation.");
  }

  const approved = await requestApproval(`Open the browser and submit application for ${selected.job.title} at ${selected.job.company}?`);
  if (!approved) {
    console.log("Submission cancelled.");
    return;
  }

  const formConfig = await loadFormConfig(args.formConfigPath);
  await fillApplicationForm(formConfig, profile, draft, true);
  console.log("Browser flow completed.");
}

main().catch((error: unknown) => {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`Error: ${message}`);
  process.exitCode = 1;
});
