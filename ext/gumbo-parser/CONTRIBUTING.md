Contributing
===========
Bug reports are very much welcome.  Please use GitHub's issue-tracking feature, as it makes it easier to keep track of bugs and makes it possible for other project watchers to view the existing issues.

Patches and pull requests are also welcome, but before accepting patches, I need you to sign the Google Contributor License Agreement:

https://developers.google.com/open-source/cla/individual
https://developers.google.com/open-source/cla/corporate

(Electronic signatures are fine for individual contributors.)

If you're unwilling to do this, it would be most helpful if you could file bug reports that include detailed prose about where in the code the error is and how to fix it, but leave out exact source code.

Project priorities
==================

Gumbo's priorities are, in rough order:

1. Conformance to the HTML5 spec
2. Security & stability
3. Compatibility, both with previous versions and with different platforms (Visual Studio, Linux, Mac, other language bindings)
4. API simplicity
5. Performance
6. Features

Patches are much more likely to be accepted if they don't jeopardize values higher in the list for the sake of ones lower in the list.  So, we will happily take performance improvements that we can get for free, but not at the expense of complicating the API or reducing conformance.  We take patches to improve simplicity, but not at the expense of backwards compatibility.  We take new features only if they don't jeopardize any of the other traits (and are often quite conservative with them because of the backwards-compatibility and simplicity guarantees).

If you have a need for additional features beyond Gumbo's basic API, one option is to wrap Gumbo with another library, translating its data structures into ones more appropriate for your own use-case and then throwing away the original parse tree.  Gumbo was built for this; it's why most of the data structures are simple structs and the parse tree is intended to be immutable.  Tree traversal overhead is measured as negligible (~2-3%) compared to parsing time.  See eg. gumbo-libxml or gumbo-query for examples.

Code hygiene
============

We accept bugfixes even without this, but here are things you can do to make our lives as maintainers easier:

1. Write unit tests.  When you discover a bug, write a test case that exposes the bug first, and then fix the bug.  We use both Travis CI (Mac/Linux) and AppVeyor (Visual Studio) for continuous integration, and both automatically run against pull requests; this makes it much easier to verify that the patch is correct.
2. Break big changes up into smaller, atomic pull requests, each of which fixes one bug or adds one feature.  In particular, separate out performance improvements from bug/correctness fixes from new features, and separate out pull requests that need to go into different branches.
3. Work off the correct branch, and submit the pull request against it.  We follow Semantic Versioning, which means that we bump the patch number for internal-only bugfixes and performance improvements, the minor number for backwards-compatible API changes, and the major number for backwards-incompatible API changes.  There are branches for all of these.
4. For performance improvements, run benchmarks before and after, and include the numbers in the commit message.  There is a benchmarking program at ./benchmark that runs the parser against some common webpages.
5. Run clang-format after making your changes to make sure your code conforms to the existing style.  There is a .clang-format file already in the codebase; run it with 'clang-format -i src/*.{h,c}'.
