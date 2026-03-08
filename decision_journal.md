# Decision Journal — Mini 1: Memory Overload

Every design choice, including failed attempts, is logged with rationale and data.

---

### 2026-03-07 Decision: Compiler Selection

**Context**: Need a compiler with OpenMP support on macOS ARM64.
**Options considered**: Apple Xcode Clang (no OpenMP), GCC 13, LLVM Clang via Homebrew.
**Decision**: LLVM Clang 22.1.0 via Homebrew (`/opt/homebrew/opt/llvm/bin/clang++`).
**Rationale**: Assignment requires non-Xcode compiler. LLVM Clang has native OpenMP support and detects 14 threads on this machine. GCC 13 also works but LLVM is recommended by the course.
**Result**: OpenMP verified working with 14 threads.
**Status**: Succeeded

---

### 2026-03-07 Decision: Date Parsing Strategy

**Context**: CSV date fields arrive in ISO 8601 format (`2026-03-06T02:53:58.000`) in the API sample, but the full download may use `MM/DD/YYYY HH:MM:SS AM/PM` format.
**Options considered**: (1) Support only ISO 8601, (2) Support only MM/DD/YYYY, (3) Auto-detect and support both.
**Decision**: Auto-detect format and support both.
**Rationale**: The sample CSV uses ISO 8601 but the bulk download endpoint may differ. A flexible parser costs minimal complexity and avoids data-dependent failures.
**Result**: TBD — implementing now.
**Status**: In progress

---
