import SwiftUI
import Metal

struct Milestone31to35View: View {
    @State private var result: NativeIntegratedFrame?
    private let stages: [(number: Int, title: String, explanation: String)] = [
        (31, "Cooperative runtime", "Two isolated x86-64 guest processes share one scheduler."),
        (32, "GPU scheduling", "Guest commands submit, complete, and synchronize with fence 1."),
        (33, "PM4 and GCN", "Nested AMD PM4 decoding and a limited software GCN shader run as guest services."),
        (34, "Dynamic ELF modules", "An ordinary ET_DYN guest resolves an imported module function and returns 42."),
        (35, "Process system services", "An isolated virtual file is opened, read, and closed by guest code.")
    ]

    var body: some View {
        NavigationStack {
            List {
                Section("Milestones 31–35 · Integrated runtime") {
                    Button {
                        result = NativeIntegratedRuntimeAPI.run()
                    } label: {
                        Label("Execute integrated guest runtime", systemImage: "cpu.fill")
                    }
                    Text("Runs a cooperative CPU/process scheduler with filesystem services, a guest graphics queue, nested AMD PM4 inspection, restricted GCN shader execution, and an independently linked ELF process in one test session.")
                        .font(.footnote).foregroundStyle(.secondary)
                    if let result {
                        Label(result.passed ? "Integrated guest runtime: PASS" : "Integrated guest runtime: FAIL",
                              systemImage: result.passed ? "checkmark.circle.fill" : "xmark.circle.fill")
                            .foregroundStyle(result.passed ? .green : .red)
                        Text("Processes=\(result.processCount)\nInstructions=\(result.instructions)\nContext switches=\(result.contextSwitches)\nVFS open/read/close=\(result.fileOpens)/\(result.fileReads)/\(result.fileCloses)\nGPU calls=\(result.gpuCalls)\nPM4 packets=\(result.pm4Packets)\nGCN shader operations=\(result.shaderOperations)\nLoaded modules / imports=\(result.loadedModules)/\(result.resolvedImports)\nCompleted fence=\(result.completedFence)/\(result.submittedFence)\nELF RAX=\(result.elfRAX)\nGuest PID=\(result.guestPID)\nGCN output record 7=\(result.outputRecord7)\nStatus=\(result.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                        if result.surfaceReady && MTLCreateSystemDefaultDevice() != nil {
                            MetalIntegratedRuntimeCanvas(frame: result)
                                .aspectRatio(16.0 / 9.0, contentMode: .fit)
                                .clipShape(RoundedRectangle(cornerRadius: 12))
                                .accessibilityLabel("Two textured sprites rendered from the integrated guest process through Metal")
                        } else if MTLCreateSystemDefaultDevice() == nil {
                            Text("Metal display is unavailable; native diagnostics can still run.")
                                .foregroundStyle(.secondary)
                        }
                    }
                }
                Section("Separate subsystem results") {
                    ForEach(stages.indices, id: \.self) { index in
                        let item = stages[index]
                        VStack(alignment: .leading, spacing: 4) {
                            HStack {
                                Text("\(item.number) · \(item.title)")
                                    .font(.subheadline.weight(.semibold))
                                Spacer()
                                if let result {
                                    Label(result.stages[index] ? "PASS" : "FAIL",
                                          systemImage: result.stages[index] ? "checkmark.circle" : "xmark.circle")
                                        .font(.caption)
                                        .foregroundStyle(result.stages[index] ? .green : .red)
                                }
                            }
                            Text(item.explanation)
                                .font(.caption).foregroundStyle(.secondary)
                        }
                    }
                }
                Section("Boundaries") {
                    Text("The integrated services and GPU queue are original Misaki diagnostics, not Sony syscalls or PS4 GNM/GNMX. AMD PM4 and GCN instruction subsets are decoded/evaluated with deliberately bounded fixtures. The two-process scheduler is cooperative. The app does not boot PS4 firmware or run commercial PS4 games.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Runtime 31–35")
        }
    }
}
