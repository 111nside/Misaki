import SwiftUI
import Metal

struct Milestone30View: View {
    @State private var plan: NativeGCN30Plan?
    @State private var gpu: MetalGCN30Result?
    @State private var running = false

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 30 · Fused GCN shader") {
                    Button {
                        gpu = nil
                        let prepared = NativeGCN30API.prepare()
                        plan = prepared
                        guard prepared.passed else { return }
                        running = true
                        Task {
                            let completed = await Task.detached(priority: .userInitiated) {
                                NativeGCN30API.execute(prepared)
                            }.value
                            gpu = completed
                            running = false
                        }
                    } label: {
                        Label("Execute fused GCN shader on Metal", systemImage: "cpu.fill")
                    }
                    .disabled(running)

                    Text("Decodes one self-authored GCN1.1 shader mixing real-format scalar/vector instructions with BUFFER_LOAD_DWORD and BUFFER_STORE_DWORD. The CPU reference and one Metal compute kernel execute identical instruction order on one emulated lane.")
                        .font(.footnote).foregroundStyle(.secondary)
                    if let plan {
                        Text("GCN fused shader decode: \(plan.passed ? "PASS" : "FAIL")\nInstructions=\(plan.instructions)\nCode DWORDs=\(plan.codeWords)\nALU/Buffer loads/Buffer stores=\(plan.aluOperations)/\(plan.loads)/\(plan.stores)\nCPU executed=\(plan.executed)\nRecord index=\(plan.recordIndex)\nLoaded/Stored=\(plan.loaded)/\(plan.stored)\nGuest code/descriptor read-only=\(plan.codeReadOnly)/\(plan.descriptorsReadOnly)\nInput read-only / output writable=\(plan.inputReadOnly)/\(plan.outputWritable)\nNative status=\(plan.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                        if !plan.passed {
                            Text("Guest shader decoding or native execution failed. Metal was not run.")
                                .foregroundStyle(.red)
                        }
                    }
                    if running { ProgressView("Compiling and running one Metal compute kernel…") }
                    if let gpu {
                        Label("Metal fused shader comparison: \(gpu.didRun && gpu.matched ? "PASS" : "FAIL")",
                              systemImage: gpu.didRun && gpu.matched ? "checkmark.circle.fill" : "xmark.circle.fill")
                            .foregroundStyle(gpu.didRun && gpu.matched ? .green : .red)
                        Text("GPU status=\(gpu.gpuStatus)\nOutput record 7=\(gpu.output.count == 64 ? String(gpu.output[7]) : "unavailable")\n\(gpu.message)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                if let plan, plan.passed {
                    Section("Expected GPU registers") {
                        let labels = ["s1", "s2", "s3", "s10", "s12", "v4", "v8", "v9"]
                        ForEach(0..<labels.count, id: \.self) { index in
                            HStack {
                                Text(labels[index])
                                    .font(.system(.subheadline, design: .monospaced))
                                Spacer()
                                Text("\(plan.expectedRegisters[index])")
                                    .font(.system(.subheadline, design: .monospaced))
                            }
                        }
                    }
                    Section("Generated Metal source") {
                        DisclosureGroup("View single-kernel MSL") {
                            Text(plan.source)
                                .font(.system(.caption2, design: .monospaced))
                                .textSelection(.enabled)
                        }
                    }
                }
                Section("What changed") {
                    Label("Real-format GCN ALU and MUBUF in one shader", systemImage: "checkmark.circle")
                    Label("One Metal compute kernel, not two staged passes", systemImage: "checkmark.circle")
                    Label("Loaded data flows through v8 → v9 → guest output", systemImage: "checkmark.circle")
                    Label("Checks 8 registers and all 64 output records", systemImage: "checkmark.circle")
                }
                Section("Current limitations") {
                    Text("This remains a single-lane diagnostic with a very small instruction whitelist and bounded linear buffer descriptors. It does not emulate GCN wavefronts, shader scheduling, arbitrary memory modes, PS4 GNM/GNMX, firmware or the original PS4 home menu.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("GCN Fused")
        }
    }
}
