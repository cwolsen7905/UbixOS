//
// Copyright (c) 2002-2026 The UbixOS Project.
//
// PoolStore — the model backing the browser: wraps a UbixFSSession, funnels all
// session access onto one serial queue (the C facade is not thread-safe), and
// publishes state changes to the AppKit UI via the onChange / onError callbacks
// (always invoked on the main thread).  See docs/design/ubixfs-mac-browser-plan.md.
//
import Foundation

final class PoolStore {
	private(set) var imageURL: URL?
	private(set) var readOnly = true
	private(set) var datasets: [String] = []
	private(set) var currentDataset: String?
	private(set) var path = "/"
	private(set) var entries: [UbixFSEntry] = []
	private(set) var statusText = "Open a UbixFS pool image to begin."
	private(set) var busy = false

	/// UI hooks (invoked on the main thread).
	var onChange: (() -> Void)?
	var onError: ((String) -> Void)?

	// Touched only on `queue`.
	private var session: UbixFSSession?
	private let queue = DispatchQueue(label: "org.ubixos.ubixfskit.session")

	var isOpen: Bool { imageURL != nil }

	// MARK: - plumbing

	private func main(_ block: @escaping () -> Void) { DispatchQueue.main.async(execute: block) }
	private func fail(_ msg: String) { main { self.busy = false; self.onError?(msg) } }
	private func publish(_ mutate: @escaping () -> Void) {
		main { self.busy = false; mutate(); self.onChange?() }
	}

	/// Run a session op off-main against the current dataset; deliver its value to
	/// `done` on the main thread.  Always re-selects `currentDataset` first so the
	/// op is correct even if the tree just read a different dataset's subtree.
	private func perform<T>(_ work: @escaping (UbixFSSession) throws -> T,
	                        done: @escaping (T) -> Void) {
		guard let s = session, let ds = currentDataset else { return }
		busy = true
		onChange?()
		queue.async {
			do {
				try s.openDataset(ds)
				let v = try work(s)
				self.publish { done(v) }
			} catch {
				self.fail(error.localizedDescription)
			}
		}
	}

	/// Load a directory listing for an explicit dataset + path (the one navigation
	/// primitive).  Re-opens the dataset every time so dataset context is never
	/// implicit — this is what lets the tree freely switch datasets.
	private func load(dataset ds: String, path p: String) {
		busy = true
		onChange?()
		queue.async {
			guard let s = self.session else { return }
			do {
				try s.openDataset(ds)
				let items = try s.readDirectory(atPath: p)
				self.publish {
					self.currentDataset = ds
					self.path = p
					self.entries = items.sortedForDisplay()
				}
			} catch {
				self.fail(error.localizedDescription)
			}
		}
	}

	/// Synchronously list a directory's *subdirectories* (for the tree).  Reads on
	/// the serial queue and restores the active-pane dataset afterwards so the
	/// async navigation/edit paths stay consistent.  Directory reads are cheap
	/// (cached objset + a few block reads); only called when not busy.
	func subdirectories(dataset ds: String, path p: String) -> [String] {
		var names: [String] = []
		queue.sync {
			guard let s = session else { return }
			let restore = currentDataset
			defer { if let r = restore, r != ds { try? s.openDataset(r) } }
			do {
				try s.openDataset(ds)
				let items = try s.readDirectory(atPath: p)
				names = items.filter { $0.isDirectory }.map { $0.name }
					.sorted { $0.localizedStandardCompare($1) == .orderedAscending }
			} catch {
				// leave names empty on error
			}
		}
		return names
	}

	// MARK: - open / close

	func open(url: URL, readOnly: Bool) {
		busy = true
		onChange?()
		queue.async {
			do {
				let s = try UbixFSSession(imagePath: url.path, readOnly: readOnly)
				let ds = try s.datasets()
				self.session?.close()
				self.session = s
				self.publish {
					self.imageURL = url
					self.readOnly = readOnly
					self.datasets = ds
					self.statusText = "\(url.lastPathComponent) — \(ds.count) dataset(s)"
						+ (readOnly ? ", read-only" : ", read-write")
				}
				if let first = ds.first { self.select(dataset: first) }
			} catch {
				self.fail(error.localizedDescription)
			}
		}
	}

	func close() {
		queue.async {
			self.session?.close()
			self.session = nil
			self.publish {
				self.imageURL = nil
				self.datasets = []
				self.currentDataset = nil
				self.entries = []
				self.path = "/"
				self.statusText = "Open a UbixFS pool image to begin."
			}
		}
	}

	/// Reopen the current image with the opposite read/write mode.
	func toggleWritable() {
		guard let url = imageURL else { return }
		open(url: url, readOnly: !readOnly)
	}

	// MARK: - navigation

	func select(dataset: String) { load(dataset: dataset, path: "/") }

	func navigate(to newPath: String) {
		guard let ds = currentDataset else { return }
		load(dataset: ds, path: newPath)
	}

	/// Navigate to a path in a specific dataset (used by tree selection).
	func navigate(dataset ds: String, to newPath: String) { load(dataset: ds, path: newPath) }

	func enter(_ entry: UbixFSEntry) {
		guard entry.isDirectory else { return }
		navigate(to: join(path, entry.name))
	}

	func goUp() {
		guard path != "/" else { return }
		navigate(to: (path as NSString).deletingLastPathComponent)
	}

	func refresh() { navigate(to: path) }

	// MARK: - copy in / out

	func export(_ entry: UbixFSEntry, to hostURL: URL) {
		let src = join(path, entry.name)
		perform({ try $0.readFile(atPath: src) },
		        done: { data in
			        do { try data.write(to: hostURL) }
			        catch { self.onError?(error.localizedDescription) }
		        })
	}

	func importFile(from hostURL: URL) {
		let dst = join(path, hostURL.lastPathComponent)
		let data: Data
		do { data = try Data(contentsOf: hostURL) }
		catch { onError?(error.localizedDescription); return }
		perform({ s -> [UbixFSEntry] in
			try s.write(data, toPath: dst, mode: 0o644)
			return try s.readDirectory(atPath: self.path)
		}, done: { self.entries = $0.sortedForDisplay() })
	}

	func makeDirectory(named name: String) {
		let dst = join(path, name)
		perform({ s -> [UbixFSEntry] in
			try s.createDirectory(atPath: dst, mode: 0o755)
			return try s.readDirectory(atPath: self.path)
		}, done: { self.entries = $0.sortedForDisplay() })
	}

	func remove(_ entry: UbixFSEntry) {
		let target = join(path, entry.name)
		perform({ s -> [UbixFSEntry] in
			try s.removeItem(atPath: target)
			return try s.readDirectory(atPath: self.path)
		}, done: { self.entries = $0.sortedForDisplay() })
	}

	// MARK: - helpers

	private func join(_ base: String, _ name: String) -> String {
		base == "/" ? "/\(name)" : "\(base)/\(name)"
	}
}

extension Array where Element == UbixFSEntry {
	/// Directories first, then files, each alphabetical.
	func sortedForDisplay() -> [UbixFSEntry] {
		sorted { a, b in
			if a.isDirectory != b.isDirectory { return a.isDirectory }
			return a.name.localizedStandardCompare(b.name) == .orderedAscending
		}
	}
}
