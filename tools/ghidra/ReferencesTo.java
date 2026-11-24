// Lists what refers to some addresses of the executable: code that calls or
// reads them, and data that points to them (tables of functions, objects'
// tables of methods). For data, it also says which function, if any, refers to
// the start of that table. Finds who uses a function through a pointer, which
// searching the decompiled code does not show.
// Usage (arguments after the script name):
//   -postScript ReferencesTo.java <0xADDRESS>... <output file>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

import java.io.PrintWriter;

public class ReferencesTo extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        try (PrintWriter out = new PrintWriter(args[args.length - 1], "UTF-8")) {
            for (int i = 0; i + 1 < args.length; i++) {
                Address target = toAddr(args[i]);
                out.println("== " + target);
                for (Reference r : getReferencesTo(target)) {
                    Address from = r.getFromAddress();
                    Function f = getFunctionContaining(from);
                    if (f != null) {
                        out.println("  code " + from + " in " + f.getName() + " (" + r.getReferenceType() + ")");
                    } else {
                        out.println("  data " + from + " (" + r.getReferenceType() + ")" + tableUsers(from));
                    }
                }
            }
        }
        println("DONE");
    }

    // Who refers to the table holding a pointer: walks back over the pointers
    // before it (4 bytes each) until something refers to one of them
    private String tableUsers(Address slot) throws Exception {
        Address at = slot;
        for (int k = 0; k < 512; k++) {
            Reference[] refs = getReferencesTo(at);
            for (Reference r : refs) {
                Function f = getFunctionContaining(r.getFromAddress());
                if (f != null) {
                    return " - table starting at " + at + " (slot " + k + " before it), used by " + f.getName();
                }
            }
            at = at.subtract(4);
        }
        return "";
    }
}
