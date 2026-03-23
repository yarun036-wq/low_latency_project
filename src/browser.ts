import { chromium } from "playwright";
import path from "node:path";

import type { ApplicationDraft, FormConfig, Profile } from "./types.js";

function resolveValue(path: string, profile: Profile, draft: ApplicationDraft): string {
  const [root, ...rest] = path.split(".");
  const source = root === "profile" ? (profile as Record<string, unknown>) : (draft as Record<string, unknown>);
  let current: unknown = source;
  for (const key of rest) {
    if (typeof current !== "object" || current === null) {
      return "";
    }
    current = (current as Record<string, unknown>)[key];
  }
  if (Array.isArray(current)) {
    return current.join("\n");
  }
  if (typeof current === "object" && current !== null) {
    return JSON.stringify(current);
  }
  return String(current ?? "");
}

export async function fillApplicationForm(
  formConfig: FormConfig,
  profile: Profile,
  draft: ApplicationDraft,
  submit: boolean
): Promise<void> {
  const browser = await chromium.launch({ headless: false });
  const page = await browser.newPage();

  try {
    await page.goto(formConfig.url, { waitUntil: "domcontentloaded" });

    for (const field of formConfig.fields) {
      const value = resolveValue(field.valueSource, profile, draft);
      await page.locator(field.selector).fill(value);
    }

    if (submit) {
      await page.locator(formConfig.submitSelector).click();
    }
  } finally {
    await browser.close();
  }
}

export async function launchApplicationReview(
  url: string,
  profile: Profile,
  draft: ApplicationDraft,
  formConfig?: FormConfig
): Promise<void> {
  const browser = await chromium.launch({ headless: false });
  const page = await browser.newPage();

  await page.goto(formConfig?.url ?? url, { waitUntil: "domcontentloaded" });

  if (formConfig) {
    for (const field of formConfig.fields) {
      const value = resolveValue(field.valueSource, profile, draft);
      await page.locator(field.selector).fill(value);
    }
  }

  await page.bringToFront();
}

async function fillIfVisible(page: import("playwright").Page, selectors: string[], value: string): Promise<boolean> {
  for (const selector of selectors) {
    const locator = page.locator(selector).first();
    if (await locator.count()) {
      try {
        await locator.fill(value);
        return true;
      } catch {
        // Ignore fields that are present but not editable in the current step.
      }
    }
  }
  return false;
}

export async function launchLinkedInEasyApplyReview(url: string, profile: Profile): Promise<void> {
  const userDataDir = path.resolve("storage", "linkedin-profile");
  const context = await chromium.launchPersistentContext(userDataDir, {
    headless: false
  });

  const page = context.pages()[0] ?? await context.newPage();
  await page.goto(url, { waitUntil: "domcontentloaded" });
  await page.bringToFront();

  const easyApplyButton = page.getByRole("button", { name: /easy apply/i }).first();
  if (await easyApplyButton.count()) {
    try {
      await easyApplyButton.click();
      await page.waitForLoadState("domcontentloaded");
    } catch {
      // LinkedIn may render the button but block interaction until login or extra checks complete.
    }
  }

  await fillIfVisible(
    page,
    [
      "input[id*='phoneNumber']",
      "input[name*='phoneNumber']",
      "input[id*='phone-number']",
      "input[name*='phone-number']",
      "input[type='tel']"
    ],
    profile.phone
  );

  await fillIfVisible(
    page,
    [
      "input[id*='email']",
      "input[name*='email']",
      "input[type='email']"
    ],
    profile.email
  );
}
