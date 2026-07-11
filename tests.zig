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
    const ptr = c.balls_backend(size);
    try std.testing.expect(ptr != null);
    try std.testing.expectEqual(@as(usize, 0), @intFromPtr(ptr.?) % @alignOf(usize));
    return ptr.?;
}

test "slab: every size class allocates writes and frees" {
    for (size_classes) |sc| {
        const ptr = try allocChecked(sc.request);
        fill(ptr, sc.request, @as(u8, @truncate(sc.class)));
        c.cock(ptr);
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
            c.cock(ptrs[i]);
        }

        i = 0;
        while (i < ptrs.len) : (i += 2) {
            ptrs[i] = try allocChecked(sc.request);
            fill(ptrs[i], sc.request, @as(u8, @truncate(0x80 + i)));
        }

        for (ptrs) |ptr| {
            c.cock(ptr);
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

    for (ptrs) |ptr| c.cock(ptr);

    for (sizes, 0..) |size, i| {
        ptrs[i] = try allocChecked(size);
        fill(ptrs[i], size, @as(u8, @truncate(i + 11)));
    }

    for (ptrs) |ptr| c.cock(ptr);
}

test "buddy: boundary sizes allocate write and free" {
    const sizes = [_]usize{ 1025, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288 };

    for (sizes) |size| {
        const ptr = try allocChecked(size);
        fill(ptr, size, @as(u8, @truncate(size >> 8)));
        c.cock(ptr);
    }
}

test "buddy: pool recovers after many same-order frees" {
    var ptrs: [128]*anyopaque = undefined;
    const size = 4096;

    for (&ptrs, 0..) |*ptr, i| {
        ptr.* = try allocChecked(size);
        fill(ptr.*, size, @as(u8, @truncate(i)));
    }

    for (ptrs) |ptr| c.cock(ptr);

    for (&ptrs, 0..) |*ptr, i| {
        ptr.* = try allocChecked(size);
        fill(ptr.*, size, @as(u8, @truncate(0x40 + i)));
    }

    for (ptrs) |ptr| c.cock(ptr);
}

test "large: mmap allocations allocate write and free" {
    const sizes = [_]usize{ 4 * 1024 * 1024, 4 * 1024 * 1024 + 1, 6 * 1024 * 1024 };

    for (sizes) |size| {
        const ptr = try allocChecked(size);
        fill(ptr, size, @as(u8, @truncate(size >> 12)));
        c.cock(ptr);
    }
}

test "debug: leak collector finds slab buddy and large leaks" {
    if (c.sigma_debug_enabled() == 0) return error.SkipZigTest;

    c.sigma_debug_reset_leaks();

    const slab = try allocChecked(64);
    const buddy = try allocChecked(4096);
    const large = try allocChecked(4 * 1024 * 1024 + 128);

    try std.testing.expectEqual(@as(usize, 3), c.sigma_debug_collect_leaks());
    try std.testing.expectEqual(@as(usize, 3), c.sigma_debug_leak_count());

    c.cock(slab);
    c.cock(buddy);
    c.cock(large);

    try std.testing.expectEqual(@as(usize, 0), c.sigma_debug_collect_leaks());
    c.sigma_debug_reset_leaks();
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
        c.cock(ptrs[i]);
        ptrs[i] = null;
    }

    i = 0;
    while (i < ptrs.len) : (i += 2) {
        const size: usize = if (i % 4 == 0) 64 + i * 3 else 2048 + i * 97;
        ptrs[i] = try allocChecked(size);
        fill(ptrs[i].?, size, @as(u8, @truncate(0x55 + i)));
    }

    for (ptrs) |ptr| {
        if (ptr) |p| c.cock(p);
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
            if (slot) |s| c.cock(s.ptr);
        }
    }

    for (0..8000) |_| {
        const index = random.uintLessThan(usize, slots.len);
        if (slots[index]) |slot| {
            c.cock(slot.ptr);
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
        c.cock(ptr);
    }
}
