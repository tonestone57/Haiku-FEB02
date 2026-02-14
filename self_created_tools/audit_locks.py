import re
import sys
import os

def audit_file(filepath):
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    patterns = [
        (r'mutex_lock\s*\(&(\w+)\)', 'mutex_lock'),
        (r'rw_lock_write_lock\s*\(&(\w+)\)', 'rw_lock_write_lock'),
        (r'rw_lock_read_lock\s*\(&(\w+)\)', 'rw_lock_read_lock'),
        (r'recursive_lock_lock\s*\(&(\w+)\)', 'recursive_lock_lock'),
        (r'acquire_sem\s*\((\w+)\)', 'acquire_sem'),
        (r'acquire_sem_etc\s*\((\w+)', 'acquire_sem_etc'),
    ]

    for i, line in enumerate(lines):
        line = line.strip()
        # Skip comments
        if line.startswith('//') or line.startswith('*'):
            continue

        for pattern, func_name in patterns:
            # We want to catch cases where the return value is NOT checked or assigned.
            # e.g. "mutex_lock(&lock);" vs "if (mutex_lock(&lock) ...)" or "status = mutex_lock..."

            # Simple heuristic: if the line starts with the function call, it's likely ignored.
            if line.startswith(func_name):
                # Ensure it's not a cast or something
                match = re.match(pattern, line)
                if match:
                    # It seems to be a direct call.
                    # Check if it is "void" cast? (void)mutex_lock...
                    if "(void)" in line:
                        continue

                    print(f"{filepath}:{i+1}: Potential ignored return value of '{func_name}'")

if __name__ == "__main__":
    for arg in sys.argv[1:]:
        if os.path.isfile(arg):
            audit_file(arg)
        elif os.path.isdir(arg):
            for root, dirs, files in os.walk(arg):
                for file in files:
                    if file.endswith('.cpp') or file.endswith('.c') or file.endswith('.h'):
                        audit_file(os.path.join(root, file))
