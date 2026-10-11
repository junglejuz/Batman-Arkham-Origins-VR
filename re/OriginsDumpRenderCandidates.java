// Export focused Ghidra evidence after defining guarded render candidates.
// @category BatmanOrigins

import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;

public class OriginsDumpRenderCandidates extends GhidraScript {
    @Override
    public void run() throws Exception {
        String hash = "1c373ed6edd3f5d42b097fcb2fc592811671b6f7d54b764b043b01e47871e5c2";
        if (currentProgram == null || !hash.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
            throw new IllegalStateException("Expected the fingerprinted Origins program");
        }
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Supply output evidence text path");
        Path output = Path.of(args[0]).toAbsolutePath();
        Files.createDirectories(output.getParent());
        DecompInterface decompiler = new DecompInterface();
        try (PrintWriter writer = new PrintWriter(Files.newBufferedWriter(output, StandardCharsets.UTF_8))) {
            writer.println("STATIC/Ghidra: focused functions only; no runtime or headset evidence");
            writer.println("Executable: " + currentProgram.getExecutablePath());
            writer.println("SHA256: " + currentProgram.getExecutableSHA256());
            writer.println("Language: " + currentProgram.getLanguageID());
            writer.println("Function names and inferred types remain hypotheses.");
            if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("Decompiler did not open program");
            long[] locations = {0x665560L, 0x665570L, 0x665590L, 0x665610L,
                0x6656f0L, 0x665710L, 0x67ed90L, 0x7d40f0L, 0x7d5080L, 0x9cfdc0L};
            for (long location : locations) {
                monitor.checkCancelled();
                Function function = getFunctionAt(toAddr(location));
                writer.println("\n=== " + toAddr(location) + " ===");
                if (function == null) {
                    writer.println("No function defined");
                    continue;
                }
                writer.println("Entry name: " + function.getName());
                DecompileResults result = decompiler.decompileFunction(function, 30, monitor);
                writer.println("Decompiler completed: " + result.decompileCompleted());
                writer.println("Decompiler diagnostic: " + result.getErrorMessage());
                if (result.getDecompiledFunction() != null) writer.println(result.getDecompiledFunction().getC());
                writer.println("-- instruction-aware disassembly --");
                InstructionIterator instructions = currentProgram.getListing().getInstructions(function.getBody(), true);
                while (instructions.hasNext()) {
                    var instruction = instructions.next();
                    writer.println(instruction.getAddress() + ": " + instruction);
                }
                writer.println("-- xrefs to entry --");
                for (Reference reference : currentProgram.getReferenceManager().getReferencesTo(function.getEntryPoint())) {
                    writer.println(reference.getFromAddress() + " -> " + reference.getToAddress() + " " + reference.getReferenceType());
                }
                writer.flush();
                println("Exported " + function.getName());
            }
        } finally {
            decompiler.dispose();
        }
        println("Evidence saved: " + output);
    }
}
