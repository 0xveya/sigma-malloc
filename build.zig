const std = @import("std");

const base_c_flags = [_][]const u8{
    "-std=c23",
    "-fblocks",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wno-auto-decl-extensions",
    "-Wshadow",
    "-Wconversion",
    "-Wdouble-promotion",
    "-Wformat=2",
    "-Wundef",
};

const release_c_flags = base_c_flags ++ [_][]const u8{
    "-fno-common",
};

const dev_c_flags = base_c_flags;

const test_dev_flags = dev_c_flags ++ [_][]const u8{
    "-fno-sanitize=all",
};

const test_release_flags = release_c_flags ++ [_][]const u8{
    "-fno-sanitize=all",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const horny_mode = b.option(bool, "horny-mode", "Enable HORNY_MODE") orelse (optimize == .Debug);
    const no_leak_reward = b.option(bool, "no-leak-reward", "Enable NO_LEAK_REWARD") orelse (optimize == .Debug);
    const use_debug_alloc = b.option(bool, "use-debug-alloc", "Enable USE_DEBUG_ALLOC") orelse (optimize == .Debug);

    const lib_path = b.option([]const u8, "lib-path", "Add search path for system libraries (e.g. BlocksRuntime)");

    const exe = b.addExecutable(.{
        .name = if (optimize == .Debug) "app-dev" else "app",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });
    exe.use_llvm = true;

    const mod = exe.root_module;
    mod.link_libc = true;
    if (lib_path) |lp| {
        mod.addLibraryPath(std.Build.LazyPath{ .cwd_relative = lp });
    }

    mod.addIncludePath(b.path("include"));

    var src_files = std.ArrayList([]const u8).empty;
    defer src_files.deinit(b.allocator);

    findCFiles(b.graph.io, b.allocator, ".", &src_files, &[_][]const u8{"test.c"}, false) catch @panic("failed to find C files");

    findCFiles(b.graph.io, b.allocator, "src", &src_files, &[_][]const u8{}, true) catch @panic("failed to find C files in src");

    const active_flags = if (optimize == .Debug) &dev_c_flags else &release_c_flags;
    mod.addCSourceFiles(.{
        .files = src_files.items,
        .flags = active_flags,
    });

    if (horny_mode) {
        mod.addCMacro("HORNY_MODE", "1");
    }
    if (no_leak_reward) {
        mod.addCMacro("NO_LEAK_REWARD", "1");
    }
    if (use_debug_alloc) {
        mod.addCMacro("USE_DEBUG_ALLOC", "1");
    }

    mod.linkSystemLibrary("BlocksRuntime", .{});

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    const run_step = b.step("run", "Run the application");
    run_step.dependOn(&run_cmd.step);

    const translate_c = b.addTranslateC(.{
        .root_source_file = b.path("tests_c.h"),
        .target = target,
        .optimize = optimize,
    });
    translate_c.addIncludePath(b.path("include"));

    const main_tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("tests.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{
                .{
                    .name = "c",
                    .module = translate_c.createModule(),
                },
            },
        }),
        .test_runner = .{
            .path = b.path("test_runner.zig"),
            .mode = .simple,
        },
    });
    main_tests.use_llvm = true;
    main_tests.bundle_compiler_rt = true;
    const tests_mod = main_tests.root_module;
    tests_mod.link_libc = true;
    tests_mod.addIncludePath(b.path("include"));
    if (lib_path) |lp| {
        tests_mod.addLibraryPath(std.Build.LazyPath{ .cwd_relative = lp });
    }
    tests_mod.linkSystemLibrary("BlocksRuntime", .{});
    tests_mod.addCMacro("SIGMA_TESTING", "1");
    var test_harness_files = std.ArrayList([]const u8).empty;
    defer test_harness_files.deinit(b.allocator);

    findCFiles(b.graph.io, b.allocator, ".", &test_harness_files, &[_][]const u8{ "test.c", "main.c" }, false) catch @panic("failed to find C files for testing");
    findCFiles(b.graph.io, b.allocator, "src", &test_harness_files, &[_][]const u8{}, true) catch @panic("failed to find C files in src for testing");

    const active_test_flags = if (optimize == .Debug) &test_dev_flags else &test_release_flags;
    tests_mod.addCSourceFiles(.{
        .files = test_harness_files.items,
        .flags = active_test_flags,
    });

    const fuzz_tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("tests_fuzz.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{
                .{
                    .name = "c",
                    .module = translate_c.createModule(),
                },
            },
        }),
        .test_runner = .{
            .path = b.path("test_runner.zig"),
            .mode = .server,
        },
    });
    fuzz_tests.use_llvm = true;
    fuzz_tests.bundle_compiler_rt = true;
    const fuzz_mod = fuzz_tests.root_module;
    fuzz_mod.link_libc = true;
    fuzz_mod.addIncludePath(b.path("include"));
    if (lib_path) |lp| {
        fuzz_mod.addLibraryPath(std.Build.LazyPath{ .cwd_relative = lp });
    }
    fuzz_mod.linkSystemLibrary("BlocksRuntime", .{});
    fuzz_mod.addCMacro("SIGMA_TESTING", "1");
    fuzz_mod.addCSourceFiles(.{
        .files = test_harness_files.items,
        .flags = active_test_flags,
    });

    const run_fuzz_tests = b.addRunArtifact(fuzz_tests);
    const run_unit_tests = b.addRunArtifact(main_tests);
    const test_step = b.step("test", "Run allocator regression and fuzz tests");
    test_step.dependOn(&run_unit_tests.step);
    test_step.dependOn(&run_fuzz_tests.step);

    const fuzz_step = b.step("alloc-fuzz", "Run allocator fuzz tests; use with --fuzz=N for fuzzing");
    fuzz_step.dependOn(&run_fuzz_tests.step);

    const test_c_path = "test.c";
    var test_exists = false;
    if (std.Io.Dir.cwd().openFile(b.graph.io, test_c_path, .{})) |file| {
        file.close(b.graph.io);
        test_exists = true;
    } else |_| {}

    if (test_exists) {
        const c_tests_exe = b.addExecutable(.{
            .name = "tests",
            .root_module = b.createModule(.{
                .target = target,
                .optimize = optimize,
            }),
        });
        c_tests_exe.use_llvm = true;
        const c_tests_mod = c_tests_exe.root_module;
        c_tests_mod.link_libc = true;
        c_tests_mod.addIncludePath(b.path("include"));
        if (lib_path) |lp| {
            c_tests_mod.addLibraryPath(std.Build.LazyPath{ .cwd_relative = lp });
        }
        c_tests_mod.linkSystemLibrary("BlocksRuntime", .{});
        c_tests_mod.addCMacro("SIGMA_TESTING", "1");

        const test_c_flags = [_][]const u8{
            "-std=c23",
            "-fblocks",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
        };

        var test_src_files = std.ArrayList([]const u8).empty;
        defer test_src_files.deinit(b.allocator);

        findCFiles(b.graph.io, b.allocator, ".", &test_src_files, &[_][]const u8{"main.c"}, false) catch @panic("failed to find test C files");

        findCFiles(b.graph.io, b.allocator, "src", &test_src_files, &[_][]const u8{}, true) catch @panic("failed to find C files in src for tests");

        c_tests_mod.addCSourceFiles(.{
            .files = test_src_files.items,
            .flags = &test_c_flags,
        });

        b.installArtifact(c_tests_exe);
    }
}

fn findCFiles(
    io: std.Io,
    alloc: std.mem.Allocator,
    dir_path: []const u8,
    files: *std.ArrayList([]const u8),
    exclude: []const []const u8,
    recursive: bool,
) !void {
    var dir = std.Io.Dir.cwd().openDir(io, dir_path, .{ .iterate = true }) catch |err| {
        if (err == error.FileNotFound) return;
        return err;
    };
    defer dir.close(io);

    var iter = dir.iterate();
    outer: while (try iter.next(io)) |entry| {
        if (entry.kind == .directory and recursive) {
            if (std.mem.startsWith(u8, entry.name, ".")) continue;
            if (std.mem.eql(u8, entry.name, "zig-out")) continue;

            const sub_path = try std.fmt.allocPrint(alloc, "{s}/{s}", .{ dir_path, entry.name });
            try findCFiles(io, alloc, sub_path, files, exclude, true);
        } else if (std.mem.endsWith(u8, entry.name, ".c")) {
            for (exclude) |ex| {
                if (std.mem.eql(u8, entry.name, ex) or std.mem.endsWith(u8, ex, entry.name)) continue :outer;
            }
            const path = try std.fmt.allocPrint(alloc, "{s}/{s}", .{ dir_path, entry.name });
            try files.append(alloc, path);
        }
    }
}
