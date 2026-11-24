// Decompiles every function of the executable and keeps those whose C code
// matches all the patterns given (to find, for example, the functions that
// read some fields of a structure). Writes their C to a file. Slow: a few
// minutes for the whole executable.
// Usage (arguments after the script name):
//   -postScript DecompileByCode.java <pattern>... <output file>
// Each pattern is a Java regular expression searched in the C code. Use \\x2b
// for + and \\x28 \\x29 for brackets if the .bat of Ghidra breaks them (it
// breaks on | and %)
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;

public class DecompileByCode extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        List<Pattern> patterns = new ArrayList<>();
        for (int i = 0; i + 1 < args.length; i++) {
            patterns.add(Pattern.compile(args[i]));
        }
        DecompInterface dec = new DecompInterface();
        dec.openProgram(currentProgram);
        int found = 0;
        try (PrintWriter out = new PrintWriter(args[args.length - 1], "UTF-8")) {
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            while (it.hasNext() && !monitor.isCancelled()) {
                Function f = it.next();
                if (f.isThunk() || f.isExternal()) {
                    continue;
                }
                DecompileResults r = dec.decompileFunction(f, 60, monitor);
                if (!r.decompileCompleted()) {
                    continue;
                }
                String c = r.getDecompiledFunction().getC();
                boolean all = true;
                for (Pattern p : patterns) {
                    if (!p.matcher(c).find()) {
                        all = false;
                        break;
                    }
                }
                if (all) {
                    out.println("// ===== " + f.getName() + " @ " + f.getEntryPoint());
                    out.println(c);
                    found++;
                }
            }
        }
        println("FOUND " + found + " functions");
    }
}
