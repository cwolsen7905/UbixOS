//
// Copyright (c) 2002-2026 The UbixOS Project.
//
// BrowserViewController — the AppKit UI built programmatically: a toolbar-style
// button bar, a path bar, a datasets/directory **tree** (NSOutlineView, lazily
// loaded), and a directory listing (NSTableView, columns name/size/mode).  The
// tree and the listing stay in sync both ways.  Drives a PoolStore.
//
import AppKit

/// A node in the sidebar tree: either a dataset (top level) or a directory
/// within one.  `children` is lazily populated (and cached) from the store, so
/// node identity is stable across reloads — which preserves expansion state.
final class FSNode {
	let dataset: String
	let path: String // absolute within the dataset; "/" for the dataset root
	let name: String // display name
	let isDataset: Bool
	var children: [FSNode]?

	init(dataset: String, path: String, name: String, isDataset: Bool) {
		self.dataset = dataset
		self.path = path
		self.name = name
		self.isDataset = isDataset
	}
}

final class BrowserViewController: NSViewController {
	private let store = PoolStore()

	private let outline = NSOutlineView()
	private let dirTable = NSTableView()
	private let pathLabel = NSTextField(labelWithString: "/")
	private let statusLabel = NSTextField(labelWithString: "")
	private let spinner = NSProgressIndicator()
	private let writableToggle = NSButton(checkboxWithTitle: "Writing", target: nil, action: nil)
	private let upButton = NSButton()
	private var importButton, exportButton, mkdirButton, deleteButton: NSButton!

	// Sidebar tree state.
	private var datasetNodes: [FSNode] = []
	private var cachedDatasets: [String] = []
	private var suppressTreeSelection = false
	private var needsTreeRefresh = false

	// MARK: - lifecycle

	override func loadView() {
		view = NSView(frame: NSRect(x: 0, y: 0, width: 820, height: 520))
		buildUI()
		wireStore()
	}

	override func viewDidAppear() {
		super.viewDidAppear()
		view.window?.title = "UbixFS Browser"
	}

	// MARK: - UI construction

	private func buildUI() {
		let bar = makeButtonBar()
		let pathBar = makePathBar()

		let treeScroll = scroll(for: outline)
		treeScroll.widthAnchor.constraint(equalToConstant: 240).isActive = true
		let treeCol = column("tree", "Files", 220)
		outline.addTableColumn(treeCol)
		outline.outlineTableColumn = treeCol
		outline.headerView = nil
		outline.indentationPerLevel = 14
		outline.autoresizesOutlineColumn = false
		outline.dataSource = self
		outline.delegate = self
		outline.target = self
		outline.doubleAction = #selector(treeDoubleClicked)

		let dirScroll = scroll(for: dirTable)
		dirTable.addTableColumn(column("name", "Name", 360))
		dirTable.addTableColumn(column("size", "Size", 90))
		dirTable.addTableColumn(column("mode", "Mode", 70))
		dirTable.dataSource = self
		dirTable.delegate = self
		dirTable.target = self
		dirTable.doubleAction = #selector(dirDoubleClicked)
		dirTable.usesAlternatingRowBackgroundColors = true
		dirTable.menu = makeContextMenu()

		let split = NSSplitView()
		split.isVertical = true
		split.dividerStyle = .thin
		split.addArrangedSubview(treeScroll)
		split.addArrangedSubview(dirScroll)
		split.translatesAutoresizingMaskIntoConstraints = false

		let statusBar = makeStatusBar()

		let stack = NSStackView(views: [bar, pathBar, split, statusBar])
		stack.orientation = .vertical
		stack.spacing = 0
		stack.alignment = .leading
		stack.translatesAutoresizingMaskIntoConstraints = false
		view.addSubview(stack)

		NSLayoutConstraint.activate([
			stack.topAnchor.constraint(equalTo: view.topAnchor),
			stack.leadingAnchor.constraint(equalTo: view.leadingAnchor),
			stack.trailingAnchor.constraint(equalTo: view.trailingAnchor),
			stack.bottomAnchor.constraint(equalTo: view.bottomAnchor),
			bar.leadingAnchor.constraint(equalTo: stack.leadingAnchor),
			bar.trailingAnchor.constraint(equalTo: stack.trailingAnchor),
			pathBar.leadingAnchor.constraint(equalTo: stack.leadingAnchor),
			pathBar.trailingAnchor.constraint(equalTo: stack.trailingAnchor),
			split.leadingAnchor.constraint(equalTo: stack.leadingAnchor),
			split.trailingAnchor.constraint(equalTo: stack.trailingAnchor),
			statusBar.leadingAnchor.constraint(equalTo: stack.leadingAnchor),
			statusBar.trailingAnchor.constraint(equalTo: stack.trailingAnchor),
		])

		updateControls()
	}

	private func makeButtonBar() -> NSView {
		let open = button("Open Image", #selector(openImage))
		importButton = button("Import", #selector(importFile))
		exportButton = button("Export", #selector(exportSelection))
		mkdirButton = button("New Folder", #selector(newFolder))
		deleteButton = button("Delete", #selector(deleteSelection))

		writableToggle.target = self
		writableToggle.action = #selector(toggleWritable)

		spinner.style = .spinning
		spinner.controlSize = .small
		spinner.isDisplayedWhenStopped = false
		spinner.translatesAutoresizingMaskIntoConstraints = false

		let spacer = NSView()
		spacer.translatesAutoresizingMaskIntoConstraints = false
		spacer.setContentHuggingPriority(.defaultLow, for: .horizontal)

		let bar = NSStackView(views: [open, importButton, exportButton, mkdirButton, deleteButton,
		                              writableToggle, spacer, spinner])
		bar.orientation = .horizontal
		bar.spacing = 8
		bar.edgeInsets = NSEdgeInsets(top: 8, left: 10, bottom: 8, right: 10)
		bar.translatesAutoresizingMaskIntoConstraints = false
		return bar
	}

	private func makePathBar() -> NSView {
		upButton.title = "↑"
		upButton.bezelStyle = .rounded
		upButton.target = self
		upButton.action = #selector(goUp)
		upButton.setContentHuggingPriority(.required, for: .horizontal)

		let refresh = button("⟳", #selector(refresh))
		refresh.setContentHuggingPriority(.required, for: .horizontal)

		pathLabel.lineBreakMode = .byTruncatingHead
		pathLabel.font = .monospacedSystemFont(ofSize: 12, weight: .medium)

		let bar = NSStackView(views: [upButton, refresh, pathLabel])
		bar.orientation = .horizontal
		bar.spacing = 6
		bar.edgeInsets = NSEdgeInsets(top: 4, left: 10, bottom: 4, right: 10)
		bar.translatesAutoresizingMaskIntoConstraints = false
		return bar
	}

	private func makeStatusBar() -> NSView {
		statusLabel.font = .systemFont(ofSize: 11)
		statusLabel.textColor = .secondaryLabelColor
		statusLabel.lineBreakMode = .byTruncatingMiddle
		let bar = NSStackView(views: [statusLabel])
		bar.orientation = .horizontal
		bar.edgeInsets = NSEdgeInsets(top: 4, left: 10, bottom: 4, right: 10)
		bar.translatesAutoresizingMaskIntoConstraints = false
		return bar
	}

	private func makeContextMenu() -> NSMenu {
		let menu = NSMenu()
		menu.addItem(NSMenuItem(title: "Export…", action: #selector(exportSelection), keyEquivalent: ""))
		menu.addItem(NSMenuItem(title: "Delete", action: #selector(deleteSelection), keyEquivalent: ""))
		return menu
	}

	private func button(_ title: String, _ action: Selector) -> NSButton {
		let b = NSButton(title: title, target: self, action: action)
		b.bezelStyle = .rounded
		b.setContentHuggingPriority(.required, for: .horizontal)
		return b
	}

	private func column(_ id: String, _ title: String, _ width: CGFloat) -> NSTableColumn {
		let c = NSTableColumn(identifier: NSUserInterfaceItemIdentifier(id))
		c.title = title
		c.width = width
		return c
	}

	private func scroll(for documentView: NSView) -> NSScrollView {
		let s = NSScrollView()
		s.documentView = documentView
		s.hasVerticalScroller = true
		s.translatesAutoresizingMaskIntoConstraints = false
		return s
	}

	// MARK: - store wiring

	private func wireStore() {
		store.onChange = { [weak self] in self?.reload() }
		store.onError = { [weak self] msg in self?.present(error: msg) }
	}

	private func reload() {
		dirTable.reloadData()
		pathLabel.stringValue = (store.currentDataset.map { "\($0):" } ?? "") + store.path
		statusLabel.stringValue = "\(store.statusText)   ·   \(store.entries.count) items"
		if store.busy { spinner.startAnimation(nil) } else { spinner.stopAnimation(nil) }
		updateControls()
		// Only touch the tree when idle — its lazy reads block on the serial queue,
		// and we don't want to stall behind an in-flight import/export.
		if !store.busy { syncTree() }
	}

	private func updateControls() {
		writableToggle.state = store.readOnly ? .off : .on
		writableToggle.isEnabled = store.isOpen
		upButton.isEnabled = store.isOpen && store.path != "/"
		let fileSelected = selectedEntry.map { !$0.isDirectory } ?? false
		let haveSelection = selectedEntry != nil
		importButton.isEnabled = store.isOpen && !store.readOnly
		mkdirButton.isEnabled = store.isOpen && !store.readOnly
		exportButton.isEnabled = fileSelected
		deleteButton.isEnabled = store.isOpen && !store.readOnly && haveSelection
	}

	// MARK: - sidebar tree

	private func children(of node: FSNode) -> [FSNode] {
		if let c = node.children { return c }
		let subs = store.subdirectories(dataset: node.dataset, path: node.path)
		let nodes = subs.map { name in
			FSNode(dataset: node.dataset,
			       path: node.path == "/" ? "/\(name)" : "\(node.path)/\(name)",
			       name: name,
			       isDataset: false)
		}
		node.children = nodes
		return nodes
	}

	/// Resolve (dataset, path) to its tree node, loading children along the way.
	private func node(forDataset ds: String?, path: String) -> FSNode? {
		guard let ds, let root = datasetNodes.first(where: { $0.dataset == ds }) else { return nil }
		var node = root
		if path != "/" {
			for comp in path.split(separator: "/").map(String.init) {
				guard let next = children(of: node).first(where: { $0.name == comp }) else { return nil }
				node = next
			}
		}
		return node
	}

	private func syncTree() {
		if store.datasets != cachedDatasets {
			cachedDatasets = store.datasets
			datasetNodes = store.datasets.map { FSNode(dataset: $0, path: "/", name: $0, isDataset: true) }
			outline.reloadData()
		}
		if needsTreeRefresh {
			needsTreeRefresh = false
			if let n = node(forDataset: store.currentDataset, path: store.path) {
				n.children = nil
				outline.reloadItem(n, reloadChildren: true)
			}
		}
		if let ds = store.currentDataset {
			revealInTree(dataset: ds, path: store.path)
		}
	}

	/// Expand to and select the node for (dataset, path) without re-triggering navigation.
	private func revealInTree(dataset ds: String, path: String) {
		guard let root = datasetNodes.first(where: { $0.dataset == ds }) else { return }
		suppressTreeSelection = true
		defer { suppressTreeSelection = false }
		outline.expandItem(root)
		var node = root
		if path != "/" {
			for comp in path.split(separator: "/").map(String.init) {
				outline.expandItem(node)
				guard let next = children(of: node).first(where: { $0.name == comp }) else { break }
				node = next
			}
		}
		let row = outline.row(forItem: node)
		if row >= 0 {
			outline.selectRowIndexes(IndexSet(integer: row), byExtendingSelection: false)
			outline.scrollRowToVisible(row)
		}
	}

	// MARK: - actions

	/// Menu hook (must be non-private so main.swift can reference the selector).
	@objc func openImageMenu() { openImage() }

	@objc private func openImage() {
		let panel = NSOpenPanel()
		panel.canChooseDirectories = false
		panel.allowsMultipleSelection = false
		panel.message = "Choose a UbixFS pool image or disk image"
		if panel.runModal() == .OK, let url = panel.url { store.open(url: url, readOnly: true) }
	}

	@objc func closeImage() { store.close() }

	@objc private func toggleWritable() { store.toggleWritable() }

	@objc private func goUp() { store.goUp() }
	@objc private func refresh() { needsTreeRefresh = true; store.refresh() }

	@objc private func treeDoubleClicked() {
		// Single-click already navigates via selection; double-click toggles expansion.
		let row = outline.clickedRow
		guard row >= 0, let node = outline.item(atRow: row) as? FSNode else { return }
		if outline.isItemExpanded(node) { outline.collapseItem(node) } else { outline.expandItem(node) }
	}

	@objc private func dirDoubleClicked() {
		guard let entry = selectedEntry else { return }
		if entry.isDirectory { store.enter(entry) } else { exportSelection() }
	}

	@objc private func importFile() {
		let panel = NSOpenPanel()
		panel.canChooseDirectories = false
		panel.allowsMultipleSelection = false
		panel.message = "Choose a file to copy into \(store.path)"
		if panel.runModal() == .OK, let url = panel.url { store.importFile(from: url) }
	}

	@objc private func exportSelection() {
		guard let entry = selectedEntry, !entry.isDirectory else { return }
		let panel = NSSavePanel()
		panel.nameFieldStringValue = entry.name
		panel.message = "Export \(entry.name) out of the pool"
		if panel.runModal() == .OK, let url = panel.url { store.export(entry, to: url) }
	}

	@objc private func newFolder() {
		let alert = NSAlert()
		alert.messageText = "New Folder"
		alert.addButton(withTitle: "Create")
		alert.addButton(withTitle: "Cancel")
		let field = NSTextField(frame: NSRect(x: 0, y: 0, width: 220, height: 24))
		field.placeholderString = "name"
		alert.accessoryView = field
		if alert.runModal() == .alertFirstButtonReturn, !field.stringValue.isEmpty {
			needsTreeRefresh = true
			store.makeDirectory(named: field.stringValue)
		}
	}

	@objc private func deleteSelection() {
		guard let entry = selectedEntry else { return }
		let alert = NSAlert()
		alert.messageText = "Delete \(entry.name)?"
		alert.informativeText = entry.isDirectory ? "The directory must be empty." : "This cannot be undone."
		alert.alertStyle = .warning
		alert.addButton(withTitle: "Delete")
		alert.addButton(withTitle: "Cancel")
		if alert.runModal() == .alertFirstButtonReturn {
			needsTreeRefresh = true
			store.remove(entry)
		}
	}

	private var selectedEntry: UbixFSEntry? {
		let row = dirTable.selectedRow
		return (row >= 0 && row < store.entries.count) ? store.entries[row] : nil
	}

	private func present(error msg: String) {
		let alert = NSAlert()
		alert.messageText = "Error"
		alert.informativeText = msg
		alert.alertStyle = .warning
		alert.runModal()
	}

	fileprivate func icon(for entry: UbixFSEntry) -> String {
		switch entry.kind {
		case .dir: return "📁"
		case .symlink: return "🔗"
		default: return "📄"
		}
	}

	fileprivate func byteString(_ n: UInt64) -> String {
		ByteCountFormatter.string(fromByteCount: Int64(n), countStyle: .file)
	}

	fileprivate func cellView(_ string: String, mono: Bool = false, secondary: Bool = false,
	                          rightAligned: Bool = false) -> NSView {
		let cell = NSTableCellView()
		let text = NSTextField(labelWithString: string)
		text.translatesAutoresizingMaskIntoConstraints = false
		text.lineBreakMode = .byTruncatingTail
		if mono { text.font = .monospacedDigitSystemFont(ofSize: 12, weight: .regular) }
		if secondary { text.textColor = .secondaryLabelColor }
		if rightAligned { text.alignment = .right }
		cell.addSubview(text)
		cell.textField = text
		NSLayoutConstraint.activate([
			text.leadingAnchor.constraint(equalTo: cell.leadingAnchor, constant: 4),
			text.trailingAnchor.constraint(equalTo: cell.trailingAnchor, constant: -4),
			text.centerYAnchor.constraint(equalTo: cell.centerYAnchor),
		])
		return cell
	}
}

// MARK: - directory table

extension BrowserViewController: NSTableViewDataSource, NSTableViewDelegate {
	func numberOfRows(in tableView: NSTableView) -> Int { store.entries.count }

	func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
		let entry = store.entries[row]
		switch tableColumn?.identifier.rawValue {
		case "name":
			return cellView(icon(for: entry) + "  " + entry.name)
		case "size":
			return cellView(entry.isDirectory ? "—" : byteString(entry.size),
			                mono: true, secondary: true, rightAligned: true)
		case "mode":
			return cellView(String(format: "%04o", entry.mode), mono: true, secondary: true)
		default:
			return nil
		}
	}

	func tableViewSelectionDidChange(_ notification: Notification) {
		if (notification.object as? NSTableView) === dirTable { updateControls() }
	}
}

// MARK: - sidebar outline

extension BrowserViewController: NSOutlineViewDataSource, NSOutlineViewDelegate {
	func outlineView(_ outlineView: NSOutlineView, numberOfChildrenOfItem item: Any?) -> Int {
		guard let node = item as? FSNode else { return datasetNodes.count }
		return children(of: node).count
	}

	func outlineView(_ outlineView: NSOutlineView, child index: Int, ofItem item: Any?) -> Any {
		guard let node = item as? FSNode else { return datasetNodes[index] }
		return children(of: node)[index]
	}

	func outlineView(_ outlineView: NSOutlineView, isItemExpandable item: Any) -> Bool {
		guard let node = item as? FSNode else { return false }
		return children(of: node).count > 0
	}

	func outlineView(_ outlineView: NSOutlineView, viewFor tableColumn: NSTableColumn?, item: Any) -> NSView? {
		guard let node = item as? FSNode else { return nil }
		return cellView((node.isDataset ? "🗄  " : "📁  ") + node.name)
	}

	func outlineViewSelectionDidChange(_ notification: Notification) {
		guard !suppressTreeSelection else { return }
		let row = outline.selectedRow
		guard row >= 0, let node = outline.item(atRow: row) as? FSNode else { return }
		store.navigate(dataset: node.dataset, to: node.path)
	}
}
