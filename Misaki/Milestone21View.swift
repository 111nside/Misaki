import SwiftUI
import Metal

struct Milestone21View: View {
    @State private var frame: NativeGraphics21Frame?
    @State private var selectedEffect: UInt32 = 0
    @State private var isAnimating = false

    private let effects = ["Normal", "Grayscale", "Invert", "Scanlines"]

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 21 · GPU render states") {
                    Picker("Shader effect", selection: $selectedEffect) {
                        ForEach(0..<effects.count, id: \.self) { index in
                            Text(effects[index]).tag(UInt32(index))
                        }
                    }
                    .pickerStyle(.menu)
                    .onChange(of: selectedEffect) { _, newValue in
                        if frame != nil { frame = NativeGraphics21API.makeFrame(effect: newValue) }
                    }
                    Button {
                        isAnimating = false
                        frame = NativeGraphics21API.makeFrame(effect: selectedEffect)
                    } label: {
                        Label("Render guest graphics pipeline", systemImage: "square.stack.3d.up.fill")
                    }
                    Text("Guest x86-64 code patches two texture positions and a shader state in isolated memory, then submits a 10-packet queue. C++ applies blend/scissor states; Metal displays the framebuffer with the selected fragment shader.")
                        .font(.footnote).foregroundStyle(.secondary)
                    if let frame {
                        Text("Guest graphics pipeline: \(frame.passed ? "PASS" : "FAIL")\nCanvas=\(frame.width)x\(frame.height)\nCommands=\(frame.commands)\nTextures=\(frame.distinctTextures)\nSprites=\(frame.sprites)\nBlend state changes=\(frame.stateChanges)\nScissor changes=\(frame.scissorChanges)\nShader effect=\(effects[Int(min(frame.effectMode, 3))])\nGuest instructions=\(frame.guestInstructions)\nGPU calls=\(frame.guestServiceCalls)\nSprite X=\(frame.firstSpriteX), \(frame.secondSpriteX)\nSubmitted/Completed=\(frame.submittedFence)/\(frame.completedFence)\nQueue drained=\(frame.queueDrained)\nStatus=\(frame.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                        if frame.passed && MTLCreateSystemDefaultDevice() != nil {
                            Group {
                                if isAnimating {
                                    TimelineView(.periodic(from: .now, by: 1.0 / 12.0)) { context in
                                        let i = UInt32(Int(context.date.timeIntervalSince1970 * 12) % 90)
                                        MetalGraphics21Canvas(frame: NativeGraphics21API.makeFrame(index: i, effect: selectedEffect))
                                    }
                                } else {
                                    MetalGraphics21Canvas(frame: frame)
                                }
                            }
                            .aspectRatio(CGFloat(frame.width) / CGFloat(frame.height), contentMode: .fit)
                            .clipShape(RoundedRectangle(cornerRadius: 12))
                            .accessibilityLabel("Metal-rendered guest scene with two moving textured sprites")
                            Button {
                                isAnimating.toggle()
                            } label: {
                                Label(isAnimating ? "Stop animation" : "Start animation",
                                      systemImage: isAnimating ? "pause.fill" : "play.fill")
                            }
                        } else if MTLCreateSystemDefaultDevice() == nil {
                            Text("Metal is unavailable on this device.")
                                .foregroundStyle(.secondary)
                        } else {
                            Text("Guest graphics submission was rejected.")
                                .foregroundStyle(.secondary)
                        }
                    }
                }
                Section("New graphics capabilities") {
                    Label("Two independent RGBA textures", systemImage: "checkmark.circle")
                    Label("Source-over and additive blending", systemImage: "checkmark.circle")
                    Label("Clip rectangles and render-state changes", systemImage: "checkmark.circle")
                    Label("Four Metal shader display effects", systemImage: "checkmark.circle")
                    Label("Guest CPU changes positions and fragment effect", systemImage: "checkmark.circle")
                }
                Section("Still experimental") {
                    Text("The queue, textures, and graphics effects are Misaki-designed prototypes. They are not AMD GCN packets, PS4 graphics drivers, GNM/GNMX, or Sony firmware. This does not run the original PS4 home menu.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Graphics Pipeline")
        }
    }
}
