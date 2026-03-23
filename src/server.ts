import "dotenv/config";

import { createServer } from "node:http";
import { readFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

import { buildDashboardData, buildJobDraft } from "./agent.js";
import { launchLinkedInEasyApplyReview } from "./browser.js";
import { loadProfile } from "./loaders.js";
import {
  approveApplication,
  buildApplicationQueue,
  ingestJobFeed,
  listStoredJobs,
  loadPipelineState,
  prepareApplicationDraft,
  runAutoApply,
  startManualReview,
  submitApplication,
  syncConfiguredSources
} from "./pipeline.js";
import { loadDb, upsertJobs } from "./store.js";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const rootDir = path.resolve(__dirname, "..");
const publicDir = path.join(rootDir, "public");

const profilePath = path.join(rootDir, "data", "profile.json");
const jobsPath = path.join(rootDir, "data", "jobs.json");
const feedPath = path.join(rootDir, "data", "job-feed.json");
const sourceConfigPath = path.join(rootDir, "data", "sources.json");
const port = Number(process.env.PORT ?? "3000");

const contentTypes: Record<string, string> = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "application/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8"
};

function sendJson(res: import("node:http").ServerResponse, status: number, payload: unknown): void {
  res.writeHead(status, { "Content-Type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(payload));
}

async function serveStatic(res: import("node:http").ServerResponse, relativePath: string): Promise<void> {
  const filePath = path.join(publicDir, relativePath);
  const body = await readFile(filePath);
  const ext = path.extname(filePath);
  res.writeHead(200, { "Content-Type": contentTypes[ext] ?? "application/octet-stream" });
  res.end(body);
}

async function readBody(req: import("node:http").IncomingMessage): Promise<string> {
  const chunks: Buffer[] = [];
  for await (const chunk of req) {
    chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk));
  }
  return Buffer.concat(chunks).toString("utf8");
}

async function ensureSeedData(): Promise<void> {
  const db = await loadDb();
  if (db.jobs.length > 0) {
    return;
  }
  const dashboard = await buildDashboardData(profilePath, jobsPath);
  await upsertJobs(dashboard.jobs);
  await ingestJobFeed(feedPath);
  const profile = await loadProfile(profilePath);
  await buildApplicationQueue(profile);
}

const server = createServer(async (req, res) => {
  try {
    await ensureSeedData();
    const url = new URL(req.url ?? "/", `http://${req.headers.host ?? "localhost"}`);

    if (req.method === "GET" && url.pathname === "/api/dashboard") {
      const data = await buildDashboardData(profilePath, jobsPath);
      sendJson(res, 200, data);
      return;
    }

    if (req.method === "GET" && url.pathname === "/api/pipeline") {
      const profile = await loadProfile(profilePath);
      const state = await loadPipelineState(profile);
      const db = await loadDb();
      sendJson(res, 200, {
        profile,
        jobs: state.jobs,
        applications: state.applications,
        lastIngestedAt: db.lastIngestedAt
      });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/draft") {
      const body = await readBody(req);
      const parsed = JSON.parse(body) as { jobId?: string };
      if (!parsed.jobId) {
        sendJson(res, 400, { error: "Missing jobId." });
        return;
      }
      const data = await buildJobDraft(profilePath, jobsPath, parsed.jobId);
      sendJson(res, 200, data);
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/ingest") {
      const dashboard = await buildDashboardData(profilePath, jobsPath);
      await upsertJobs(dashboard.jobs);
      const jobs = await ingestJobFeed(feedPath);
      const profile = await loadProfile(profilePath);
      const applications = await buildApplicationQueue(profile);
      sendJson(res, 200, { jobs, applications });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/sources/sync") {
      const jobs = await syncConfiguredSources(sourceConfigPath);
      const profile = await loadProfile(profilePath);
      const applications = await buildApplicationQueue(profile);
      sendJson(res, 200, { jobs, applications });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/linkedin/review") {
      const body = await readBody(req);
      const parsed = JSON.parse(body) as { url?: string };
      if (!parsed.url) {
        sendJson(res, 400, { error: "Missing LinkedIn job URL." });
        return;
      }
      const profile = await loadProfile(profilePath);
      await launchLinkedInEasyApplyReview(parsed.url, profile);
      sendJson(res, 200, { ok: true });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/queue/build") {
      const profile = await loadProfile(profilePath);
      const applications = await buildApplicationQueue(profile);
      sendJson(res, 200, { applications });
      return;
    }

    if (req.method === "POST" && url.pathname === "/api/queue/auto-apply") {
      const results = await runAutoApply(profilePath);
      sendJson(res, 200, { results });
      return;
    }

    if (req.method === "GET" && url.pathname === "/api/jobs") {
      const jobs = await listStoredJobs();
      sendJson(res, 200, { jobs });
      return;
    }

    const applicationDraftMatch = req.method === "POST"
      ? url.pathname.match(/^\/api\/applications\/([^/]+)\/draft$/)
      : null;
    if (applicationDraftMatch) {
      const applicationId = decodeURIComponent(applicationDraftMatch[1]);
      const application = await prepareApplicationDraft(profilePath, applicationId);
      sendJson(res, 200, { application });
      return;
    }

    const applicationApproveMatch = req.method === "POST"
      ? url.pathname.match(/^\/api\/applications\/([^/]+)\/approve$/)
      : null;
    if (applicationApproveMatch) {
      const applicationId = decodeURIComponent(applicationApproveMatch[1]);
      const application = await approveApplication(applicationId);
      sendJson(res, 200, { application });
      return;
    }

    const applicationSubmitMatch = req.method === "POST"
      ? url.pathname.match(/^\/api\/applications\/([^/]+)\/submit$/)
      : null;
    if (applicationSubmitMatch) {
      const applicationId = decodeURIComponent(applicationSubmitMatch[1]);
      const profile = await loadProfile(profilePath);
      const application = await submitApplication(profile, applicationId);
      sendJson(res, 200, { application });
      return;
    }

    const applicationReviewMatch = req.method === "POST"
      ? url.pathname.match(/^\/api\/applications\/([^/]+)\/review$/)
      : null;
    if (applicationReviewMatch) {
      const applicationId = decodeURIComponent(applicationReviewMatch[1]);
      const application = await startManualReview(profilePath, applicationId);
      sendJson(res, 200, { application });
      return;
    }

    if (req.method === "GET" && (url.pathname === "/" || url.pathname === "/index.html")) {
      await serveStatic(res, "index.html");
      return;
    }

    if (req.method === "GET" && url.pathname.startsWith("/")) {
      const requested = url.pathname.slice(1);
      if (/^[A-Za-z0-9._-]+$/.test(requested) && contentTypes[path.extname(requested)]) {
        await serveStatic(res, requested);
        return;
      }
    }

    sendJson(res, 404, { error: "Not found" });
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    sendJson(res, 500, { error: message });
  }
});

server.listen(port, () => {
  console.log(`Job agent dashboard running at http://localhost:${port}`);
});
