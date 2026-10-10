import SwiftUI

struct Milestone23View: View {
    @State private var trace: NativePM423Snapshot?

    private func hex(_ value: UInt32) -> String {
        "0x" + String(value, radix: 16).uppercased()
    }

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 23 · Indirect GCN PM4") {
                    Button {
                        trace = NativePM423API.runDiagnostic()
                    } label: {
                        Label("Decode nested PM4 buffers", systemImage: "point.3.connected.trianglepath.dotted")
                    }
                    Text("Reads authentic-form AMD PM4 packet headers from three isolated guest-memory buffers. Tracks parent/child packet order, selected color-target registers, and draw/event metadata without executing GPU commands.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    if let trace {
                        Text("Nested GCN PM4: \(trace.passed ? "PASS" : "FAIL")\nPackets=\(trace.packetCount)\nIndirect buffers=\(trace.indirectBuffers)\nConst buffers=\(trace.indirectConstBuffers)\nMax depth=\(trace.maximumDepth)\nGuest DWORDs=\(trace.guestWords)\nRegister writes=\(trace.registerWrites)\nDraw/Event=\(trace.drawPackets)/\(trace.eventPackets)\nRender-target metadata=\(trace.targetMetadataObserved)\nChecksum=0x\(String(trace.checksum, radix: 16))\nStatus=\(trace.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                if let trace, trace.passed {
                    Section("Observed GPU register values") {
                        LabeledContent("CB_COLOR0_BASE", value: hex(trace.colorBase))
                        LabeledContent("CB_COLOR0_PITCH", value: hex(trace.colorPitch))
                        LabeledContent("CB_COLOR0_INFO", value: hex(trace.colorInfo))
                        LabeledContent("CB_TARGET_MASK", value: hex(trace.targetMask))
                        LabeledContent("SCISSOR TL / BR", value: "\(hex(trace.scissorTopLeft)) / \(hex(trace.scissorBottomRight))")
                        LabeledContent("Pixel shader LO / HI", value: "\(hex(trace.pixelShaderLow)) / \(hex(trace.pixelShaderHigh))")
                    }
                    Section("Decoded execution order") {
                        ForEach(trace.packets) { packet in
                            VStack(alignment: .leading, spacing: 4) {
                                Text("\(packet.id + 1). \(packet.label)")
                                    .font(.system(.subheadline, design: .monospaced))
                                Text("Depth \(packet.depth) · Guest 0x\(String(packet.guestAddress, radix: 16).uppercased()) · \(packet.bodyWords) DWORD payload")
                                    .font(.system(.caption, design: .monospaced))
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                }
                Section("Decoder safeguards") {
                    Label("Recursive cycle and nesting limits", systemImage: "checkmark.circle")
                    Label("Per-buffer and global size limits", systemImage: "checkmark.circle")
                    Label("Guest address and read-permission checks", systemImage: "checkmark.circle")
                    Label("Unknown/predicated/chained packets rejected", systemImage: "checkmark.circle")
                    Label("Deterministic packet ordering and checksums", systemImage: "checkmark.circle")
                }
                Section("Scope") {
                    Text("These are raw, synthetic PM4 register values, not a working render target. The decoder does not execute shaders, resolve GPU virtual memory, translate GCN to Metal, or load PS4 firmware. Your earlier graphics and PM4 tests remain separate.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("PM4 Indirect")
        }
    }
}
