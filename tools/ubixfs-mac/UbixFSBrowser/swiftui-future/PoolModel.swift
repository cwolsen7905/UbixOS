//
// Copyright (c) 2002-2026 The UbixOS Project.
//
// PoolModel — the observable view model wrapping a UbixFSSession.  All session
// access is funnelled onto one serial queue (the C facade is not thread-safe);
// @Published state is mutated only on the main actor.  See
// docs/design/ubixfs-mac-browser-plan.md.
//
import Foundation
import Combine

@MainActor
final class PoolModel: ObservableObject {
	@Published var imageURL: URL?
	@Published var readOnly = true
	@Published var datasets: [String] = []
	@Published var currentDataset: String?
	@Published var path = "/"
	@Published var entries: [UbixFSEntry] = []
	@Published var status = "Open a UbixFS pool image to begin."
	@Published var errorMessage: String?
	@Published var busy = false

	// Touched only on `queue`.
	private var session: UbixFSSession?
	private let queue = DispatchQueue(label: "org.ubixos.ubixfskit.session")

	var isOpen: Bool { imageURL != nil }

	/// Run a session operation off-main, deliver its Result back on the main actor.
	private func run<T>(_ work: @escaping (UbixFSSession) throws -> T,
	                    then done: @escaping (T) -> Void) {
		guard session != nil else { return }
		busy = true
		queue.async {
			let result: Result<T, Error>
			do { result = .success(try work(self.session!)) }
			catch { result = .failure(error) }
			DispatchQueue.main.async {
				self.busy = false
				switch result {
				case .success(let value): done(value)
				case .failure(let err): self.errorMessage = err.localizedDescription
				}
			}
		}
	}

	// MARK: - open / close

	func open(url: URL, readOnly: Bool) {
		busy = true
		queue.async {
			let result: Result<(UbixFSSession, [String]), Error>
			do {
				let s = try UbixFSSession(imagePath: url.path, readOnly: readOnly)
				let ds = try s.datasets()
				result = .success((s, ds))
			} catch {
				result = .failure(error)
			}
			DispatchQueue.main.async {
				self.busy = false
				switch result {
				case .failure(let err):
					self.errorMessage = err.localizedDescription
				case .success(let (s, ds)):
					self.session = s
					self.imageURL = url
					self.readOnly = readOnly
					self.datasets = ds
					self.status = "\(url.lastPathComponent) — \(ds.count) dataset(s)\(readOnly ? ", read-only" : ", read-write")"
					if let first = ds.first { self.select(dataset: first) }
				}
			}
		}
	}

	func close() {
		queue.async {
			self.session?.close()
			self.session = nil
			DispatchQueue.main.async {
				self.imageURL = nil
				self.datasets = []
				self.currentDataset = nil
				self.entries = []
				self.path = "/"
				self.status = "Open a UbixFS pool image to begin."
			}
		}
	}

	/// Reopen the current image with the opposite read/write mode.
	func toggleWritable() {
		guard let url = imageURL else { return }
		let wantReadOnly = !readOnly
		close()
		open(url: url, readOnly: wantReadOnly)
	}

	// MARK: - navigation

	func select(dataset: String) {
		run({ s in
			try s.openDataset(dataset)
			return try s.readDirectory(atPath: "/")
		}, then: { items in
			self.currentDataset = dataset
			self.path = "/"
			self.entries = items.sortedForDisplay()
		})
	}

	func navigate(to newPath: String) {
		run({ try $0.readDirectory(atPath: newPath) },
		    then: { items in
			    self.path = newPath
			    self.entries = items.sortedForDisplay()
		    })
	}

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

	/// Export `entry` (a file in the current dir) to a host URL.
	func export(_ entry: UbixFSEntry, to hostURL: URL) {
		let src = join(path, entry.name)
		run({ try $0.readFile(atPath: src) },
		    then: { data in
			    do { try data.write(to: hostURL) }
			    catch { self.errorMessage = error.localizedDescription }
		    })
	}

	/// Import a host file into the current directory.
	func importFile(from hostURL: URL) {
		let dst = join(path, hostURL.lastPathComponent)
		let data: Data
		do { data = try Data(contentsOf: hostURL) }
		catch { errorMessage = error.localizedDescription; return }
		run({ s -> [UbixFSEntry] in
			try s.writeData(data, toPath: dst, mode: 0o644)
			return try s.readDirectory(atPath: self.path)
		}, then: { items in self.entries = items.sortedForDisplay() })
	}

	func makeDirectory(named name: String) {
		let dst = join(path, name)
		run({ s -> [UbixFSEntry] in
			try s.createDirectory(atPath: dst, mode: 0o755)
			return try s.readDirectory(atPath: self.path)
		}, then: { items in self.entries = items.sortedForDisplay() })
	}

	func remove(_ entry: UbixFSEntry) {
		let target = join(path, entry.name)
		run({ s -> [UbixFSEntry] in
			try s.removeItem(atPath: target)
			return try s.readDirectory(atPath: self.path)
		}, then: { items in self.entries = items.sortedForDisplay() })
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
