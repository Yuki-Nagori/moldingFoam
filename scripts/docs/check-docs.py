#!/usr/bin/env python3
"""ai-docs health check: index coverage, header fields, rotting references.

Checks (each failure is a maintenance defect, not a style opinion):

1. every ai-docs/tasks/*.md is listed in ai-docs/README.md;
2. every task file carries a 状态 field (the repo convention);
3. no `file.C:123` line-number references to files of this repository:
   line numbers rot with every edit, a symbol name does not (see
   ai-docs/README.md conventions); upstream OpenFOAM references are fine;
4. no `tests/cases/<name>` reference to a case that does not exist;
5. no reference to a README section that does not exist
   ("README 第 N 节", "README §N").

Usage: python3 scripts/docs/check-docs.py [repoRoot]   (manual tool;
       exits non-zero on findings)
"""
import os
import re
import sys

root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")
docs = os.path.join(root, "ai-docs")
tasks = os.path.join(docs, "tasks")

# every source file of this repository, by basename (line refs to these rot)
own_sources = set()
for dirpath, _, files in os.walk(os.path.join(root, "src")):
    for f in files:
        own_sources.add(f)

index = open(os.path.join(docs, "README.md")).read()
readme = open(os.path.join(root, "README.md")).read()
readme_sections = set(
    re.findall(r"(?m)^##+\s+(\d+(?:\.\d+)*)\.", readme))

problems = []

task_files = sorted(f for f in os.listdir(tasks) if f.endswith(".md"))
doc_files = ([(os.path.join("tasks", n), os.path.join(tasks, n))
              for n in task_files]
             + [(n, os.path.join(docs, n))
                for n in sorted(os.listdir(docs)) if n.endswith(".md")])

for name, path in doc_files:
    text = open(path).read()
    is_task = name.startswith("tasks/") or (
        not os.path.dirname(name) and name[:3].isdigit())

    if not is_task:
        # reference documents (audits, reports, uncertainty) need no header
        # fields, but must be linked from the index
        if os.path.basename(name) == "README.md":
            continue
        if os.path.basename(name) not in index:
            problems.append("%s: not linked from ai-docs/README.md" % name)
        continue

    if name not in index:
        problems.append("%s: not listed in ai-docs/README.md" % name)

    if not re.search(r"(?m)^-\s*状态[：:]", text):
        problems.append("%s: missing the 状态 header field" % name)

    for m in re.finditer(r"[\w/]+\.(?:C|H|py|sh|lua):\d+", text):
        ref = m.group(0)
        if os.path.basename(ref.split(":")[0]) in own_sources:
            problems.append("%s: line-number reference '%s' (use a symbol)"
                            % (name, ref))

    for m in re.finditer(r"README\s*(?:第\s*(\d+)\s*节|§\s*(\d+))", text):
        section = m.group(1) or m.group(2)
        if section not in readme_sections:
            problems.append("%s: README section %s does not exist"
                            % (name, section))

    for m in re.finditer(r"tests/cases/([A-Za-z0-9_]+)", text):
        case = m.group(1)
        if not os.path.isdir(os.path.join(root, "tests", "cases", case)):
            problems.append("%s: references missing case tests/cases/%s"
                            % (name, case))

# every task should also be reachable from the index by its number
for name in task_files:
    num = name.split("-")[0]
    if num not in index and not num.rstrip("ab") in index:
        problems.append("%s: task number %s missing from the index"
                        % (name, num))

if problems:
    print("ai-docs check: %d finding(s)" % len(problems))
    for p in sorted(set(problems)):
        print("  - " + p)
    sys.exit(1)

print("ai-docs check: OK (%d task files, %d index rows)"
      % (len(task_files), len(re.findall(r"(?m)^\| \[", index))))
