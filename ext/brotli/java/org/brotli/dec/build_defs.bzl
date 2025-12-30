"""Utilities for Java brotli tests."""

load("@rules_java//java:java_test.bzl", "java_test")

_TEST_JVM_FLAGS = [
    "-DBROTLI_ENABLE_ASSERTS=true",
]

_KOTLIN_DECODER_DEPS = [
    "//org/brotli/dec/kt:dec",
]

def brotli_java_test(name, main_class = None, jvm_flags = None, test_kotlin = False, runtime_deps = [], **kwargs):
    """test duplication rule that creates 32/64-bit test pair.

    Args:
       name: target name prefix
       main_class: override for test_class
       jvm_flags: base Java VM options
       test_kotlin: add target for Kotlin BrotliInputStream
       runtime_deps: runtime target dependencies
       **kwargs: pass-through
    """

    if jvm_flags == None:
        jvm_flags = []
    jvm_flags = jvm_flags + _TEST_JVM_FLAGS

    test_package = native.package_name().replace("/", ".").replace("third_party.brotli.java.", "")

    if main_class == None:
        test_class = test_package + "." + name
    else:
        test_class = None
    java_test(
        name = name + "_32",
        main_class = main_class,
        test_class = test_class,
        jvm_flags = jvm_flags + ["-DBROTLI_32_BIT_CPU=true"],
        visibility = ["//visibility:private"],
        runtime_deps = runtime_deps,
        **kwargs
    )
    java_test(
        name = name + "_64",
        main_class = main_class,
        test_class = test_class,
        jvm_flags = jvm_flags + ["-DBROTLI_32_BIT_CPU=false"],
        visibility = ["//visibility:private"],
        runtime_deps = runtime_deps,
        **kwargs
    )

    if test_kotlin:
        java_test(
            name = name + "_kt",
            main_class = main_class,
            test_class = test_class,
            jvm_flags = jvm_flags + ["-DBROTLI_INPUT_STREAM=org.brotli.dec.kt.BrotliInputStream"],
            visibility = ["//visibility:private"],
            runtime_deps = runtime_deps + _KOTLIN_DECODER_DEPS,
            **kwargs
        )
