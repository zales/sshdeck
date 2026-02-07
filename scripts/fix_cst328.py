Import("env")
import os

def fix_cst328_constants():
    """Fix missing #endif in CSE_CST328_Constants.h (library bug in v0.1)"""
    libdeps = os.path.join(env["PROJECT_LIBDEPS_DIR"], env["PIOENV"])
    filepath = os.path.join(libdeps, "CSE_CST328", "src", "CSE_CST328_Constants.h")
    
    if not os.path.exists(filepath):
        return
    
    with open(filepath, "r") as f:
        content = f.read()
    
    # Count #ifndef vs #endif to check if balanced
    ifndef_count = content.count("#ifndef")
    endif_count = content.count("#endif")
    
    if endif_count >= ifndef_count:
        return  # Already balanced
    
    with open(filepath, "a") as f:
        f.write("\n#endif // CSE_CST328_CONSTANTS_H\n")
    
    print("*** Patched CSE_CST328_Constants.h: added missing #endif")

# Run immediately when script is loaded (before any compilation)
fix_cst328_constants()
