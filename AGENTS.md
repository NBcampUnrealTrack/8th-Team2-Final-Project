<!-- revvy:shared-memory:start -->
Shared memory for ALL agents (Claude/Codex/Kimi) lives at `.revvy/shared-memory.md`.
Read it at session start. When you make a durable decision, adopt a convention,
or leave work half-done, append a dated one-line entry there so the next agent
(possibly a different model) picks up where you left off.
<!-- revvy:shared-memory:end -->

For ProjectM menu/popup UI layout work, read `Tools/UILayoutContract/SKILL.md` first.
It defines the shared panel origin, header/content insets, footer button sizing,
hub row ownership, and runtime visual checks. Its final authoring pass supersedes
older per-screen positioning scripts; preserve the scope of the user's request.
