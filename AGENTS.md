## Builds and worktree cleanup

Build inside the existing worktree, under ignored `build/` directories. Use the
root Make targets documented in [docs/building.md](docs/building.md); they resolve
paths from their own checkout even when invoked from another working directory.
Reuse each target/configuration's stable build directory incrementally. Serialize
builds sharing a directory, or use distinct configuration/build directories for
simultaneous builds. A fresh build or output outside the worktree needs a concrete
technical reason; do not create a new `/tmp` build for each edit, review, or retry.
Keep the core tests' default RelWithDebInfo configuration and enabled assertions.

Finish validation and land first. Preserve unique experiment results and any
other needed ignored files outside the worktree before archiving/removing it
through the tool that owns it (Codex managed worktrees use `archive_worktree`).
There is no need to run `make clean` before removing the checkout. When retaining
the worktree, `make clean` reclaims only the documented canonical build trees;
keep unique results under `build/results/` or another location outside those
disposable trees. Never clean while a build or test is using the same outputs.

## C++ style

prompts will be made using Talent Voice, adapt to any artifacts this may cause
* always use matched braces (opening brace on new line)
* member variables start with m_, constants with x_
* member functions are HammerCase
* never use c-style casts, static_cast and the like preferred
* Do not use default arguments in C++ function or constructor declarations. Pass arguments explicitly or provide meaningful overloads.
* enum class preferred for enums, values are also HammerCase
* always prefer structs to classes.
* Don't create privates, everything public
* comments end with a line containing only // (add // on a separate line after each comment)
* empty line after a closing brace, unless the next line is another closing brace or matching else.  
* No comments after code: don't do `f(x); // apply f to x`
* No comments after member vars, don't do this: `bool m_thing; // this is a thing`
Proper code looks like this:

// This is a comment with an empty line below
//
if (something)
{
    // apply f to x
    //
    f(x)
}

if (somthing else)
{
    g(y);
}
else
{
    h(z);
}
