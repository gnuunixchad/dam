const std = @import("std");
const Build = std.Build;
const fs = std.fs;
const mem = std.mem;

pub fn build(b: *Build) void {
	const target = b.standardTargetOptions(.{});
	const optimize = b.standardOptimizeOption(.{});

	const dam = b.addExecutable(.{
		.name = "dam",
		.target = target,
		.optimize = optimize,
	});
	dam.addIncludePath(b.path(""));
	dam.addCSourceFile(.{
		.file = b.path("dam.c"),
	});

	dam.linkLibC();
	dam.linkSystemLibrary("wayland-client");
	dam.linkSystemLibrary("fcft");
	dam.linkSystemLibrary("pixman-1");

	const scanner = Scanner.create(b, .{}, dam);
	scanner.addSystemProtocol("/stable/xdg-shell/xdg-shell.xml");
	scanner.addCustomProtocol("wlr-layer-shell-unstable-v1.xml");
	scanner.addCustomProtocol("river-control-unstable-v1.xml");
	scanner.addCustomProtocol("river-status-unstable-v1.xml");

	b.installArtifact(dam);
}

pub const Scanner = struct {
	build: *Build,
	wayland_protocols_path: []const u8,
	wayland_scanner_path: []const u8,
	compile: *Build.Step.Compile,

	const opts = struct {
		wayland_protocols_path: ?[]const u8 = null,
		wayland_scanner_path: ?[]const u8 = null,
	};

	pub fn create(b: *Build, opt: opts, c: *Build.Step.Compile) *Scanner {
		const wayland_protocols_path = opt.wayland_protocols_path orelse blk: {
			const pathr = b.run(&.{ "pkg-config", "--variable=pkgdatadir", "wayland-protocols" });
			break :blk mem.trim(u8, pathr, &std.ascii.whitespace);
		};
		const scanner_path = opt.wayland_scanner_path orelse blk: {
			const pathr = b.run(&.{ "pkg-config", "--variable=wayland_scanner", "wayland-scanner" });
			break :blk mem.trim(u8, pathr, &std.ascii.whitespace);
		};

		const scanner = b.allocator.create(Scanner) catch @panic("OOM");
		scanner.* = .{
			.wayland_protocols_path = wayland_protocols_path,
			.wayland_scanner_path = scanner_path,
			.build = b,
			.compile = c,
		};

		return scanner;
	}

	pub fn addSystemProtocol(scanner: *Scanner, relative_path: []const u8) void {
		const full_path = scanner.build.pathJoin(&.{ scanner.wayland_protocols_path, relative_path });
		scanner.addCustomProtocol(full_path);
	}

	pub fn addCustomProtocol(scanner: *Scanner, path: []const u8) void {
		scanner.generateCHeader(path);
		scanner.generateCode(path);
	}

	fn generateCHeader(scanner: *Scanner, protocol: []const u8) void {
		const cmd = scanner.build.addSystemCommand(&.{ "wayland-scanner", "client-header", protocol });
		const out_name = mem.concat(scanner.build.allocator, u8, &.{ fs.path.stem(protocol), "-protocol.h" }) catch @panic("OOM");

		const c_header = cmd.addOutputFileArg(out_name);
		scanner.compile.addIncludePath(c_header.dirname());
	}

	fn generateCode(scanner: *Scanner, protocol: []const u8) void {
		const cmd = scanner.build.addSystemCommand(&.{ "wayland-scanner", "private-code", protocol });
		const out_name = mem.concat(scanner.build.allocator, u8, &.{ fs.path.stem(protocol), "-protocol.c" }) catch @panic("OOM");

		const c_file = cmd.addOutputFileArg(out_name);
		scanner.compile.addCSourceFile(.{
			.file = c_file,
		});
	}
};
