import SwiftUI
import Metal

struct Milestone27View: View {
    @State private var plan: NativeMetalIR27Plan?
    @State private var gpuResult: MetalIR27Execution?
    @State private var running = false

    private let names = ["s1", "s2", "s3", "v0", "v1", "v2"]

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 27 · GCN IR → Metal compute") {
                    Button {
                        running = false
                        gpuResult = nil
                        let next = NativeMetalIR27API.generate()
                        plan = next
                        guard next.passed else { return }
                        running = true
                        // Dynamic GPU shader compilation can be slow; do it off
                        // the main thread to keep the iOS diagnostic responsive.
                        let source = next.source
                        let expected = next.expected
                        Task {
                            let completed = await Task.detached(priority: .userInitiated) {
                                MetalIR27Runner.execute(source: source, expected: expected)
                            }.value
                            gpuResult = completed
                            running = false
                        }
                    } label: {
                        Label("Compile and execute translated Metal shader",
                              systemImage: "cpu.fill")
                    }
                    .disabled(running)
                    Text("Generates a real Metal compute kernel from the bounded GCN intermediate representation, runs it on your device's GPU, then compares six raw register results with the C++ software evaluator.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    if let plan {
                        Text("IR → MSL translation: \(plan.passed ? "PASS" : "FAIL")\nIR operations=\(plan.instructions)\nEmitted ALU statements=\(plan.statements)\nGenerated MSL bytes=\(plan.sourceBytes)\nSource hash=0x\(String(plan.metalHash, radix: 16))\nNative status=\(plan.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                    if running {
                        ProgressView("Compiling and running shader on Metal…")
                    }
                    if let outcome = gpuResult {
                        Label("Metal GPU comparison: \(outcome.didRun && outcome.matched ? "PASS" : "FAIL")",
                              systemImage: outcome.didRun && outcome.matched ? "checkmark.circle.fill" : "xmark.circle.fill")
                            .foregroundStyle(outcome.didRun && outcome.matched ? .green : .red)
                        Text(outcome.message)
                            .font(.footnote)
                            .textSelection(.enabled)
                    }
                }
                if let plan, plan.passed {
                    Section("Software vs actual GPU results") {
                        ForEach(names.indices, id: \.self) { i in
                            VStack(alignment: .leading, spacing: 4) {
                                Text(names[i])
                                    .font(.system(.subheadline, design: .monospaced))
                                HStack {
                                    Text("CPU 0x\(String(plan.expected[i], radix: 16).uppercased())")
                                    Spacer()
                                    if let outcome = gpuResult, outcome.received.count == 6 {
                                        Text("GPU 0x\(String(outcome.received[i], radix: 16).uppercased())")
                                            .foregroundStyle(outcome.received[i] == plan.expected[i] ? .green : .red)
                                    } else {
                                        Text("GPU pending")
                                            .foregroundStyle(.secondary)
                                    }
                                }
                                .font(.system(.caption, design: .monospaced))
                            }
                        }
                    }
                    Section("Generated compute shader") {
                        DisclosureGroup("Inspect generated Metal Shading Language") {
                            Text(plan.source)
                                .font(.system(.caption2, design: .monospaced))
                                .textSelection(.enabled)
                        }
                    }
                }
                Section("Scope") {
                    Text("This is a single-thread compute experiment for a strict GCN ALU subset. It does not execute arbitrary PS4 shaders, translate wavefront semantics, sample guest textures, or render PS4 graphics. Earlier PM4 and Metal diagnostics are unchanged.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("GCN → Metal")
        }
    }
}
