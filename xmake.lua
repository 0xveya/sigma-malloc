set_project("sigma_malloc")
set_version("0.2.4")
set_languages("c23")
set_toolchains("clang")
set_toolset("ld", "clang")
add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

package("sigma_sys")
    set_homepage("https://github.com/0xveya/sigma_libft")
    set_description("Sigma's typed Linux syscall boundary")
    add_urls("https://github.com/0xveya/sigma_libft.git")
    on_install(function (package)
        import("package.tools.xmake").install(package,
            {sigma_sys_only = true}, {target = "sigma_sys"})
    end)
package_end()
add_requires("sigma_sys v0.4.0", {system = false})

option("malloc_backend")
    set_default(false)
    set_showmenu(true)
option_end()

option("parser_example_free")
    set_default(true)
    set_showmenu(true)
option_end()

local function configure(target_name)
    target(target_name)
        set_warnings("all", "extra", "pedantic")
        add_cflags("-Wno-auto-decl-extensions", "-Wshadow", "-Wconversion", "-Wdouble-promotion", "-Wformat=2", "-Wundef", {force = true})
        add_includedirs("include", {public = true})
        add_packages("sigma_sys", {public = true})
        add_cflags("-ffreestanding", "-fno-builtin", "-fno-stack-protector", {force = true})
        if is_mode("debug") then
            add_defines("HORNY_MODE=1", "NO_LEAK_REWARD=1", "USE_DEBUG_ALLOC=1")
        else
            add_cflags("-fno-common", {force = true})
        end
        if has_config("malloc_backend") then
            add_defines("SIGMA_MALLOC_BACKEND=1")
        end
end

configure("sigma_malloc")
    set_kind("static")
    add_headerfiles("include/(**.h)")
    add_files("src/**.c")

configure("app")
    set_kind("binary")
    add_deps("sigma_malloc")
    add_files("main.c")

configure("parser-example")
    set_kind("binary")
    set_default(false)
    add_deps("sigma_malloc")
    add_includedirs("examples")
    add_files("examples/main.c", "examples/parser.c")
    if not has_config("parser_example_free") then
        add_defines("PARSER_EXAMPLE_FREE=0")
    end

configure("allocator-stress")
    set_kind("binary")
    set_default(false)
    add_deps("sigma_malloc")
    add_defines("SIGMA_TESTING=1")
    add_includedirs("tests/stress")
    add_files("tests/stress/main.c")
    add_tests("single", {runargs = {"--allocator", "custom", "--target", "2G", "--max-size", "64K", "--slots", "4096", "--cycles", "2", "--verify", "full", "--seed", "12345", "--output", "quiet"}})
    add_tests("threaded", {runargs = {"--allocator", "custom", "--threads", "4", "--target", "8M", "--max-size", "64K", "--slots", "2048", "--cycles", "3", "--verify", "full", "--seed", "12345", "--output", "quiet"}})
    add_tests("arena", {runargs = {"--allocator", "custom-arena", "--target", "8M", "--max-size", "64K", "--slots", "2048", "--cycles", "2", "--verify", "full", "--seed", "12345", "--output", "quiet"}})

configure("allocator-tests")
    set_kind("binary")
    set_default(false)
    add_deps("sigma_malloc")
    add_defines("SIGMA_TESTING=1")
    add_includedirs("tests")
    add_syslinks("pthread")
    add_files("tests/regression.c", "tests/test_support.c")
    local tests = {
        "slab: every size class allocates writes and frees", "slab: freed slots can be reused in every size class", "slab: boundary sizes stay reusable",
        "slab: concurrent cross-thread frees are reclaimed by the owning arena", "buddy: concurrent cross-thread frees are reclaimed by the owning arena",
        "buddy: boundary sizes allocate write and free", "buddy: pool recovers after many same-order frees", "large: mmap allocations allocate write and free",
        "debug: leak collector finds slab buddy and large leaks", "debug: concurrent large allocations keep leak tracking consistent",
        "mixed fragmentation: slab and buddy holes can be refilled", "allocator: deterministic randomized churn",
        "allocator: deterministic fuzz-sized single allocation cases", "generic allocator: alignment calloc and realloc",
        "arena allocator: composes over sigma and releases as one lifetime", "debug: allocation source survives realloc",
        "debug: arena forwards allocation source to its parent", "fuzz: allocator single allocation sizes", "fuzz: allocator single threaded operation sequences"
    }
    for _, name in ipairs(tests) do add_tests(name, {runargs = {name}}) end
