//
// Copyright (c) 2002-2026 The UbixOS Project.
//
// ContentView — the browser UI: a dataset sidebar + a directory table with a
// breadcrumb path bar and a toolbar for open / import / export / mkdir / delete.
//
import SwiftUI
import UniformTypeIdentifiers

// Entries are unique by name within a single directory listing — enough for the
// table's row identity.
extension UbixFSEntry: @retroactive Identifiable {
	public var id: String { name }
}

struct ContentView: View {
	@EnvironmentObject var model: PoolModel
	@State private var selection: String?
	@State private var showNewFolder = false
	@State private var newFolderName = ""

	var body: some View {
		NavigationSplitView {
			List(model.datasets, id: \.self, selection: $model.currentDataset) { ds in
				Label(ds, systemImage: "internaldrive").tag(ds)
			}
			.navigationSplitViewColumnWidth(min: 160, ideal: 200)
			.onChange(of: model.currentDataset) { _, new in
				if let new { model.select(dataset: new) }
			}
		} detail: {
			if model.isOpen {
				browser
			} else {
				ContentUnavailableView("No Image Open",
				                       systemImage: "externaldrive.badge.questionmark",
				                       description: Text("Open a UbixFS pool image (raw or a disk image) to browse it."))
			}
		}
		.navigationTitle(model.imageURL?.lastPathComponent ?? "UbixFS Browser")
		.toolbar { toolbarContent }
		.alert("Error",
		       isPresented: Binding(get: { model.errorMessage != nil },
		                            set: { if !$0 { model.errorMessage = nil } })) {
			Button("OK", role: .cancel) { model.errorMessage = nil }
		} message: {
			Text(model.errorMessage ?? "")
		}
		.alert("New Folder", isPresented: $showNewFolder) {
			TextField("Name", text: $newFolderName)
			Button("Create") {
				if !newFolderName.isEmpty { model.makeDirectory(named: newFolderName) }
				newFolderName = ""
			}
			Button("Cancel", role: .cancel) { newFolderName = "" }
		}
	}

	// MARK: - directory browser

	private var browser: some View {
		VStack(spacing: 0) {
			pathBar
			Divider()
			Table(model.entries, selection: $selection) {
				TableColumn("Name") { entry in
					Label(entry.name, systemImage: icon(for: entry))
						.contentShape(Rectangle())
						.onTapGesture(count: 2) { open(entry) }
				}
				TableColumn("Size") { entry in
					Text(entry.isDirectory ? "—" : byteString(entry.size))
						.foregroundStyle(.secondary).monospacedDigit()
				}
				.width(90)
				TableColumn("Mode") { entry in
					Text(String(format: "%04o", entry.mode))
						.foregroundStyle(.secondary).monospaced()
				}
				.width(70)
			}
			.contextMenu(forSelectionType: String.self) { _ in
				if let entry = selectedEntry {
					if !entry.isDirectory {
						Button("Export…") { exportEntry(entry) }
					}
					if !model.readOnly {
						Button("Delete", role: .destructive) { model.remove(entry) }
					}
				}
			}
			statusBar
		}
	}

	private var pathBar: some View {
		HStack(spacing: 6) {
			Button { model.goUp() } label: { Image(systemName: "chevron.up") }
				.disabled(model.path == "/")
			Button { model.refresh() } label: { Image(systemName: "arrow.clockwise") }
			Text(model.currentDataset.map { "\($0):" } ?? "")
				.foregroundStyle(.secondary)
			Text(model.path).fontWeight(.medium).lineLimit(1).truncationMode(.head)
			Spacer()
			if model.busy { ProgressView().controlSize(.small) }
		}
		.padding(.horizontal, 10).padding(.vertical, 6)
	}

	private var statusBar: some View {
		HStack {
			Text(model.status).font(.caption).foregroundStyle(.secondary).lineLimit(1)
			Spacer()
			Text("\(model.entries.count) items").font(.caption).foregroundStyle(.secondary)
		}
		.padding(.horizontal, 10).padding(.vertical, 4)
	}

	// MARK: - toolbar

	@ToolbarContentBuilder
	private var toolbarContent: some ToolbarContent {
		ToolbarItemGroup {
			Button { openImage() } label: { Label("Open Image", systemImage: "folder") }

			if model.isOpen {
				Toggle(isOn: Binding(get: { !model.readOnly },
				                     set: { _ in model.toggleWritable() })) {
					Label("Writing", systemImage: model.readOnly ? "lock" : "lock.open")
				}
				.help(model.readOnly ? "Enable writing (reopens read-write)" : "Read-write")

				Button { importFile() } label: { Label("Import", systemImage: "square.and.arrow.down") }
					.disabled(model.readOnly)
				Button { showNewFolder = true } label: { Label("New Folder", systemImage: "folder.badge.plus") }
					.disabled(model.readOnly)
				Button { if let e = selectedEntry, !e.isDirectory { exportEntry(e) } }
					label: { Label("Export", systemImage: "square.and.arrow.up") }
					.disabled(selectedEntry == nil || (selectedEntry?.isDirectory ?? true))
			}
		}
	}

	// MARK: - actions

	private var selectedEntry: UbixFSEntry? {
		model.entries.first { $0.name == selection }
	}

	private func open(_ entry: UbixFSEntry) {
		if entry.isDirectory { model.enter(entry) } else { exportEntry(entry) }
	}

	private func openImage() {
		let panel = NSOpenPanel()
		panel.allowsMultipleSelection = false
		panel.canChooseDirectories = false
		panel.message = "Choose a UbixFS pool image or disk image"
		if panel.runModal() == .OK, let url = panel.url {
			model.open(url: url, readOnly: true)
		}
	}

	private func importFile() {
		let panel = NSOpenPanel()
		panel.allowsMultipleSelection = false
		panel.canChooseDirectories = false
		panel.message = "Choose a file to copy into \(model.path)"
		if panel.runModal() == .OK, let url = panel.url {
			model.importFile(from: url)
		}
	}

	private func exportEntry(_ entry: UbixFSEntry) {
		let panel = NSSavePanel()
		panel.nameFieldStringValue = entry.name
		panel.message = "Export \(entry.name) out of the pool"
		if panel.runModal() == .OK, let url = panel.url {
			model.export(entry, to: url)
		}
	}

	// MARK: - formatting

	private func icon(for entry: UbixFSEntry) -> String {
		switch entry.kind {
		case .dir: return "folder.fill"
		case .symlink: return "arrow.up.right.square"
		default: return "doc"
		}
	}

	private func byteString(_ n: UInt64) -> String {
		ByteCountFormatter.string(fromByteCount: Int64(n), countStyle: .file)
	}
}
