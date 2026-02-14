import re
import sys
import os

def audit_file(filepath):
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    errors = []

    # Patterns to match allocations and resource creations
    # We look for: var = func(...)
    # and then check if the next lines check for NULL or error

    patterns = [
        (r'(\w+)\s*=\s*(?:new(?:\(std::nothrow\)|(?:\(nothrow\)))?)\s+\w+', 'new(nothrow)'),
        (r'(\w+)\s*=\s*malloc\(', 'malloc'),
        (r'(\w+)\s*=\s*calloc\(', 'calloc'),
        (r'(\w+)\s*=\s*strdup\(', 'strdup'),
        (r'(\w+)\s*=\s*spawn_kernel_thread\(', 'spawn_kernel_thread'),
        (r'(\w+)\s*=\s*create_area\(', 'create_area'),
        (r'(\w+)\s*=\s*create_sem\(', 'create_sem'),
        (r'(\w+)\s*=\s*create_port\(', 'create_port'),
    ]

    for i, line in enumerate(lines):
        line = line.strip()
        # Skip comments
        if line.startswith('//') or line.startswith('*'):
            continue

        for pattern, func_name in patterns:
            match = re.search(pattern, line)
            if match:
                var_name = match.group(1)

                # Check next few lines for a check
                # We look for "if (var" or "if (!var" or "if (var < 0" or "var < B_OK"

                found_check = False
                for j in range(1, 6): # Check next 5 lines
                    if i + j >= len(lines):
                        break
                    next_line = lines[i+j].strip()

                    # Heuristic checks
                    if f"if ({var_name}" in next_line: found_check = True
                    if f"if (!{var_name}" in next_line: found_check = True
                    if f"if ( {var_name}" in next_line: found_check = True
                    if f"if ( !{var_name}" in next_line: found_check = True

                    # For integer returns (create_area, etc), check for < 0 or < B_OK
                    if func_name in ['spawn_kernel_thread', 'create_area', 'create_sem', 'create_port']:
                        if f"{var_name} < 0" in next_line: found_check = True
                        if f"{var_name} < B_OK" in next_line: found_check = True

                    # Stop if we hit another assignment or function call that might look like usage
                    if "=" in next_line and var_name in next_line.split('=')[0]:
                         # Re-assignment?
                         pass

                    if found_check:
                        break

                if not found_check:
                    # Double check if it is returned immediately
                    for j in range(1, 4):
                        if i + j >= len(lines): break
                        if f"return {var_name};" in lines[i+j]:
                            found_check = True
                            break

                if not found_check:
                    # Ignore if it's in a helper function or complex expression we missed
                    # But report it for review
                    print(f"{filepath}:{i+1}: Potential missing check for '{var_name}' after {func_name}")

if __name__ == "__main__":
    for arg in sys.argv[1:]:
        if os.path.isfile(arg):
            audit_file(arg)
        elif os.path.isdir(arg):
            for root, dirs, files in os.walk(arg):
                for file in files:
                    if file.endswith('.cpp') or file.endswith('.c') or file.endswith('.h'):
                        audit_file(os.path.join(root, file))
