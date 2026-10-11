// Define only fingerprinted Origins render candidates missed by normal analysis.
// Run only after the current automatic analysis completes.
// @category BatmanOrigins
// @menupath Tools.BatmanOrigins.Define render candidates

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class OriginsDefineRenderCandidates extends GhidraScript {
    private static final String HASH =
        "1c373ed6edd3f5d42b097fcb2fc592811671b6f7d54b764b043b01e47871e5c2";
    private static final long[] LOCATIONS = {
        0x665560L, 0x665570L, 0x665590L, 0x665610L, 0x6656f0L,
        0x665710L, 0x67ed90L, 0x7d40f0L, 0x7d5080L, 0x9cfdc0L
    };
    private static final String[] PREFIXES = {
        "8b49048b018b5060ffd2b808000000c3", "b8b4154901c3cccccccccccccccccccc",
        "8b49048b018b50646a006a00ffd2b808", "83ec08538b1d8c52390156578d44240c",
        "b878164901c3cccccccccccccccccccc", "8b41088b49045051e8f3feffff83c408",
        "6aff687221270164a1000000005081ec", "558bec83e4f06aff6891b7270164a100",
        "558bec83e4f86aff68c8b7270164a100", "558bec83e4f06aff68f2a2280164a100"
    };
    private static final String[] NAMES = {
        "BO_STATIC_BeginDrawingCommand_Execute", "BO_STATIC_BeginDrawingCommand_DescribeA",
        "BO_STATIC_EndDrawingCommand_ExecuteA", "BO_STATIC_EndDrawing_Dispatch",
        "BO_STATIC_BeginDrawingCommand_DescribeB", "BO_STATIC_EndDrawingCommand_ExecuteB",
        "BO_CANDIDATE_ViewportDrawRoot", "BO_CANDIDATE_CalcSceneView",
        "BO_CANDIDATE_ViewHelper_Unresolved", "BO_CANDIDATE_SceneViewConstructor"
    };

    @Override
    public void run() throws Exception {
        if (currentProgram == null || currentProgram.getImageBase().getOffset() != 0x400000L ||
            !HASH.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
            throw new IllegalStateException("Expected the fingerprinted retail BatmanOrigins.exe; no changes made");
        }
        // Validate every candidate before defining any. Verify imported memory,
        // not just the source-file metadata. Do not overwrite analysis conflicts.
        for (int i = 0; i < LOCATIONS.length; ++i) {
            monitor.checkCancelled();
            byte[] bytes = new byte[16];
            currentProgram.getMemory().getBytes(toAddr(LOCATIONS[i]), bytes);
            StringBuilder hex = new StringBuilder();
            for (byte value : bytes) hex.append(String.format("%02x", value & 0xff));
            if (!PREFIXES[i].equals(hex.toString())) {
                throw new IllegalStateException("Candidate bytes differ at " + Long.toHexString(LOCATIONS[i]) + "; no changes made");
            }
        }
        int created = 0;
        for (int i = 0; i < LOCATIONS.length; ++i) {
            monitor.checkCancelled();
            Address address = toAddr(LOCATIONS[i]);
            Function existing = getFunctionContaining(address);
            if (existing != null) {
                println("Preserved " + existing.getName() + " at " + existing.getEntryPoint());
                continue;
            }
            if (!disassemble(address)) {
                println("Could not disassemble " + address + "; preserved existing listing");
                continue;
            }
            Function function = createFunction(address, NAMES[i]);
            if (function != null) {
                ++created;
                println("Defined " + function.getName() + " at " + address);
            } else {
                println("Could not define function at " + address);
            }
        }
        println("Created " + created + " candidate functions. No prototypes or camera layouts asserted.");
        println("Only Ghidra analysis changed; no executable or game config was edited.");
    }
}
