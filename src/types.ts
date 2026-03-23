export type Profile = {
  name: string;
  email: string;
  phone: string;
  location: string;
  targetRoles: string[];
  preferredLocations: string[];
  minimumSalaryUsd: number;
  workAuthorization: string;
  yearsOfExperience: number;
  skills: string[];
  resumeText: string;
};

export type Job = {
  id: string;
  title: string;
  company: string;
  location: string;
  salaryUsd?: number;
  employmentType: string;
  description: string;
  requirements: string[];
  preferredSkills?: string[];
  applicationUrl: string;
  source?: string;
  applicationType?: "easy_apply" | "external_form" | "manual_only";
  formConfigPath?: string;
};

export type CompatibilityReport = {
  score: number;
  verdict: "Strong match" | "Possible match" | "Weak match";
  matchedSkills: string[];
  missingSkills: string[];
  titleMatch: boolean;
  locationMatch: boolean;
  salaryMatch: boolean;
  explanation: string[];
};

export type ApplicationDraft = {
  fitSummary: string;
  coverLetter: string;
  tailoredHighlights: string[];
  screeningAnswers: Record<string, string>;
};

export type FormFieldConfig = {
  name: string;
  selector: string;
  valueSource: string;
};

export type FormConfig = {
  url: string;
  fields: FormFieldConfig[];
  submitSelector: string;
};

export type RawJobFeedItem = {
  source: string;
  externalId: string;
  title: string;
  company: string;
  location: string;
  salaryUsd?: number;
  employmentType: string;
  description: string;
  requirements: string[];
  preferredSkills?: string[];
  applicationUrl: string;
  applicationType: "easy_apply" | "external_form" | "manual_only";
  formConfigPath?: string;
};

export type ApplicationStatus =
  | "new"
  | "queued"
  | "draft_ready"
  | "approved"
  | "submitted"
  | "blocked"
  | "error";

export type ApplicationRecord = {
  id: string;
  jobId: string;
  compatibilityScore: number;
  compatibilityVerdict: CompatibilityReport["verdict"];
  status: ApplicationStatus;
  notes: string[];
  draft?: ApplicationDraft;
  createdAt: string;
  updatedAt: string;
};

export type BatchApplicationRunResult = {
  applicationId: string;
  jobId: string;
  title: string;
  company: string;
  outcome: "submitted" | "skipped" | "failed";
  reason: string;
  status: ApplicationStatus;
};

export type LocalDatabase = {
  jobs: Job[];
  applications: ApplicationRecord[];
  lastIngestedAt?: string;
};

export type SourceConfig =
  | {
      id: string;
      type: "greenhouse";
      boardToken: string;
      enabled: boolean;
    }
  | {
      id: string;
      type: "lever";
      site: string;
      enabled: boolean;
    }
  | {
      id: string;
      type: "workday";
      label: string;
      feedUrl: string;
      enabled: boolean;
    }
  | {
      id: string;
      type: "linkedin_manual";
      filePath: string;
      enabled: boolean;
    };
