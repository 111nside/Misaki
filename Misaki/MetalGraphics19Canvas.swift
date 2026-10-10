import SwiftUI
import Metal
import MetalKit

/// Two actual Metal render passes: guest RGBA8 texture -> offscreen BGRA8
/// framebuffer -> screen drawable. The CPU service composes guest pixels;
/// Metal performs textured sampling and a programmable presentation shader.
struct MetalGraphics19Canvas: UIViewRepresentable {
    let frame: NativeGraphics19Frame

    func makeCoordinator() -> Coordinator { Coordinator(frame: frame) }

    func makeUIView(context: Context) -> MTKView {
        let view = MTKView(frame: .zero, device: MTLCreateSystemDefaultDevice())
        view.colorPixelFormat = .bgra8Unorm
        view.framebufferOnly = true
        view.isPaused = true
        view.enableSetNeedsDisplay = true
        view.delegate = context.coordinator
        context.coordinator.prepare(device: view.device)
        view.setNeedsDisplay()
        return view
    }

    func updateUIView(_ view: MTKView, context: Context) {
        context.coordinator.frame = frame
        view.setNeedsDisplay()
    }

    final class Coordinator: NSObject, MTKViewDelegate {
        var frame: NativeGraphics19Frame
        private var queue: MTLCommandQueue?
        private var inputPipeline: MTLRenderPipelineState?
        private var presentPipeline: MTLRenderPipelineState?
        private var input: MTLTexture?
        private var offscreen: MTLTexture?
        private var cachedChecksum: UInt64 = 0

        init(frame: NativeGraphics19Frame) { self.frame = frame }

        func prepare(device: MTLDevice?) {
            guard let device else { return }
            queue = device.makeCommandQueue()
            let source = """
            #include <metal_stdlib>
            using namespace metal;
            struct QuadVertex { float4 position [[position]]; float2 uv; };
            vertex QuadVertex misaki19_vertex(uint index [[vertex_id]]) {
                const float2 pos[6] = {
                    float2(-1.0,  1.0), float2(-1.0, -1.0), float2(1.0,  1.0),
                    float2( 1.0,  1.0), float2(-1.0, -1.0), float2(1.0, -1.0)
                };
                const float2 uv[6] = {
                    float2(0.0, 0.0), float2(0.0, 1.0), float2(1.0, 0.0),
                    float2(1.0, 0.0), float2(0.0, 1.0), float2(1.0, 1.0)
                };
                QuadVertex v;
                v.position = float4(pos[index], 0.0, 1.0);
                v.uv = uv[index];
                return v;
            }
            fragment float4 misaki19_input(QuadVertex v [[stage_in]],
                                             texture2d<float> pixels [[texture(0)]]) {
                constexpr sampler s(coord::normalized, address::clamp_to_edge,
                                    filter::nearest);
                return pixels.sample(s, v.uv);
            }
            fragment float4 misaki19_present(QuadVertex v [[stage_in]],
                                               texture2d<float> rendered [[texture(0)]],
                                               constant float &pulse [[buffer(0)]]) {
                constexpr sampler s(coord::normalized, address::clamp_to_edge,
                                    filter::linear);
                float4 c = rendered.sample(s, v.uv);
                float gain = 0.94 + 0.06 * pulse;
                return float4(saturate(c.rgb * gain), c.a);
            }
            """
            guard let library = try? device.makeLibrary(source: source, options: nil),
                  let vertex = library.makeFunction(name: "misaki19_vertex"),
                  let first = library.makeFunction(name: "misaki19_input"),
                  let second = library.makeFunction(name: "misaki19_present") else { return }
            let descriptor = MTLRenderPipelineDescriptor()
            descriptor.vertexFunction = vertex
            descriptor.colorAttachments[0].pixelFormat = .bgra8Unorm
            descriptor.fragmentFunction = first
            inputPipeline = try? device.makeRenderPipelineState(descriptor: descriptor)
            descriptor.fragmentFunction = second
            presentPipeline = try? device.makeRenderPipelineState(descriptor: descriptor)
        }

        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

        func draw(in view: MTKView) {
            guard frame.passed, let device = view.device, let queue,
                  let inputPipeline, let presentPipeline,
                  let drawable = view.currentDrawable,
                  let presentation = view.currentRenderPassDescriptor else { return }
            let width = Int(frame.width)
            let height = Int(frame.height)
            guard width > 0, height > 0 else { return }

            if input == nil || cachedChecksum != frame.checksum {
                let description = MTLTextureDescriptor.texture2DDescriptor(
                    pixelFormat: .rgba8Unorm, width: width, height: height, mipmapped: false)
                description.usage = [.shaderRead]
                description.storageMode = .shared
                guard let texture = device.makeTexture(descriptor: description) else { return }
                frame.pixels.withUnsafeBytes { bytes in
                    guard let pointer = bytes.baseAddress else { return }
                    texture.replace(region: MTLRegionMake2D(0, 0, width, height),
                                    mipmapLevel: 0, withBytes: pointer,
                                    bytesPerRow: width * 4)
                }
                input = texture
                cachedChecksum = frame.checksum
            }
            if offscreen == nil || offscreen?.width != width || offscreen?.height != height {
                let description = MTLTextureDescriptor.texture2DDescriptor(
                    pixelFormat: .bgra8Unorm, width: width, height: height, mipmapped: false)
                description.usage = [.renderTarget, .shaderRead]
                description.storageMode = .private
                offscreen = device.makeTexture(descriptor: description)
            }
            guard let input, let offscreen,
                  let commandBuffer = queue.makeCommandBuffer() else { return }

            // Pass 1: texture sampling into a reusable offscreen framebuffer.
            let offscreenPass = MTLRenderPassDescriptor()
            offscreenPass.colorAttachments[0].texture = offscreen
            offscreenPass.colorAttachments[0].loadAction = .clear
            offscreenPass.colorAttachments[0].storeAction = .store
            offscreenPass.colorAttachments[0].clearColor = MTLClearColor(
                red: 0.0, green: 0.0, blue: 0.0, alpha: 1.0)
            guard let first = commandBuffer.makeRenderCommandEncoder(descriptor: offscreenPass)
                else { return }
            first.setRenderPipelineState(inputPipeline)
            first.setFragmentTexture(input, index: 0)
            first.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 6)
            first.endEncoding()

            // Pass 2: shader sampling offscreen texture into the iOS drawable.
            presentation.colorAttachments[0].loadAction = .clear
            presentation.colorAttachments[0].storeAction = .store
            guard let second = commandBuffer.makeRenderCommandEncoder(descriptor: presentation)
                else { return }
            second.setRenderPipelineState(presentPipeline)
            second.setFragmentTexture(offscreen, index: 0)
            var pulse = Float(frame.frameIndex % 90) / 89.0
            second.setFragmentBytes(&pulse, length: MemoryLayout<Float>.stride, index: 0)
            second.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 6)
            second.endEncoding()
            commandBuffer.present(drawable)
            commandBuffer.commit()
        }
    }
}
