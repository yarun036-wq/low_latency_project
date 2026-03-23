# Job Application Agent MVP

This project is a supervised job-application agent. It scores jobs against your resume/profile, explains whether a role is compatible, drafts tailored application material with OpenAI, and can fill a web application form with Playwright. It does not submit unless you explicitly approve it in the terminal.

## What it does

- Reads your profile from `data/profile.json`
- Reads candidate jobs from `data/jobs.json`
- Ingests normalized jobs from `data/job-feed.json`
- Produces a resume compatibility score for each job
- Explains missing skills, strengths, and risks
- Generates a tailored application draft with OpenAI when `OPENAI_API_KEY` is set
- Fills a web form from a selector config in `data/form-config.sample.json`
- Requires a manual approval before any submit action
- Stores jobs and application queue state in `storage/db.json`

## Safety model

- Submission is blocked by default until you answer `yes` in the CLI
- Browser automation runs only on the URL you provide
- The script is intended for your own applications and data only
- Review the generated answers before approving submission

## Setup

1. Copy `.env.example` to `.env`
2. Set `OPENAI_API_KEY`
3. Install dependencies:

```bash
npm install
```

4. Build:

```bash
npm run build
```

5. Run:

```bash
npm start -- --profile data/profile.json --jobs data/jobs.json
```

## Web dashboard

Start the local dashboard:

```bash
npm run start:web
```

Then open `http://localhost:3000` in your browser.

The dashboard shows:

- your current profile summary
- jobs ranked by resume compatibility
- an application queue with statuses like `queued`, `draft_ready`, `approved`, and `submitted`
- missing and matched skills
- the generated draft for the selected role

## Easy Apply style pipeline

The web UI now supports:

- `Ingest Jobs`: seed and normalize jobs into local storage
- `Sync Sources`: pull jobs from configured adapters in `data/sources.json`
- `Build Queue`: create application records based on resume compatibility
- `Generate Draft`: prepare the tailored application content for a queue item
- `Approve`: move an application into approved state
- `Submit`: submit an approved application

Application types:

- `easy_apply`: local submission flow without a browser form dependency
- `external_form`: uses a Playwright form config when available
- `manual_only`: kept visible but not auto-submitted

## Source adapters

Configured in `data/sources.json`:

- `greenhouse`: syncs from `https://boards-api.greenhouse.io/v1/boards/<board>/jobs?content=true`
- `lever`: syncs from `https://api.lever.co/v0/postings/<site>?mode=json`
- `workday`: syncs from a custom JSON feed URL you provide
- `linkedin_manual`: imports from a local JSON file instead of scraping LinkedIn directly

This keeps the pipeline stable while avoiding brittle login scraping as the default path.

## LinkedIn manual review

There is now a LinkedIn-specific manual review API path for Easy Apply style jobs.

- It opens LinkedIn in a persistent Playwright browser profile stored under `storage/linkedin-profile`
- It can reuse your LinkedIn login across sessions
- It attempts to open the Easy Apply modal and prefill basic fields like phone and email
- It stops before final submission so you can review and click yourself

## Optional form automation

To fill a specific application page:

```bash
npm start -- --profile data/profile.json --jobs data/jobs.json --job-id job-001 --apply --form-config data/form-config.sample.json
```

The terminal will show the compatibility result first, then ask for approval before the browser is allowed to submit.

## Data files

- `data/profile.json`: your resume profile, skills, constraints, and raw resume text
- `data/jobs.json`: jobs to evaluate
- `data/form-config.sample.json`: a sample selector map for browser automation

## Notes

- This is an MVP. Real job boards often have anti-bot controls, CAPTCHAs, and terms that may limit automation.
- The quality of matching and drafting depends heavily on the completeness of your profile data.
