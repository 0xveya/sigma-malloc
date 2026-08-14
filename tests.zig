const std = @import("std");

const c = @import("c");

const SizeClass = struct {
    request: usize,
    class: usize,
};

const size_classes = [_]SizeClass{
    .{ .request = 1, .class = 16 },
    .{ .request = 16, .class = 16 },
    .{ .request = 17, .class = 32 },
    .{ .request = 32, .class = 32 },
    .{ .request = 33, .class = 64 },
    .{ .request = 64, .class = 64 },
    .{ .request = 65, .class = 128 },
    .{ .request = 128, .class = 128 },
    .{ .request = 129, .class = 256 },
    .{ .request = 256, .class = 256 },
    .{ .request = 257, .class = 512 },
    .{ .request = 512, .class = 512 },
    .{ .request = 513, .class = 768 },
    .{ .request = 768, .class = 768 },
    .{ .request = 769, .class = 1024 },
    .{ .request = 1024, .class = 1024 },
};

fn fill(ptr: *anyopaque, size: usize, seed: u8) void {
    const bytes: [*]volatile u8 = @ptrCast(ptr);
    for (0..size) |i| {
        bytes[i] = seed +% @as(u8, @truncate(i));
    }
}

fn allocChecked(size: usize) !*anyopaque {
    const ptr = c.sigma_alloc(&c.sigma_test_allocator, size, @alignOf(usize));
    try std.testing.expect(ptr != null);
    try std.testing.expectEqual(@as(usize, 0), @intFromPtr(ptr.?) % @alignOf(usize));
    return ptr.?;
}

fn freeFromAnotherThread(ptrs: []const *anyopaque) void {
    for (ptrs) |ptr| c.sigma_free(&c.sigma_test_allocator, ptr);
}

fn allocateLargeFromThread(slot: *?*anyopaque) void {
    slot.* = c.sigma_alloc(&c.sigma_test_allocator, 4 * 1024 * 1024 + 1, @alignOf(usize));
}

test "slab: every size class allocates writes and frees" {
    for (size_classes) |sc| {
        const ptr = try allocChecked(sc.request);
        fill(ptr, sc.request, @as(u8, @truncate(sc.class)));
        c.sigma_free(&c.sigma_test_allocator, ptr);
    }
}

test "slab: freed slots can be reused in every size class" {
    for (size_classes) |sc| {
        var ptrs: [48]*anyopaque = undefined;

        for (&ptrs, 0..) |*ptr, i| {
            ptr.* = try allocChecked(sc.request);
            fill(ptr.*, sc.request, @as(u8, @truncate(i)));
        }

        var i: usize = 0;
        while (i < ptrs.len) : (i += 2) {
            c.sigma_free(&c.sigma_test_allocator, ptrs[i]);
        }

        i = 0;
        while (i < ptrs.len) : (i += 2) {
            ptrs[i] = try allocChecked(sc.request);
            fill(ptrs[i], sc.request, @as(u8, @truncate(0x80 + i)));
        }

        for (ptrs) |ptr| {
            c.sigma_free(&c.sigma_test_allocator, ptr);
        }
    }
}

test "slab: boundary sizes stay reusable" {
    const sizes = [_]usize{ 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 511, 512, 513, 767, 768, 769, 1023, 1024 };
    var ptrs: [sizes.len]*anyopaque = undefined;

    for (sizes, 0..) |size, i| {
        ptrs[i] = try allocChecked(size);
        fill(ptrs[i], size, @as(u8, @truncate(size)));
    }

    for (ptrs) |ptr| c.sigma_free(&c.sigma_test_allocator, ptr);

    for (sizes, 0..) |size, i| {
        ptrs[i] = try allocChecked(size);
        fill(ptrs[i], size, @as(u8, @truncate(i + 11)));
    }

    for (ptrs) |ptr| c.sigma_free(&c.sigma_test_allocator, ptr);
}

test "slab: concurrent cross-thread frees are reclaimed by the owning arena" {
    var ptrs: [128]*anyopaque = undefined;
    for (&ptrs) |*ptr| {
        ptr.* = try allocChecked(64);
    }

    var threads: [4]std.Thread = undefined;
    for (0..threads.len) |i| {
        const start = i * 32;
        threads[i] = try std.Thread.spawn(.{}, freeFromAnotherThread, .{ptrs[start .. start + 32]});
    }
    for (threads) |thread| thread.join();

    // The owner drains remote frees every 64 allocations.  These allocations
    // must remain valid while that drain returns the remotely-freed objects.
    var replacements: [64]*anyopaque = undefined;
    for (&replacements) |*ptr| {
        ptr.* = try allocChecked(64);
        fill(ptr.*, 64, 0xa5);
    }
    for (replacements) |ptr| c.sigma_free(&c.sigma_test_allocator, ptr);
}

test "buddy: concurrent cross-thread frees are reclaimed by the owning arena" {
    var ptrs: [64]*anyopaque = undefined;
    for (&ptrs) |*ptr| {
        ptr.* = try allocChecked(4096);
    }

    var threads: [4]std.Thread = undefined;
    for (0..threads.len) |i| {
        const start = i * 16;
        threads[i] = try std.Thread.spawn(.{}, freeFromAnotherThread, .{ptrs[start .. start + 16]});
    }
    for (threads) |thread| thread.join();

    // These allocations run on the owning thread and drain the remote stack.
    var replacements: [64]*anyopaque = undefined;
    for (&replacements) |*ptr| {
        ptr.* = try allocChecked(4096);
        fill(ptr.*, 4096, 0x5a);
    }
    for (replacements) |ptr| c.sigma_free(&c.sigma_test_allocator, ptr);
}

test "buddy: boundary sizes allocate write and free" {
    const sizes = [_]usize{ 1025, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288 };

    for (sizes) |size| {
        const ptr = try allocChecked(size);
        fill(ptr, size, @as(u8, @truncate(size >> 8)));
        c.sigma_free(&c.sigma_test_allocator, ptr);
    }
}

test "buddy: pool recovers after many same-order frees" {
    var ptrs: [128]*anyopaque = undefined;
    const size = 4096;

    for (&ptrs, 0..) |*ptr, i| {
        ptr.* = try allocChecked(size);
        fill(ptr.*, size, @as(u8, @truncate(i)));
    }

    for (ptrs) |ptr| c.sigma_free(&c.sigma_test_allocator, ptr);

    for (&ptrs, 0..) |*ptr, i| {
        ptr.* = try allocChecked(size);
        fill(ptr.*, size, @as(u8, @truncate(0x40 + i)));
    }

    for (ptrs) |ptr| c.sigma_free(&c.sigma_test_allocator, ptr);
}

test "large: mmap allocations allocate write and free" {
    const sizes = [_]usize{ 4 * 1024 * 1024, 4 * 1024 * 1024 + 1, 6 * 1024 * 1024 };

    for (sizes) |size| {
        const ptr = try allocChecked(size);
        fill(ptr, size, @as(u8, @truncate(size >> 12)));
        c.sigma_free(&c.sigma_test_allocator, ptr);
    }
}

test "debug: leak collector finds slab buddy and large leaks" {
    if (c.sigma_debug_enabled() == 0) return error.SkipZigTest;

    c.sigma_debug_reset_leaks();

    const slab = try allocChecked(64);
    const buddy = try allocChecked(4096);
    const large = try allocChecked(4 * 1024 * 1024 + 128);

    try std.testing.expectEqual(@as(usize, 3), c.sigma_debug_collect_leaks(&c.sigma_test_allocator));
    try std.testing.expectEqual(@as(usize, 3), c.sigma_debug_leak_count());

    c.sigma_free(&c.sigma_test_allocator, slab);
    c.sigma_free(&c.sigma_test_allocator, buddy);
    c.sigma_free(&c.sigma_test_allocator, large);

    try std.testing.expectEqual(@as(usize, 0), c.sigma_debug_collect_leaks(&c.sigma_test_allocator));
    c.sigma_debug_reset_leaks();
}

test "debug: concurrent large allocations keep leak tracking consistent" {
    if (c.sigma_debug_enabled() == 0) return error.SkipZigTest;

    c.sigma_debug_reset_leaks();
    var ptrs: [8]?*anyopaque = .{null} ** 8;
    var threads: [ptrs.len]std.Thread = undefined;

    for (&ptrs, 0..) |*ptr, i| {
        threads[i] = try std.Thread.spawn(.{}, allocateLargeFromThread, .{ptr});
    }
    for (threads) |thread| thread.join();

    for (ptrs) |ptr| try std.testing.expect(ptr != null);
    try std.testing.expectEqual(ptrs.len, c.sigma_debug_collect_leaks(&c.sigma_test_allocator));

    for (ptrs) |ptr| c.sigma_free(&c.sigma_test_allocator, ptr.?);
    try std.testing.expectEqual(@as(usize, 0), c.sigma_debug_collect_leaks(&c.sigma_test_allocator));
}

test "mixed fragmentation: slab and buddy holes can be refilled" {
    var ptrs: [96]?*anyopaque = .{null} ** 96;

    for (&ptrs, 0..) |*ptr, i| {
        const size: usize = if (i % 3 == 0)
            32 + (i % 17) * 13
        else
            1025 + (i * 251);
        ptr.* = try allocChecked(size);
        fill(ptr.*.?, size, @as(u8, @truncate(i)));
    }

    var i: usize = 0;
    while (i < ptrs.len) : (i += 2) {
        c.sigma_free(&c.sigma_test_allocator, ptrs[i]);
        ptrs[i] = null;
    }

    i = 0;
    while (i < ptrs.len) : (i += 2) {
        const size: usize = if (i % 4 == 0) 64 + i * 3 else 2048 + i * 97;
        ptrs[i] = try allocChecked(size);
        fill(ptrs[i].?, size, @as(u8, @truncate(0x55 + i)));
    }

    for (ptrs) |ptr| {
        if (ptr) |p| c.sigma_free(&c.sigma_test_allocator, p);
    }
}

test "allocator: deterministic randomized churn" {
    var prng = std.Random.DefaultPrng.init(0x51a6_a110c);
    const random = prng.random();
    var slots: [64]?struct {
        ptr: *anyopaque,
        size: usize,
    } = .{null} ** 64;

    defer {
        for (slots) |slot| {
            if (slot) |s| c.sigma_free(&c.sigma_test_allocator, s.ptr);
        }
    }

    for (0..8000) |_| {
        const index = random.uintLessThan(usize, slots.len);
        if (slots[index]) |slot| {
            c.sigma_free(&c.sigma_test_allocator, slot.ptr);
            slots[index] = null;
            continue;
        }

        const bucket = random.uintLessThan(u8, 10);
        const size: usize = if (bucket < 7)
            1 + random.uintLessThan(usize, 1024)
        else
            1025 + random.uintLessThan(usize, 32 * 1024);

        const ptr = try allocChecked(size);
        fill(ptr, size, @as(u8, @truncate(index)));
        slots[index] = .{ .ptr = ptr, .size = size };
    }
}

test "allocator: deterministic fuzz-sized single allocation cases" {
    var prng = std.Random.DefaultPrng.init(0xf00d_cafe_5151);
    const random = prng.random();

    for (0..1500) |_| {
        const size = 1 + random.uintLessThan(usize, 256 * 1024);
        const ptr = try allocChecked(size);
        fill(ptr, size, @as(u8, @truncate(size)));
        c.sigma_free(&c.sigma_test_allocator, ptr);
    }
}

test "generic allocator: alignment calloc and realloc" {
    const sigma = c.sigma_allocator(&c.sigma_test_allocator);
    const alignments = [_]usize{ 16, 64, 4096 };

    for (alignments) |alignment| {
        const ptr = c.allocator_alloc_aligned(sigma, 8192, alignment);
        try std.testing.expect(ptr != null);
        try std.testing.expectEqual(@as(usize, 0), @intFromPtr(ptr.?) % alignment);
        c.allocator_free_aligned(sigma, ptr, 8192, alignment);
    }

    const zeroed = c.sigma_calloc(&c.sigma_test_allocator, 256, 1, 64);
    try std.testing.expect(zeroed != null);
    const zeroed_bytes: [*]const u8 = @ptrCast(zeroed.?);
    for (0..256) |i| try std.testing.expectEqual(@as(u8, 0), zeroed_bytes[i]);

    const grown = c.sigma_realloc(&c.sigma_test_allocator, zeroed, 256, 1024, 64);
    try std.testing.expect(grown != null);
    try std.testing.expectEqual(@as(usize, 0), @intFromPtr(grown.?) % 64);
    const grown_bytes: [*]const u8 = @ptrCast(grown.?);
    for (0..256) |i| try std.testing.expectEqual(@as(u8, 0), grown_bytes[i]);
    c.sigma_free(&c.sigma_test_allocator, grown);
}

test "arena allocator: composes over sigma and releases as one lifetime" {
    var arena: c.allocator_arena_t = undefined;
    c.allocator_arena_init(&arena, c.sigma_allocator(&c.sigma_test_allocator), 4096);
    defer c.allocator_arena_deinit(&arena);

    const allocator = c.allocator_arena(&arena);
    const first = c.allocator_alloc_aligned(allocator, 100, 64);
    const second = c.allocator_alloc_aligned(allocator, 9000, 4096);
    try std.testing.expect(first != null);
    try std.testing.expect(second != null);
    try std.testing.expectEqual(@as(usize, 0), @intFromPtr(first.?) % 64);
    try std.testing.expectEqual(@as(usize, 0), @intFromPtr(second.?) % 4096);

    const grown = c.allocator_realloc_aligned(allocator, first, 100, 300, 64);
    try std.testing.expect(grown != null);
    try std.testing.expectEqual(@as(usize, 0), @intFromPtr(grown.?) % 64);

    c.allocator_arena_reset(&arena);
    try std.testing.expect(arena.blocks == null);
}

test "debug: allocation source survives realloc" {
    if (c.sigma_debug_enabled() == 0) return error.SkipZigTest;

    const file = "debug-origin.c";
    const function = "debug_origin";
    const ptr = c.sigma_alloc_debug(&c.sigma_test_allocator, 32, @alignOf(usize), file, function, 4242);
    try std.testing.expect(ptr != null);

    const grown = c.sigma_realloc(&c.sigma_test_allocator, ptr, 32, 4096, 64);
    try std.testing.expect(grown != null);
    defer c.sigma_free(&c.sigma_test_allocator, grown);

    var info: c.LeakInfo = undefined;
    try std.testing.expect(c.sigma_debug_allocation_info(grown, &info));
    try std.testing.expectEqual(@as(c_int, 4242), info.line);
    try std.testing.expectEqualStrings(file, std.mem.span(info.file));
    try std.testing.expectEqualStrings(function, std.mem.span(info.func));
    try std.testing.expectEqual(@as(usize, 4096), info.size);
}

test "debug: arena forwards allocation source to its parent" {
    if (c.sigma_debug_enabled() == 0) return error.SkipZigTest;

    var sigma: c.sigma_allocator_t = undefined;
    c.sigma_allocator_init(&sigma, &c.mmap_memory_source);

    var arena: c.allocator_arena_t = undefined;
    c.allocator_arena_init(&arena, c.sigma_allocator(&sigma), 4096);
    defer c.allocator_arena_deinit(&arena);

    const allocator = c.allocator_arena(&arena);
    const ptr = c.allocator_alloc_aligned_debug(
        allocator,
        64,
        @alignOf(usize),
        "arena-origin.c",
        "arena_origin",
        6767,
    );
    try std.testing.expect(ptr != null);
    try std.testing.expectEqual(@as(usize, 1), c.sigma_debug_collect_leaks(&sigma));

    var info: c.LeakInfo = undefined;
    try std.testing.expect(c.sigma_debug_leak_info(0, &info));
    try std.testing.expectEqualStrings("arena-origin.c", std.mem.span(info.file));
    try std.testing.expectEqualStrings("arena_origin", std.mem.span(info.func));
    try std.testing.expectEqual(@as(c_int, 6767), info.line);
}
