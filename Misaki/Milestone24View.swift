import SwiftUI

struct Milestone24View: View {
    @State private var snapshot: NativePM424Snapshot?

    private func hex(_ value: UInt64) -> String {
        "0x" + String(value, radix: 16).uppercased()
    }

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 24 · GCN draw-time state") {
                    Button {
                        snapshot = NativePM424API.runDiagnostic()
                    } label: {
                        Label("Analyze PM4 draw state", systemImage: "slider.horizontal.3")
                    }
                    Text("Replays PM4 register updates across nested guest-memory command buffers and captures the active color-target, scissor, and pixel-shader metadata at DRAW_INDEX_AUTO.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    if let snapshot {
                        Text("GCN draw-state analysis: \(snapshot.passed ? "PASS" : "FAIL")\nPackets=\(snapshot.packets)\nIndirect buffers=\(snapshot.indirectBuffers)\nRegister writes=\(snapshot.registerWrites)\nDraws=\(snapshot.drawCount)\nConsistent/Rejected=\(snapshot.readyDraws)/\(snapshot.rejectedDraws)\nChecksum=\(hex(snapshot.checksum))\nStatus=\(snapshot.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                if let snapshot, snapshot.passed {
                    ForEach(snapshot.draws) { draw in
                        Section("Draw \(draw.id + 1) · metadata only") {
                            LabeledContent("Guest packet", value: hex(draw.packetAddress))
                            LabeledContent("Packet/depth", value: "\(draw.packetIndex) / \(draw.depth)")
                            LabeledContent("Vertex count", value: "\(draw.vertices)")
                            LabeledContent("Color target candidate", value: hex(draw.colorAddress))
                            LabeledContent("Pixel shader candidate", value: hex(draw.pixelShaderAddress))
                            LabeledContent("Scissor origin", value: "(\(draw.scissorX), \(draw.scissorY))")
                            LabeledContent("Scissor size", value: "\(draw.scissorWidth) × \(draw.scissorHeight)")
                            LabeledContent("Pitch tile max (raw)", value: "\(draw.pitchTileMax)")
                            LabeledContent("Color format field", value: hex(UInt64(draw.colorFormatField)))
                            LabeledContent("Target mask", value: hex(UInt64(draw.targetMask)))
                            LabeledContent("Metadata internally consistent", value: draw.metadataComplete ? "Yes" : "No")
                        }
                    }
                }
                Section("Implemented") {
                    Label("Capture state at each draw packet", systemImage: "checkmark.circle")
                    Label("Respect nested PM4 packet ordering", systemImage: "checkmark.circle")
                    Label("Interpret bounded address/scissor fields", systemImage: "checkmark.circle")
                    Label("Reject invalid or incomplete draw metadata", systemImage: "checkmark.circle")
                }
                Section("Still missing") {
                    Text("These addresses are derived from raw register values; they are not mapped GPU resources. Misaki does not yet translate GCN shaders, validate complete tiled render-target layouts, execute PS4 draw calls, or boot the PS4 home menu. The PM4 packets are self-authored test fixtures.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("PM4 State")
        }
    }
}
