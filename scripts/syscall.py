import re

with open("source/cxx/arch/x86/syscall.cpp") as f:
    match = re.search(r"static x86::syscall_handler syscalls_list[] = [^{]*\{([^}]*)}", f.read())
    syscall_text = match.group(1)

    syscall_list = syscall_text.splitlines()
    syscall_list = [s.strip().strip(',') for s in syscall_list if 
        "syscall_" in s.strip().strip(',') or
        "nullptr" in s.strip().strip(',')]
    
    mlibc_list = []

    for idx, line in enumerate(syscall_list):
        line = line.strip()
        line = line.strip(',')
        if line.startswith("syscall_"):
            syscall_name = line.removeprefix("syscall_")
            mlibc_list.append(f"#define SYS_{syscall_name} {idx}")
    
    print("\n".join(mlibc_list))