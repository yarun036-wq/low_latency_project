const profileSummary = document.getElementById("profileSummary");
const jobList = document.getElementById("jobList");
const queueList = document.getElementById("queueList");
const draftView = document.getElementById("draftView");
const draftStatus = document.getElementById("draftStatus");
const jobCount = document.getElementById("jobCount");
const queueCount = document.getElementById("queueCount");
const refreshButton = document.getElementById("refreshButton");
const ingestButton = document.getElementById("ingestButton");
const syncSourcesButton = document.getElementById("syncSourcesButton");
const queueButton = document.getElementById("queueButton");
const autoApplyButton = document.getElementById("autoApplyButton");

let pipelineState = null;

function badgeClass(verdict) {
  if (verdict === "Strong match") return "score-badge score-strong";
  if (verdict === "Possible match") return "score-badge score-possible";
  return "score-badge score-weak";
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function setDraftMessage(message) {
  draftStatus.textContent = "Waiting";
  draftView.classList.add("empty-state");
  draftView.textContent = message;
}

function formatBatchResults(results) {
  if (!results.length) {
    return "No eligible queued applications were found.";
  }

  return results
    .map((result) => `${result.outcome.toUpperCase()}: ${result.title} @ ${result.company} - ${result.reason}`)
    .join("\n");
}

function renderProfile(profile, lastIngestedAt) {
  profileSummary.innerHTML = `
    <p><strong>${escapeHtml(profile.name)}</strong><br />${escapeHtml(profile.location)}<br />${escapeHtml(profile.email)}<br />${escapeHtml(profile.phone)}</p>
    <p><strong>Experience:</strong> ${escapeHtml(profile.yearsOfExperience)} years</p>
    <p><strong>Target roles:</strong> ${escapeHtml(profile.targetRoles.join(", "))}</p>
    <p><strong>Last ingest:</strong> ${lastIngestedAt ? escapeHtml(new Date(lastIngestedAt).toLocaleString()) : "Not run yet"}</p>
    <p><strong>Resume summary:</strong> ${escapeHtml(profile.resumeText)}</p>
    <div class="chips">
      ${profile.skills.slice(0, 16).map((skill) => `<span class="chip">${escapeHtml(skill)}</span>`).join("")}
    </div>
  `;
}

function renderJobs(applications) {
  jobCount.textContent = `${applications.length} jobs`;
  jobList.innerHTML = applications.map((entry) => `
    <article class="job-card">
      <h3>${escapeHtml(entry.job.title)}</h3>
      <p class="job-meta">${escapeHtml(entry.job.company)} • ${escapeHtml(entry.job.location)} • ${escapeHtml(entry.job.employmentType)}</p>
      <div class="score-row">
        <span class="${badgeClass(entry.compatibility.verdict)}">${escapeHtml(entry.compatibility.score)}/100</span>
        <span class="muted">${escapeHtml(entry.compatibility.verdict)}</span>
      </div>
      <p>${escapeHtml(entry.compatibility.explanation[0])}</p>
      <div class="chips">
        ${entry.compatibility.matchedSkills.slice(0, 6).map((skill) => `<span class="chip">${escapeHtml(skill)}</span>`).join("")}
      </div>
      <p class="muted">${entry.compatibility.missingSkills.length ? `Missing: ${escapeHtml(entry.compatibility.missingSkills.join(", "))}` : "No required skill gaps."}</p>
      <button data-application-id="${escapeHtml(entry.id)}" data-action="open">Open Queue Item</button>
    </article>
  `).join("");

  for (const button of jobList.querySelectorAll("button[data-application-id]")) {
    button.addEventListener("click", async () => {
      const applicationId = button.getAttribute("data-application-id");
      if (applicationId) {
        await openApplication(applicationId);
      }
    });
  }
}

function renderQueue(applications) {
  queueCount.textContent = `${applications.length} items`;
  queueList.innerHTML = applications.map((entry) => `
    <article class="queue-card">
      <h3>${escapeHtml(entry.job.title)}</h3>
      <p class="job-meta">${escapeHtml(entry.job.company)} • ${escapeHtml(entry.job.source ?? "direct")} • ${escapeHtml(entry.job.applicationType ?? "manual_only")}</p>
      <span class="${badgeClass(entry.compatibility.verdict)}">${escapeHtml(entry.compatibility.score)}/100</span>
      <div class="status-pill">Status: ${escapeHtml(entry.status)}</div>
      <p class="muted">${escapeHtml(entry.notes[0] ?? "No notes")}</p>
      <div class="queue-actions">
        <button data-application-id="${escapeHtml(entry.id)}" data-action="open">View</button>
        <button data-application-id="${escapeHtml(entry.id)}" data-action="draft">Generate Draft</button>
        <button data-application-id="${escapeHtml(entry.id)}" data-action="approve">Approve</button>
        <button class="primary" data-application-id="${escapeHtml(entry.id)}" data-action="submit">Submit</button>
      </div>
    </article>
  `).join("");

  for (const button of queueList.querySelectorAll("button[data-application-id]")) {
    button.addEventListener("click", async () => {
      const applicationId = button.getAttribute("data-application-id");
      const action = button.getAttribute("data-action");
      if (!applicationId || !action) {
        return;
      }
      if (action === "open") {
        await openApplication(applicationId);
        return;
      }
      await runAction(action, applicationId);
    });
  }
}

function renderDraft(entry) {
  draftStatus.textContent = `${entry.job.title} @ ${entry.job.company}`;
  draftView.classList.remove("empty-state");
  draftView.innerHTML = `
    <section class="draft-section">
      <h3>Compatibility</h3>
      <p><span class="${badgeClass(entry.compatibility.verdict)}">${escapeHtml(entry.compatibility.score)}/100</span></p>
      <ul class="detail-list">
        ${entry.compatibility.explanation.map((line) => `<li>${escapeHtml(line)}</li>`).join("")}
      </ul>
    </section>
    <section class="draft-section">
      <h3>Application State</h3>
      <p><strong>Status:</strong> ${escapeHtml(entry.status)}</p>
      <ul class="detail-list">
        ${entry.notes.map((line) => `<li>${escapeHtml(line)}</li>`).join("")}
      </ul>
    </section>
    ${entry.draft ? `
      <section class="draft-section">
        <h3>Tailored Highlights</h3>
        <ul class="detail-list">
          ${entry.draft.tailoredHighlights.map((line) => `<li>${escapeHtml(line)}</li>`).join("")}
        </ul>
      </section>
      <section class="draft-section">
        <h3>Fit Summary</h3>
        <p>${escapeHtml(entry.draft.fitSummary)}</p>
      </section>
      <section class="draft-section">
        <h3>Cover Letter</h3>
        <div class="preformatted">${escapeHtml(entry.draft.coverLetter)}</div>
      </section>
      <section class="draft-section">
        <h3>Screening Answers</h3>
        <ul class="detail-list">
          ${Object.entries(entry.draft.screeningAnswers).map(([key, value]) => `<li><strong>${escapeHtml(key)}:</strong> ${escapeHtml(value)}</li>`).join("")}
        </ul>
      </section>
    ` : `
      <section class="draft-section">
        <h3>No Draft Yet</h3>
        <p>Generate a draft from the queue to prepare this application.</p>
      </section>
    `}
  `;
}

async function fetchJson(url, options = undefined) {
  const response = await fetch(url, options);
  const payload = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(payload.error || `Request failed: ${url}`);
  }
  return payload;
}

async function refreshPipeline() {
  const payload = await fetchJson("/api/pipeline");
  pipelineState = payload;
  renderProfile(payload.profile, payload.lastIngestedAt);
  renderJobs(payload.applications);
  renderQueue(payload.applications.filter((entry) => entry.status !== "blocked"));
}

async function openApplication(applicationId) {
  if (!pipelineState) {
    await refreshPipeline();
  }
  const entry = pipelineState.applications.find((application) => application.id === applicationId);
  if (!entry) {
    throw new Error("Application not found in dashboard state.");
  }
  renderDraft(entry);
}

async function runAction(action, applicationId) {
  draftStatus.textContent = "Working...";
  try {
    if (action === "draft") {
      await fetchJson(`/api/applications/${encodeURIComponent(applicationId)}/draft`, { method: "POST" });
    } else if (action === "approve") {
      await fetchJson(`/api/applications/${encodeURIComponent(applicationId)}/approve`, { method: "POST" });
    } else if (action === "submit") {
      await fetchJson(`/api/applications/${encodeURIComponent(applicationId)}/submit`, { method: "POST" });
    }
    await refreshPipeline();
    await openApplication(applicationId);
  } catch (error) {
    setDraftMessage(error instanceof Error ? error.message : String(error));
  }
}

refreshButton.addEventListener("click", async () => {
  try {
    await refreshPipeline();
    setDraftMessage("Select a queue item to inspect or act on it.");
  } catch (error) {
    setDraftMessage(error instanceof Error ? error.message : String(error));
  }
});

ingestButton.addEventListener("click", async () => {
  try {
    await fetchJson("/api/ingest", { method: "POST" });
    await refreshPipeline();
    setDraftMessage("Job ingestion completed. Queue has been refreshed.");
  } catch (error) {
    setDraftMessage(error instanceof Error ? error.message : String(error));
  }
});

queueButton.addEventListener("click", async () => {
  try {
    await fetchJson("/api/queue/build", { method: "POST" });
    await refreshPipeline();
    setDraftMessage("Queue rebuilt from the latest jobs and profile.");
  } catch (error) {
    setDraftMessage(error instanceof Error ? error.message : String(error));
  }
});

syncSourcesButton.addEventListener("click", async () => {
  try {
    await fetchJson("/api/sources/sync", { method: "POST" });
    await refreshPipeline();
    setDraftMessage("Configured sources synced and queue refreshed.");
  } catch (error) {
    setDraftMessage(error instanceof Error ? error.message : String(error));
  }
});

autoApplyButton.addEventListener("click", async () => {
  draftStatus.textContent = "Running auto apply...";
  try {
    const payload = await fetchJson("/api/queue/auto-apply", { method: "POST" });
    await refreshPipeline();
    setDraftMessage(formatBatchResults(payload.results));
  } catch (error) {
    setDraftMessage(error instanceof Error ? error.message : String(error));
  }
});

refreshPipeline()
  .then(() => {
    setDraftMessage("Select a queue item to inspect or act on it.");
  })
  .catch((error) => {
    setDraftMessage(error instanceof Error ? error.message : String(error));
  });
