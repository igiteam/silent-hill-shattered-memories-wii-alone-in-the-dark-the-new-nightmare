// Gives names to functions of the executable from a list (address name per
// line, # for comments): tools/ghidra/functionNames.txt. Run it with
// run.ps1 -Write, so the project is saved.
// Usage: -postScript RenameFunctions.java <list file>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

public class RenameFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        List<String> lines = Files.readAllLines(Paths.get(getScriptArgs()[0]));
        int named = 0;
        for (String line : lines) {
            line = line.trim();
            if (line.isEmpty() || line.startsWith("#")) {
                continue;
            }
            String[] parts = line.split("\s+");
            Address address = toAddr(parts[0]);
            Function f = getFunctionAt(address);
            if (f == null) {
                f = createFunction(address, null);
            }
            if (f == null) {
                println("NO FUNCTION at " + parts[0]);
                continue;
            }
            f.setName(parts[1], SourceType.USER_DEFINED);
            named++;
        }
        println("NAMED " + named + " functions");
    }
}
