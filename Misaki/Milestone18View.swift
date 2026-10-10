import SwiftUI
import Metal

struct Milestone18View: View {
    @State private var scene: NativeGPUFrame?

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 18 · GPU → Metal") {
                    Button {
                        scene = NativeGPUAPI.makeDemoFrame()
                    } label: {
                        Label("Render guest GPU frame", systemImage: "rectangle.on.rectangle.angled")
                    }
                    Text("Runs Misaki's original bounded C++ 2D drawing protocol, then renders the guest rectangles using Apple's Metal API.")
                        .font(.footnote).foregroundStyle(.secondary)
                    if let scene {
                        Text("GPU command stream: \(scene.passed ? "PASS" : "FAIL")\nCanvas=\(scene.width)x\(scene.height)\nCommands=\(scene.commandCount)\nRectangles=\(scene.rectangles)\nClear/Present=\(scene.clears)/\(scene.presents)\nChecksum=0x\(String(scene.checksum, radix: 16))\nStatus=\(scene.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                        if scene.passed && MTLCreateSystemDefaultDevice() != nil {
                            MetalGuestCanvas(scene: scene)
                                .aspectRatio(CGFloat(scene.width) / CGFloat(scene.height), contentMode: .fit)
                                .clipShape(RoundedRectangle(cornerRadius: 12))
                                .accessibilityLabel("Metal-rendered guest test frame with five colored rectangles")
                        } else if MTLCreateSystemDefaultDevice() == nil {
                            Text("Metal is unavailable on this device.")
                                .foregroundStyle(.secondary)
                        } else {
                            Text("Guest commands were rejected; no frame will be rendered.")
                                .foregroundStyle(.secondary)
                        }
                    }
                }
                Section("Architecture") {
                    Label("C++ command validation and checksums", systemImage: "checkmark.circle")
                    Label("Swift-to-native C ABI bridge", systemImage: "checkmark.circle")
                    Label("Metal rectangle rendering on device", systemImage: "checkmark.circle")
                }
                Section("Not implemented") {
                    Text("This is a custom research-only 2D GPU command protocol, not Sony GNM/GNMX, AMD GCN packets, PS4 GPU registers, or a PlayStation home menu. No PS4 firmware or game data is loaded.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Graphics")
        }
    }
}
