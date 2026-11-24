// Prints how many functions Ghidra found and how many still have automatic names (FUN_...)
// Usage: analyzeHeadless.bat <project folder>\ghidra Alone4 -process alone4.exe -noanalysis -scriptPath tools\ghidra -postScript CountFunctions.java
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class CountFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        int total = 0, unnamed = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            total++;
            if (f.getName().startsWith("FUN_")) {
                unnamed++;
            }
        }
        println("FUNCTIONS total=" + total + " unnamed=" + unnamed);
    }
}
