# Agent workflow

Read [CONTRIBUTING.md](CONTRIBUTING.md) before making changes. Use the worktree and branch authorized for the task; preserve other agents' changes and active testing worktrees.

## Apply relevant skills

The user wants installed skills used during development. Consult `skills-lock.json` and the available skill catalog. Read the applicable `SKILL.md` before first use, and load its references when the task needs them. Project installations live under `.agents/skills/<name>/`; that directory stays ignored. A lock entry alone does not prove a skill or its tools are available locally.

- **C++ changes and optimization:** use `cpp-pro` with `cpp-coding-standards`. Review ownership, allocation, concurrency and compiler settings. Profile representative workloads before changing a hot path and measure afterward. Adapt compiler flags and sanitizer procedures to the actual Windows/UWP or portable-core toolchain; record unsupported checks. Review example code before adopting it.
- **Unexpected behavior or performance:** use `systematic-debugging`. Establish a reproduction, instrument the relevant module interfaces, and distinguish observed evidence from a proposed cause before applying a fix.
- **Windows behavior and limits:** use `microsoft-docs` for current official Game Bar, DXGI, D3D11 and Windows profiling documentation. Discover available documentation tools; use the skill's CLI fallback when needed and available. Do not claim a lookup occurred without reading its result.
- **Interface design and refactoring:** use `codebase-design`. Keep modules behind small interfaces, make ownership and ordering requirements explicit, and test behavior through the interface. Introduce a seam when a dependency actually needs to vary.
- **Feature or branch reviews:** use `code-review` with an explicit comparison base and the originating requirements. Use the task's known base/spec when provided; follow the skill's separate standards and specification reviews, including its parallel reviewers. The installed `pr-review` targets the win-dev-skills repository and is not the default review workflow here.
- **Browser work:** use `agent-browser` when its CLI is available and appropriate for source HUD page testing or website interaction, subject to higher-priority tool instructions. Load the installed CLI's `skills get core` guide before running browser operations. It does not verify native UWP/Game Bar presentation or monitor scanout.
- **Documentation and releases:** use `readme-blueprint-generator` and `changelog-automation` when their tasks apply. Base documentation on this repository's source and verified behavior. Retain Windows four-part package versions and the current release process unless the user requests a change.
- **Every completion or checkpoint:** use `verification-before-completion`. Run the relevant checks afresh, inspect their full results, and state the scope and remaining limitations accurately.

Skill installation does not authorize unrelated tool installation, deployment, remote input, messages, or changes to the user's active sessions. Follow the user's existing authorization without repeatedly asking for it.

## Video and performance requirements

- Keep video presentation inside the Xbox Game Bar UWP widget unless the user explicitly approves another renderer.
- Preserve encoded frame reference order, decoded GPU frame lifetimes, and required D3D11 immediate-context synchronization.
- Preserve retained colors and keying behavior when optimizing. Compare quality as well as timing.
- Separate transport arrival, decoder submission, draw/Present calls, actual displayed frames and optical latency. A faster CPU call or accepted Present is not proof of lower end-to-end latency or displayed frame rate.
- Keep GPU-heavy benchmarks and disruptive live tests out of active gameplay. Record the source workload, codec, bitrate, refresh rates and concurrent streams for controlled comparisons; keep private runtime evidence ignored.

## Publish checkpoints

Keep changes scoped. Run the checks in CONTRIBUTING.md that apply to the change, inspect the intended staged diff, and run the staged publication audit. Commit and push authorized checkpoints, then verify the remote branch matches the local commit. Keep personal data, keys, pairing state, captures, logs and build outputs out of Git.

See [skills and workflow details](docs/REPOSITORY-SKILLS.md).
