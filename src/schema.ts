import { z } from "zod";

export const profileSchema = z.object({
  name: z.string().min(1),
  email: z.string().min(3),
  phone: z.string().min(3),
  location: z.string().min(1),
  targetRoles: z.array(z.string()).min(1),
  preferredLocations: z.array(z.string()).min(1),
  minimumSalaryUsd: z.number().nonnegative(),
  workAuthorization: z.string().min(1),
  yearsOfExperience: z.number().nonnegative(),
  skills: z.array(z.string()).min(1),
  resumeText: z.string().min(20)
});

export const jobSchema = z.object({
  id: z.string().min(1),
  title: z.string().min(1),
  company: z.string().min(1),
  location: z.string().min(1),
  salaryUsd: z.number().nonnegative().optional(),
  employmentType: z.string().min(1),
  description: z.string().min(20),
  requirements: z.array(z.string()).min(1),
  preferredSkills: z.array(z.string()).optional(),
  applicationUrl: z.string().url()
});

export const jobsSchema = z.array(jobSchema).min(1);

export const formConfigSchema = z.object({
  url: z.string().url(),
  fields: z.array(
    z.object({
      name: z.string().min(1),
      selector: z.string().min(1),
      valueSource: z.string().min(1)
    })
  ),
  submitSelector: z.string().min(1)
});
