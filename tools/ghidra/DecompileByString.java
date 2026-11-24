// Decompiles every function that uses a text of the executable matching a pattern,
// or the function at an address. Writes the C that Ghidra produces to a file.
// Usage (arguments after the script name):
//   -postScript DecompileByString.java <pattern or 0xADDRESS>... <output file>
// Each pattern is a Java regular expression searched in the defined strings (the
// .bat of Ghidra breaks on | and %: give several patterns instead of a|b)
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

import java.io.PrintWriter;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.regex.Pattern;

public class DecompileByString extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Set<Function> functions = new LinkedHashSet<>();
        for (int i = 0; i + 1 < args.length; i++) {
            collect(args[i], functions);
        }
        DecompInterface dec = new DecompInterface();
        dec.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(args[args.length - 1], "UTF-8")) {
            for (Function f : functions) {
                DecompileResults r = dec.decompileFunction(f, 120, monitor);
                out.println("// ===== " + f.getName() + " @ " + f.getEntryPoint());
                out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// failed: " + r.getErrorMessage());
            }
        }
        println("DECOMPILED " + functions.size() + " functions");
    }

    // Adds the function at an address, or every function using a matching text
    private void collect(String what, Set<Function> functions) throws Exception {
        if (what.startsWith("0x")) {
            Address a = toAddr(what);
            Function f = getFunctionContaining(a);
            if (f != null) {
                functions.add(f);
            }
        } else {
            Pattern p = Pattern.compile(what);
            DataIterator it = currentProgram.getListing().getDefinedData(true);
            while (it.hasNext()) {
                Data d = it.next();
                if (!d.hasStringValue()) {
                    continue;
                }
                String s = StringDataInstance.getStringDataInstance(d).getStringValue();
                if (s == null || !p.matcher(s).find()) {
                    continue;
                }
                for (Reference r : getReferencesTo(d.getAddress())) {
                    Function f = getFunctionContaining(r.getFromAddress());
                    if (f != null) {
                        functions.add(f);
                    }
                }
            }
        }
    }
}
