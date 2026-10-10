import SwiftUI
import Metal
import MetalKit

/// Guest-owned graphics command queue -> CPU-composited RGBA8 framebuffer ->
/// offscreen Metal texture -> shader-selected presentation to iOS drawable.
/// Neither guest packets nor these shaders are AMD GCN/PlayStation commands.
struct MetalGraphics21Canvas: UIViewRepresentable {
    let frame: NativeGraphics21Frame

    func makeCoordinator() -> Coordinator { Coordinator(frame: frame) }

    func makeUIView(context: Context) -> MTKView {
        let view = MTKView(frame: .zero, device: MTLCreateSystemDefaultDevice())
        view.colorPixelFormat = .bgra8Unorm
        view.framebufferOnly = true
        view.isPaused = true
        view.enableSetNeedsDisplay = true
        view.delegate = context.coordinator
        context.coordinator.prepare(device: view.device, pixelFormat: view.colorPixelFormat)
        view.setNeedsDisplay()
        return view
    }

    func updateUIView(_ view: MTKView, context: Context) {
        context.coordinator.frame = frame
        view.setNeedsDisplay()
    }

    final class Coordinator: NSObject, MTKViewDelegate {
        var frame: NativeGraphics21Frame
        private var queue: MTLCommandQueue?
        private var uploadPipeline: MTLRenderPipelineState?
        private var presentationPipeline: MTLRenderPipelineState?
        private var uploaded: MTLTexture?
        private var offscreen: MTLTexture?
        private var cachedChecksum: UInt64 = 0

        init(frame: NativeGraphics21Frame) { self.frame = frame }

        func prepare(device: MTLDevice?, pixelFormat: MTLPixelFormat) {
            guard let device else { return }
            queue = device.makeCommandQueue()
            let source = """
            #include <metal_stdlib>
            using namespace metal;
            struct MisakiVertex { float4 position [[position]]; float2 uv; };
            vertex MisakiVertex misaki21_vertex(uint i [[vertex_id]]) {
                const float2 positions[6] = {
                    float2(-1,1), float2(-1,-1), float2(1,1),
                    float2(1,1), float2(-1,-1), float2(1,-1)
                };
                const float2 uv[6] = {
                    float2(0,0), float2(0,1), float2(1,0),
                    float2(1,0), float2(0,1), float2(1,1)
                };
                MisakiVertex v;
                v.position = float4(positions[i], 0.0, 1.0);
                v.uv = uv[i];
                return v;
            }
            fragment float4 misaki21_upload(MisakiVertex v [[stage_in]],
                                               texture2d<float> pixels [[texture(0)]]) {
                constexpr sampler s(coord::normalized, address::clamp_to_edge,
                                    filter::nearest);
                return pixels.sample(s, v.uv);
            }
            fragment float4 misaki21_present(MisakiVertex v [[stage_in]],
                                                texture2d<float> pixels [[texture(0)]],
                                                constant uint &effect [[buffer(0)]]) {
                constexpr sampler s(coord::normalized, address::clamp_to_edge,
                                    filter::linear);
                float4 c = pixels.sample(s, v.uv);
                if (effect == 1) {
                    float luminance = dot(c.rgb, float3(0.2126, 0.7152, 0.0722));
                    c.rgb = float3(luminance);
                } else if (effect == 2) {
                    c.rgb = float3(1.0) - c.rgb;
                } else if (effect == 3) {
                    uint pixelRow = uint(v.uv.y * 180.0);
                    if ((pixelRow & 1u) != 0u) c.rgb *= 0.67;
                }
                return c;
            }
            """
            guard let library = try? device.makeLibrary(source: source, options: nil),
                  let vertex = library.makeFunction(name: "misaki21_vertex"),
                  let upload = library.makeFunction(name: "misaki21_upload"),
                  let present = library.makeFunction(name: "misaki21_present") else { return }
            let descriptor = MTLRenderPipelineDescriptor()
            descriptor.vertexFunction = vertex
            descriptor.colorAttachments[0].pixelFormat = pixelFormat
            descriptor.fragmentFunction = upload
            uploadPipeline = try? device.makeRenderPipelineState(descriptor: descriptor)
            descriptor.fragmentFunction = present
            presentationPipeline = try? device.makeRenderPipelineState(descriptor: descriptor)
        }

        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

        func draw(in view: MTKView) {
            guard frame.passed, let device = view.device,
                  let queue, let uploadPipeline, let presentationPipeline,
                  let drawable = view.currentDrawable,
                  let onscreenPass = view.currentRenderPassDescriptor else { return }
            let w = Int(frame.width), h = Int(frame.height)
            guard w > 0, h > 0 else { return }

            if uploaded == nil || cachedChecksum != frame.checksum {
                let descriptor = MTLTextureDescriptor.texture2DDescriptor(
                    pixelFormat: .rgba8Unorm, width: w, height: h, mipmapped: false)
                descriptor.usage = [.shaderRead]
                descriptor.storageMode = .shared
                guard let texture = device.makeTexture(descriptor: descriptor) else { return }
                frame.pixels.withUnsafeBytes { raw in
                    guard let ptr = raw.baseAddress else { return }
                    texture.replace(region: MTLRegionMake2D(0, 0, w, h), mipmapLevel: 0,
                                    withBytes: ptr, bytesPerRow: w * 4)
                }
                uploaded = texture
                cachedChecksum = frame.checksum
            }
            if offscreen == nil || offscreen?.width != w || offscreen?.height != h {
                let descriptor = MTLTextureDescriptor.texture2DDescriptor(
                    pixelFormat: .bgra8Unorm, width: w, height: h, mipmapped: false)
                descriptor.usage = [.renderTarget, .shaderRead]
                descriptor.storageMode = .private
                offscreen = device.makeTexture(descriptor: descriptor)
            }
            guard let uploaded, let offscreen,
                  let commandBuffer = queue.makeCommandBuffer() else { return }
            let intermediate = MTLRenderPassDescriptor()
            intermediate.colorAttachments[0].texture = offscreen
            intermediate.colorAttachments[0].loadAction = .clear
            intermediate.colorAttachments[0].storeAction = .store
            intermediate.colorAttachments[0].clearColor = MTLClearColor(red: 0, green: 0, blue: 0, alpha: 1)
            guard let first = commandBuffer.makeRenderCommandEncoder(descriptor: intermediate)
                else { return }
            first.setRenderPipelineState(uploadPipeline)
            first.setFragmentTexture(uploaded, index: 0)
            first.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 6)
            first.endEncoding()

            onscreenPass.colorAttachments[0].loadAction = .clear
            onscreenPass.colorAttachments[0].storeAction = .store
            guard let second = commandBuffer.makeRenderCommandEncoder(descriptor: onscreenPass)
                else { return }
            second.setRenderPipelineState(presentationPipeline)
            second.setFragmentTexture(offscreen, index: 0)
            var effectMode = frame.effectMode
            second.setFragmentBytes(&effectMode, length: MemoryLayout<UInt32>.stride, index: 0)
            second.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 6)
            second.endEncoding()
            commandBuffer.present(drawable)
            commandBuffer.commit()
        }
    }
}
