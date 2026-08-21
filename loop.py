import subprocess
import time

PROMPT = """
Please refer to PLAN.md and PROGRESS.md, then continue working.

Work autonomously on the highest priority task. Make changes, run appropriate
tests / builds, and update PROGRESS.md with your progress before finishing.
"""

try:
    while True:
        subprocess.run(
            [
                "cmd.exe",
                "/c",
                "codex",
                "--approve-for-me",
                "exec",
                "-",
            ],
            input=PROMPT,
            text=True,
        )

        print("\nStarting next iteration...\n")
        time.sleep(5)

except KeyboardInterrupt:
    print("\nLoop stopped.")