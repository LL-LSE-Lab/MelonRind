set_xmakever("3.0.0")
add_rules("mode.debug", "mode.release")
set_languages("c++20")
set_allowedplats("windows")
set_allowedarchs("x64")
set_runtimes("MD")

option("target_type")
    set_default("client")
    set_values("client")
option_end()
option("model_tests_only")
    set_default(false)
    set_showmenu(true)
option_end()

if not has_config("model_tests_only") then
    add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")
    add_requireconfs("levilamina.bedrockdata", {override = true, version = "v26.51.1-client.7"})
    add_requires("levilamina 26.51.6", {configs = {target_type = "client"}})
    add_requires("levibuildscript 0.6.1")

    target("MelonRind")
        set_kind("shared")
        set_toolchains("clang-cl")
        set_symbols("debug")
        add_rules("@levibuildscript/linkrule")
        add_rules("@levibuildscript/modpacker", {modVersion = "0.1.2"})
        add_packages("levilamina")
        add_syslinks("Version")
        add_includedirs("src")
        add_files("src/**.cpp")
        add_defines("NOMINMAX", "UNICODE", "LL_PLAT_C")
        set_exceptions("none")
        add_cxflags(
            "/EHa", "/utf-8", "/W4", "/permissive-",
            "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204"
        )
        add_cxflags(
            "/EHs",
            "-Wno-microsoft-cast",
            "-Wno-invalid-offsetof",
            "-Wno-c++2b-extensions",
            "-Wno-microsoft-include",
            "-Wno-overloaded-virtual",
            "-Wno-ignored-qualifiers",
            "-Wno-missing-field-initializers",
            "-Wno-potentially-evaluated-expression",
            "-Wno-pragma-system-header-outside-header",
            {tools = {"clang_cl"}}
        )
    target_end()
end

target("model-tests")
    set_kind("binary")
    set_toolchains("msvc")
    add_includedirs("src")
    add_files("tests/model_tests.cpp", "src/model/*.cpp")
    add_cxflags("/utf-8", "/W4", "/EHsc")
target_end()

