import OpenAI from "openai";

import type { ApplicationDraft, CompatibilityReport, Job, Profile } from "./types.js";

const DEFAULT_MODEL = process.env.OPENAI_MODEL ?? "gpt-5.4";

export function createOpenAIClient(): OpenAI | null {
  if (!process.env.OPENAI_API_KEY) {
    return null;
  }
  return new OpenAI({ apiKey: process.env.OPENAI_API_KEY });
}

export async function generateApplicationDraft(
  client: OpenAI | null,
  profile: Profile,
  job: Job,
  compatibility: CompatibilityReport
): Promise<ApplicationDraft> {
  if (!client) {
    return {
      fitSummary: `${compatibility.verdict} with score ${compatibility.score}/100.`,
      coverLetter: `Hello ${job.company},\n\nI am interested in the ${job.title} role. My background includes ${profile.skills.slice(0, 6).join(", ")}, which aligns with your requirements in ${compatibility.matchedSkills.join(", ")}.\n\nRegards,\n${profile.name}`,
      tailoredHighlights: compatibility.matchedSkills.slice(0, 5).map((skill) => `Relevant experience and study work in ${skill}.`),
      screeningAnswers: {
        work_authorization: profile.workAuthorization,
        location: profile.location
      }
    };
  }

  const response = await client.responses.create({
    model: DEFAULT_MODEL,
    input: [
      {
        role: "system",
        content: [
          {
            type: "input_text",
            text: "You create concise, truthful job application drafts. Only use facts present in the candidate profile and compatibility report. Do not invent employers, years, or projects."
          }
        ]
      },
      {
        role: "user",
        content: [
          {
            type: "input_text",
            text: JSON.stringify({
              profile,
              job,
              compatibility,
              task: "Return JSON with keys fitSummary, coverLetter, tailoredHighlights, screeningAnswers. Keep the cover letter under 220 words."
            })
          }
        ]
      }
    ],
    text: {
      format: {
        type: "json_schema",
        name: "application_draft",
        schema: {
          type: "object",
          additionalProperties: false,
          properties: {
            fitSummary: { type: "string" },
            coverLetter: { type: "string" },
            tailoredHighlights: {
              type: "array",
              items: { type: "string" }
            },
            screeningAnswers: {
              type: "object",
              additionalProperties: { type: "string" }
            }
          },
          required: ["fitSummary", "coverLetter", "tailoredHighlights", "screeningAnswers"]
        }
      }
    }
  });

  const output = response.output_text;
  return JSON.parse(output) as ApplicationDraft;
}
