# Repository maintenance skills

Researched on 2026-10-06 through skills.sh, the skills CLI and current upstream skill files. These are recommendations; this cleanup does not install additional skills.

| Skill | Fit for this project | skills.sh installs |
| --- | --- | --- |
| [README blueprint generator](https://www.skills.sh/github/awesome-copilot/readme-blueprint-generator) | GitHub-maintained README structure; adapt its Copilot-specific inputs to this repository's docs. | About 9.7K |
| [Changelog automation](https://www.skills.sh/wshobson/agents/changelog-automation) | Release notes and change grouping. Keep Windows four-part package versions; adopting automated semantic releases is a separate decision. | About 13K |
| [GitHub Actions templates](https://www.skills.sh/wshobson/agents/github-actions-templates) | Matrix build/test workflows. Adapt examples to the portable C++ core and verify current action revisions. | About 16.6K |

The README skill's source is [github/awesome-copilot](https://github.com/github/awesome-copilot/blob/main/skills/readme-blueprint-generator/SKILL.md), a repository maintained by GitHub with about 40K stars. The two community skills are maintained in [wshobson/agents](https://github.com/wshobson/agents), also with about 40K stars; their current [changelog](https://github.com/wshobson/agents/blob/main/plugins/documentation-generation/skills/changelog-automation/SKILL.md) and [Actions](https://github.com/wshobson/agents/blob/main/plugins/cicd-automation/skills/github-actions-templates/SKILL.md) source files were verified. Counts change over time.

## Optional installation

Run these only when choosing to add the skills to your agent environment:

```text
npx skills add github/awesome-copilot --skill readme-blueprint-generator
npx skills add wshobson/agents --skill changelog-automation
npx skills add wshobson/agents --skill github-actions-templates
```

Read the installed skill before applying it. Its example commands and release automation should be adapted to the task; installing a skill does not authorize deployment, shared-history rewriting or messages to other people.

## Current maintenance checks

The repository includes a short README, changelog, contributor guide, editor/line-ending rules, generated/private-file ignore patterns, a read-only publication/link audit, and CPU-only core CI. Historical experiments remain in [development history](DEVELOPMENT-HISTORY.md). Active worktrees and the testing branch are retained.

See [contributing](../CONTRIBUTING.md) for the commands and the scope of each check.
