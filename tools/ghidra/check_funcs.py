# check_funcs.py -- Ghidra script (PyGhidra): do these addresses resolve to
# defined functions, and how big are they? Args: comma-separated hex addresses.
# Used to prove a BSim result measures BSim, not our import quality.
# @runtime PyGhidra
args = getScriptArgs()
fm = currentProgram.getFunctionManager()
af = currentProgram.getAddressFactory()
print("CHECK program=%s functions=%d" % (currentProgram.getName(), fm.getFunctionCount()))
for tok in args[0].split(","):
    a = af.getAddress(tok.strip())
    f = fm.getFunctionContaining(a)
    if f is None:
        print("CHECK %s -> NO FUNCTION" % tok)
    else:
        print("CHECK %s -> %s entry=%s size=%d %s" % (
            tok, f.getName(), f.getEntryPoint(), f.getBody().getNumAddresses(),
            "EXACT" if f.getEntryPoint().toString() == tok.replace("0x","").lower() else "interior"))
