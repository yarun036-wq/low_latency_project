type ParsedArgs = {
  profilePath: string;
  jobsPath: string;
  jobId?: string;
  apply: boolean;
  formConfigPath?: string;
};

export function parseArgs(argv: string[]): ParsedArgs {
  const parsed: ParsedArgs = {
    profilePath: "data/profile.json",
    jobsPath: "data/jobs.json",
    apply: false
  };

  for (let index = 0; index < argv.length; index += 1) {
    const current = argv[index];
    const next = argv[index + 1];

    if (current === "--profile" && next) {
      parsed.profilePath = next;
      index += 1;
      continue;
    }
    if (current === "--jobs" && next) {
      parsed.jobsPath = next;
      index += 1;
      continue;
    }
    if (current === "--job-id" && next) {
      parsed.jobId = next;
      index += 1;
      continue;
    }
    if (current === "--form-config" && next) {
      parsed.formConfigPath = next;
      index += 1;
      continue;
    }
    if (current === "--apply") {
      parsed.apply = true;
    }
  }

  return parsed;
}
