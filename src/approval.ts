import { stdin as input, stdout as output } from "node:process";
import { createInterface } from "node:readline/promises";

export async function requestApproval(prompt: string): Promise<boolean> {
  const rl = createInterface({ input, output });
  try {
    const answer = await rl.question(`${prompt} Type "yes" to continue: `);
    return answer.trim().toLowerCase() === "yes";
  } finally {
    rl.close();
  }
}
