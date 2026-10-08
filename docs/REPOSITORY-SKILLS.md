# Skills and development workflow

The user requested that installed skills become part of the development workflow. On 2026-10-07, the five new project installations below were read locally and their source records checked in `skills-lock.json`. The repository's [agent instructions](../AGENTS.md) define when to apply them. Read the installed skill before using it and load its task-specific references as needed.

## New project skills

| Skill | Source | Use in Software Fuser |
| --- | --- | --- |
| `cpp-pro` | [Jeffallan/claude-skills](https://github.com/Jeffallan/claude-skills/blob/main/skills/cpp-pro/SKILL.md) | Review allocation, ownership, concurrency and build settings; profile representative workloads before and after a performance change. |
| `microsoft-docs` | [microsoft/skills](https://github.com/microsoft/skills/blob/main/.github/skills/microsoft-docs/SKILL.md) | Retrieve current official documentation for Game Bar, Windows graphics behavior and profiling. |
| `code-review` | [mattpocock/skills](https://github.com/mattpocock/skills/blob/main/skills/engineering/code-review/SKILL.md) | Review a defined feature/branch diff against repository standards and the originating requirements separately. |
| `codebase-design` | [mattpocock/skills](https://github.com/mattpocock/skills/blob/main/skills/engineering/codebase-design/SKILL.md) | Design small module interfaces with explicit ownership, ordering and error requirements; test through those interfaces. |
| `agent-browser` | [vercel-labs/agent-browser](https://github.com/vercel-labs/agent-browser/blob/main/skills/agent-browser/SKILL.md) | Exercise the source HUD page and browser flows when its CLI is available. Read the version-matched CLI guide first. |

The existing C++ coding standards, systematic debugging and verification skills remain part of the workflow. The installed `pr-review` is written for microsoft/win-dev-skills; use the new `code-review` for this project's feature reviews.

## Performance workflow

1. Establish the symptom and a representative baseline using systematic debugging. Record the source workload, codec, bitrate, refresh rates and concurrent streams.
2. Consult official Windows documentation for the behavior being investigated. Use the Microsoft Learn tools or the documentation skill's CLI fallback when available.
3. Use the C++ and design skills to evaluate the measured hot path. Preserve decoded GPU frame lifetimes, encoded reference order, required context synchronization and retained colors.
4. Make a scoped change and compare the same workload before and after it. Report means and tail timings with their measurement scope.
5. Review substantial feature changes against both standards and requirements using the known comparison base and task spec.
6. Run applicable checks, inspect the results, audit the staged files, and commit/push the authorized checkpoint.

Transport arrival, decoder submission, draw/Present timing, actual displayed frames and optical latency are different measurements. CPU timing or successful Present submission alone cannot establish displayed frame rate or end-to-end latency. Browser tests cannot establish native Game Bar presentation behavior. Keep disruptive live tests and GPU-heavy benchmarks out of active gameplay.

Adapt generic C++ compiler and sanitizer examples to the actual Windows/UWP or portable-core toolchain. Record unavailable checks rather than claiming they ran. Review reference code before adopting it, especially allocation and lock-free examples.

## Documentation and release skills

These skills were researched through skills.sh, the skills CLI and upstream source on 2026-10-06. Their local installations remain recorded in the project skill lock.

| Skill | Source | Use in Software Fuser |
| --- | --- | --- |
| `readme-blueprint-generator` | [github/awesome-copilot](https://github.com/github/awesome-copilot/blob/main/skills/readme-blueprint-generator/SKILL.md) | Keep the README concise, based on this repository's architecture, setup and verified behavior. Adapt the skill's Copilot-specific inputs to the existing docs. |
| `changelog-automation` | [wshobson/agents](https://github.com/wshobson/agents/blob/main/plugins/documentation-generation/skills/changelog-automation/SKILL.md) | Group meaningful user-facing release changes and verify each claim. Keep Windows four-part package versions and the existing release process. |

GitHub Actions templates remains an optional recommendation, not a verified local installation. Installing a skill does not adopt its example release automation or change the project's versioning policy.

## Local availability and publication

`skills-lock.json` records upstream sources, source paths and computed content hashes. Local skill files remain under ignored `.agents/`; tool dependencies must be checked separately. A lock entry is not evidence that a CLI, profiler or live test is available or has run.

Private host addresses, user paths, PINs, credentials, captures and runtime logs stay under ignored locations. Skill installation does not authorize unrelated deployment, shared-history rewriting or messages to other people. Preserve existing user authorization and other agents' active worktrees.

The repository includes contributor guidance, generated/private-file ignore patterns, a publication/link audit and CPU-only core CI. Historical experiments remain in [development history](DEVELOPMENT-HISTORY.md). See [contributing](../CONTRIBUTING.md) for validation commands and their scope.
