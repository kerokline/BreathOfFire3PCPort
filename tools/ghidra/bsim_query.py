# bsim_query.py -- Ghidra script (PyGhidra): query functions of the current
# program against a BSim database and dump the ranked matches as JSON.
#
# Run headless by tools/bsim_probe.py. Args:
#   getScriptArgs()[0]  BSim database URL (file:/D:/.../bof3psx)
#   getScriptArgs()[1]  output .json path
#   getScriptArgs()[2]  comma-separated function addresses in the CURRENT
#                       program, e.g. 0x497740,0x4976d0
#
# Output: {"query": <program>, "results": [{addr, name, matches: [
#   {exe, name, addr, similarity, significance}, ...]}]}
#
# Thresholds are deliberately floored at 0 — this is a validation run and the
# question is where the true counterpart RANKS, not whether it clears a bar.
# @runtime PyGhidra
import json

import ghidra.features.bsim.query.BSimClientFactory as BSimClientFactory
import ghidra.features.bsim.query.GenSignatures as GenSignatures
import ghidra.features.bsim.query.protocol.QueryNearest as QueryNearest

MATCHES_PER_FUNC = 50

args = getScriptArgs()
db_url, out_path, addr_list = args[0], args[1], args[2]

url = BSimClientFactory.deriveBSimURL(db_url)
database = BSimClientFactory.buildClient(url, False)
if not database.initialize():
    print("BSIM ERROR: %s" % database.getLastError().message)
    raise SystemExit(1)

gensig = GenSignatures(False)
gensig.setVectorFactory(database.getLSHVectorFactory())
gensig.openProgram(currentProgram, None, None, None, None, None)

fm = currentProgram.getFunctionManager()
wanted = []
for tok in addr_list.split(","):
    tok = tok.strip()
    if not tok:
        continue
    a = currentProgram.getAddressFactory().getAddress(tok)
    f = fm.getFunctionContaining(a)
    if f is None:
        print("BSIM WARN: no function at %s" % tok)
        continue
    if f.getEntryPoint() != a:
        print("BSIM WARN: %s is interior to %s" % (tok, f.getEntryPoint()))
    gensig.scanFunction(f)
    wanted.append((tok, f))
print("BSIM: scanned %d functions" % len(wanted))

q = QueryNearest()
q.manage = gensig.getDescriptionManager()
q.max = MATCHES_PER_FUNC
q.thresh = 0.0
q.signifthresh = 0.0

response = database.query(q)
if response is None:
    print("BSIM ERROR: %s" % database.getLastError().message)
    raise SystemExit(1)

out = {"query": currentProgram.getName(), "results": []}
it = response.result.iterator()
while it.hasNext():
    sim = it.next()
    base = sim.getBase()
    rec = {"addr": "0x%s" % base.getAddress(), "name": base.getFunctionName(),
           "matches": []}
    sub = sim.iterator()
    while sub.hasNext():
        note = sub.next()
        fd = note.getFunctionDescription()
        rec["matches"].append({
            "exe": fd.getExecutableRecord().getNameExec(),
            "name": fd.getFunctionName(),
            "addr": "0x%X" % fd.getAddress(),
            "similarity": note.getSimilarity(),
            "significance": note.getSignificance()})
    out["results"].append(rec)

with open(out_path, "w") as fh:
    json.dump(out, fh, indent=1)
print("BSIM: wrote %s (%d base functions)" % (out_path, len(out["results"])))
gensig.dispose()
database.close()
