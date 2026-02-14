import re
import sys
import os

def audit_file(filepath):
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    interrupts_disabled = False

    # Simple state machine to track disable_interrupts() blocks.
    # This is very basic and won't handle complex control flow, but good for scanning.

    for i, line in enumerate(lines):
        sline = line.strip()

        # Check for disabling
        if "disable_interrupts()" in sline or "cpu_status state = disable_interrupts()" in sline:
            interrupts_disabled = True

        # Check for restoring
        if "restore_interrupts(" in sline:
            interrupts_disabled = False

        if interrupts_disabled:
            # Check for mutex_lock or other blocking calls
            if "mutex_lock(" in sline and not "mutex_unlock" in sline:
                 print(f"{filepath}:{i+1}: Potential mutex_lock while interrupts disabled")
            if "rw_lock_write_lock(" in sline:
                 print(f"{filepath}:{i+1}: Potential rw_lock_write_lock while interrupts disabled")
            if "rw_lock_read_lock(" in sline:
                 print(f"{filepath}:{i+1}: Potential rw_lock_read_lock while interrupts disabled")
            if "snooze(" in sline:
                 print(f"{filepath}:{i+1}: Potential snooze while interrupts disabled")

if __name__ == "__main__":
    for arg in sys.argv[1:]:
        if os.path.isfile(arg):
            audit_file(arg)
        elif os.path.isdir(arg):
            for root, dirs, files in os.walk(arg):
                for file in files:
                    if file.endswith('.cpp') or file.endswith('.c') or file.endswith('.h'):
                        audit_file(os.path.join(root, file))
