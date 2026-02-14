import os
import re

def audit_allocations(root_dir):
    allocation_funcs = r'\b(malloc|calloc|realloc|strdup|new|new\s*\(std::nothrow\))\b'

    findings = []

    if os.path.isfile(root_dir):
        files = [root_dir]
    else:
        files = []
        for dirpath, _, filenames in os.walk(root_dir):
            for filename in filenames:
                if filename.endswith(('.c', '.cpp', '.h')):
                    files.append(os.path.join(dirpath, filename))

    for filepath in files:
        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
        except Exception as e:
            continue

        for i, line in enumerate(lines):
            match = re.search(allocation_funcs, line)
            if match:
                has_check = False
                context = "".join(lines[i:i+5])

                if "new (std::nothrow)" in line or "new(std::nothrow)" in line:
                        pass
                elif "new" in match.group(0) and "std::nothrow" not in line:
                        pass

                if "=" in line:
                    var_name_match = re.search(r'(\w+)\s*=', line)
                    if var_name_match:
                        var_name = var_name_match.group(1)
                        if var_name in context and ("if" in context or "NULL" in context):
                            has_check = True

                if re.search(r'if\s*\(.*' + match.group(0), line) or "return" in line:
                    has_check = True

                if not has_check:
                    findings.append(f"{filepath}:{i+1}: {line.strip()}")

    return findings

if __name__ == "__main__":
    import sys
    import glob
    target = sys.argv[1]
    if '*' in target:
        files = glob.glob(target)
        for f in files:
            results = audit_allocations(f)
            for r in results:
                print(r)
    else:
        results = audit_allocations(target)
        for r in results:
            print(r)
