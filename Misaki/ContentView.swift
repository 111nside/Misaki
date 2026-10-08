import SwiftUI
import UniformTypeIdentifiers

struct ContentView: View {
    @State private var showingImporter = false
    @State private var fileName: String?
    @State private var header: ELFHeader?
    @State private var fileSize: Int64?
    @State private var errorMessage: String?

    var body: some View {
        NavigationStack {
            List {
                Section {
                    HStack(spacing: 14) {
                        Image(systemName: "gamecontroller.fill")
                            .font(.system(size: 42))
                            .foregroundStyle(.indigo)
                        VStack(alignment: .leading, spacing: 4) {
                            Text("Misaki").font(.largeTitle.bold())
                            Text("PS4 emulation research for iOS")
                                .foregroundStyle(.secondary)
                        }
                    }.padding(.vertical, 8)
                }
                Section("Import executable") {
                    Button {
                        showingImporter = true
                    } label: {
                        Label("Choose ELF file", systemImage: "square.and.arrow.down")
                    }
                    Text("Inspects a standard ELF64 header without executing code.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                if let fileName {
                    Section("File details") {
                        LabeledContent("Name", value: fileName)
                        if let fileSize {
                            LabeledContent("Size", value: ByteCountFormatter.string(fromByteCount: fileSize, countStyle: .file))
                        }
                        if let header {
                            LabeledContent("Architecture", value: header.machineLabel)
                            LabeledContent("ELF type", value: String(header.objectType))
                            LabeledContent("Entry point", value: String(format: "0x%llX", header.entryPoint))
                            LabeledContent("Program headers", value: String(header.programHeaderCount))
                        }
                    }
                }
                Section("Emulation status") {
                    Label("Executable inspection: prototype", systemImage: "checkmark.circle")
                    Label("CPU execution: not implemented", systemImage: "cpu")
                    Label("Graphics rendering: not implemented", systemImage: "display")
                    Label("PS4 game booting: not implemented", systemImage: "lock")
                }
            }
            .navigationTitle("Misaki")
            .fileImporter(isPresented: $showingImporter, allowedContentTypes: [.data, .item]) { result in
                switch result {
                case .failure(let error): errorMessage = error.localizedDescription
                case .success(let url): inspectFile(at: url)
                }
            }
            .alert("Cannot inspect file", isPresented: Binding(
                get: { errorMessage != nil },
                set: { if !$0 { errorMessage = nil } }
            )) {
                Button("OK", role: .cancel) { errorMessage = nil }
            } message: {
                Text(errorMessage ?? "Unknown error")
            }
        }
    }

    private func inspectFile(at url: URL) {
        let access = url.startAccessingSecurityScopedResource()
        defer { if access { url.stopAccessingSecurityScopedResource() } }
        do {
            let handle = try FileHandle(forReadingFrom: url)
            defer { try? handle.close() }
            let attributes = try FileManager.default.attributesOfItem(atPath: url.path)
            let prefix = try handle.read(upToCount: 64) ?? Data()
            let parsed = try ELFInspector.inspect(prefix)
            fileName = url.lastPathComponent
            fileSize = (attributes[.size] as? NSNumber)?.int64Value
            header = parsed
            errorMessage = nil
        } catch {
            fileName = nil
            fileSize = nil
            header = nil
            errorMessage = error.localizedDescription
        }
    }
}
