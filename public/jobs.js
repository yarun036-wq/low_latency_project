const jobsGrid = document.getElementById("jobsGrid");
const jobsSummary = document.getElementById("jobsSummary");
const jobsStatus = document.getElementById("jobsStatus");
const jobsDetail = document.getElementById("jobsDetail");
const searchInput = document.getElementById("searchInput");
const scoreFilter = document.getElementById("scoreFilter");
const typeFilter = document.getElementById("typeFilter");
const sourceFilter = document.getElementById("sourceFilter");
const syncJobsButton = document.getElementById("syncJobsButton");
const rebuildQueueButton = document.getElementById("rebuildQueueButton");

let state = null;

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function badgeClass(verdict) {
  if (verdict === "Strong match") return "score-badge score-strong";
  if (verdict === "Possible match") return "score-badge score-possible";
  return "score-badge score-weak";
}

async function fetchJson(url, options) {
  const response = await fetch(url, options);
  const payload = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(payload.error || `Request failed: ${url}`);
  }
  return payload;
}

function setDetail(message) {
  jobsStatus.textContent = "Ready";
  jobsDetail.classList.add("empty-state");
  jobsDetail.textContent = message;
}

function getFilteredApplications() {
  if (!state) {
    return [];
  }

  const query = searchInput.value.trim().toLowerCase();
  const minScore = Number(scoreFilter.value);
  const selectedType = typeFilter.value;
  const selectedSource = sourceFilter.value;

  return state.applications.filter((entry) => {
    const job = entry.job;
    if (!job) {
      return false;
    }

    if (entry.compatibility.score < minScore) {
      return false;
    }
    if (selectedType !== "all" && (job.applicationType ?? "manual_only") !== selectedType) {
      return false;
    }
    if (selectedSource !== "all" && (job.source ?? "direct") !== selectedSource) {
      return false;
    }
    if (!query) {
      return true;
    }

    const haystack = [
      job.title,
      job.company,
      job.location,
      job.source ?? "direct",
      ...(job.requirements ?? []),
      ...(entry.compatibility.matchedSkills ?? [])
    ].join(" ").toLowerCase();

    return haystack.includes(query);
  });
}

function populateSourceFilter(applications) {
  const existing = new Set(Array.from(sourceFilter.options).map((option) => option.value));
  const sources = [...new Set(applications.map((entry) => entry.job?.source ?? "direct"))].sort();
  sourceFilter.innerHTML = '<option value="all">All</option>';
  for (const source of sources) {
    const option = document.createElement("option");
    option.value = source;
    option.textContent = source;
    sourceFilter.append(option);
  }
  if (existing.has(sourceFilter.value)) {
    sourceFilter.value = sourceFilter.value;
  }
}

function renderJobs() {
  const applications = getFilteredApplications();
  jobsSummary.textContent = `${applications.length} jobs`;

  jobsGrid.innerHTML = applications.map((entry) => {
    const job = entry.job;
    const applicationType = job.applicationType ?? "manual_only";
    const source = job.source ?? "direct";
    return `
      <article class="job-browser-card">
        <div class="job-browser-head">
          <div>
            <h3>${escapeHtml(job.title)}</h3>
            <p class="job-meta">${escapeHtml(job.company)} | ${escapeHtml(job.location)}</p>
          </div>
          <div class="job-browser-badges">
            <span class="${badgeClass(entry.compatibility.verdict)}">${escapeHtml(entry.compatibility.score)}/100</span>
            <span class="status-pill">${escapeHtml(entry.status)}</span>
          </div>
        </div>
        <p class="muted">${escapeHtml(source)} | ${escapeHtml(applicationType)}</p>
        <p>${escapeHtml(entry.compatibility.explanation[0] ?? "No explanation available.")}</p>
        <div class="chips">
          ${entry.compatibility.matchedSkills.slice(0, 6).map((skill) => `<span class="chip">${escapeHtml(skill)}</span>`).join("")}
        </div>
        <div class="job-browser-actions">
          <a class="primary-link-button" href="${escapeHtml(job.applicationUrl)}" target="_blank" rel="noreferrer">Open Job Link</a>
          <button data-id="${escapeHtml(entry.id)}" data-action="view">View</button>
          <button data-id="${escapeHtml(entry.id)}" data-action="draft">Generate Draft</button>
          <button data-id="${escapeHtml(entry.id)}" data-action="review">Manual Review</button>
          <button data-id="${escapeHtml(entry.id)}" data-action="auto">Auto Apply</button>
        </div>
      </article>
    `;
  }).join("");

  for (const button of jobsGrid.querySelectorAll("button[data-id]")) {
    button.addEventListener("click", async () => {
      const id = button.getAttribute("data-id");
      const action = button.getAttribute("data-action");
      if (!id || !action) {
        return;
      }
      if (action === "view") {
        showApplication(id);
        return;
      }
      await runAction(id, action);
    });
  }
}

function showApplication(applicationId) {
  const entry = state?.applications.find((application) => application.id === applicationId);
  if (!entry) {
    setDetail("Application not found.");
    return;
  }
  jobsStatus.textContent = `${entry.job.title} @ ${entry.job.company}`;
  jobsDetail.classList.remove("empty-state");
  jobsDetail.innerHTML = `
    <section class="draft-section">
      <h3>${escapeHtml(entry.job.title)}</h3>
      <p><strong>Company:</strong> ${escapeHtml(entry.job.company)}</p>
      <p><strong>Platform:</strong> ${escapeHtml(entry.job.source ?? "direct")}</p>
      <p><strong>Type:</strong> ${escapeHtml(entry.job.applicationType ?? "manual_only")}</p>
      <p><strong>Apply URL:</strong> <a href="${escapeHtml(entry.job.applicationUrl)}" target="_blank" rel="noreferrer">${escapeHtml(entry.job.applicationUrl)}</a></p>
    </section>
    <section class="draft-section">
      <h3>Compatibility</h3>
      <ul class="detail-list">
        ${entry.compatibility.explanation.map((line) => `<li>${escapeHtml(line)}</li>`).join("")}
      </ul>
    </section>
    <section class="draft-section">
      <h3>Notes</h3>
      <ul class="detail-list">
        ${entry.notes.map((line) => `<li>${escapeHtml(line)}</li>`).join("")}
      </ul>
    </section>
    ${entry.draft ? `
      <section class="draft-section">
        <h3>Draft</h3>
        <div class="preformatted">${escapeHtml(entry.draft.coverLetter)}</div>
      </section>
    ` : ""}
  `;
}

async function refreshState() {
  jobsStatus.textContent = "Loading";
  state = await fetchJson("/api/pipeline");
  populateSourceFilter(state.applications);
  renderJobs();
  jobsStatus.textContent = "Ready";
}

async function runAction(applicationId, action) {
  try {
    jobsStatus.textContent = "Working";
    if (action === "draft") {
      await fetchJson(`/api/applications/${encodeURIComponent(applicationId)}/draft`, { method: "POST" });
    } else if (action === "review") {
      await fetchJson(`/api/applications/${encodeURIComponent(applicationId)}/review`, { method: "POST" });
    } else if (action === "auto") {
      await fetchJson(`/api/applications/${encodeURIComponent(applicationId)}/draft`, { method: "POST" });
      await fetchJson(`/api/applications/${encodeURIComponent(applicationId)}/approve`, { method: "POST" });
      await fetchJson(`/api/applications/${encodeURIComponent(applicationId)}/submit`, { method: "POST" });
    }
    await refreshState();
    showApplication(applicationId);
  } catch (error) {
    jobsStatus.textContent = "Error";
    jobsDetail.classList.add("empty-state");
    jobsDetail.textContent = error instanceof Error ? error.message : String(error);
  }
}

for (const element of [searchInput, scoreFilter, typeFilter, sourceFilter]) {
  element.addEventListener("input", () => renderJobs());
  element.addEventListener("change", () => renderJobs());
}

syncJobsButton.addEventListener("click", async () => {
  try {
    jobsStatus.textContent = "Syncing";
    await fetchJson("/api/sources/sync", { method: "POST" });
    await refreshState();
    setDetail("Sources synced. Open a job link or run auto-apply on a supported job.");
  } catch (error) {
    jobsStatus.textContent = "Error";
    jobsDetail.classList.add("empty-state");
    jobsDetail.textContent = error instanceof Error ? error.message : String(error);
  }
});

rebuildQueueButton.addEventListener("click", async () => {
  try {
    jobsStatus.textContent = "Rebuilding";
    await fetchJson("/api/queue/build", { method: "POST" });
    await refreshState();
    setDetail("Queue rebuilt from the latest profile and jobs.");
  } catch (error) {
    jobsStatus.textContent = "Error";
    jobsDetail.classList.add("empty-state");
    jobsDetail.textContent = error instanceof Error ? error.message : String(error);
  }
});

refreshState()
  .then(() => setDetail("Select a job to inspect it or use its application link."))
  .catch((error) => {
    jobsStatus.textContent = "Error";
    jobsDetail.textContent = error instanceof Error ? error.message : String(error);
  });
