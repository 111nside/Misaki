import SwiftUI
import UniformTypeIdentifiers
import Foundation

struct Milestone56to60View: View {
    @State private var report: NativeMega60Snapshot?
    @State private var analysis: NativeCompatibility60Snapshot?
    @State private var fileLabel: String?
    @State private var fileError: String?
    @State private var importing = false
    @State private var inspecting = false

    private let subsystems = [
        "56 · SCE dynamic dependencies and relocations",
        "57 · Bounded, process-isolated test services",
        "58 · Process-owned GPU surfaces and fences",
        "59 · Persistent cooperative guest processes",
        "60 · Read-only executable inspection"
    ]

    var body: some View {
        NavigationStack {
            List {
                Section("Milestones 56–60 · Combined regression") {
                    Button {
                        report = NativeMega60API.run()
                    } label: {
                        Label("Run five-subsystem diagnostic", systemImage: "checklist")
                    }
                    if let r = report {
                        Text("Milestones 56–60: \(r.passed ? "PASS" : "FAIL")\n" +
                             "ELF dependencies / RELA=\(r.dependencies)/\(r.relocations)\n" +
                             "Guest services / rejected=\(r.serviceCalls)/\(r.serviceFailures)\n" +
                             "GPU pixels / copies=\(r.pixels)/\(r.gpuCopies)\n" +
                             "Processes / exits=\(r.guestProcesses)/\(r.guestExits)\n" +
                             "Guest instructions=\(r.guestInstructions)\n" +
                             "Context switches=\(r.contextSwitches)\n" +
                             "Encrypted SELF segments=\(r.encryptedSelfSegments)\n" +
                             "Real firmware blockers=\(r.blockers)\n" +
                             "Boot attempted=\(r.firmwareAttempted)\n" +
                             "Home menu booted=\(r.homeMenuBooted)\n" +
                             "Status=\(r.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                Section("Five independently tested components") {
                    ForEach(subsystems.indices, id: \.self) { i in
                        HStack {
                            Text(subsystems[i]).font(.subheadline)
                            Spacer()
                            if let r = report {
                                Label(r.stages[i] ? "PASS" : "FAIL",
                                      systemImage: r.stages[i] ? "checkmark.circle" : "xmark.circle")
                                    .font(.caption)
                                    .foregroundStyle(r.stages[i] ? .green : .red)
                            }
                        }
                    }
                }
                Section("Inspect your own PS4 executable") {
                    Button {
                        importing = true
                    } label: {
                        Label("Choose SELF / SPRX / ELF file", systemImage: "doc.text.magnifyingglass")
                    }
                    .disabled(inspecting)
                    if inspecting {
                        ProgressView("Inspecting a bounded file prefix…")
                    }
                    Text("Imports a file using the iOS Files picker and inspects up to 8 MiB of its headers locally. No decryption, execution, uploading, or firmware boot is attempted. Large files are marked as partially inspected.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    if let fileLabel {
                        Text("Selected: \(fileLabel)").font(.footnote)
                    }
                    if let fileError {
                        Text(fileError).foregroundStyle(.red).font(.footnote)
                    }
                    if let a = analysis {
                        Text("Format: \(a.kindLabel)\n" +
                             "Recognized=\(a.recognized)\n" +
                             "Encrypted segments=\(a.encryptedSegments)\n" +
                             "Partial inspection=\(a.partial)\n" +
                             "Required modules identified=\(a.neededModules)\n" +
                             "Compatibility findings=\(a.findings)\n" +
                             "Execution refused=\(a.executionRefused)\n" +
                             "Status=\(a.status)")
                            .font(.system(.footnote, design: .monospaced))
                        Text(a.text)
                            .font(.system(.caption, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                Section("Limitations") {
                    Text("Successful diagnostic fixtures are not evidence that real PS4 software can boot. Encrypted Sony SELF content, system libraries, full GCN wavefront execution, and firmware initialization remain unsupported. Imported executables are examined only, never run.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("PS4 Analyzer")
            .fileImporter(isPresented: $importing, allowedContentTypes: [.item],
                          allowsMultipleSelection: false) { result in
                switch result {
                case .success(let files):
                    guard let first = files.first else {
                        fileError = "No file was selected."
                        return
                    }
                    inspectFile(first)
                case .failure(let error):
                    fileError = "File selection failed: \(error.localizedDescription)"
                }
            }
        }
    }

    private func inspectFile(_ url: URL) {
        inspecting = true
        analysis = nil
        fileError = nil
        fileLabel = url.lastPathComponent
        DispatchQueue.global(qos: .userInitiated).async {
            let scoped = url.startAccessingSecurityScopedResource()
            defer { if scoped { url.stopAccessingSecurityScopedResource() } }
            do {
                let attributes = try FileManager.default.attributesOfItem(atPath: url.path)
                guard attributes[.type] as? FileAttributeType == .typeRegular else {
                    throw NSError(domain: "MisakiInspection", code: 1,
                                  userInfo: [NSLocalizedDescriptionKey: "Choose a regular file, not a folder."])
                }
                let fileLength = (attributes[.size] as? NSNumber)?.uint64Value ?? 0
                let handle = try FileHandle(forReadingFrom: url)
                defer { try? handle.close() }
                let prefix = try handle.read(upToCount: 8 * 1024 * 1024) ?? Data()
                guard prefix.count >= 4, fileLength >= UInt64(prefix.count) else {
                    throw NSError(domain: "MisakiInspection", code: 2,
                                  userInfo: [NSLocalizedDescriptionKey: "File is empty or its size could not be verified."])
                }
                let result = NativeMega60API.analyze(prefix: prefix, totalSize: fileLength)
                DispatchQueue.main.async {
                    self.analysis = result
                    self.inspecting = false
                }
            } catch {
                DispatchQueue.main.async {
                    self.fileError = "Cannot inspect file: \(error.localizedDescription)"
                    self.inspecting = false
                }
            }
        }
    }
}
